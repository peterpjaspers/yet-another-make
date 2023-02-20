#include "WatchedFileRepository.h"
#include "SourceDirectoryNode.h"
#include "FileNode.h"
#include "DirectoryWatcher.h"
#include "SourceFileNode.h"
#include "ExecutionContext.h"

namespace
{
	using namespace YAM;

	const std::filesystem::path overflowPath("overflow");

	bool isDirNode(Node* node) {
		return dynamic_cast<SourceDirectoryNode*>(node) != nullptr;
	}

	bool isFileNode(Node* node) {
		return dynamic_cast<FileNode*>(node) != nullptr;
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
	WatchedFileRepository::WatchedFileRepository(
		std::string const& repoName,
		std::filesystem::path const& directory,
		ExecutionContext* context)
		: FileRepository(repoName, directory)
		, _context(context)
		, _watcher(std::make_shared<DirectoryWatcher>(
			directory,
			true,
			Delegate<void, FileChange const&>::CreateLambda(
				[&](FileChange const& change)
				{
					FileChange c(change);
					// Access to nodes is only allowed in main thread context.
					// Therefore post change handling to main thread.
					_context->mainThreadQueue().push(
		            Delegate<void>::CreateLambda([c, this]() { _addChange(c); }));
				})
			)
		) {}

	void WatchedFileRepository::_addChange(FileChange change) {
		if (change.action == FileChange::Action::Overflow) {
			_changes.clear();
			_changes.insert({ overflowPath, change });
		} else if (!_changes.contains(overflowPath)) {
			std::filesystem::path absPath(directory() / change.fileName);
			auto it = _changes.find(absPath);
			if (it == _changes.end()) {
				_changes.insert({ absPath, change });
			} else {
				it->second = change;
			}
		}
	}

	void WatchedFileRepository::consumeChanges() {
		for (auto const& pair : _changes) {
			_handleChange(pair.second);
		}
		_changes.clear();
	}

	bool WatchedFileRepository::hasChanged(std::filesystem::path const& path) const {
		bool contains = _changes.contains(path) || _changes.contains(overflowPath);
		return contains;
	}

	void WatchedFileRepository::_handleChange(FileChange const& change) {
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

	void WatchedFileRepository::_handleAdd(FileChange const& change) {
		std::filesystem::path dirOrFile(directory() / change.fileName);
		std::filesystem::path parentDir = dirOrFile.parent_path();
		// take care: cannot use change.lastWriteTime because it applies to
		// change.fileName, not to parentDir.
		std::shared_ptr<Node> node = _invalidateNode(parentDir, std::chrono::utc_clock::now());
		if (node != nullptr && dynamic_cast<SourceDirectoryNode*>(node.get()) == nullptr) {
			throw std::exception("unexpected node type");
		}
	}

	void WatchedFileRepository::_handleRemove(FileChange const& change) {
		std::filesystem::path dirOrFile(directory() / change.fileName);
		std::filesystem::path parentDir = dirOrFile.parent_path();
		// take care: cannot use change.lastWriteTime because it applies to
		// change.fileName, not to parentDir.
		std::shared_ptr<Node> node = _invalidateNode(parentDir, std::chrono::utc_clock::now());
		if (node != nullptr && dynamic_cast<SourceDirectoryNode*>(node.get()) == nullptr) {
			throw std::exception("unexpected node type");
		}
		_invalidateNodeRecursively(dirOrFile);
	}

	void WatchedFileRepository::_handleModification(FileChange const& change) {
		std::filesystem::path dirOrFile(directory() / change.fileName);
		_invalidateNode(dirOrFile, change.lastWriteTime);
	}

	void WatchedFileRepository::_handleRename(FileChange const& change) {
		FileChange remove = change;
		remove.fileName = change.oldFileName;
		_handleRemove(remove);
		_handleAdd(change);
	}

	void WatchedFileRepository::_handleOverflow() {
		std::vector<std::shared_ptr<Node>> nodesInRepo;
		std::filesystem::path const& repoDir = directory();
		auto includeNode = Delegate<bool, std::shared_ptr<Node> const&>::CreateLambda(
			[&repoDir](std::shared_ptr<Node> const& node) {
				return isNodeInRepo(node.get(), repoDir);
			});
		_context->nodes().find(includeNode, nodesInRepo);
		for (auto node : nodesInRepo) {
			node->setState(Node::State::Dirty);
		}
	}

	std::shared_ptr<Node> WatchedFileRepository::_invalidateNode(
		std::filesystem::path const& path,
		std::chrono::time_point<std::chrono::utc_clock> const& lastWriteTime
	) {
		std::shared_ptr<Node> node = _context->nodes().find(path);
		if (node != nullptr) {
			bool dirty = true;
			auto fileNode = dynamic_pointer_cast<FileNode>(node);
			if (fileNode != nullptr) {
				dirty = fileNode->lastWriteTime() != lastWriteTime;
			} else {
				auto dirNode = dynamic_pointer_cast<SourceDirectoryNode>(node);
				if (dirNode != nullptr) {
					dirty = dirNode->lastWriteTime() != lastWriteTime;
				}
			}
			if (dirty) {
				node->setState(Node::State::Dirty);
			}
		}
		return node;
	}

	void WatchedFileRepository::_invalidateNodeRecursively(std::filesystem::path const& path) {
		std::shared_ptr<Node> node = _context->nodes().find(path);
		if (node != nullptr) _invalidateNodeRecursively(node);
	}

	void WatchedFileRepository::_invalidateNodeRecursively(std::shared_ptr<Node> const& node) {
		node->setState(Node::State::Dirty);
		auto dirNode = dynamic_pointer_cast<SourceDirectoryNode>(node);
		if (dirNode != nullptr) {
			for (auto const& pair : dirNode->getContent()) {
				_invalidateNodeRecursively(pair.second);
			}
		}
	}
}