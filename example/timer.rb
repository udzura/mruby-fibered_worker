loop = FiberedWorker::MainLoop.new
pid = fork do
  puts "[#{Process.pid}] This is child"
  loop do
    sleep 1
    # puts "[#{Process.pid}] Child is looping"
  end
end

loop.pid = pid
loop.register_handler(FiberedWorker::SIGRTMIN, false) do |signo|
  puts "Hey, this is SIGRTMIN: #{signo}"
end

loop.register_handler(FiberedWorker::SIGRTMIN+1, true) do |signo|
  puts "Hey, this is SIGRTMIN+1 then stop: #{signo}"
  Process.kill :TERM, pid
end

puts "Parent: [#{Process.pid}]"

t = FiberedWorker::Timer.new(FiberedWorker::SIGRTMIN)
t.start(500, 500)
t2 = FiberedWorker::Timer.new(FiberedWorker::SIGRTMIN+1)
t2.start(3000)
loop.run
puts "Loop exited"
