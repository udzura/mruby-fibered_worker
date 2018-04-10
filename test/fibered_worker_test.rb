assert("FiberedWorker::MainLoop.new") do
  t = FiberedWorker::MainLoop.new
  assert_equal(FiberedWorker::MainLoop, t.class)
end
