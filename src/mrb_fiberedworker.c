/*
** mrb_fiberedworker.c - FiberedWorker class
**
** Copyright (c) Uchio Kondo 2018
**
** See Copyright Notice in LICENSE
*/

#include "mrb_fiberedworker.h"
#include <mruby.h>
#include <mruby/error.h>
#include <signal.h>

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

void mrb_mruby_fibered_worker_gem_init(mrb_state *mrb)
{
  struct RClass *fiberedworker;
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
  DONE;
}

void mrb_mruby_fibered_worker_gem_final(mrb_state *mrb)
{
}
