#pragma once

#include "NodeSet.h"
#include "Dispatcher.h"
#include "Thread.h"
#include "ThreadPool.h"
#include "FileAspectSet.h"
#include "ExecutionStatistics.h"
#include "ILogBook.h"

#include <memory>

namespace YAM
{
	class FileRepository;
	class BuildRequest;

	class __declspec(dllexport) ExecutionContext
	{
	public:
		ExecutionContext(std::shared_ptr<ILogBook> logBook = nullptr);
		~ExecutionContext();

		ThreadPool& threadPool();
		Thread& mainThread();
		Dispatcher& threadPoolQueue();
		Dispatcher& mainThreadQueue();

		ExecutionStatistics& statistics();

	    // Add repository, return whether it was added, i..e had a unique name.
		bool addRepository(std::shared_ptr<FileRepository> repo);
		// Remove repository, return whether it was removed.
		bool removeRepository(std::string const& repoName);

		// Find repository by name, return found repo, nullptr when not found.
		std::shared_ptr<FileRepository> findRepository(std::string const& repoName) const;

		// Find repository that contains path, return found repo, nullptr when not found.
		std::shared_ptr<FileRepository> findRepositoryContaining(std::filesystem::path const& path) const;

		// Return repositories.
		std::map<std::string, std::shared_ptr<FileRepository>> const& repositories() const;

		// Return the file aspects applicable to the file with the given path name.
		std::vector<FileAspect> findFileAspects(std::filesystem::path const& path) const;

		// Return the file aspect set identified by the given name.
		FileAspectSet const& findFileAspectSet(std::string const& aspectSetName) const;

		NodeSet & nodes();
		// Return the nodes that are in state Node::State::Dirty
		void getDirtyNodes(std::vector<std::shared_ptr<Node>>& dirtyNodes);

		std::shared_ptr<ILogBook> logBook() const;
		void addToLogBook(LogRecord const& record);

		void buildRequest(std::shared_ptr<BuildRequest> request);
		std::shared_ptr<BuildRequest> buildRequest() const;

	private:
		Dispatcher _mainThreadQueue;
		Dispatcher _threadPoolQueue;
		Thread _mainThread;
		ThreadPool _threadPool;
		ExecutionStatistics _statistics;

		std::map<std::string, std::shared_ptr<FileRepository>> _repositories;

		//TODO: add interfaces to add/remove aspects and aspectSets
		std::map<std::string, FileAspect> _fileAspects;
		std::map<std::string, FileAspectSet> _fileAspectSets;

		NodeSet _nodes;
		
		std::shared_ptr<ILogBook> _logBook;
		std::shared_ptr<BuildRequest> _request;

	};
}
