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

SIGRT5  = FiberedWorker::SIGRTMIN+5
SIGRT10 = FiberedWorker::SIGRTMIN+10

assert("FiberedWorker::MainLoop#run") do
  l = FiberedWorker::MainLoop.new
  pid = make_worker
  l.pid = pid
  l.register_handler(SIGRT5) do
    ::Process.kill FiberedWorker::SIGTERM, pid
  end

  t = FiberedWorker::Timer.new(SIGRT5)
  t.start 100

  l.run
  assert_true true
end
