# mruby-fibered_worker   [![Build Status](https://travis-ci.org/udzura/mruby-fibered_worker.svg?branch=master)](https://travis-ci.org/udzura/mruby-fibered_worker)

mruby-made timer & non-blocking process supervisor, using mruby-fiber, independent from pthread or libuv

## Install by mrbgems

Add conf.gem line to `build_config.rb`

```ruby
MRuby::Build.new do |conf|
  conf.gem :github => 'udzura/mruby-fibered_worker'
end
```

## Example

Please see [example](https://github.com/udzura/mruby-fibered_worker/tree/master/example)

### Worker watcher

```ruby
loop = FiberedWorker::MainLoop.new
pid = fork do
  loop do
    sleep 1
  end
end

loop.pid = pid
loop.register_handler(FiberedWorker::SIGINT) do |signo|
  puts "Accept #{signo}... exitting"
  Process.kill :TERM, pid
end

loop.register_handler(FiberedWorker::SIGRTMIN, false) do |signo|
  puts "Hey, this is SIGRTMIN: #{signo}"
end

puts "Kill to: [#{Process.pid}]"
loop.run
puts "Loop exited"
```

### Interval log and timer exit

```ruby
loop = FiberedWorker::MainLoop.new
pid = fork do
  loop do
    sleep 1
  end
end

loop.pid = pid

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
# killed after 3000ms
puts "Loop exited"
```

## Note

This supervisor should be used under these condition:

* Single-threaded daemon
* Exists only one `mrb_state *` in one process

## License

Under the MIT License: see [LICENSE](https://github.com/udzura/mruby-fibered_worker/tree/master/LICENSE) file
