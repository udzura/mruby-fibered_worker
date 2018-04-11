loop = FiberedWorker::MainLoop.new
pid = fork do
  puts "[#{Process.pid}] This is child"
  loop do
    sleep 1
    # puts "[#{Process.pid}] Child is looping"
  end
end

loop.pid = pid
loop.register_timer(FiberedWorker::SIGRTMIN, 500, 500) do |signo|
  puts "Hey, this is SIGRTMIN: #{signo}"
end

loop.register_timer(FiberedWorker::SIGRTMIN+1, 3000) do |signo|
  puts "Hey, this is SIGRTMIN+1 then stop: #{signo}"
  Process.kill :TERM, pid
end

puts "Parent: [#{Process.pid}]"
loop.run
puts "Loop exited"
