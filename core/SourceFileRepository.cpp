#include "SourceFileRepository.h"
#include "SourceDirectoryNode.h"
#include "FileRepositoryWatcher.h"
#include "ExecutionContext.h"

namespace YAM
{
	SourceFileRepository::SourceFileRepository(
		std::string const& repoName,
		std::filesystem::path const& directory,
		RegexSet const& excludes,
		ExecutionContext* context)
		: FileRepository(repoName, directory)
		, _watcher(std::make_shared<FileRepositoryWatcher>(directory, context))
		, _excludePatterns(excludes)
		, _context(context)
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

	void SourceFileRepository::consumeChanges() {
		_watcher->consumeChanges();
	}

	bool SourceFileRepository::hasChanged(std::filesystem::path const& path) {
		return _watcher->hasChanged(path);
	}

	void SourceFileRepository::clear() {
		_context->nodes().remove(_directoryNode);
		_directoryNode->clear();
		_directoryNode->setState(Node::State::Dirty);
	}
}