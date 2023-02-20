#include "SourceFileRepository.h"
#include "SourceDirectoryNode.h"
#include "ExecutionContext.h"

namespace YAM
{
	SourceFileRepository::SourceFileRepository(
		std::string const& repoName,
		std::filesystem::path const& directory,
		RegexSet const& excludes,
		ExecutionContext* context)
		: WatchedFileRepository(repoName, directory, context)
		, _excludePatterns(excludes)
		, _directoryNode(std::make_shared<SourceDirectoryNode>(context, directory))
	{
		_context->nodes().add(_directoryNode);
	}

	std::shared_ptr<SourceDirectoryNode> SourceFileRepository::directoryNode() const {
		return _directoryNode; 
	}

	RegexSet const& SourceFileRepository::excludePatterns() const {
		return _excludePatterns;
	}

	void SourceFileRepository::clear() {
		_context->nodes().remove(_directoryNode);
		_directoryNode->clear();
		_directoryNode->setState(Node::State::Dirty);
	}
}