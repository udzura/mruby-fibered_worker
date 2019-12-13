def make_worker
  fork { loop {sleep 1} }
end
SIGRT4 = FiberedWorker::SIGRTMIN+4
SIGRT5 = FiberedWorker::SIGRTMIN+5

l = FiberedWorker::MainLoop.new
pid1 = make_worker
pid2 = make_worker
l.pids = [pid1, pid2]
l.register_handler(SIGRT4) do
  puts "Killing #{pid1}"
  ::Process.kill FiberedWorker::SIGTERM, pid1
end
cnt = 0
l.register_handler(SIGRT5, false) do
  cnt += 1
  puts "Got SIGRT5 in #{cnt} time(s)"
  if cnt >= 5
    puts "Then killing #{pid2}"
    ::Process.kill FiberedWorker::SIGTERM, pid2
  end
end

t = FiberedWorker::Timer.new(SIGRT4)
t.start 200
t2 = FiberedWorker::Timer.new(SIGRT5)
t2.start 300, 300

p "pids: #{l.pids}"
ret = l.run
p ret
