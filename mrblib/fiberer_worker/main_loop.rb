module FiberedWorker
  class MainLoop
    attr_accessor :pid, :handlers

    def initialize
      @handlers = []
    end

    def new_watchdog
      target = self.pid
      return Fiber.new do
        ret = nil
        quit = false
        until quit
          ret = Process.waitpid2(target, Process::WNOHANG)
          if ret
            break
          end
          Fiber.yield ret
        end
      end
    end

    def register_handler(signo, once=true, &blk)
      FiberedWorker.register_internal_handler(signo)
      self.handlers << Fiber.new do
        keep = true
        while keep
          ret = nil
          if FiberedWorker.signaled_nonblock?(signo)
            ret = blk.call(signo)
            if once
              keep = false
            end
          end
          Fiber.yield ret
        end
      end
    end

    def run
      watchdog = new_watchdog
      before = Time.now
      until watchdog.resume
        # infinite loop to wait
        after = Time.now
        if after.to_i - before.to_i >= 1
          puts "[#{Process.pid}] nonblocking run: #{after.inspect}"
          before = after
        end

        self.handlers.each do |fib|
          if fib.alive?
            fib.resume
          end
        end
      end
    end
  end
end
