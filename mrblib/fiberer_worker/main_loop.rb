module FiberedWorker
  class MainLoop
    attr_accessor :pids, :interval
    attr_reader   :handlers, :timers, :fd_checkers

    def initialize(opt={})
      @interval = opt[:interval] || 0 # by msec
      @pids = opt[:pid] || nil
      @pids = [@pids] unless @pids.is_a?(Array)
      @handlers = {}
      @on_worker_exit = lambda {}
      @timers = []
      @fd_checkers = []
    end
    alias pid pids
    def pid=(newpid)
      self.pids = newpid.is_a?(Array) ? newpid : [newpid]
    end

    def on_worker_exit(&b)
      if b
        @on_worker_exit = b
      else
        @on_worker_exit
      end
    end

    def new_watchdog
      target = self.pids
      unless target
        raise "Target pids must be set"
      end

      return Fiber.new do
        until self.pids.empty?
          ret = Process.waitpid2(-1, Process::WNOHANG)
          if ret
            self.pids.delete(ret[0])
          end
          Fiber.yield ret
        end
      end
    end

    def register_handler(signo, once=true, &blk)
      key = FiberedWorker.obj2signo(signo)
      if self.handlers[key]
        raise "Handler already registered for signal #{signo}"
      end

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

    def register_fd(fd_no, &blk)
      FiberedWorker.nonblocking_fd!(fd_no)
      self.fd_checkers << Fiber.new do
        keep = true
        while keep
          ret = FiberedWorker.read_nonblock(fd_no)
          if ret
            keep = false
            blk.call(ret)
          end
          Fiber.yield ret
        end
      end
    end

    def registered_signals
      self.handlers.keys
    end

    def registered?(signo)
      !! self.handlers[FiberedWorker.obj2signo(signo)]
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
      termed = []
      until self.pids.empty?
        until ret = watchdog.resume
          @fd_checkers.each do |checker|
            if checker.alive?
              checker.resume
            else
              @fd_checkers.delete checker
            end
          end
          if sig = FiberedWorker.sigtimedwait(self.handlers.keys, @interval)
            fib = self.handlers[sig]
            fib.resume if fib.alive?
          end
        end
        self.on_worker_exit.call(ret, self.pids)
        termed << ret
      end

      return termed
    end
  end
end
