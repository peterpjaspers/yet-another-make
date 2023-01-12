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

	// A DirectoryNode keeps track of the content of a directory:
	//    - creates a FileNode for each file in the directory
	//	  - creates a DirectoryNode for each subdir in the directory
	//    - maintains the directory hash. This hash is computed from the
    //      names of the files and subdirs in the directory.
	//
	class __declspec(dllexport) DirectoryNode : public Node
	{
	public:
		DirectoryNode(ExecutionContext* context, std::filesystem::path const& dirName);

        // Inherited via Node
        virtual bool supportsPrerequisites() const override;
        virtual void getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const override;

        virtual bool supportsOutputs() const override;
        virtual void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;

        virtual bool supportsInputs() const override;
        virtual void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;
        // End of Inherited via Node

        // Query the directory content, vector content is sorted by node name.
        void getFiles(std::vector<std::shared_ptr<FileNode>>& filesInDir);
        void getSubDirs(std::vector<std::shared_ptr<DirectoryNode>>& subDirsInDir);

        std::map<std::filesystem::path, std::shared_ptr<Node>> const& getContent(); // file + dir nodes

        // Pre: state() == State::Ok
        // Return the directory hash.
        XXH64_hash_t getHash() const;

    protected:
        // Inherited via Node
        virtual bool pendingStartSelf() const override;
        virtual void startSelf() override;

    private:
        std::shared_ptr<Node> createNode(std::filesystem::directory_entry const& dirEntry);
        bool updateLastWriteTime();
        void updateContent();
        void updateHash();
        void execute();

        // last seen modification time of the directory
        std::chrono::time_point<std::chrono::utc_clock> _lastWriteTime;
        XXH64_hash_t _hash;

        std::map<std::filesystem::path, std::shared_ptr<Node>> _content;
	};
}


