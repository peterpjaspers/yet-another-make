#pragma once
#include "CommandNode.h"
#include "BuildFile.h"

#include "xxhash.h"

#include <memory>
#include <vector>

namespace YAM {
    class SourceFileNode;
    class DirectoryNode;
    class FileNode;
    class GlobNode;
    template<typename T> class AcyclicTrail;
    namespace BuildFile { class File; }
    class Parser;

    // Excuting a BuildFileParserNode:
    //    - If the buildfile is a text file: parse the text file.
    //    - If the buildfile is an executable file: run the build file and
    //      parse its output.
    // 
    // See BuildFileParser for details on syntax.
    class __declspec(dllexport) BuildFileParserNode : public Node
    {
    public:
        BuildFileParserNode(); // needed for deserialization
        BuildFileParserNode(ExecutionContext* context, std::filesystem::path const& name);

        // Set/get the buildfile. Either a .txt file with syntax as specified by
        // class BuildFileParser or an executable file that outputs text with
        // syntax as specified by class BuildFileParser.
        void buildFile(std::shared_ptr<SourceFileNode> const& newFile);
        std::shared_ptr<SourceFileNode> buildFile() const;

        std::shared_ptr<DirectoryNode> buildFileDirectory() const;

        // Overrides from Node class
        std::string className() const override { return "BuildFileParserNode"; }
        void start(PriorityClass prio) override;

        // Return the hash of the parseTree and of the globs in the parseTree's
        // buildfile dependency and cmd and order-only input sections.
        XXH64_hash_t executionHash() const;

        // The parse tree representing the buildfile content.
        BuildFile::File const& parseTree() const;

        // The buildfile dependencies declared in the buildfile represented by 
        // their parser nodes.
        std::vector<BuildFileParserNode const*> const& dependencies();

        // Walk the buildfile parser dependency graph. Return whether no cycle was 
        // detected in the dependency graph.
        bool walkDependencies(AcyclicTrail<BuildFileParserNode const*>& trail) const;

        std::shared_ptr<CommandNode> const& executor() const {
            return _executor;
        }

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamer (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    private: //functions
        XXH64_hash_t computeExecutionHash() const;
        std::shared_ptr<FileNode> fileToParse() const;
        bool composeDependencies();
        BuildFileParserNode* findDependency(
            std::filesystem::path const& depPath,
            bool requireBuildFile);

        void handleRequisitesCompletion(Node::State state);
        void postParseCompletion();
        void handleParseCompletion();
        void startGlobs();
        void handleGlobsCompletion(Node::State state);
        void notifyParseCompletion(Node::State newState);

    private: //fields
        std::shared_ptr<SourceFileNode> _buildFile;

        // nullptr if buildfile is a .txt file. Else the executor executes the
        // buildfile and stores its stdout in buildfile_yam.txt.
        std::shared_ptr<CommandNode> _executor;

        // If _executor == nullptr: the hash of the source buildfile
        // else: the hash of the generated buildfile
        // A change in this hash requires re-parsing of the buildfile.
        XXH64_hash_t _buildFileHash;

        // Parses buildfile_yam.txt, either the source file or the one generated
        // by _executor.
        std::shared_ptr<Parser> _parser;

        BuildFile::File _parseTree;

        // The nodes specified in the buildfile dependency  section of the
        // buildfile. A node can be a GlobNode, DirectoryNode or SourceFileNode.
        // A glob matches buildfiles and or directories. A directory dependency 
        // means a dependency on the buildfile in that directory.
        std::map<std::filesystem::path, std::shared_ptr<Node>> _buildFileDeps;

        // The parser nodes associated with the buildfiles specified by 
        // _buildFileDeps.
        std::vector<BuildFileParserNode const*> _dependencies;

        // A change in _executionHash means that the _parseTree and/or 
        // _dependencies list has changed.
        // A change in this hash requires re-compilation of the parse tree.
        XXH64_hash_t _executionHash;
    };
}

