YAM="C:\\Users\\peter\\Documents\\yam\\github\\main\\x64\\Release\\yam.exe"

# This program does same thing as measure_windows_spawn_speed by
# using yam to perform the spawn operation as defined in 
# sources\buildfile_yam.txt
# Note that yam runs yamSleep always from a cmd shell: yam spawns cmd,
# cmd spawn yamSleep
#

def measure()
    threads=[ 1, 2, 4, 6, 8, 10, 12, 14, 16 ]
    #threads=[ 12 ].reverse
    threads.each{ |t|
        totalElapsed = 0
        repeat = 5;
        repeat.times{|r|
            system("#{YAM} --clean >\> logs\\build_#{t}.txt")
            start = Time.now
            system("#{YAM} --threads=#{t} >\> logs\\build_#{t}.txt")
            elapsed = Time.now   - start
            totalElapsed += elapsed;
            puts "#{t} threads #{elapsed} seconds"
        }
        puts "#{t} threads average #{totalElapsed/repeat} seconds"
    }
end

measure()

