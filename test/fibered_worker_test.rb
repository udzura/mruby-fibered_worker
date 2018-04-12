assert("FiberedWorker::MainLoop.new") do
  t = FiberedWorker::MainLoop.new
  assert_equal(FiberedWorker::MainLoop, t.class)
end

def __timeout_wait(cycle, &blk)
  cycle.times do
    if blk.call
      return true
    end
    usleep 1000
  end
  raise "Timeout!"
end

def __time_now_msec
  Time.now.to_i * 1000 + Time.now.usec / 1000
end

def make_worker
  fork { loop {sleep 1} }
end

SIGRT5 = FiberedWorker::SIGRTMIN+5
SIGRT6 = FiberedWorker::SIGRTMIN+6
SIGRT7 = FiberedWorker::SIGRTMIN+7

assert("FiberedWorker::MainLoop#register_handler") do
  l = FiberedWorker::MainLoop.new
  pid = make_worker
  l.pid = pid
  l.register_handler(SIGRT5) do
    ::Process.kill FiberedWorker::SIGTERM, pid
  end

  t = FiberedWorker::Timer.new(SIGRT5)
  t.start 50

  ret = l.run
  assert_true ret[0].is_a?(Fixnum), "ret[0] should be a pid"
  assert_true ret[1].is_a?(Process::Status), "ret[1] should be a Process::Status instance"
end

assert("FiberedWorker::MainLoop#register_timer") do
  l = FiberedWorker::MainLoop.new
  pid = make_worker
  l.pid = pid
  l.register_timer(SIGRT6, 100) do
    ::Process.kill FiberedWorker::SIGTERM, pid
  end
  start = __time_now_msec
  l.run
  done = __time_now_msec
  assert_true((done - start) >= 100)
end

assert("FiberedWorker::MainLoop#register_timer with interval") do
  l = FiberedWorker::MainLoop.new
  pid = make_worker
  count = 0
  l.pid = pid
  l.register_timer(SIGRT7, 50, 50) do
    count += 1
    if count >= 3
      ::Process.kill FiberedWorker::SIGTERM, pid
    end
  end
  start = __time_now_msec
  l.run
  done = __time_now_msec
  assert_true((done - start) >= 150)
end
