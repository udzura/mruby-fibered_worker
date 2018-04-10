# mruby-fibered_worker   [![Build Status](https://travis-ci.org/udzura/mruby-fibered_worker.svg?branch=master)](https://travis-ci.org/udzura/mruby-fibered_worker)
FiberedWorker class
## install by mrbgems
- add conf.gem line to `build_config.rb`

```ruby
MRuby::Build.new do |conf|

    # ... (snip) ...

    conf.gem :github => 'udzura/mruby-fibered_worker'
end
```
## example
```ruby
p FiberedWorker.hi
#=> "hi!!"
t = FiberedWorker.new "hello"
p t.hello
#=> "hello"
p t.bye
#=> "hello bye"
```

## License
under the MIT License:
- see LICENSE file
