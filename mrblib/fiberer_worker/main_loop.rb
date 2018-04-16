module FiberedWorker
  class MainLoop
    attr_accessor :pid, :interval
    attr_reader   :handlers, :timers

    def initialize(opt={})
      @interval = opt[:interval] || 0 # by msec
      @pid = opt[:pid] || nil
      @handlers = {}
      @timers = []
    end

    def new_watchdog
      target = self.pid
      unless target
        raise "Target pid must be set"
      end

      return Fiber.new do
        keep = true
        while keep
          ret = Process.waitpid2(target, Process::WNOHANG)
          if ret
            keep = false
          end
          Fiber.yield ret
        end
      end
    end

    def register_handler(signo, once=true, &blk)
      key = FiberedWorker.obj2signo(signo)
      self.handlers[key] = Fiber.new do
        keep = true
        while keep
          ret = blk.call(key)
          if once
            keep = false
          end
          Fiber.yield ret
        end
      end
    end

    def registered_signals
      self.handlers.keys
    end

    def register_timer(signo, start, interval=0, &blk)
      register_handler(signo, (interval == 0), &blk)
      self.timers << [
        FiberedWorker::Timer.new(signo),
        [start, interval]
      ]
    end

    def run
      FiberedWorker.sigprocmask(self.handlers.keys)
      watchdog = new_watchdog
      self.timers.each do |data|
        timer = data[0]
        args = data[1]
        timer.start(args[0], args[1])
      end

      ret = nil
      until ret = watchdog.resume
        if sig = FiberedWorker.sigtimedwait(self.handlers.keys, @interval)
          fib = self.handlers[sig]
          fib.resume if fib.alive?
        end
      end

      return ret
    end
  end
end
