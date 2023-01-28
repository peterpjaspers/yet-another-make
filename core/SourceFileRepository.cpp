#include "SourceFileRepository.h"
#include "DirectoryNode.h"
#include "SourceFileNode.h"
#include "ExecutionContext.h"

namespace
{
	using namespace YAM;

	const std::size_t queueCapacity = 32*1024;

	bool isDirNode(Node* node) {
		return dynamic_cast<DirectoryNode*>(node) != nullptr;
	}

	bool isFileNode(Node* node) {
		return dynamic_cast<FileNode*>(node) != nullptr;
	}

	bool startsWith(std::string const& str, std::string const& startStr) {
		return str.rfind(startStr) == 0;
	}

	bool isSubpath(std::filesystem::path const& path, std::filesystem::path const& base) {
		const auto pair = std::mismatch(path.begin(), path.end(), base.begin(), base.end());
		return pair.second == base.end();
	}

	bool isNodeInRepo(Node* node, std::filesystem::path const& repoDir) {
		return
			(isFileNode(node) || isDirNode(node))
			&& isSubpath(node->name(), repoDir);
	}
}

namespace YAM
{
	SourceFileRepository::SourceFileRepository(
		std::string const& repoName,
		std::filesystem::path const& directory,
		RegexSet const& excludePatterns,
		ExecutionContext* context)
		: _name(repoName)
		, _directory(std::make_shared<DirectoryNode>(context, directory))
		, _context(context)
		, _excludes(excludePatterns)
		, _suspended(false)
		, _watcher(
			directory,
			true,
			Delegate<void, FileChange const&>::CreateLambda(
				[&](FileChange const& change)
				{
					FileChange c(change);
					// Access to nodes is only allowed in main thread context.
					// Therefore post change handling to main thread.
	                _context->mainThreadQueue().push(
			        Delegate<void>::CreateLambda([c, this]() { _enqueueChange(c); }));

				})
			)
	{
		_context->nodes().add(_directory);
	}

	std::string const& SourceFileRepository::name() const { return _name; }
	std::filesystem::path const& SourceFileRepository::directoryName() const { return _directory->name(); }
	std::shared_ptr<DirectoryNode> SourceFileRepository::directory() const { return _directory; }
	RegexSet const& SourceFileRepository::excludes() const { return _excludes; }

	void SourceFileRepository::excludes(RegexSet const& newExcludes) {
		_excludes = newExcludes;
		_invalidateNodeRecursively(_directory);
	}

	void SourceFileRepository::suspend() { _suspended = true; }
	void SourceFileRepository::resume() {
		if (_suspended) {
			_suspended = false;
			_processChangeQueue();
		}
	}

	void SourceFileRepository::_enqueueChange(FileChange change) {
		if (_changeQueue.size() > queueCapacity) {
			while (_changeQueue.size() > 0) _changeQueue.pop();
			change.action = FileChange::Action::Overflow;
		}
		_changeQueue.push(change);
		if (!_suspended) _processChangeQueue();
	}

	void SourceFileRepository::_processChangeQueue() {
		while (!_changeQueue.empty()) {
			FileChange const& change = _changeQueue.front();
			_handleChange(change);
			_changeQueue.pop();
		}
	}

	void SourceFileRepository::_handleChange(FileChange const& change) {
		if (change.action == FileChange::Action::Added) {
			_handleAdd(change);
		} else if (change.action == FileChange::Action::Removed) {
			_handleRemove(change);
		} else if (change.action == FileChange::Action::Modified) {
			_handleModification(change);
		} else if (change.action == FileChange::Action::Renamed) {
			_handleRename(change);
		} else if (change.action == FileChange::Action::Overflow) {
			_handleOverflow();
		} else {
			throw std::exception("bad action");
		}
	}

	void SourceFileRepository::_handleAdd(FileChange const& change) {
		std::filesystem::path dirOrFile(directory()->name() / change.fileName);
		std::filesystem::path parentDir = dirOrFile.parent_path();
		std::shared_ptr<Node> node = _invalidateNode(parentDir);
		if (node != nullptr && dynamic_cast<DirectoryNode*>(node.get()) == nullptr) {
			throw std::exception("unexpected node type");
		}
	}

	void SourceFileRepository::_handleRemove(FileChange const& change) {
		std::filesystem::path dirOrFile(directory()->name() / change.fileName);
		_invalidateNodeRecursively(dirOrFile);
	}

	void SourceFileRepository::_handleModification(FileChange const& change) {
		std::filesystem::path dirOrFile(directory()->name() / change.fileName);
		_invalidateNode(dirOrFile);
	}

	void SourceFileRepository::_handleRename(FileChange const& change) {
		FileChange remove = change;
		remove.fileName = change.oldFileName;
		_handleRemove(remove);
		_handleAdd(change);
	}

	void SourceFileRepository::_handleOverflow() {
		std::filesystem::path const& repoDir = directoryName();
		for (auto pair : _context->nodes().nodes()) {
			std::shared_ptr<Node> node = pair.second;
			if (isNodeInRepo(node.get(), repoDir)) {
				node->setState(Node::State::Dirty);
			}
		}
	}

	std::shared_ptr<Node> SourceFileRepository::_invalidateNode(std::filesystem::path const& path) {
		std::shared_ptr<Node> node = _context->nodes().find(path);
		if (node == nullptr) {
			// happens when path was excluded from repo or when
			// _directory has not yet been executed
			;
		} else {
			node->setState(Node::State::Dirty);
		}
		return node;
	}

	void SourceFileRepository::clear() {
		_context->nodes().remove(_directory);
		_directory->clear();
		_directory->setState(Node::State::Dirty);
	}

	void SourceFileRepository::_invalidateNodeRecursively(std::filesystem::path const& path) {
		std::shared_ptr<Node> node = _context->nodes().find(path);
		if (node != nullptr) _invalidateNodeRecursively(node);
	}

	void SourceFileRepository::_invalidateNodeRecursively(std::shared_ptr<Node> node) {
		node->setState(Node::State::Dirty);
		auto dirNode = dynamic_pointer_cast<DirectoryNode>(node);
		if (dirNode != nullptr) {
			for (auto const& pair : dirNode->getContent()) {
				_invalidateNodeRecursively(pair.second);
			}
		}
	}

	bool SourceFileRepository::contains(std::filesystem::path const& path) const {
		bool const contains = path != path.lexically_proximate(directoryName());
		return contains;
	}
}