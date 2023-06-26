# What is YAM?

YAM (Yet Another Make) is a software build tool. It derives it name from the first of all build tools: [Make](https://en.wikipedia.org/wiki/Make_(software))
<br>There are many 
software build tools, e.g. Make, Apache Maven, Buck, Gradle, Bazel, MSBuild, tup. So why create another one? Section [Features](#features)
tries to answer this question.

# Directed Acyclic Graph (DAG)

Like all build systems YAM uses a DAG as its main data structure.  
Question to chatGPT:  
Explain the use of a directed acyclic graph in software build systems

The answer of chatGPT:  
A directed acyclic graph (DAG) is a data structure that consists of nodes 
connected by directed edges, where the edges only flow in one direction and
there are no cycles. DAGs have various applications in computer science, 
including software build systems.  

In the context of software build systems, a DAG is often used to represent the
dependencies between different components or modules of a software project. 
Each node in the DAG represents a component, and the edges represent the 
dependencies between the components. The directed edges indicate the direction
of dependency, meaning that one component depends on another for its build or
execution.

Here's how a DAG is used in a software build system:

Dependency Management: The DAG helps manage dependencies between components. 
When a component changes, only the components directly or indirectly dependent
on it need to be rebuilt. By traversing the DAG, the build system can determine
the minimal set of components that need to be rebuilt, avoiding unnecessary 
builds and saving time.

Build Order Determination: The DAG defines the order in which components should
be built. Since the edges in the DAG indicate the dependencies, a build system
can traverse the DAG in a topological order, where each component is built 
after all its dependencies have been built. This ensures that a component is 
built using the most up-to-date version of its dependencies.

Incremental Builds: With a DAG, a build system can perform incremental builds 
by tracking changes in the DAG. When a component or its dependencies change, 
the build system can identify the affected components by examining the paths in
the DAG and rebuild only those components. This approach saves time by avoiding
unnecessary rebuilding of unaffected components.

Parallelization: The DAG allows for parallel execution of independent build 
tasks. Since a DAG represents the dependencies between components, build tasks
that are not dependent on each other can be executed concurrently. This enables
the build system to take advantage of multi-core processors or distributed build
environments, leading to faster builds.

Popular software build systems like Make, Apache Maven, and Gradle utilize DAGs
to manage dependencies, determine build order, and optimize the build process.
By representing the dependencies between components as a DAG, these build
systems can efficiently and accurately build software projects while minimizing
unnecessary work and maximizing parallelization.

End of chatGPT answer.

## DAG example
The following example illustrates the use of a DAG in C++ development.  
Given a C++ header file c.h and C++ implementation files a.cpp and b.cpp.
Both a.cpp and b.cpp include c.h. 
The .cpp files are compiled into object files and the executable app.exe is linked from these object files by the following build script:
```
  cc a.cpp -out a.obj
  cc b.cpp -out b.obj
  link.exe a.obj b.obj -out app.exe
```
This build script is represented by the following DAG: 

```
++-----------++-----------------------------------------++-------------------++
||   Inputs  ||                 Build task              ||      Outputs      ||
++-----------++-----------------------------------------++-------------------++
||           ||                                         ||                   ||
||           ||                                         ||                   ||
||      +----||-----> link a.obj b.obj -out app.exe ----||----> app.exe      ||
||      |    ||                                         ||                   ||
||      +----||-----------------------------------------||--------------+    ||
||           ||                                         ||              |    ||
|| a.cpp ----||-----> cc a.cpp -out a.obj --------------||----> a.obj --+    ||
||           ||     |                                   ||              |    ||
|| c. h -----||----->                                   ||              |    ||
||           ||     |                                   ||              |    ||
|| b.cpp ----||-----> cc b.cpp -out b.obj --------------||----> b.obj --+    ||
||           ||                                         ||                   ||
||           ||                                         ||                   ||
++-----------++-----------------------------------------++-------------------++
```

YAM executes the build tasks defined by this DAG as follows:
- compilations will be done before linking (as dictated by DAG topology)
- a.obj is re-compiled upon a change in a.cpp and/or c.h and/or in the compile 
  command and/or when the user tampered with a.obj (e.g. deleted it).
- b.obj is re-compiled upon a change in b.cpp and/or c.h and/or in the compile 
  command and/or when the user tampered with b.obj (e.g. deleted it).
- app.exe is re-linked upon a change in a.obj and/or b.obj and/or in the link
  command and/or when the user tampered with app.exe (e.g. deleted it).

# Features

YAM is heavily influenced by Mike Shal's paper [Build System Rules and 
Algorithms](https://gittup.org/tup/build_system_rules_and_algorithms.pdf) 
and by the [tup build system](https://gittup.org/tup/)
The paper discusses typical problems in build systems and defines rules
and algorithms that solve these problems. Mike Shal implemented these
rules and algorithms in tup. And so does YAM, with additions and improvements
that intend to make it an even better build system than tup.
The following sections describe how YAM adheres to Mike Shal's Rules and 
Algorithms.

## Correctness - Incremental build

YAM only re-executes build tasks when necessary. This section explains how YAM 
implements this.  

YAM detects the in/output files of a build task as it gets executed. The name,
last-write-time and version (a hash of the file content) of each of these files
is stored in the DAG. When YAM detects that the last-write-time of a file has 
changed it re-computes the version of this file. When the version has changed
it re-executes all build tasks that depend on the changed file.

Note: many build systems, re-execute build tasks when a file last-write-time
has changed. In YAM editing a file, saving it, undoing all edits and then
running YAM will not cause re-execution of build tasks because, although the
file last-write-time has changed, the file version has not.

Note: the detection of input files by YAM implies that the user need not 
specify the C++ source files as input of the compile commands in the C++ 
example in section [DAG example](#dag-example). YAM will detect those input files 
and add them to the DAG. YAM also detects that a.obj and b.obj are outputs of the
respective compilation tasks and that a.obj and b.obj are inputs of the link task.
There is however no way for YAM to find out that the compilation tasks must be
executed before the link task. Best case the link fails because the object
files were not yet produced, worst case the link task links out-dated object
files. YAM therefore requires the user to specify the outputs of build tasks 
and for tasks that read those outputs to specify those output files as their inputs.

Note: YAM incremental build is entirely based on changes in input files, output files
and build task logic. YAM will not re-execute tasks after changes in environment 
variables, registry settings, etc. 

## Correctness - DAG enforcement

YAM uses the detected in/outputs to verify that the in/outputs in the DAG, as
declared by the user, match the detected in/outputs. YAM fails the build when 
detected in/inputs do not match declared in/outputs.  
Rationale:
- DAG enforcement ensures proper build order 
- DAG enforcement contributes to build reproducability

Note: inputs that are source files need not be declared by the user. YAM 
detects such inputs as the build task executes and adds them to the DAG.  

## Correctness - Reproducability

Some build tools (e.g. Buck, Bazel) implement reproducability by requiring the
user to specify all source file inputs of a build task. YAM believes that this
puts too large a maintenance burden on the user. E.g. imagine having to declare
(and maintain) the list of all files included (recursively) by a C++ file. 
Instead YAM detects which source files are read by a build task and automatically 
adds them to the DAG.

The info in the DAG can be used in several ways to facilitate reproduction
of a build: 
    - Copy all input files used by the build into a sandbox. The build can now
      reliably be reproduced by running it in the sandbox.
    - By comparing build states of builds A and B one can verify whether A and
      B are equal (i.e. use same build task logic, use same input versions and 
      produce same output versions).


## Scalability - Build avoidance by using file aspect versions

Definition: a file aspect version is a hash of an aspect (subset) of the file.
Examples:
    - The 'code aspect version' is a hash of the code sections in a source code
      file, excluding comment sections from the hash computation.
    - The 'comment aspect version' is a hash of the comment sections in a
      source code file.

The file version as defined in section 3.1 is shorthand for the 'entire file 
aspect version', a hash of all content of the file.

YAM by default configures a build task to depend on input file versions, i.e.
to re-execute when input file versions change.
YAM allows the build task to be re-configured to depend on changes in selected 
input file aspect versions, i.e. to re-execute only when the selected input
file aspect versions change.

### Example - compilation
Re-compiling a C++ file that only has changes in comment sections will produce 
the same object file. In YAM the compilation task can be configured to depend 
on the code aspect versions of its .cpp and .h input files, i.e. to re-compile
only when the code aspect version of one or more input files has changed. 
Rationale: you have no more excuses to refrain yourself from improving the
comments in a C++ include file because of the massive amount of re-compilations
that would otherwise be triggered by such a change.

### Example - linking of dynamic load libraries
In the Windows operating system a dynamic load library, e.g. C.dll, has a
so-called import library C.lib. Executables and dlls that want to load C.dll at
run-time must be statically linked with C.lib. Relinking of exes and dlls that
depend on C.lib is only necessary when the exports section (i.e. the interface
of the dll) of C.lib changes.
In YAM this can be achieved by configuring the link task to depend on the 
exports aspect version of its .lib input files. The exports aspect version is a
hash of only the exports section of the .lib file.
Rationale: when using plain file versions changing implementation and/or 
interface of a dll will recursively re-link all dlls and exes that depend on
the changed dll. In large software systems the dll dependency graph is often 
very wide and/or deep, thus causing massive re-linking.
Using the exports aspect version, instead of the plain file version, breaks the
recursive re-linking:
    - when only dll implementation aspects were changed: only the changed dll  
      is re-linked.
    - when also dll interface aspects were changed: only the changed dll and
      the dlls and exes that directly depend on it will be re-linked.

## Scalability - Build avoidance by using a build cache

Builds can be configured to store their outputs in a build cache and to
re-use previously stored outputs when possible, i.e. when build task logic and
all input file (aspect) versions are identical 

## Scalability - Beta build

Many build systems are alpha-build systems. An alpha build retrieves the 
last-write-times of all files involved in the the build to figure out which
files have changed since the previous build. The time complexity of an alpha 
build is therefore O(#AllFiles). A beta build only retrieves the last-write-
times of files that have been write-accessed since the previous build. The time
complexity of a beta build is therefore O(#ModifiedFiles). Beta build enables 
YAM to quickly start the build tasks that need to re-execute. Start time will 
be sub-second in the typical development scenario where only a few files were
modified since the previous build, also in a repository that contains a huge 
number of files.

Note: YAM uses a server that continuously monitors the file system to register
which files were write-accessed since previous build. The server also 
re-computes the versions of the write-accessed files in-between builds as to 
not delay build task execution when the user starts the next build.

Note: the first build after restart of the YAM server is an alpha build.

## Scalability - Parallellized execution

By default YAM parallellizes build execution by queuing independent build tasks
to a thread pool. The pool contains 1 thread per available processing core.
The user can adjust the number of cores used by YAM.  

YAM has gone to great length to minimize the amount of non-parallelizable code.
As shown by [Amdahl's law](https://en.wikipedia.org/wiki/Amdahl%27s_law)
serial code is detrimental for scalability.
YAM claims a speedup of ~800 when using 1000 cores to execute build tasks that 
each take 1 second, assuming tasks are independent (i.e. can run in parallel),
the build runs on a system with infinite large I/O bandwidth and zero overhead. 
E.g. a build that executes 1000 compilations that each take 1 second will take
1000/800 = 1,25* seconds. 
Note: scalability de/increases as build tasks execution time de/increases.

Note: YAM does not support out-of-the-box distributed execution. YAM can be 
integrated relatively easy with distributed execution tools like 
[Incredibuild](https://www.incredibuild.com/).


## Usability - One command

Running YAM is the only thing you need to (and can) do to build the system.
YAM takes care of:
    - re-executing build tasks that depend on the changed files/directories.
    - re-executing tasks after changes in their task logic.
    - deletion of stale output files. An output file becomes stale when the 
      build task that produced the file is changed to no longer produce this
      output.
    - detection of build order issues, as explained in section 3.2.

## Usability - Build files and build language

Build files contain the user-defined DAG. Build files are to be treated as
source files and should be version controlled. YAM parses the build files to 
construct the in-memory representation of the DAG. YAM (re-)parses only the build 
files that changed since the previous build.

The build file language has not yet been defined. Ideally YAM supports various main-stream build file languages in order to make it easy to migrate to YAM.

## Usability - Unbiased towards languages, tool sets and more

YAM makes no assumptions about the kind of build problem that the user tries
to tackle. In particular it makes no assumptions about used programming languages,
compilers, linkers, package managers, test frameworks, target hardwares, target
operating systems.
