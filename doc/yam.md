# What is Yam?
Yam (Yet another make) is a software build system. It is intended to be used by 
software developers to automate the build of their software.  
Wikipedia: In software development, a build is the process of converting source 
code files into standalone software artifact(s) that can be run on a computer, 
or the result of doing so.

Yam is a general purpose file-based build system: it executes user-defined commands
 in a user-defined order. A command transforms input files into output files. 
 Output files of one command can be used as input files of other commands. Yam 
 is fully ignorant of how commands produce output files. This makes Yam suitable
  for other usages than just software builds. For example you can define commands to run unit tests, to convert MS Word documents into PDF documents, to transform a zip file with raw images into a zip file with processed images.

Yam pays tribute to the [tup](https://gittup.org/tup/index.html) build system from which it re-used many concepts. See [Comparison of tup and yam](#comparison-of-tup-and-yam) for a comparison of Yam and tup.

Yam runs on the Windows operating system. Future versions will also support Linux.

# Why use Yam?
Yam solves a comprehensive set of problems. Most other build systems often address only a subset of these problems. 

## Performance

- **No more waiting for the build system to figure out which files to rebuild**  
For the typical way-of-working in which you modify only a few files in-between two builds it will take Yam, independent of the number of files in your repository, less than 10 milliseconds to figure out which files to rebuild.
Many build systems scan the entire source file repository to find the files that were modified since the previous build. This takes time proportional to the number of files in the repository. For large repositories this scan time may become larger than the time needed to rebuild the modified files.
Yam takes a different approach to discover modified files: it continuously monitors the file system for changes. When you start a build Yam already knows what files need to be rebuild.

- **No more waiting for a build to finish while the CPU load is only a few percent**  
Yam parallelizes the execution of commands that do not depend on each other. By 
default it does so using all available local CPU cores.

- **No more starting the build in a sub-directory to make it go faster**  
Yam does not care where you start the build. It will always build all out-dated 
output files. Unless you tell Yam, using command line options, to only build a 
subset.
- **No more recompilation after changing a comment**  
Ever changed a comment in a C/C++ header file that is included by hundreds of cpp files? Most build systems will recompile all these cpp files, even though comment changes do not affect the compilation results.  
Not Yam. Yam can be setup to ignore changes in comments, thus allowing you to 
improve comments without fear of long builds.
- **No more recompilation after undoing edits**  
Ever modified and saved a C/C++ header file that is included by hundreds of C/C++cpp files, then changed your mind, undid the changes and start a build? Most 
 build systems will recompile all the cpp files.  
Not Yam. A change in a file's last-write-time is a signal to Yam to verify whether its content has changed. Rebuilds only happen on content changes.
- **No more tens or more of relinking dlls after changing an implementation detail**  
Ever modified an implementation detail in a dynamic load library (dll) in a large repository where a large number of dlls depend on the changed one, either directly or indirectly via other dlls? Most build systems will relink all these dlls, recursively up to the executables that use them.  
Not Yam. Yam can be setup to only relink dependent dlls when the interface of the modified dll has changed.

## Ease of use
- **No more figuring out what directories to build based on the change you made**
- **No more forgetting about a dependency you needed to build resulting in inconsistent binaries and wasted time debugging**
- **No more clean rebuilds after you changed some of your build commands**
- **No more clean rebuilds after you renamed/moved files or directories**
- **No more clean rebuilds after you checked-out another git branch in your worktree**
- **No more clean rebuilds at all**  
Developers and Continuous Integration engineers can confidently rely on incremental builds
    
- **No more manual deletion of stale build results**  
You removed a build command that produced some file X? Yam will delete X.
You renamed the output of a build command from X to Y? Yam will delete X and create Y.

- **No more hard-to-reproduce build errors because your build commands are not parallel-safe**  
Yam fails the build when multiple build commands attempt to update the same output file. And Yam will tell you who the culprits are.  
When a build command C depends on an output file F produced by build command P 
then Yam demands that you declare C to be dependent on F in order to guarantee 
proper build order of P before C. Yam will tell you when you failed to do so.

## Correct, deterministic and hermetic
Deterministic implies that a build with the same input files and commands give 
the same output files on the same machine. Hermetic implies that unexpectedly 
different input files are not possible, and builds are deterministic across 
machines/environments.

- **No more wondering what build command produced these unexpected output files**  
Yam demands you to declare the output files of a command. Output files can be 
mandatory, optional or ignored. Yam fails the build when not all mandatory outputs are updated by the command. Yam also fails the build when the command writes files that are not declared as optional or ignored. Yam deletes ignored output files. Globs can be used to define the optional and ignore files.

- **No more builds that update source files**  
Yam fails the build when a command writes to a source file. 

- **No more figuring out why building the same repository succeeds on computer A
 but fails on computer B**  
Typical causes are: programs (e.g. compiler, linker) were installed in different directories, environment variables have different values.
Yam demands all directory trees from which files are read by the build commands 
to be declared. Yam will fail the build when a command reads a file from a 
not-declared directory tree and tell you which command tried to read what file. 
Also Yam does not allow commands to depend on environment variables. These measures allow you to solve most issues in minutes instead of hours.

## Build language
- **No more days spent learning yet-another-build-language**  
When using Yam you define build rules in so-called build files. These text files contain build rule definitions written in the Yam declarative build language. This is a small and simple language that can be mastered in a matter
 of hours. 
You don't like the language? Then move on to the next bullet.
- **No more being locked in the build language dictated by your build system**  
Yam recognizes that there is no such thing as the perfect build language. Yam 
therefore allows you to write custom build files in the language(s) of your 
choice, e.g. Python, Ruby, Perl, Lua. A custom build file, when run, outputs build rules in the Yam build language syntax. When writing custom build files you will typically use only a very small subset of the Yam language. When Yam detects a custom build file it executes it and parses its output in the same way as it would parse the content of a Yam build file.

# When not to use Yam?
Yam does not support distributed build and distributed build cache.
This makes Yam not suitable for building huge repositories that are maintained by hundreds or thousands of engineers, like the ones in Google, Microsoft, Amazon, Facebook. 

# A short history of Yam
The creators of Yam worked in a company that used the ClearCase version management system to manage a ~50,000 file repository containing ~8,000,000 lines of code 
(C++, C# and many other file types). This repository was actively developed by ~100-200 software engineers. Engineers developed and built software on 4-6 core high-end laptops with 16 GB RAM and 512Mb-1TB SSD storage.
software was built using omake, the build system that comes with ClearCase. 
Omake does not parallellize the build.
A full build on such a laptop took 24 hours. An incremental build to update all 
out-dated output files took 30 minutes (!) to figure out which commands to 
re-execute. The net command execution time for a typical incremental build is up
to a few minutes. This resulted in engineers trying to outsmart the build system
by only building the output files that they thought were impacted by their 
source code changes. This frequently resulted in mistakes and lots of wasted 
time in debugging.

The company then decided to replace ClearCase by git and to develop in-house a 
new build system (not Yam) tailored to the archive structure and the company's 
development process. 
Using git and this new build system a full build takes 3-4 hours. Typical 
incremental builds take sub-second time to figure out what to build and command 
execution takes from a few seconds to a few minutes. The CI (Continuous Integration)
 pipeline performs incremental builds on Pull requests and full builds during 
 nights. A full CI build is performed on a 10-core computer in 1.5-2 hours. 
 Adding more cores did not further reduce build time due to the serialization 
 inherent in the command dependency graph.
The new build system was not suited to open-source because it requires a very 
specific directory structure and expensive third-party software. A few years 
later the authors retired and decided to use their build system experiences to 
create Yam.

# Core concepts

## Performance
Some build systems claim to be fast. But what does this mean? 
Build execution time is determined by the following factors:
- The net execution time of the build commands (i.e. without overhead incurred 
by the build system)
- The amount to which execution of build commands can be parallellized  
  This depends on:
  - The structure of the command dependency graph
  - The number of available processor cores
- The computer hardware
- The overhead of the build system

The first three factors are not determined by the build system. 
Build system overhead consists of:
- Time needed to figure out which files have been modified since the previous 
build
- Time needed to re-parse modified build files
- Time needed to figure out which commands to re-execute given the list of 
modified files
- Time needed to parallelize command execution
- Time needed to monitor command execution and update the dependency graph

# Comparison of tup and yam

TODO: build language comparison
TODO: run-script
TODO: no %<group>, instead input <group> always treated as list of files

Yam removes limitations mentioned by several users in the [tup users mailing
 list](https://groups.google.com/g/tup-users).

***Tup***: output files are stored in the same directory as the buildfile or in 
a variant directory.  
***Yam***: user decides where to store output files.

***Tup***: mandatory output files must be declared by name. Outputs to be ignored
 must be declared by name or glob.  
Tup fails the build when a command produces non-declared outputs or when not all
 mandatory outputs are produced.  
***Yam***: mandatory output files must be declared by name. Outputs to be ignored
 must be declared by name or glob. Optional outputs must be declared by name or
glob.
Yam fails the build outputs when a command produces non-declared outputs or when 
not all mandatory outputs are produced.

***Tup***: when a rule in buildfile X (Tupfile in directory X) references input 
file Y/someFile, then tup parses the Tupfile in Y to figure out whether 
Y/someFile is defined as an output (generated file) of a rule in Y. Tup can do 
so because outputs are always in the directory that contains the buildfile. If 
Y/someFile is not a generated file then tup treats it as a normal (source) file
 and checks existence of that file.
***Yam***: in the above example the user must declared in buildfile X a 
dependency on buildfile Y. Failure to do so will result in an error: Y/someFile 
does not exist. This is the price to pay for allowing the user to store output
 files anywhere, while keeping the incremental parsing of buildfiles. 

***Tup***: output files can optionally be stored in one group and in one bin.  
***Yam***: output files can optionally be stored in multiple groups and bins.
 Both mandatory and optional outputs are stored in the specified groups/bins.

Optional outputs allow for rules where the names of the output files are not 
known a-priori. This allows for rules like: 
```
images.zip |> unzip %f -outDir images |> images/*.bmp <images>
foreach <images> |> smooth %f -out %o |> smoothed/%B.%e <smoothed>
<smoothed> |> zip %f -out %o |> smoothedImages.zip
```


