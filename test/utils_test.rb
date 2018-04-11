assert("FiberedWorker.obj2signo") do
  i = FiberedWorker.obj2signo(:INT)
  assert_equal Fixnum, i.class

  i = FiberedWorker.obj2signo(:SIGINT)
  assert_equal Fixnum, i.class

  i = FiberedWorker.obj2signo(:RT1)
  assert_equal Fixnum, i.class
end
