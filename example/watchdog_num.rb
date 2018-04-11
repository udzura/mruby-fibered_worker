loop = FiberedWorker::MainLoop.new
pid = fork do
  puts "[#{Process.pid}] This is child"
  loop do
    sleep 1
  end
end

loop.pid = pid
loop.register_handler(:SIGINT) do |signo|
  puts "Accept #{signo}... exitting"
  Process.kill :TERM, pid
end

loop.register_handler(:RT0, false) do |signo|
  puts "Hey, this is SIGRTMIN: #{signo}"
end

puts "Kill to: [#{Process.pid}]"
loop.run
puts "Loop exited"
