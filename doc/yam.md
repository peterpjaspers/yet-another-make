- [What is Yam?](#what-is-yam)
- [When to use Yam?](#when-to-use-yam)
- [When not to use Yam?](#when-not-to-use-yam)
- [Concepts](#concepts)
  - [Build commands, build rules](#build-commands-build-rules)
  - [Build files](#build-files)
  - [Yam file repository](#yam-file-repository)
  - [Multi repository build](#multi-repository-build)
  - [Directed Acyclic Graph](#directed-acyclic-graph)
  - [File aspect hash](#file-aspect-hash)
  - [File last-write-time](#file-last-write-time)
  - [Incremental build](#incremental-build)
  - [File access monitoring](#file-access-monitoring)
  - [Stale output files](#stale-output-files)
  - [Build state](#build-state)
  - [Yam server](#yam-server)
  - [File change monitoring](#file-change-monitoring)
- [Claims](#claims)
  - [Quick edit-compile-test cycle](#quick-edit-compile-test-cycle)
    - [No slow build due to finding modified files](#no-slow-build-due-to-finding-modified-files)
    - [No slow build due to under-utilization of CPU cores](#no-slow-build-due-to-under-utilization-of-cpu-cores)
    - [No slow build due to unnecessary command re-execution](#no-slow-build-due-to-unnecessary-command-re-execution)
  - [Correct incremental build](#correct-incremental-build)
    - [Failure to re-execute a command](#failure-to-re-execute-a-command)
    - [Missing, unexpected and stale output files](#missing-unexpected-and-stale-output-files)
    - [Wrong build order](#wrong-build-order)
    - [Build commands that are not parallel-safe](#build-commands-that-are-not-parallel-safe)
    - [Commands that update source files](#commands-that-update-source-files)
  - [Reproducable builds](#reproducable-builds)
  - [Automatic deletion of obsolete output files](#automatic-deletion-of-obsolete-output-files)
  - [Write rules in a simple build language](#write-rules-in-a-simple-build-language)
  - [Write rules in a build language of your choice](#write-rules-in-a-build-language-of-your-choice)
  - [Ease-of-use](#ease-of-use)
    - [Scalability](#scalability)
    - [Correctness](#correctness)
  - [Reproducability, deterministic and hermetic](#reproducability-deterministic-and-hermetic)
  - [Build language](#build-language)
- [Some differences between Tup and Yam](#some-differences-between-tup-and-yam)
- [A short history of Yam](#a-short-history-of-yam)




# What is Yam?

Yam (Yet another make) is a software build tool. It is intended for
software developers to automate software builds.  
[Wikipedia](https://en.wikipedia.org/wiki/Software_build): In software development, a build is the process of converting source
code files into standalone software artifact(s) that can be run on a computer,
or the result of doing so.

Yam is a general purpose file-based build tool: it executes user-defined
commands where each command transforms input files to output files. Output 
files of one command can be used as input files of other commands. A command
is a script in Windows `cmd.exe` shell syntax.

Yam is fully ignorant of how commands produce output files. This makes Yam
suitable for other usages than just software builds. For example you can define
commands to run unit tests, to convert MS Word documents into PDF documents, to
transform a zip file with raw images into a zip file with processed images.

Yam pays tribute to the [tup](https://gittup.org/tup/index.html) build tool
 from which it re-used many concepts.
 See [Some differences between Tup and Yam](#some-differences-between-tup-and-yam) .

Yam runs on the Windows operating system. Future versions will also support Linux.

# When to use Yam?
Consider using Yam when you want: 
- a quick edit-compile-test cycle, also for large (>10,000 files) repositories.
- correct incremental builds (or: no need to ever run clean builds).
- reproducable builds.
- to use C++ modules.
- obsolete output files to be deleted automatically.
- to write build rules in a simple out-of-the-box language.
- to write build rules in a language of your choice.
- ease-of-use.

Chapter [Claims]() explains how Yam fulfills these claims by describing how
it addresses common problems in the areas of performance, correctness, 
reproducability, build language and ease-of-use.
The [Claims]() chapter is best read after reading chapter [Concepts]().

# When not to use Yam?
Yam parallelizes a build using local CPU cores. It cannot distribute the build
across cores on multiple computers. This limits the degree of parallization.  
This makes Yam not suited for building huge repositories like those at 
Google, Microsoft, Amazon. 

For huge repositories Yam would still provide a quick edit-compile-test 
cycle for the typical use case in which only a few files are modified in 
between builds. Yam however would take excessive amounts for builds when huge
amounts of commands need to be re-executed, e.g. in the very first build.

[A short history of Yam](#a-short-history-of-yam) gives an idea of build times
in a reasonably large repository.

# Concepts

## Build commands, build rules
Yam executes commands. The user specifies these commands by means of build 
rules. There are two types of build rules:

```
inputFiles |> command script |> outputFiles
foreach inputFiles |> command script |> outputFiles
```

`inputFiles` and `outputFiles` are file lists.
`command` is a script written in Windows `cmd.exe` shell syntax that derives
output files from input files. Output files are generated files. Input files
are generated files or source files.

The first rule defines one command that transforms the set of input files
into the output files. E.g. link the input object files into a library.
The second rule defines multiple commands, one for each file in inputFiles.
E.g. compile each file in inputFiles.

See [Buildfile syntax]() for details.

## Build files
Build rules are stored in build files.
Each directory in the repository contains 0 or 1 build file.
Yam recognizes a build file by its name: `buildfile_yam[post][ext]`
where `[post]` is a user-defined postfix and `[ext]` is the file extension.
Build files with extension `.txt` contain build rules in [Buildfile syntax]().
Build files with other extensions are executable files that output build rules.
E.g. Yam will execute `buildfile_yam.py` by invoking the Python interpreter and
Yam will parse the output as if it had been stored in `buildfile_yam.txt`.


## Yam file repository
A Yam file repository, or file repository, is a directory tree from which
Yam reads build files and source files and/or into which Yam writes output 
files.

The first time you run Yam in a directory it will create a hidden directory 
that marks the root of a Yam file repository. Yam will then scan the entire 
directory tree for build files. The build files dictate where Yam reads 
source files and where Yam writes output files.
A repository in which you start a build is called the home repository of
that build.

Builds often read source files outside the home repository. E.g. C++ std
library files are typically not stored in the home repository.
Build files may specify output files outside the home repository, e.g.
to avoid the home repository to become cluttered with output files.

Yam requires the user to declare all file repositories from which files
are read and/or into which files are written. For each of these repositories
you must specify whether it is to be build, tracked or ignored:
- A to-be-build repository is treated as if it had been a subdirectory tree
of the home repository, i.e. it will process build files, monitor the tree for
changes and register dependencies on files in the tree in the build state.
- A to-be-tracked repository is treated as if it had been a subdirectory tree
of the home repository except that Yam will not process build files in that
tree.
- A to-be-ignored repository will ignore all changes in that repository, i.e.
commands in the home repository that depend on files in a to-be-ignored
repository will not be re-executed when these files change. This repository
type is used for repositories that are guaranteed to be read-only. E.g. the
Windows include file tree.
Using to-be-ignored repositories reduces resource usage because Yam does not
need to keep track of dependencies and does not need to monitor the 
repository for changes.

Yam will fail a build when a command accesses files that are not in the 
declared file repositories.

## Multi repository build
Many organizations use a multi-repository setup to store their software.
E.g. an organization stores software that is reused by multiple applications
in a platform repository. Each application is stored in an application 
repository that depends in (a specific version of) the platform repository.
When using git modules the platform shows itself as a subdirectory of the
repository directory. Building the application repository will build both
platform and application software without requiring any special support
from the build tool.

Junctions could be an alternative for git modules: the application repository
then contains a junction to the platform repository.  
Unfortunately Yam does not support the use of junctions.

Yam however does support multi-repository builds by allowing you to specify
a dependent repository as to-be-build.
 
## Directed Acyclic Graph
Yam uses the build rules to create a Directed Acyclic Graph (DAG). The nodes
(vertices) in the DAG represent source files, commands and generated files.
Edges represent the dependencies of commands on their input files and of the 
relation between commands and their output files.
Example of a graph that compiles C++ files and links them into a library.

```
TODO
```

The DAG dictates the order in which commands are executed: when a command X
produces an output file F that is an input file of command Y, then Yam
executes X before Y. E.g. Yam executes all compilation commands before
executing the link-library command.
Yam parallellizes the execution of commands that do not depend on each others
output files. E.g. Yam executes the compilation commands in parallel

## File aspect hash
Yam computes for each file a hash of its content. The hash values are stored
in the file nodes of the DAG. Commands that depend on files whose hash have
changed since the previous build are re-executed.

Sometimes a command's output only depends on a specific aspect of an input 
file. E.g. an object file only depends on code, not on comments.
E.g. a library or executable that uses a Windows dll (dynamic load library) 
only depends on the dll interface, not on its implementation details. 

In such cases unnecessary command re-execution can be avoided by using a
hash of the relevant aspect of the input file instead of using a hash of the
entire file content. Yam supports this by allowing you to configure:
- for each file aspect: the function that computes the file aspect hash.
- for each input file type: the aspect hashes to be computed for files of that
  type.
- for each command: the relevant aspects of its input files.

## File last-write-time
To minimize overhead Yam only re-computes hashes when a file's last-write-time
has changed. This poses a risk: a rapid succession of write operations may not 
result in an update of a file's last-write-time. As a consequence Yam will not
re-compute the file hash, hence will not detect that the file was modified, 
hence will not re-execute impacted commands.
The higher the last-write-time resolution the lower the likelihood of this
error to occur.
 
Note: Windows [File time](https://learn.microsoft.com/en-us/windows/win32/sysinfo/file-times) has 100 ns precision. File time resolution however is typically (much)
lower. E.g. the last-write-time resolution on the author's computer was measured
to be 1 ms.

## Incremental build
A Yam build is always an incremental build: Yam only re-executes a command when
its output files do not exist or are no longer valid. A command's output files 
become invalid when one or more of the following have changed since the 
previous build:
- Relevant aspect hashes of the command's input files.
- Entire-file aspect hashes of the command's output files.  
  This happens when the user tampers with output files, e.g. deletes them.
- Command script.  
  E.g. the user modified a compilation option in a build rule.

Yam does not re-execute commands when environment variables change.
This is not needed because commands do not inherit the environment of the
shell from wich Yam is started.

To ensure build correctness Yam must know:
- All input and output files of a command.  
  Yam achieves this through [File access monitoring](#file-access-monitoring).
- Which files were modified since the previous build.  
  Yam achieves this through [File change monitoring](#file-change-monitoring).

## File access monitoring
Yam must know all input and output files of a command to ensure correct 
build ordering and correct re-execution of commands that depend on
modified input files or tampered-with output files.

To ensure proper build order Yam requires the user to specify for each command
all of its output files and all of its generated input files, i.e. input file 
that are produced by other commands.    
Yam does not require the user to specify all of its input source files.
E.g. for a C++ compilation the user only specifies the cpp file to be compiled. 
It is not needed to list all header files included by that cpp file.

Yam can hence not rely on the input and output files specified in the build rules
because these specifications are incomplete and prone to user error.  
Example of user error:
```
foreach a.cpp b.cpp |> compile %f -o %o |> %B.obj
                    |> link a.obj b.obj -out %o |> my.lib
```
Note: %f is replaced by the input file name, %o by the output file name.  
The link rule does not specify inputs `a.obj` and `b.obj` while these files
will be read by the link command. As a consequence Yam will execute the compile
and link rules in parallel which results in undefined content of `my.lib`.
Correct link rules are:
```
a.obj b.obj |> link %f -out %o |> my.lib
*.obj       |> link %f -out %o |> another.lib
```

Yam solves these issues by monitoring the file accesses made by the command.
Yam thus discovers all actual input and output files of the command.
This allows Yam to detect the following errors in build rules and command 
scripts:
- Wrong build order, due to incomplete input specification.
- Unknown output files, due to error in command script or output specification.
- Too few output files, due to error in command script or output specification.
- Multiple commands that write the same file.
- Commands that write source files.

These errors are caused by user errors in build rule specification and
commands that are not parallel-safe.

## Stale output files
Yam deletes stale output files when:
- Output files in a build rule specification are renamed or removed.
- Source files are renamed/moved or deleted.

Example:
Assume files `foo.cpp` and `bar.cpp` and rule  `foreach *.cpp |> compile %f -o %o |> %B.obj`  
A build will produce files `foo.obj` and `bar.obj`.  
When you:
- Delete `bar.cop`, Yam will delete `bar.obj`.  
- Rename `foo.cpp` to `main.cpp`, Yam will delete `foo.obj` and produce `main.obj`. 
- Rename in rule  `%B.obj` into `bin/%B.obj`, Yam will delete  `foo.obj` and  `bar.obj`
and produce `bin/foo.obj` and  `bin/bar.obj`
  

## Build state
The build state is the DAG as derived from the build rules augmented with the 
input and output file nodes for the files found by file access monitoring of 
the commands. The build state is stored in a single file.

## Yam server
Yam always reads the entire build state into memory. This takes time 
proportional to the size of repository. Yam therefore uses a client-server 
design. The server caches the build state and performs builds on request of 
the client.

## File change monitoring
The Yam server continously monitors the repository for file changes. 
On receipt of a file change notification the associated file node and the
command nodes that depend on the file are marked dirty. When the next build
is started Yam knows what to do: re-compute the hashes of dirty files and 
re-execute dirty commands when relevant hashes have changed.  

# Claims

## Quick edit-compile-test cycle
This section discusses how Yam avoids some common causes of slow builds. 

### No slow build due to finding modified files  
Most build tools scan the entire file repository to find files modified since
the previous build. This requires time proportional to the number of files in 
the repository. The scan time is perceived as the time between start of build 
and start of command execution. For large repositories this can exceed the time 
needed to execute the out-dated command(s). E.g. the build tool may take 10 
seconds to start a 1 second compilation.  
Yam does not have this problem because it continuously monitors the file 
system for changes.

*For the typical way-of-working in which you modify only a few files in-between
two builds it will take Yam, independent of the number of files in your 
repository, less than 10 milliseconds to determine which commands to
re-execute.*

### No slow build due to under-utilization of CPU cores 
Yam parallelizes the execution of commands that do not depend on each others
output files. Commands that can be executed in parallel are queued to a 
thread pool. By default the thread pool size is equal to the number of CPU
cores. This ensures 100% CPU utilization as long as the thread pool queue  
contains sufficient commands. Note that command dependencies limit
parallelization.

### No slow build due to unnecessary command re-execution
Yam minimizes unnecessary command re-executions through the use of 
[file aspect hashes](#file-aspect-hash).

## Correct incremental build
Section [Incremental Build](#incremental-build) explains how Yam achieves
correct builds.

Typical causes of incorrect incremental build are:
- Failure to re-execute a command while its outputs were invalidated.
- Wrong build order.
- Commands that are not parallel-safe.
- Failure to delete stale output files.
  
### Failure to re-execute a command
Yam re-executes a command when one or more of the following is true:
- Relevant aspects of the command's input files have changed.
- The command description (inputs, command script, outputs) has changed.  
- The command output files have been tampered with.

Note: Yam does not re-execute commands when environment variables change. 
This is not needed because Yam executes commands in a cmd shell with an 
empty environment.

Yam must know all input files of a command. Yam requires the user to specify
all input files that are outputs files of other commands. Yam does 
not require the user to specify all input source files. E.g. for a C++ 
compilation the user only specifies the cpp file to be compiled. It is not
needed to list all header files included by that cpp file because Yam 
discovers all input and output files of a command by monitoring the 
command's file accesses.

Yam only determines whether (aspects of) file content changed when a file's
last-write-time has changed. A fast succession of write operations may not 
update the last-write-time. This causes Yam to incorrectly skip command 
execution.

*Some older build tools like `make` do not re-execute a command when the
command script changes.*
*Some older build tools like `make` only re-execute a command when an output
file is older than its input files. When undoing file changes by resetting it
to its committed version the version management systems may reset the file's 
last-write-time to the one of the committed version. In this case the time
decreases and `make` will not re-execute the command.*

### Missing, unexpected and stale output files
Stale output files are output files of deleted commands or of commands 
that were modified to no longer output these files. Stale output files can lead
to errors. Example: assume a command produces oldProgram.exe. A test script 
exists that tests oldProgram.exe. The command is now changed to produce 
newProgram.exe, causing the previously produce oldProgram.exe to become stale.
If the stale file is not deleted and the user forgets to update the test script
then the test script incorrectly passes the test.

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
after P. Proper build order can only be ensured when all input dependencies
are known. Yam therefore fails the build if command C reads F while F is not
specified as input of C.  
Yam can detect such errors because it monitors the command's file accesses.

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

## Reproducable builds
Yam helps making builds reproducable by:
- Requiring you to specify all repositories from/to which Yam is allowed to 
read/write files.  
- Executing build commands with an empty environment.  
The shell that executes the command does not inherit the environment
variables of the shell in which Yam is started.

## Automatic deletion of obsolete output files
You removed a build command that produced some file X? Yam will delete X.
You renamed the output of a build command from X to Y? Yam will delete X
and create Y.
You renamed/moved a directory? 

## Write rules in a simple build language
## Write rules in a build language of your choice

## Ease-of-use
From [Build System Rules and Algorithms](https://gittup.org/tup/build_system_rules_and_algorithms.pdf):  
Any build system must abide by the following rules:
1. Scalability: Given an already up-to-date system, applying a change to the
system and building it should take time that is proportionate to the
changes required to bring the system up to date.
2. Correctness: Given an up-to-date system in state X, applying change A
and updating the system may result in state Y. Undoing change A and
performing an update must return the system to state X.
3. Usability: There must not be different ways of building the system that
the user must remember in order to accommodate rules 1) or 2).
There should only one way to obtain a correct build: run the build tool.

Yam adheres to all of these rules, many other build tools do not.

This section describes violations of rules 1) and 2) that force the 
user to use different ways of building the system than just running the build tool, thus violating rule 3).

### Scalability
Many build tools scan the entire repository to find files modified since
the previous build, thus violating rule 1). This tempts engineers to outsmart 
the build tool by only  building the output files that they thought were 
impacted by their source code changes. This frequently results in mistakes
and lots of wasted time in debugging.
No need for this in Yam: simply run yam. 
See section [File change monitoring ](#file-change-monitoring).

### Correctness  
Section [Correct incremental build](#correct-incremental-build) explains how 
Yam ensures correct build results.

Incorrect build outputs caused by not parallel-safe commands are sometimes 
only detected after lots of wasted time in debugging. Users recover by running
an, often time-consuming, clean build.
No need for this in Yam: simply run yam.

`Make` build tool does not re-execute commands when the command has changed.
The user must be aware of this and run a clean build after command
changes. 
No need for this in Yam: simply run yam.

Some build tools do not properly handle renaming of files.
This results in stale output files. This requires the user to manually
delete these files and run a clean build.  
No need for this in Yam: simply run yam.

`Make` build tool only re-execute commands when input files are newer
than the output files. Replacing the content of your worktree with an older
version of your repository may result in file times to become older.
This requires the user to run a clean build.
Yam's use of last-write-time and file hashes avoids such problems.
No need for this in Yam: simply run yam.

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
- **No more being locked in the build language dictated by your build tool**  
  Yam recognizes that there is no such thing as the perfect build language. Yam
  therefore allows you to write custom build files in the language(s) of your
  choice, e.g. Python, Ruby, Perl, Lua. A custom build file, when run, outputs
  build rules in the Yam build language syntax. When writing custom build files
  you will typically use only a very small subset of the Yam language. When Yam
  detects a custom build file it executes it and parses its output in the same
  way as it would parse the content of a Yam build file.

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

# A short history of Yam
The creators of Yam worked in a company that used the ClearCase version
management system to manage a ~50,000 file repository containing ~8,000,000
lines of code (C++, C# and many other file types). This repository was actively
developed by ~100-200 software engineers. Engineers developed and built software
on 4-6 core high-end laptops with 16 GB RAM and 512Mb-1TB SSD storage. Software
was built using omake, the build tool that comes with ClearCase. Omake does not
parallellize the build. A full build on such a laptop took 24 hours. An
incremental build to update all out-dated output files took 30 minutes (!) to
figure out which commands to re-execute. The net command execution time for a
typical incremental build was less than a few minutes, often only a few seconds.
This resulted in engineers trying to outsmart the build tool by only building
the output files that they thought were impacted by their source code changes.
This frequently resulted in mistakes and lots of wasted time in debugging.

The company then decided to replace ClearCase by git and to develop a new build
system (not Yam) tailored to the archive structure and the company's development
process. Using git and this new build tool a full build took 3-4 hours.
Typical incremental builds took sub-second time to figure out what to build and
command execution takes from a few seconds to a few minutes. The CI
(Continuous Integration) pipeline performed incremental builds on Pull requests
and full builds during nights. A full CI build was performed on a 10-core
computer in 1.5-2 hours. Adding more cores did not further reduce build time due
to the serialization inherent in archive source files.
The new build tool was not suited to be open-sourced because it required a
very specific directory structure and expensive third-party software. A few
years later the authors retired and decided to use their build tool experiences
to create Yam.