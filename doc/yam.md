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
- correct incremental builds, such that you never need to run clean builds.
- reproducable builds.
- obsolete output files to be deleted automatically.
- to write build rules/commands in a simple out-of-the-box language, and/or...
- ...when you wnat to write build rules in a language of your choice.
- ease-of-use.

Chapter [Claims]() explains how Yam fulfills these claims by describing how
it addresses common problems in the areas of performance, correctness, 
reproducability, build language and ease-of-use.
The [Claims]() chapter is best read after reading chapter [Concepts]().

# When not to use Yam?
Yam can only parallelize a build using local CPU cores. This makes Yam not
suited for building huge (>100,000 files) repositories that are maintained by
hundreds or thousands of engineers, like those at Google, Microsoft, Amazon.

Yam would still provide a quick edit-compile-test cycle for such large
repositories. Its initial build however would take an excessive amount of time.

TODO: consider preparing Yam for integration with IncrediBuild.


# Core concepts

## Build commands, build rules

Yam executes commands. Commands are derived from build rules.
There are two types of build rules:

```
inputFiles |> command |> outputFiles
foreach inputFiles |> command |> outputFiles
```

`inputFiles` and `outputFiles` are lists of 0, 1 or more files.
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
Yam recognizes a build file by its name format: `buildfile_yam[post][ext]`
where [post] is a user-defined postfix and [ext] is the file extension.
Build files with extension `.txt` contain build rules in [Buildfile syntax]().
Build files with other extensions, e.g. `.py`, are executable files that
output build rules to `stdout`. 
Each directory in the repository contains 0 or 1 build file.


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
Yam parallellizes the execution of commands that do not depend on each others
output files. E.g. Yam executes the compilation commands in parallel

## File last-write-time, file hash

Yam retrieves for each input/output file used/produced by the build its
last-write-time and computes a hash of its content.

Last-write-time and hash are stored in a file node in the DAG.
At the next build Yam re-computes the hashes of files whose
last-write-time has changed since the previous build.

Commands that depend on a file are re-executed when the file's hash
has changed.

Note: the resolution of last-write-time is important. A rapid succession
of write operations may not result in an update of last-write-time. As a
consequence Yam will not re-compute the file hash and will not
re-execute commands that depend on the modified file.
Do not confuse precision and resolution. E.g. the NTFS filesystem has a
last-write-time precision of 100 ns while the resolution on the author's
computer is measured to be 10 times the precision, i.e. 1 ms.

## File aspect hashes

Sometimes a command's output only depends on certain aspects of an input file.
E.g. a compilation command only depends on the code sections. Changes to 
comments in a source file do not impact the resulting object file.
E.g. a library or executable that uses a Windows dll (dynamic load library) 
only depends on the interface of that dll. Changes in a dynamic load library 
that do not affect its interface do not impact libraries and executables that 
are linked against (the import library of) that dll.

In such cases unnecessary command re-execution can be avoided by using a
hash of the relevant aspect of the input file. Yam supports this by allowing
you to configure:
- for each file aspect: the function that computes the file aspect hash.
- for each input file type: the aspect hashes to be computed for files of that
  type.
- for each command: the relevant aspects of its input files.


## Incremental build

Yam re-executes a command when one or more of the following has changed 
since the previous build:
- Relevant aspect hashes of the command's input files.  
- Command's output files.
  E.g. the user deleted output files (which he/she is not supposed to do).
- Command script.  
  E.g. the user modified a compilation option in a build rule.

An aspect of an input file of a command is relevant to that command when 
changes in the input aspect invalidate the command's output file. 
E.g. a change in the code-aspect of a header file invalidates all object 
files that depend on that header file. 
A command can be configured with its relevant aspects.
By default the relevant aspect is entire-file.

The use of file aspect hashes avoids command re-execution in cases where a
file's last-write-time changes while the file aspect hash remains unchanged.
E.g. a source file was edited, saved and then restored to its state before
the edits. This file will have an unchanged hash and will therefore not
cause a compilation command to be re-executed.
E.g. a compilation command that uses the default entire-file aspect
will re-execute when the user only edits comments in its input files.
The hash of the resulting object file however will not change (assuming a 
deterministic compiler). Link command that link the object file will 
therefore not be re-executed.

