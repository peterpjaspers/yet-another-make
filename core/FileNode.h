#pragma once

#include "Node.h"
#include "FileAspect.h"
#include "../xxHash/xxhash.h"

#include <map>

namespace YAM
{
    // A file node computes aspect hashes of its associated file. The computed
    // aspect hashes and the file's last-write-time are cached in member fields
    // of the node.
    // 
    // Aspects must be made known to the node with the addAspect(..) function.
    // Adding an aspect is a no-op when the aspect was already known, else the
    // aspect is added and its cached hash value is initialized to a random value.
    // 
    // Hashes of aspects known to the file node are (re-)computed and cached
    // during file node execution. As an optimization hashes are only (re-)
    // computed when the file's last-write-time has changed since previous
    // execution of the node. Hashing a non-existing file results in random
    // hash values.
    // Note: a file node execution with an empty set of known aspects will
    // only update the cached file's last-write-time.
    // 
    // The cached hash value of an aspect can be retrieved with the node's 
    // hashOf(..) function. An exception is thrown when retrieving the hash
    // of an aspect that is not known by the file node.
    // 
    // The cached hash value of an aspect can be re-computed with the node's
    // rehash(..) function. rehashAll() will re-compute all known hashes.
    // 
    // The intended use of addAspect, hashOf and rehash functions is described 
    // for the following scenarios (C and P are command nodes):
    //   1) Source file F is detected as input of C
    //   2) Output file F is produced by C
    //   3) Output file F, produced by command P, is detected as input of C
    //  
    // 1) Source file F is detected as input of C
    // C will act as follows:
    //     if file node F does not exist:
    //         - create file node F
    //           Note:creating file node for F must be done in main thread 
    //           context because graph is not thread-safe.
    //         - execute F (to retrieve last_write_time and compute aspect hashes
    //           and move file node to Ok state)
    //         - Note: multiple concurrent command executions may discover F as
    //           input. Take care to create and execute node F only once.
    //     if F.addAspect(aspectRelevantToC), i.e. aspect was not known:
    //         - call rehash(aspectRelevantToC) 
    //     add F to C's input files and prerequisites
    //     compute executionHash from the inputs, outputs and script hashes
    //     cache executionHash in member field of C
    // During next build: once C's prerequisites have been executed C's 
    // pendingStartSelf will compare the cached executionHash with the current
    // executionHash to detect whether re-execution of C is needed.
    // 
    // 2) Output file F is produced by C
    // Output file node F is created when C is created (note that at that time
    // file F does not yet exist). C adds the entireFile aspect to F and adds
    // F to its prerequisites in order to be able to detect that F has been 
    // tampered with.
    // Once C's prerequisites have been executed C's pendingStartSelf will use
    // F's hashOf(entireAspect) to compute C's executionHash and compare it with
    // C's previous executionHash to detect whether re-execution of C is needed.
    // Once C has been executed the aspect hashes of F are out-dated (because
    // C's execution has written new content to F). C calls rehashAll() to force 
    // F to re-compute all of its known aspect hashes.  
    // 
    // 3) Output file F, produced by P is detected, as input of C
    // On first detection of input F by C the relevant aspect needed by C may
    // not yet be known by F.
    // If relevant aspect was unknown by F:
    //      - C adds the relevant aspect to F
    //      - C calls F.rehash(aspectRelevantToC) to compute and cache the hash
    //      - C uses F.hashOf(aspectRelevantToC) to compute C's executionHash
    // Else:
    //      - C uses F.hashOf(aspectRelevantToC) to compute C's executionHash
    // 
    // Note: care must be taken that all file aspect hash computation is done 
    // in thread-pool context, either as part of the file node's execution or
    // or as part of the command node's execution.
    //
    // Note: a file node F maintains one last-write-time for all aspect hashes.
    // The last-write-time is only updated by the node execution and rehashAll.
    // In the same build rehash calls to F can be made after F's execution. 
    // In the interval between F's execution and rehash call the user may 
    // have modified the (source) file, resulting in the hashes to have been
    // computed from different file versions. This is not a problem because the
    // file node will re-compute all hashes during the next build because of 
    // the changed last-write-time.
    // 
    // Note: a user may edit a source file in the time interval between
    // completion of the command script that used that file as input and
    // the subsequent computation of the file's aspect hashes. In this
    // case the next build will not detect that the file has changed (because
    // last-write-time has not changed) and the command will not re-execute,
    // resulting in wrong content of the command's output files.
    // This problem can be fixed in several ways:
    //    1- Do not allow modification of source files during the build.
    //    2- Capture last-write-times and hash code of all source files before
    //       using them.
    //       Because input files are detected after the build this can only
    //       be implemented by creating file nodes for all files in the worktree
    //       before starting cmd node execution. Note that source files can
    //       easily be distinguished from generated files because yam requires
    //       all output (==generated) files to be specified in the build file.
    //    3- Detect which source files are modified during the build.
    //       At next build force the commands that used these files as input
    //       to re-execute.      
    // Option 1 is not user-friendly and not easy to implement.
    // Option 2 is easy to implement but may result in more file nodes than
    // actually used as inputs. It also simplifies handling input file
    // detection during cmd node execution because the file node already
    // exists at that time.
    // Option 3 may cause unnecessary re-builds in case the file was modified 
    // before it was used as input. Also different commands may use the same 
    // input at different times. This complicates the implementation.
    // 
    // Note: a user may tamper with an output file in the time interval between
    // the time of its update by the command script and the time of the 
    // retrieval/computation of its last-write-time and hash (which happens 
    // after command completion). In this case the next build will not detect
    // that the file has changed (because last-write-time has not changed) and
    // will not re-execute the command, resulting in wrong content of the 
    // output file. 
    // This problem can be fixed by detecting, during the build, which output 
    // files are modified by other actors than commands and, at the next build,
    // force the commands that produced these files to re-execute.
    // 
    class __declspec(dllexport) FileNode : public Node
    {
    public:
        FileNode(ExecutionContext* context, std::filesystem::path const& name);

        // Inherited via Node
        virtual bool supportsPrerequisites() const override;
        virtual void getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const override;

        virtual bool supportsOutputs() const override;
        virtual void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;

        virtual bool supportsInputs() const override;
        virtual void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;

        // Pre: state() == State::Ok
        // Return the cached hash of given aspect.
        // Throw exception when aspect is unknown.
        XXH64_hash_t hashOf(std::string const& aspectName);

        // Pre: state() == State::Ok
        // Retrieve and cache file's last-write-time, then compute and cache 
        // hashes of all applicable file aspects.
        void rehashAll();

    protected:
        // Inherited via Node
        virtual bool pendingStartSelf() const override;
        virtual void startSelf() override;

    private:
        bool updateLastWriteTime();
        void rehashAll(bool doUpdateLastWriteTime);
        void execute();

        // last seen file modification time
        std::chrono::time_point<std::chrono::utc_clock> _lastWriteTime;

        // aspect name => hash value. All hashes are computed after last
        // update of _lastWriteTime.
        std::map<std::string, XXH64_hash_t> _hashes;
    };
}

