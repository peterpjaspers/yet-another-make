#include "SourceFileRepository.h"
#include "DirectoryNode.h"
#include "ExecutionContext.h"

namespace YAM
{
	SourceFileRepository::SourceFileRepository(
		std::string const& repoName,
		std::filesystem::path const& directory,
		RegexSet const& excludes,
		ExecutionContext* context)
		: WatchedFileRepository(repoName, directory, excludes, context)
		, _directoryNode(std::make_shared<DirectoryNode>(context, directory))
	{
		_context->nodes().add(_directoryNode);
	}

	std::shared_ptr<DirectoryNode> SourceFileRepository::directoryNode() const {
		return _directoryNode; 
	}

	void SourceFileRepository::clear() {
		_context->nodes().remove(_directoryNode);
		_directoryNode->clear();
		// Mark Dirty to invalidate all nodes that use the directory node
		// prerequisite.
		_directoryNode->setState(Node::State::Dirty);
	}
}