##
## FiberedWorker Test
##

assert("FiberedWorker#hello") do
  t = FiberedWorker.new "hello"
  assert_equal("hello", t.hello)
end

assert("FiberedWorker#bye") do
  t = FiberedWorker.new "hello"
  assert_equal("hello bye", t.bye)
end

assert("FiberedWorker.hi") do
  assert_equal("hi!!", FiberedWorker.hi)
end
