require 'fileutils'

SRC_DIR="C:\\Users\\peter\\repos\\test_speed_yam\\sources"

def copyFile(nThreads, thread, nCopy)
     start = 1 + (nCopy*thread)
     dstdir = "#{SRC_DIR}\\bin\\copies\\#{nThreads}_#{thread+1}"
     if File.directory?(dstdir)
         FileUtils.remove_dir(dstdir) 
     end
     FileUtils.mkdir_p(dstdir)
     nCopy.times{|i|
         src = "#{SRC_DIR}\\foo_#{start+i}.cpp"
         dst = "#{dstdir}\\foo_#{start+i}.obj"
         #puts "copy(#{src}, #{dst})"
         FileUtils.cp(src, dst)
     }
end

def cmd_copyFile(nThreads, thread, nCopy)
     start = 1 + (nCopy*thread)
     dstdir = "#{SRC_DIR}\\bin\\cmd_copies\\#{nThreads}_#{thread+1}"
     if File.directory?(dstdir)
         FileUtils.remove_dir(dstdir) 
     end
     FileUtils.mkdir_p(dstdir)
     nCopy.times{|i|
         src = "#{SRC_DIR}\\foo_#{start+i}.cpp"
         dst = "#{dstdir}\\foo_#{start+i}.obj"
         #puts "cmd /c copy #{src} #{dst} > nul"
         system("cmd /c copy #{src} #{dst} > nul")
     }
end

def measure(func, interval)
    threadCount = [1, 2, 4, 6, 8, 10, 12, 14, 16]
    #threadCount = [2].reverse
    count = 1000
    threadCount.each{ |nThreads|
        nCopy = count/nThreads
        totalElapsed = 0
        repeat = 10;
        repeat.times{|r|
            start = Time.now
            threads = []
            nThreads.times{|t|
                threads << Thread.new{ method(func).call(nThreads, t, nCopy) }
            }
            threads.each { | thr| thr.join }
            elapsed = Time.now - start
            totalElapsed += elapsed;
        }
        puts "#{nThreads} threads, each thread doing #{nCopy} file-copies, took #{totalElapsed/repeat} seconds"
    }
end

puts "measure cmd_copyFile (average of 10 measurements)"
measure(:cmd_copyFile, 0)
#puts
#puts "measure copyFile"
#measure(:copyFile, 0)