## File access monitoring
For a correct incremental build Yam must know all input files of a command.
Yam cannot rely on the input files specified in the build rules:
- Yam does not require the user to specify all input source files. 
E.g. for a C++ compilation the user only specifies the cpp file to be compiled.
It is not needed to list all header files included by that cpp file. This
would be redundant information, a maintenance nightmare and error prone.
- Yam does require the user to specify all generated input files, i.e. files
produced by other rules. This is needed to ensure proper build order.

This is prone to user errors. Example:
```
foreach a.cpp b.cpp |> gcc %f -o %o |> %B.obj
|> link a.obj b.obj -out %o |> my.lib
```
The link rule does not specify inputs `a.obj` and `b.obj` while these files
will be read by the link command. Yam will execute the compile and link rules 
in parallel which results in undefined content of `my.lib`.
Correct link rules are:
```
a.obj b.obj |> link %f -out %o |> my.lib
*.obj |> link %f -out %o |> another.lib
```

Yam solves these issues by monitoring the file accesses made by the command.
GA HIER VERDER



## File change monitoring

## Build state

# Claims

## Performance

### Long time between start of build and start of command execution  
Build tools only re-execute commands that are out-dated. A command is 
out-dated when its input files were modified since the previous build.
To find these files many build tools scan the entire source file repository
which requires time proportional to the number of files in the repository.
This scan time is perceived as the time between start of build and start of 
command execution. For large repositories this can exceed the time needed to
execute the out-dated command(s). E.g. the build tool may take 10 seconds to 
start a 1 second compilation.  
Yam takes a different approach to discover modified files: it continuously
monitors the file system for changes. When you start a build Yam already knows
which files were modified, and hence which commands to re-execute.

*For the typical way-of-working in which you modify only a few files in-between
two builds it will take Yam, independent of the number of files in your 
repository, less than 10 milliseconds to determine which commands to
re-execute.*

*Long times between start of build and start of command execution tempt
engineers to outsmart the build tool by only building the output files that
they thought were impacted by their source code changes. This frequently 
results in mistakes and lots of wasted time in debugging.*

### Long build time due to under-utilization of CPU cores 
Yam parallelizes the execution of commands that do not depend on each others
output files. E.g. C++ compile commands execute in parallel while a link 
command executes after the compile commands that produce its input object 
files. Commands that can be executed in parallel are queued to a thread pool.
By default the thread pool size is equal to the number of CPU cores. This 
ensures 100% CPU utilization as long as the thread pool queue is not empty.

*Input-output dependencies between commands limit parallelization.
This is outside control of the build tool.*

### Long build time due to unnecessary command re-execution
To avoid unnecessary command re-execution Yam can be setup to only re-execute a
command when relevant aspects of the command's input files change. Examples:

- Unnecessary recompilation after changing comments  
Assume you change a comment in a C/C++ header file that is included by hundreds
of cpp files and start a build. Most build tools will recompile all these cpp 
files, even though comment changes do not affect the resulting object files.  
Not so in Yam. Yam can be setup to only re-execute a compile command when the 
non-comment sections of the command's input files change. This allows you to 
improve comments without the cost of long builds. 

- Unnecessary recompilation after undoing edits  
Assume you modify and save a C/C++ header file that is included by hundreds of
C/C++cp files, then change your mind, undo the changes and start a build.
Most build tools will recompile all the cpp files because their 
last-write-times have changed.  
Not so in Yam. By default a change in a file's last-write-time is a signal to
Yam to verify whether (aspects of) its content has changed. Rebuilds only happen
on content changes.

- Unnecessary relinking after changing a dynamic load library
Assume you modify an implementation detail in a C++ file that is linked into a
dynamic load library (dll). Assume that a large number of dlls and/or 
executables depend on the changed dll, either directly or indirectly via other
dlls. Most build tools will relink all these dlls, recursively up to the 
executables that use them.  
Not so in Yam: Yam can be setup to only re-execute a link command a dll when 
the interface aspect of the command's input dlls change.


## Correctness

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