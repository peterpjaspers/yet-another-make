#pragma once

#include <unordered_set>

namespace YAM
{
	class Node;

	class __declspec(dllexport) ExecutionStatistics
	{
	public:
		ExecutionStatistics();

		void reset();
		void registerStarted(Node * node);
		void registerSelfExecuted(Node * node);

		// number of nodes started
		unsigned int nStarted;
		// number of nodes self-executed
		unsigned int nSelfExecuted;

		// true => fill sets, else only increment counters
		bool registerNodes;

		std::unordered_set<Node*> started;
		std::unordered_set<Node*> selfExecuted;
	};
}
