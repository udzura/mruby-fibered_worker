loop = FiberedWorker::MainLoop.new
pid = fork do
  puts "[#{Process.pid}] This is child"
  sleep 10
  puts "[#{Process.pid}] Child going to term"
end

loop.pid = pid
loop.run
