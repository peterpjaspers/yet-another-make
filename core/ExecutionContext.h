#pragma once

#include "NodeSet.h"
#include "Dispatcher.h"
#include "Thread.h"
#include "ThreadPool.h"
#include "ExecutionStatistics.h"

namespace YAM
{
	class __declspec(dllexport) ExecutionContext
	{
	public:
		ExecutionContext();
		~ExecutionContext();

		ThreadPool& threadPool();
		Dispatcher& threadPoolQueue();
		Dispatcher& mainThreadQueue();

		ExecutionStatistics& statistics();

		NodeSet& nodes();

	private:
		Dispatcher _mainThreadQueue;
		Dispatcher _threadPoolQueue;
		Thread _mainThread;
		ThreadPool _threadPool;
		ExecutionStatistics _statistics;
		NodeSet _nodes;
		
		// TODO: 
		// LogBook _logBook;
		// BuildOptions _options;

	};
}
