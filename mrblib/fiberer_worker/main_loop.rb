class FiberedWorker
  class MainLoop
    attr_accessor :pid

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
      end
    end
  end
end
