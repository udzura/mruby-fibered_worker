loop = FiberedWorker::MainLoop.new
r, w = IO.pipe

pid = fork do
  puts "Child: [#{Process.pid}]"
  r.close
  5.times do
    puts "[#{Process.pid}] wait..."
    sleep 1
  end
  w.write 'a'
  loop do
    sleep 1
  end
end
w.close
loop.pid = pid
loop.register_timer(FiberedWorker::SIGRTMIN, 1000, 1000) do |signo|
  puts "[#{Process.pid}] wait..."
end

loop.register_fd(r.fileno) do |data|
  puts "received: #{data}"
  Process.kill :TERM, pid
end

puts "Parent: [#{Process.pid}]"
loop.run
puts "Loop exited"
