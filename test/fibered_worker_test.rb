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

assert("FiberedWorker::MainLoop#register_handler") do
  l = FiberedWorker::MainLoop.new
  pid = make_worker
  l.pid = pid
  l.register_handler(SIGRT5) do
    ::Process.kill FiberedWorker::SIGTERM, pid
  end

  t = FiberedWorker::Timer.new(SIGRT5)
  t.start 50

  l.run
  assert_true true
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
