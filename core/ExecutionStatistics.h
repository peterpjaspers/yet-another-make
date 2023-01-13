#pragma once

#include <unordered_set>
#include <mutex>
#include <atomic>

namespace YAM
{
	class Node;
	class FileNode;
	class DirectoryNode;

	class __declspec(dllexport) ExecutionStatistics
	{
	public:
		ExecutionStatistics();

		void reset();

		void registerStarted(Node * node);
		void registerSelfExecuted(Node* node);

		void registerUpdatedFile(FileNode* node);
		void registerUpdatedDirectory(DirectoryNode* node);

		// number of nodes started
		unsigned int nStarted;
		// number of nodes self-executed
		unsigned int nSelfExecuted;

		// true => fill sets, else only increment counters
		std::atomic<bool> registerNodes;

		std::unordered_set<Node*> started;
		std::unordered_set<Node*> selfExecuted;

		// These two fields are incremented from threadpool,
		// hence use atomics.
		std::atomic<unsigned int> nFileUpdates;
		std::atomic<unsigned int> nDirectoryUpdates;

		std::mutex mutex;
		// These 2 fields are updated from threadpool.
		// Use mutex to do MT-safe update.
		std::unordered_set<FileNode*> updatedFiles;
		std::unordered_set<DirectoryNode*> updatedDirectories;
	};
}
