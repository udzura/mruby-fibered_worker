module FiberedWorker
  class MainLoop
    attr_accessor :pids, :interval, :use_legacy_watchdog
    attr_reader   :handlers, :timers, :fd_checkers

    def initialize(opt={})
      @use_legacy_watchdog = opt[:use_legacy_watchdog] || false
      # by msec
      @interval = opt[:interval] || (@use_legacy_watchdog ? 0 : 1000)
      @pids = opt[:pid] || nil
      @pids = [@pids] unless @pids.is_a?(Array)
      @handlers = {}
      @on_worker_exit = lambda {|_, _| }
      @timers = {}
      @fd_checkers = []
      @termed = []

      @last_ret = []
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

      termed = @termed
      return Fiber.new do
        until self.pids.empty?
          ret = Process.waitpid2(-1, Process::WNOHANG)
          if ret
            self.pids.delete(ret[0])
            termed << ret
          end
          Fiber.yield ret
        end
      end
    end

    def new_nil_watchdog
      last_ret = @last_ret
      return Fiber.new do
        loop do
          if last_ret[0]
            Fiber.yield last_ret.shift
          else
            Fiber.yield nil
          end
        end
      end
    end

    def register_sigchld_watchdog
      last_ret = @last_ret
      pids_ = self.pids
      termed = @termed
      register_handler(:CHLD, false) do
        while !pids_.empty? && ret = Process.waitpid2(-1, Process::WNOHANG)
          pids_.delete(ret[0])
          last_ret << ret
          termed << ret
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

    def register_fd(fd_no, once=true, &blk)
      FiberedWorker.nonblocking_fd!(fd_no)
      self.fd_checkers << Fiber.new do
        keep = true
        while keep
          ret = FiberedWorker.read_nonblock(fd_no)
          if ret
            keep = false if once
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
      self.timers[FiberedWorker.obj2signo(signo)] = [
        FiberedWorker::Timer.new(signo),
        [start, interval]
      ]
    end

    def timer_for(signo)
      self.timers[FiberedWorker.obj2signo(signo)]
    end

    def run
      unless use_legacy_watchdog
        register_sigchld_watchdog
      end

      FiberedWorker.sigprocmask(self.handlers.keys)
      watchdog = use_legacy_watchdog ? new_watchdog : new_nil_watchdog
      self.timers.each do |signo, data|
        timer = data[0]
        args = data[1]
        timer.start(args[0], args[1])
      end

      ret = nil
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
      end

      return @termed
    end
  end
end
