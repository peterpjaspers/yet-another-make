# What is Yam?

Yam (Yet another make) is a software build system. It is intended for
software developers to automate software builds.  
Wikipedia: In software development, a build is the process of converting source
code files into standalone software artifact(s) that can be run on a computer,
or the result of doing so.

Yam is a general purpose file-based build system: it executes user-defined
commands where each command transforms input files to output files. Output 
files of one command can be used as input files of other commands. A command
is a script in Windows `cmd.exe` shell syntax.

Yam is fully ignorant of how commands produce output files. This makes
Yam suitable for other usages than just software builds. For example you can define
commands to run unit tests, to convert MS Word documents into PDF documents, to
transform a zip file with raw images into a zip file with processed images.

Yam pays tribute to the [tup](https://gittup.org/tup/index.html) build system
from which it re-used many concepts.  
See [Some differences between Tup and Yam](#some-differences-between-tup-and-yam) .

Yam runs on the Windows operating system. Future versions will also support Linux.

# Why use Yam?

This section discusses how Yam addresses problems related to performance, 
correctness, reproducability and ease-of-use. Most build systems often address
only a subset of these problems or address them with less rigour than Yam does.

## Performance

### Long time between start of build and start of command execution  
Build systems only re-execute commands that are out-dated. A command is 
out-dated when its input files were modified since the previous build.
To find these files many build systems scan the entire source file repository.
This requires time proportional to the number of files in the repository.
The scan time is perceived as the time between start of build and start of 
command execution. For large repositories this can exceed the time needed to
execute the out-dated command(s). E.g. the build system takes 10 seconds to 
start a 1 second compilation.  
Yam takes a different approach to discover modified files: it continuously
monitors the file system for changes. When you start a build Yam already knows
which files were modified, and hence which commands to re-execute.

*For the typical way-of-working in which you modify only a few files in-between
two builds it will take Yam, independent of the number of files in your 
repository, less than 10 milliseconds to determine which commands to
re-execute.*

*Long times between start of build and start of command execution tempt
engineers to outsmart the build system by only building the output files that
they thought were impacted by their source code changes. This frequently 
results in mistakes and lots of wasted time in debugging.*

### Long build time due to under-utilization of CPU cores 
Yam parallellizes the execution of commands that do not depend on each others
output files. E.g. C++ compile commands execute in parallel, a link command 
executes after the compile commands that produce its input object files. 
Commands that can be executed parallel are queued to a thread pool. By default
the thread pool size is equal to the number of CPU cores.
Yam thus ensures 100% CPU utilization as long as sufficient commands can execute 
parallel.

*Input-output dependencies between commands limit parallelization.
This is outside control of the build system.*

### Long build time due to unnecessary command re-execution
To avoid unnecessary command re-execution Yam can be setup to only re-execute a
command when relevant aspects of the command's input files change. Examples:

- Unnecessary recompilation after changing comments  
Assume you change a comment in a C/C++ header file that is included by hundreds
of cpp files and start a build. Most build systems will recompile all these cpp 
files, even though comment changes do not affect the resulting object files.  
Not so in Yam. Yam can be setup to only re-execute a compile command when the 
non-comment sections of the command's input files change. This allows you to 
improve comments without the cost of long builds. 

- Unnecessary recompilation after undoing edits  
Assume you modify and save a C/C++ header file that is included by hundreds of
C/C++cp files, then change your mind, undo the changes and start a build.
Most build systems will recompile all the cpp files because their 
last-write-times have changed.  
Not so in Yam. By default a change in a file's last-write-time is a signal to
Yam to verify whether (aspects of) its content has changed. Rebuilds only happen
on content changes.

- Unnecessary relinking after changing a dynamic load library
Assume you modify an implementation detail in a C++ file that is linked into a
dynamic load library (dll). Assume that a large number of dlls and/or 
executables depend on the changed dll, either directly or indirectly via other
dlls. Most build systems will relink all these dlls, recursively up to the 
executables that use them.  
Not so in Yam: Yam can be setup to only re-execute a link command a dll when 
the interface aspect of the command's input dlls change.


## Correctness
This section discusses various aspects of build correctness.  

### Failure to re-execute a command
Yam re-executes a command when one or more of the following is true:
- Relevant aspects of the command's input files have changed.
- The command script has changed.  
- The command output files have been tampered with.

Note: Yam does not re-execute commands when environment variables change. 
This is not needed because Yam executes commands in a cmd shell with an 
empty environment. Commands can only use environment variables that are set 
in the command script itself. E.g. the command script can set the PATH
variable. The PATH setting is only visible for the command that set it.

Yam must know all input files of a command. Yam requires the user to specify
all input files that are outputs files of other commands. Yam however does 
not require the user to specify all input source files. E.g. for a C++ 
compilation the user only specifies the cpp file to be compiled. It is not
needed to list all header files included by that cpp file because Yam 
discovers all input and output files of a command by monitoring the 
command's file accesses.

Yam only determines whether (aspects of) file content changed when a file's
last-write-time has changed. The probability of failure to re-execute a 
command therefore depends on the last-write-time resolution.  
Note: Yam only looks at a change in last-write-time. Yam does not care whether
the change is an increase or decrease.

*Some older build systems like `Make` only re-execute a command when an output
file is older than its input files. This results in out-dated output when 
changing the command. When undoing file changes by resetting it to its 
committed version the version management systems may reset the file's 
last-write-time to the one of the committed version. In this case the time
decreases and `Make` sees no reason to re-execute the command.*

### Missing, unexpected and stale output files
Stale output files are output files of deleted commands or of commands 
that were modified to no longer output these files. Stale output files can lead
to errors. E.g. a test script tests output file oldProgram.exe. The command that 
produces oldProgram.exe is now changed to produce newProgram.exe. The user
forgets to update the test script. If the stale oldProgram.exe is not deleted
then the test script continues to pass the test.
Unexpected output files may also cause problems, e.g. excessive diskspace usage.

Yam avoids such errors by: 
- requiring the user to declare the output files of a command.  
Output files can be mandatory, optional or to-be-ignored. Globs can be used to 
define optional and to-be-ignored files.
- failing the build when not all mandatory outputs are updated by the command.
- failing the build when the command writes files that are not declared as 
optional or to-be-ignored. 
- deleting to-be-ignored output files.
- deleting stale output files.

Note: Yam discovers all input and output files of by monitoring the command's 
file accesses. This allows Yam to compare previous outputs with current outputs
and to compare actual outputs with declared outputs.

### Wrong build order
Assume command P produces file F and command C reads F. For C to produce
up-to-date output it must use the latest version of F. Hence C must be executed
after P.  
Yam deduces build order by requiring the user to specify all input files that
are produced by other commands. Yam will fail the build if the user fails to do 
so. Yam can do so because it monitors the command's file accesses.

### Build commands that are not parallel-safe
Yam fails the build when multiple commands write to the same file. 
To reduce the likelihood of commands accessing the same temporary files Yam
will create for each command a unique temporary directory. Yam passes this 
directory to the cmd shell via the environment variable `TEMP`. The temporary
directory is deleted when the command completes.
  
### Commands that update source files
Yam considers all files that are not declared as output files to be source
files. Yam fails the build when a command writes to a source file.  
Note: a file that contains code generated by a command, e.g. from an IDL
file, is an output file, ***not*** a source file.

## Ease of use

- **No more clean rebuilds after you changed some of your build commands**
- **No more clean rebuilds after you renamed/moved files or directories**
- **No more clean rebuilds after you checked-out another git branch in your worktree**
- **No more clean rebuilds at all**  
  Developers and Continuous Integration engineers can confidently rely on
  incremental builds
- **No more manual deletion of stale build results**  
  You removed a build command that produced some file X? Yam will delete X.
  You renamed the output of a build command from X to Y? Yam will delete X
  and create Y.

- **No more hard-to-reproduce build errors because your build commands are not parallel-safe**  
  Yam fails the build when multiple build commands attempt to update the same
  output file. And Yam will tell you who the culprits are.  
  When a build command C depends on an output file F produced by build command P
  then Yam demands that you declare C to be dependent on F in order to guarantee
  proper build order of P before C. Yam will tell you when you failed to do so.

## Reproducability, deterministic and hermetic

A reproducable, also known as deterministic, build implies that a build with the
same input files and commands give
the same output files on the same machine. Hermetic implies that unexpectedly
different input files are not possible, and builds are deterministic across
machines/environments.

- **No more wondering what build command produced these unexpected output files**  
  Yam demands you to declare the output files of a command. Output files can be
  mandatory, optional or ignored. Yam fails the build when not all mandatory
  outputs are updated by the command. Yam also fails the build when the command
  writes files that are not declared as optional or ignored. Yam deletes ignored
  output files. Globs can be used to define the optional and ignore files.

- **No more builds that update source files**  
  Yam fails the build when a command writes to a source file.

- **No more figuring out why building the same repository succeeds on computer A but fails on computer B**  
  Typical causes are: programs (e.g. compiler, linker) were installed in
  different directories, environment variables have different values.
  Yam demands all directory trees from which files are read by the build commands
  to be declared. Yam will fail the build when a command reads a file from a
  not-declared directory tree and tell you which command tried to read what file.
  Also Yam does not allow commands to depend on environment variables. These
  measures allow you to solve most issues in minutes instead of hours.

## Build language

- **No more days spent learning yet-another-build-language**  
  When using Yam you define build rules in so-called build files. These text
  files contain build rule definitions written in the Yam declarative build
  language. This is a small and simple language that can be mastered in a matter
  of hours. You don't like the language? Then move on to the next bullet.
- **No more being locked in the build language dictated by your build system**  
  Yam recognizes that there is no such thing as the perfect build language. Yam
  therefore allows you to write custom build files in the language(s) of your
  choice, e.g. Python, Ruby, Perl, Lua. A custom build file, when run, outputs
  build rules in the Yam build language syntax. When writing custom build files
  you will typically use only a very small subset of the Yam language. When Yam
  detects a custom build file it executes it and parses its output in the same
  way as it would parse the content of a Yam build file.

# When not to use Yam?

Yam does not support distributed build and distributed build cache. This makes
Yam not suitable for building huge repositories that are maintained by hundreds
or thousands of engineers, like those at Google, Microsoft, Amazon, Facebook.

# A short history of Yam

The creators of Yam worked in a company that used the ClearCase version
management system to manage a ~50,000 file repository containing ~8,000,000
lines of code (C++, C# and many other file types). This repository was actively
developed by ~100-200 software engineers. Engineers developed and built software
on 4-6 core high-end laptops with 16 GB RAM and 512Mb-1TB SSD storage. Software
was built using omake, the build system that comes with ClearCase. Omake does not
parallellize the build. A full build on such a laptop took 24 hours. An
incremental build to update all out-dated output files took 30 minutes (!) to
figure out which commands to re-execute. The net command execution time for a
typical incremental build was less than a few minutes, often only a few seconds.
This resulted in engineers trying to outsmart the build system by only building
the output files that they thought were impacted by their source code changes.
This frequently resulted in mistakes and lots of wasted time in debugging.

The company then decided to replace ClearCase by git and to develop a new build
system (not Yam) tailored to the archive structure and the company's development
process. Using git and this new build system a full build took 3-4 hours.
Typical incremental builds took sub-second time to figure out what to build and
command execution takes from a few seconds to a few minutes. The CI
(Continuous Integration) pipeline performed incremental builds on Pull requests
and full builds during nights. A full CI build was performed on a 10-core
computer in 1.5-2 hours. Adding more cores did not further reduce build time due
to the serialization inherent in archive source files.
The new build system was not suited to be open-sourced because it required a
very specific directory structure and expensive third-party software. A few
years later the authors retired and decided to use their build system experiences
to create Yam.

# Core concepts

## Build rules

The user defines build rules in build files. A rule looks like:

```
inputFiles |> command |> outputFiles
```

where the command transforms the input files into output files.
The input files of a rule can be source files or output files of earlier
defined rules.

## Directed Acyclic Graph

Yam uses the build rules to create a Directed Acyclic Graph (DAG). The nodes
(vertices) in the DAG represent source files, commands and generated files.
Edges represent the relations between input files and commands and between
commands and their output files.
Example of a graph that compiles C++ files and links them into a library.

```
TODO
```

The DAG dictates the order in which commands are executed: when a command X
produces an output file F that is an input file of command Y, then Yam
executes X before Y. E.g. Yam executes all compilation commands before
executing the link-library command.
Yam parallellizes the execution of commands that do not depend in each others
output files. E.g. Yam executes the compilation commands in parallel.

## File last-write-time, file hash

Yam retrieves for each input/output file used/produced by the build its
last-write-time and computes a hash of its content.
Last-write-time and hash are stored in the file node in the DAG.
At the next build Yam re-computes the file hashes of files whose
last-write-time has changed since the previous build.

## Build avoidance

Yam implements the following build avoidance techniques:

- incremental build
  Only re-execute commands when files on which it depends have changed.
- file aspect hashes
  Only re-execute commands when relevant aspects of files on which it depends
  have changed.
- build cache
  Builds store their outputs in a cache to allow them to be re-used by other
  builds.

Subsequent sections discuss these techniques in more detail.

## Incremental build

Yam re-executes a command when one or more of the following are true:

- hashes of the command's input files have changed since the previous build.  
  E.g. the user edited a source file.
- hashes of the command's output files have changed.
  E.g. the user deleted output files.
- the command text has changed since the previous build.  
  E.g. the user modified a compilation option in a build rule.

The use of file hashes avoids command re-execution in cases where a file's
last-write-time changes while the file hash remains unchanged. This avoids
many unnecessary command re-executions.
Examples: a source file was edited, saved and then restored to its state before
the edits. This file will have an unchanged hash and will therefore not
cause a compilation command to be re-executed.
A source file where only comments were edited will have a changed hash and
will cause re-compilation. The resulting object file however will not change.
A command that links the object file will therefore not be re-executed.

## File aspect hashes

Sometimes a command's output only depends on certain aspects of an input file.
Examples:  
A compilation command only depends on the code sections. Changes to comments in
a source file do not impact the resulting object file.
A library or executable that uses a Windows dll (dynamic load library) only
depends on the interface of that dll. Changes in a dynamic load library that
do not affect its interface do not impact libraries and executables that are
linked against that dll.

In such cases unnecessary command re-execution can be avoided by using a
hash of the relevant aspect of the input file. Yam supports this by allowing
you to configure:

- for each file aspect: the function that computes the file aspect hash.
- for each input file type: the aspect hashes to be computed for files of that
  type.
- for each command: the relevant aspects of its input files.

## Build state

## Performance

Build execution time is determined by the following factors:

- The net execution time of the build commands (i.e. without overhead incurred
  by the build system)
- The degree to which execution of build commands can be parallellized  
  This depends on:
  - The structure of the command dependency graph
  - The number of available processor cores
- The computer hardware, speed of I/O system
- The overhead of the build system
    - Determine minimum set of commands to be up-to-date

The first three factors are not determined by the build system.
Build system overhead consists of:

- Time needed to figure out which files have been modified since the previous
  build
- Time needed to re-parse modified build files
- Time needed to figure out which commands to re-execute given the list of
  modified files
- Time needed to parallelize command execution
- Time needed to monitor command execution and update the dependency graph

# Some differences between Tup and Yam

TODO: build language comparison
TODO: run-script
TODO: no %<group>, instead input <group> always treated as list of files

Yam removes limitations mentioned by several users in the [tup users mailing
list](https://groups.google.com/g/tup-users).

**_Tup_**: output files are stored in the same directory as the buildfile or in
a variant directory.  
**_Yam_**: user decides where to store output files.

**_Tup_**: mandatory output files must be declared by name. Outputs to be ignored
must be declared by name or glob.  
Tup fails the build when a command produces non-declared outputs or when not all
mandatory outputs are produced.  
**_Yam_**: mandatory output files must be declared by name. Outputs to be ignored
must be declared by name or glob. Optional outputs must be declared by name or
glob.
Yam fails the build outputs when a command produces non-declared outputs or when
not all mandatory outputs are produced.

**_Tup_**: when a rule in buildfile X (Tupfile in directory X) references input
file Y/someFile, then tup parses the Tupfile in Y to figure out whether
Y/someFile is defined as an output (generated file) of a rule in Y. Tup can do
so because outputs are always in the directory that contains the buildfile. If
Y/someFile is not a generated file then tup treats it as a normal (source) file
and checks existence of that file.
**_Yam_**: in the above example the user must declared in buildfile X a
dependency on buildfile Y. Failure to do so will result in an error: Y/someFile
does not exist. This is the price to pay for allowing the user to store output
files anywhere, while keeping the incremental parsing of buildfiles.

**_Tup_**: output files can optionally be stored in one group and in one bin.  
**_Yam_**: output files can optionally be stored in multiple groups and bins.
Both mandatory and optional outputs are stored in the specified groups/bins.

Optional outputs allow for rules where the names of the output files are not
known a-priori. This allows for rules like:

```
images.zip |> unzip %f -outDir images |> images/*.bmp <images>
foreach <images> |> smooth %f -out %o |> smoothed/%B.%e <smoothed>
<smoothed> |> zip %f -out %o |> smoothedImages.zip
```
