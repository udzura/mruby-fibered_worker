/*
** mrb_fiberedworker.c - FiberedWorker class
**
** Copyright (c) Uchio Kondo 2018
**
** See Copyright Notice in LICENSE
*/

#include "mrb_fiberedworker.h"
#include <errno.h>
#include <mruby.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/error.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "signames.defs"

#define DONE mrb_gc_arena_restore(mrb, 0);

static int signm2signo(const char *nm)
{
  const struct signals *sigs;

  for (sigs = siglist; sigs->signm; sigs++) {
    if (strcmp(sigs->signm, nm) == 0) {
      return sigs->signo;
    }
  }

  /* Handle RT Signal#0 as special for strtol's err spec */
  if (strcmp("RT0", nm) == 0) {
    return SIGRTMIN;
  }

  if (strncmp("RT", nm, 2) == 0) {
    int ret = (int)strtol(nm + 2, NULL, 0);
    if (!ret || (SIGRTMIN + ret > SIGRTMAX)) {
      return 0;
    }
    return SIGRTMIN + ret;
  }
  return 0;
}

static int mrb_to_signo(mrb_state *mrb, mrb_value vsig)
{
  int sig = -1;
  const char *s;

  switch (mrb_type(vsig)) {
  case MRB_TT_FIXNUM:
    sig = mrb_fixnum(vsig);
    if (sig < 0 || sig >= SIGRTMAX) {
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid signal number (%S)", vsig);
    }
    break;
  case MRB_TT_SYMBOL:
    s = mrb_sym2name(mrb, mrb_symbol(vsig));
    if (!s) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "bad signal");
    }
    goto str_signal;
  default:
    vsig = mrb_string_type(mrb, vsig);
    s = RSTRING_PTR(vsig);

  str_signal:
    if (memcmp("SIG", s, 3) == 0) {
      s += 3;
    }
    sig = signm2signo(s);
    if (sig == 0 && strcmp(s, "EXIT") != 0) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "unsupported signal");
    }
    break;
  }
  return sig;
}

static mrb_value mrb_fw_sigprocmask(mrb_state *mrb, mrb_value self)
{
  mrb_value *signals;
  mrb_int siglen;
  mrb_get_args(mrb, "a", &signals, &siglen);
  int i;
  sigset_t current_set;
  if (sigemptyset(&current_set) < 0) {
    mrb_sys_fail(mrb, "sigemptyset");
  }

  for (i = 0; i < siglen; i++) {
    int signo_idx = mrb_to_signo(mrb, signals[i]);
    if (sigaddset(&current_set, signo_idx) < 0) {
      mrb_sys_fail(mrb, "sigaddset");
    }
  }

  if (sigprocmask(SIG_BLOCK, &current_set, NULL) < 0) {
    mrb_sys_fail(mrb, "sigprocmask");
  }

  return mrb_fixnum_value(siglen);
}

static mrb_value mrb_fw_sigtimedwait(mrb_state *mrb, mrb_value self)
{
  mrb_value *signals;
  mrb_int siglen;
  mrb_int interval_msec = 0;
  mrb_get_args(mrb, "ai", &signals, &siglen, &interval_msec);
  sigset_t current_set;
  siginfo_t sig;
  struct timespec ts;

  if (sigemptyset(&current_set) < 0) {
    mrb_sys_fail(mrb, "sigemptyset");
  }

  int i;
  for (i = 0; i < siglen; i++) {
    int signo_idx = mrb_to_signo(mrb, signals[i]);
    if (sigaddset(&current_set, signo_idx) < 0) {
      mrb_sys_fail(mrb, "sigaddset");
    }
  }

  mrb_assert(interval_msec >= 0);
  ts.tv_sec = (time_t)(interval_msec / 1000);
  ts.tv_nsec = (long)((interval_msec - ts.tv_sec * 1000) * 1000 * 1000);

  if (sigtimedwait(&current_set, &sig, &ts) == -1) {
    switch (errno) {
    case EAGAIN:
    case EINTR:
      return mrb_nil_value();
    default:
      mrb_sys_fail(mrb, "sigtimedwait");
      return mrb_nil_value();
    }
  } else {
    return mrb_fixnum_value(sig.si_signo);
  }
}

static mrb_value mrb_fw_do_nonblocking_fd(mrb_state *mrb, mrb_value self)
{
  int val;
  mrb_int fd;
  mrb_get_args(mrb, "i", &fd);

  val = fcntl((int)fd, F_GETFL, 0);
  if(val < 0) {
    mrb_sys_fail(mrb, "fcntl F_GETFL");
  }

  if(val & O_NONBLOCK) {
    return mrb_true_value();
  } else {
    if(fcntl((int)fd, F_SETFL, val | O_NONBLOCK) < 0){
      mrb_sys_fail(mrb, "fcntl F_SETFL");
    }
    return mrb_true_value();
  }
}

