loop = FiberedWorker::MainLoop.new
mode = ARGV[0]
if mode == 'legacy'
  loop.use_legacy_watchdog = true
  loop.interval = ARGV[1] ? ARGV[1].to_i : 0
end
puts "Mode: #{mode == 'legacy' ? mode : 'sigchld'}"

pid = fork do
  puts "[#{Process.pid}] This is child"
  loop do
    sleep 1
    # puts "[#{Process.pid}] Child is looping"
  end
end

loop.pid = pid
loop.register_timer(FiberedWorker::SIGRTMIN, 500, 500) do |signo|
  puts "current CPU usage:"
  system "ps auf | grep -e CPU -e mruby | grep -v grep"
end

loop.register_timer(FiberedWorker::SIGRTMIN+1, 3000) do |signo|
  puts "Then exit..."
  Process.kill :TERM, pid
end

puts "Loop exited: #{loop.run.inspect}"
