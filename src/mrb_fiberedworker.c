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
#include <signal.h>
#include <time.h>

#define DONE mrb_gc_arena_restore(mrb, 0);

int mrb_signo_registered[64];
int mrb_signo_signaled[64];

static void mrb_fw_sighandler_func(int signo)
{
  mrb_signo_signaled[signo] = 1;
}

static int mrb_fw__is_registerd_signal(int signo)
{
  return mrb_signo_registered[signo];
}

static mrb_value mrb_fw_register_internal_handler(mrb_state *mrb, mrb_value self)
{
  mrb_int signo;
  mrb_get_args(mrb, "i", &signo);
  int signo_idx = (int)signo;
  struct sigaction act;
  if (mrb_fw__is_registerd_signal(signo_idx)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Signal already registered");
  }
  if (signo_idx > SIGRTMAX) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Signal NR must be less than SIGRTMAX");
  }

  act.sa_handler = mrb_fw_sighandler_func;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;

  if (sigaction(signo, &act, NULL) == -1) {
    mrb_sys_fail(mrb, "sigaction");
  }
  mrb_signo_registered[signo_idx] = 1;
  return mrb_fixnum_value(signo);
}

static mrb_value mrb_fw_is_registered(mrb_state *mrb, mrb_value self)
{
  mrb_int signo;
  mrb_get_args(mrb, "i", &signo);
  int signo_idx = (int)signo;

  return mrb_bool_value((mrb_bool)mrb_fw__is_registerd_signal(signo_idx));
}

static mrb_value mrb_fw_is_signaled(mrb_state *mrb, mrb_value self)
{
  mrb_int signo;
  mrb_get_args(mrb, "i", &signo);
  int signo_idx = (int)signo;

  if (!mrb_signo_registered[signo_idx]) {
    return mrb_false_value();
  }
  mrb_bool signaled = (mrb_bool)mrb_signo_signaled[signo_idx];
  if (signaled) {
    mrb_signo_signaled[signo_idx] = 0;
  }

  return mrb_bool_value(signaled);
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
  mrb_int signo;
  struct sigevent sev;
  memset(&sev, 0, sizeof(struct sigevent));

  mrb_get_args(mrb, "i", &signo);
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = (int)signo;

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
  int i;
  for (i = 0; i < SIGRTMAX; i++) {
    mrb_signo_registered[i] = 0;
    mrb_signo_signaled[i] = 0;
  }

  fiberedworker = mrb_define_module(mrb, "FiberedWorker");
  mrb_define_module_function(mrb, fiberedworker, "register_internal_handler", mrb_fw_register_internal_handler, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, fiberedworker, "registered?", mrb_fw_is_registered, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, fiberedworker, "signaled_nonblock?", mrb_fw_is_signaled, MRB_ARGS_REQ(1));

  mrb_define_const(mrb, fiberedworker, "SIGINT", mrb_fixnum_value(SIGINT));
  mrb_define_const(mrb, fiberedworker, "SIGHUP", mrb_fixnum_value(SIGHUP));
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
