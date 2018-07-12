loop = FiberedWorker::MainLoop.new
r, w = IO.pipe

pid = fork do
  puts "[#{Process.pid}] This is child"
  r.close
  5.times do
    puts "wait..."
    sleep 1
  end
  w.write 'a'
  loop do
    sleep 1
  end
end
w.close
loop.pid = pid
loop.register_fd(r.fileno) do |data|
  puts "received: #{data}"
  Process.kill :TERM, pid
end

puts "Parent: [#{Process.pid}]"
loop.run
puts "Loop exited"