static mrb_value mrb_fw_read_nonblock(mrb_state *mrb, mrb_value self)
{
  int ret;
  mrb_int fd;
  uint64_t result;
  mrb_get_args(mrb, "i", &fd);
  ret = read(fd, &result, sizeof(result));
  if (ret == -1) {
    if (errno == EAGAIN) {
      return mrb_nil_value();
    } else {
      mrb_sys_fail(mrb, "read nonblock");
    }
  }

  return mrb_fixnum_value((mrb_int)result);
}

static mrb_value mrb_fw_obj2signo(mrb_state *mrb, mrb_value self)
{
  mrb_value signo;
  mrb_get_args(mrb, "o", &signo);
  int signo_idx = mrb_to_signo(mrb, signo);

  return mrb_fixnum_value((mrb_int)signo_idx);
}

// FiberedWorker::Timer
typedef struct {
  timer_t *timer_ptr;
  int timer_signo;
  clock_t clockid;
} mrb_fw_timer_data;

static void mrb_timer_posix_free(mrb_state *mrb, void *p)
{
  mrb_fw_timer_data *data = (mrb_fw_timer_data *)p;
  timer_delete(*data->timer_ptr);
  mrb_free(mrb, data->timer_ptr);
  mrb_free(mrb, data);
}
static const struct mrb_data_type mrb_timer_posix_data_type = {"mrb_fw_timer_data", mrb_timer_posix_free};

static mrb_value mrb_timer_posix_init(mrb_state *mrb, mrb_value self)
{
  mrb_fw_timer_data *data;
  timer_t *timer_ptr;
  mrb_value signo;
  struct sigevent sev;
  memset(&sev, 0, sizeof(struct sigevent));

  mrb_get_args(mrb, "o", &signo);
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = mrb_to_signo(mrb, signo);

  timer_ptr = (timer_t *)mrb_malloc(mrb, sizeof(timer_t));
  if (timer_create(CLOCK_REALTIME, &sev, timer_ptr) == -1) {
    mrb_sys_fail(mrb, "timer_create failed");
  }
  data = (mrb_fw_timer_data *)mrb_malloc(mrb, sizeof(mrb_fw_timer_data));
  data->timer_ptr = timer_ptr;
  data->timer_signo = sev.sigev_signo;
  data->clockid = CLOCK_REALTIME;

  DATA_TYPE(self) = &mrb_timer_posix_data_type;
  DATA_PTR(self) = data;
  return self;
}

static int mrb_set_itimerspec(mrb_int start, mrb_int start_nsec, mrb_int interval, mrb_int interval_nsec, struct itimerspec *ts)
{
  if (start < 0 || interval < 0 || ts == NULL) {
    errno = EINVAL;
    return -1;
  }

  ts->it_value.tv_sec = (time_t)start;
  ts->it_value.tv_nsec = (long)start_nsec;

  ts->it_interval.tv_sec = (time_t)interval;
  ts->it_interval.tv_nsec = (long)interval_nsec;

  return 0;
}

static mrb_value mrb_timer_posix_start(mrb_state *mrb, mrb_value self)
{
  mrb_fw_timer_data *data = DATA_PTR(self);
  mrb_int start, interval = -1;
  mrb_int s_sec, s_nsec, i_sec = 0, i_nsec = 0;
  struct itimerspec ts;

  /* start and interval should be **msec** */
  if (mrb_get_args(mrb, "i|i", &start, &interval) == -1) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Cannot get arguments");
  }

  s_sec = (mrb_int)(start / 1000);
  s_nsec = (start % 1000) * 1000000;

  if (interval >= 0) {
    i_sec = (mrb_int)(interval / 1000);
    i_nsec = (interval % 1000) * 1000000;
  }

  if (mrb_set_itimerspec(s_sec, s_nsec, i_sec, i_nsec, &ts) == -1) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Values must be 0 or positive");
  }

  if (timer_settime(*(data->timer_ptr), 0, &ts, NULL) == -1) {
    mrb_sys_fail(mrb, "timer_settime");
  }

  return self;
}

static mrb_value mrb_timer_posix_stop(mrb_state *mrb, mrb_value self)
{
  mrb_fw_timer_data *data = DATA_PTR(self);
  struct itimerspec ts;
  if (mrb_set_itimerspec(0, 0, 0, 0, &ts) == -1) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid value for stop");
  }

  if (timer_settime(*(data->timer_ptr), 0, &ts, NULL) == -1) {
    mrb_sys_fail(mrb, "timer_settime");
  }

  return self;
}

