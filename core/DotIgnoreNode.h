#pragma once
#include "FileNode.h"

#include <vector>
#include <memory>

namespace YAM
{
    class DirectoryNode;
    class SourceFileNode;
    class FileRepositoryNode;

    // NOT YET IMPLEMENTED
    // A DotIgnoreNode parses a .gitignore and/or .yamignore file in a
    // given directory. Both files adhere to the gitignore specification,
    // see https://git-scm.com/docs/gitignore. The ignore() member function 
    // applies the precedence rules as specified in same specification.
    // 
    // Special case: homeRepo/yamConfig/ is ignored.
    // Rationale: permanent SourceFileNodes are created for the files in
    // that directory by RepositoriesNode and FileExecSpecsNode (and possibly
    // more as new configuration files get introduced). These filenodes must
    // not be set to Node::State::Deleted when removed from the filesystem.  
    //
    class __declspec(dllexport) DotIgnoreNode : public Node
    {
    public:
        DotIgnoreNode(); // needed for deserialization
        DotIgnoreNode(
            ExecutionContext* context,
            std::filesystem::path const& name,
            DirectoryNode *directory);

        std::string className() const override { return "DotIgnoreNode"; }

        // Add the prerequisites (i.e, the .gitignore and .yamignore file nodes
        // to the execution context.
        void addPrerequisitesToContext();

        void setState(State newState) override;
        void start(PriorityClass prio) override;

        // return the hash of the ignore patterns. 
        XXH64_hash_t hash() const { return _hash; }

        // Return whether given path is not a source file or a source file that is
        // not allowed to be accessed by the build.
        bool ignore(std::shared_ptr<FileRepositoryNode> const& repo, std::filesystem::path const& path) const;

        // Remove the .gitignore and .yamignore nodes from context->nodes().
        void clear();

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    protected:

    private:
        friend class DirectoryNode;

        void directory(DirectoryNode* directory);
        XXH64_hash_t computeHash() const;
        void handleRequisiteCompletion(Node::State state); 
        void parseDotIgnoreFiles();


        DirectoryNode* _directory;

        // The input files, i.e. the .gitignore and/or .yamignore files
        std::vector<std::shared_ptr<SourceFileNode>> _dotIgnoreFiles;
        // TODO: the patterns retrieved from the input files.
         
        // The hash of the hashes of the _dotIgnoreFiles.
        XXH64_hash_t _hash;
    };
}

