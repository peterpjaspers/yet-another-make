#pragma once
#include "Node.h"
#include "../xxHash/xxhash.h"

#include <chrono>
#include <vector>
#include <map>

namespace YAM
{
    class ExecutionContext;
    class FileNode;

	// A DirectoryNode keeps track of the source files in a directory:
	//    - creates a SourceFileNode for each file in the directory that
    //      is not excluded by the FileRepository that contains that
    //      file.
	//	  - creates a DirectoryNode for each subdir in the directory that
    //      is not excluded by the FileRepository that contains that
    //      subdir.
	//    - maintains the directory hash. This hash is computed from the
    //      names of the files and subdirs in the directory.
	// FileRepositories are registered in the execution context of the node.
    //
	class __declspec(dllexport) DirectoryNode : public Node
	{
	public:
		DirectoryNode(ExecutionContext* context, std::filesystem::path const& dirName);

        // Inherited via Node
        virtual bool supportsPrerequisites() const override;
        virtual void getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const override;

        virtual bool supportsPostrequisites() const override;
        virtual void getPostrequisites(std::vector<std::shared_ptr<Node>>& postrequisites) const override;

        virtual bool supportsOutputs() const override;
        virtual void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;

        virtual bool supportsInputs() const override;
        virtual void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;
        // End of Inherited via Node

        // Query the directory content, vector content is sorted by node name.
        void getFiles(std::vector<std::shared_ptr<FileNode>>& filesInDir);
        void getSubDirs(std::vector<std::shared_ptr<DirectoryNode>>& subDirsInDir);

        std::map<std::filesystem::path, std::shared_ptr<Node>> const& getContent(); // file + dir nodes

        std::chrono::time_point<std::chrono::utc_clock> const& lastWriteTime();

        // Pre: state() == State::Ok
        // Return the directory hash.
        XXH64_hash_t getHash() const;

        // Recursively remove the directory node from context->nodes().
        // Intended to be used when the repo is no longer to be mirrored
        // by YAM.
        void clear();

        void execute();

    protected:
        // Inherited via Node
        virtual bool pendingStartSelf() const override;
        virtual void startSelf() override;

    private:
        std::shared_ptr<Node> createNode(std::filesystem::directory_entry const& dirEntry);
        bool updateLastWriteTime();
        void updateContent();
        void updateHash();
        void _removeChildRecursively(std::shared_ptr<Node> child);

        // last seen modification time of the directory
        std::chrono::time_point<std::chrono::utc_clock> _lastWriteTime;
        XXH64_hash_t _hash;

        std::map<std::filesystem::path, std::shared_ptr<Node>> _content;
	};
}


