assert("FiberedWorker::Timer.new") do
  t = FiberedWorker::Timer.new(10)
  assert_equal FiberedWorker::Timer, t.class
end
