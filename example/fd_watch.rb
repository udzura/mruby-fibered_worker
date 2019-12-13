loop = FiberedWorker::MainLoop.new
r, w = IO.pipe

pid = fork do
  puts "Child: [#{Process.pid}]"
  r.close
  5.times do
    puts "[#{Process.pid}] wait..."
    sleep 1
  end
  data = 'mruby!'.split('')
  while c = data.shift
    sleep 1
    puts "Sending #{c.inspect}"
    w.write c
  end
  w.close
end
w.close
loop.pid = pid
loop.register_timer(FiberedWorker::SIGRTMIN, 1000, 1000) do |signo|
  puts "[#{Process.pid}] wait..."
end

loop.register_fd(r.fileno, false) do |data|
  puts "received: #{data}(#{data.chr.inspect})"
  if data == '!'.ord
    puts "Then finish"
    Process.kill :TERM, pid
  end
end

puts "Parent: [#{Process.pid}]"
loop.run
puts "Loop exited"
