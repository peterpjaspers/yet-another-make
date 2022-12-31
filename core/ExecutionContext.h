#pragma once

#include "NodeSet.h"
#include "Dispatcher.h"
#include "Thread.h"
#include "ThreadPool.h"
#include "FileAspectSet.h"
#include "ExecutionStatistics.h"

namespace YAM
{
	class __declspec(dllexport) ExecutionContext
	{
	public:
		ExecutionContext();
		~ExecutionContext();

		ThreadPool& threadPool();
		Thread& mainThread();
		Dispatcher& threadPoolQueue();
		Dispatcher& mainThreadQueue();

		ExecutionStatistics& statistics();

		// Return the file aspects applicable to the file with the given path name.
		std::vector<FileAspect> findFileAspects(std::filesystem::path const& path) const;

		// Return the file aspect set identified by the given name.
		FileAspectSet const& findFileAspectSet(std::string const& aspectSetName) const;

		NodeSet& nodes();

	private:
		Dispatcher _mainThreadQueue;
		Dispatcher _threadPoolQueue;
		Thread _mainThread;
		ThreadPool _threadPool;
		ExecutionStatistics _statistics;

		//TODO: add interfaces to add/remove aspects and aspectSets
		std::map<std::string, FileAspect> _fileAspects;
		std::map<std::string, FileAspectSet> _fileAspectSets;

		NodeSet _nodes;
		
		// TODO: 
		// LogBook _logBook;
		// BuildOptions _options;

	};
}
