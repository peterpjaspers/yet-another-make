require 'fileutils'

YAM="C:\\Users\\peter\\Documents\\yam\\github\\main\\x64\\Release\\yamSleep.exe"

def yamSleep(interval, dummy)
    system("#{YAM} #{interval}")
end

def cmd_yamSleep(interval, dummy)
    system("cmd /c #{YAM} #{interval}")
end

def cmd_yamSleep_append(interval, fileName)
    cmd = "cmd /c #{YAM} #{interval} report >\> #{fileName}"
    system(cmd)
end


def measure(func, interval)
    threadCount = [1, 2, 4, 6, 8, 10, 12, 14, 16].reverse
    count = 1000
    threadCount.each{ |nThreads|
        nSleep = count/nThreads
        totalElapsed = 0
        repeat = 10;
        repeat.times{|r|
            dirName = "bin\\#{nThreads}_#{nSleep}_#{r}"
            FileUtils.mkdir_p(dirName)
            start = Time.now
            threads = []
            nThreads.times{|t|
                threads << Thread.new{ 
                    nSleep.times{|s|
                        fileName = "#{dirName}\\thr_#{t}_slp#{s}.txt"
                        method(func).call(interval, fileName)
                    }
                }
            }
            threads.each { | thr| thr.join }
            elapsed = Time.now - start
            totalElapsed += elapsed;
            FileUtils.remove_dir(dirName)
        }
        puts "#{nThreads} threads, each thread doing #{nSleep} sleeps, took #{totalElapsed/repeat} seconds"
    }
end

puts "Measuring execution time of 'yamSleep 0'\\n"
measure(:yamSleep, 0)
#puts "Measuring execution time of 'yamSleep 10'\\n"
#measure(:yamSleep, 10)

#puts "Measuring execution time of 'cmd /c yamSleep'\\n"
#measure(:cmd_yamSleep, 0)

#puts "Measuring execution time of 'cmd /c yamSleep 0 > file'\\n"
#measure(:cmd_yamSleep_append, 0)


