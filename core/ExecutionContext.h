#pragma once

#include "NodeSet.h"
#include "Dispatcher.h"
#include "Thread.h"
#include "ThreadPool.h"
#include "FileAspectHasher.h"
#include "InputFileAspects.h"
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

		FileAspectHasherSet& aspectHashers();
		InputFileAspectsSet & inputAspects();

		NodeSet& nodes();

	private:
		Dispatcher _mainThreadQueue;
		Dispatcher _threadPoolQueue;
		Thread _mainThread;
		ThreadPool _threadPool;
		ExecutionStatistics _statistics;
		FileAspectHasherSet _aspectHashers;
		InputFileAspectsSet _inputAspects;

		NodeSet _nodes;
		
		// TODO: 
		// LogBook _logBook;
		// BuildOptions _options;

	};
}