static mrb_value mrb_timer_posix_status_raw(mrb_state *mrb, mrb_value self)
{
  mrb_fw_timer_data *data = DATA_PTR(self);
  struct itimerspec ts;
  mrb_value ret;

  if (timer_gettime(*(data->timer_ptr), &ts) == -1) {
    mrb_sys_fail(mrb, "timer_gettime");
  }

  ret = mrb_hash_new_capa(mrb, 2);
  mrb_hash_set(mrb, ret, mrb_str_new_lit(mrb, "value.sec"), mrb_fixnum_value((mrb_int)ts.it_value.tv_sec));
  mrb_hash_set(mrb, ret, mrb_str_new_lit(mrb, "value.nsec"), mrb_fixnum_value((mrb_int)ts.it_value.tv_nsec));
  mrb_hash_set(mrb, ret, mrb_str_new_lit(mrb, "interval.sec"), mrb_fixnum_value((mrb_int)ts.it_interval.tv_sec));
  mrb_hash_set(mrb, ret, mrb_str_new_lit(mrb, "interval.nsec"), mrb_fixnum_value((mrb_int)ts.it_interval.tv_nsec));

  return ret;
}

static mrb_value mrb_timer_posix_is_running(mrb_state *mrb, mrb_value self)
{
  mrb_fw_timer_data *data = DATA_PTR(self);
  struct itimerspec ts;

  if (timer_gettime(*(data->timer_ptr), &ts) == -1) {
    mrb_sys_fail(mrb, "timer_gettime");
  }
  return mrb_bool_value(ts.it_value.tv_sec || ts.it_value.tv_nsec);
}

void mrb_mruby_fibered_worker_gem_init(mrb_state *mrb)
{
  struct RClass *fiberedworker, *timer;

  fiberedworker = mrb_define_module(mrb, "FiberedWorker");
  mrb_define_module_function(mrb, fiberedworker, "sigprocmask", mrb_fw_sigprocmask, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, fiberedworker, "sigtimedwait", mrb_fw_sigtimedwait, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, fiberedworker, "nonblocking_fd!", mrb_fw_do_nonblocking_fd, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, fiberedworker, "read_nonblock", mrb_fw_read_nonblock, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, fiberedworker, "obj2signo", mrb_fw_obj2signo, MRB_ARGS_REQ(1));

  mrb_define_const(mrb, fiberedworker, "SIGINT", mrb_fixnum_value(SIGINT));
  mrb_define_const(mrb, fiberedworker, "SIGHUP", mrb_fixnum_value(SIGHUP));
  mrb_define_const(mrb, fiberedworker, "SIGTERM", mrb_fixnum_value(SIGTERM));
  mrb_define_const(mrb, fiberedworker, "SIGRTMIN", mrb_fixnum_value(SIGRTMIN));
  mrb_define_const(mrb, fiberedworker, "SIGRTMAX", mrb_fixnum_value(SIGRTMAX));

  timer = mrb_define_class_under(mrb, fiberedworker, "Timer", mrb->object_class);
  MRB_SET_INSTANCE_TT(timer, MRB_TT_DATA);
  mrb_define_method(mrb, timer, "initialize", mrb_timer_posix_init, MRB_ARGS_ARG(0, 1));
  mrb_define_method(mrb, timer, "start", mrb_timer_posix_start, MRB_ARGS_ARG(1, 1));
  mrb_define_method(mrb, timer, "stop", mrb_timer_posix_stop, MRB_ARGS_NONE());
  mrb_define_method(mrb, timer, "__status_raw", mrb_timer_posix_status_raw, MRB_ARGS_NONE());
  mrb_define_method(mrb, timer, "running?", mrb_timer_posix_is_running, MRB_ARGS_NONE());

  // mrb_define_method(mrb, timer, "signo", mrb_timer_posix_signo, MRB_ARGS_NONE());
  // mrb_define_method(mrb, timer, "clock_id", mrb_timer_posix_clockid, MRB_ARGS_NONE());

  /*   EXPORT_CLOCK_CONST(CLOCK_REALTIME); */
  /*   EXPORT_CLOCK_CONST(CLOCK_MONOTONIC); */
  /*   EXPORT_CLOCK_CONST(CLOCK_PROCESS_CPUTIME_ID); */
  /*   EXPORT_CLOCK_CONST(CLOCK_THREAD_CPUTIME_ID); */
  /* #ifdef CLOCK_BOOTTIME */
  /*   EXPORT_CLOCK_CONST(CLOCK_BOOTTIME); */
  /* #endif */
  /* #ifdef CLOCK_REALTIME_ALARM */
  /*   EXPORT_CLOCK_CONST(CLOCK_REALTIME_ALARM); */
  /* #endif */
  /* #ifdef CLOCK_BOOTTIME_ALARM */
  /*   EXPORT_CLOCK_CONST(CLOCK_BOOTTIME_ALARM); */
  /* #endif */
  DONE;
}

void mrb_mruby_fibered_worker_gem_final(mrb_state *mrb)
{
}
