loop = FiberedWorker::MainLoop.new
pid = fork do
  puts "[#{Process.pid}] This is child"
  loop do
    sleep 1
    # puts "[#{Process.pid}] Child is looping"
  end
end

loop.pid = pid
loop.interval = 100
loop.register_handler(FiberedWorker::SIGINT) do |signo|
  puts "Accept #{signo}... exitting"
  Process.kill :TERM, pid
end

[:RT1, :RT2, :RT3].each do |sig|
  loop.register_handler(sig, false) do |signo|
    puts "Hey, this is signal: #{sig}"
  end
end

puts "Spawning: [#{Process.pid}]"
loop.run
puts "Loop exited"
