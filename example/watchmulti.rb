loop = FiberedWorker::MainLoop.new
pid1 = fork do
  puts "[#{Process.pid}] This is child1"
  sleep 3
end

pid2 = fork do
  puts "[#{Process.pid}] This is child2"
  sleep 4
end

pid3 = fork do
  puts "[#{Process.pid}] This is child3"
  5.times do
    puts "."
    sleep 1
  end
end

loop.pids = [pid1, pid2, pid3]
loop.register_handler(FiberedWorker::SIGINT) do |signo|
  puts "Accept #{signo}... exitting"
  loop.pis.each do |pid|
    Process.kill :TERM, pid
  end
end

loop.on_worker_exit do |status, rest|
  puts "Termed process! #{status} rest: #{rest}"
end

puts "Parent: [#{Process.pid}]"
loop.run
puts "Loop exited"
