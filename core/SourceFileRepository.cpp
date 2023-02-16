#include "SourceFileRepository.h"
#include "DirectoryNode.h"
#include "FileNode.h"
#include "DirectoryWatcher.h"
#include "SourceFileNode.h"
#include "ExecutionContext.h"

namespace
{
	using namespace YAM;

	const std::filesystem::path overflowPath("overflow");

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
		: _readOnly(false)
		, _name(repoName)
		, _directory(std::make_shared<DirectoryNode>(context, directory))
		, _context(context)
		, _excludes(excludePatterns)
		, _buildInProgress(false)
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
	) {
		_context->nodes().add(_directory);
	}

	SourceFileRepository::
		SourceFileRepository(
			std::string const& repoName,
			std::filesystem::path const& directory)
		: _readOnly(true)
		, _name(repoName)
		, _directory(nullptr)
		, _context(nullptr)
		, _excludes()
		, _buildInProgress(false)
		, _watcher(nullptr)
	{}

	bool SourceFileRepository::readOnly() const { return _readOnly; }
	std::string const& SourceFileRepository::name() const { return _name; }
	std::filesystem::path const& SourceFileRepository::directoryName() const { return _directory->name(); }
	std::shared_ptr<DirectoryNode> SourceFileRepository::directory() const { return _directory; }
	RegexSet const& SourceFileRepository::excludes() const { return _excludes; }

	void SourceFileRepository::excludes(RegexSet const& newExcludes) {
		if (!_readOnly) {
			_excludes = newExcludes;
			_invalidateNodeRecursively(_directory);
		}
	}

	void SourceFileRepository::_addChange(FileChange change) {
		if (change.action == FileChange::Action::Overflow) {
			_changes.clear();
			_changes.insert({ overflowPath, change });
		} else if (!_changes.contains(overflowPath)) {
			std::filesystem::path absPath(directory()->name() / change.fileName);
			auto it = _changes.find(absPath);
			if (it == _changes.end()) {
				if (_buildInProgress && _excludes.matches(absPath.string())) {
					// Ignore changes to generated files during a build
				} else {
					_changes.insert({ absPath, change });
				}
			} else {
				it->second = change;
			}
		}
	}

	void SourceFileRepository::buildInProgress(bool inProgress) {
		_buildInProgress = inProgress;
	}
		
	void SourceFileRepository::consumeChanges() {
		for (auto const& pair : _changes) {
			_handleChange(pair.second);
		}
		_changes.clear();
	}

	bool SourceFileRepository::hasChanged(std::filesystem::path const& path) const {
		return _changes.contains(path) || _changes.contains(overflowPath);
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
		// take care: cannot use change.lastWriteTime because it applies to
		// change.fileName, not to parentDir.
		std::shared_ptr<Node> node = _invalidateNode(parentDir, std::chrono::utc_clock::now());
		if (node != nullptr && dynamic_cast<DirectoryNode*>(node.get()) == nullptr) {
			throw std::exception("unexpected node type");
		}
	}

	void SourceFileRepository::_handleRemove(FileChange const& change) {
		std::filesystem::path dirOrFile(directory()->name() / change.fileName);
		std::filesystem::path parentDir = dirOrFile.parent_path();
		// take care: cannot use change.lastWriteTime because it applies to
		// change.fileName, not to parentDir.
		std::shared_ptr<Node> node = _invalidateNode(parentDir, std::chrono::utc_clock::now());
		if (node != nullptr && dynamic_cast<DirectoryNode*>(node.get()) == nullptr) {
			throw std::exception("unexpected node type");
		}
		_invalidateNodeRecursively(dirOrFile);
	}

	void SourceFileRepository::_handleModification(FileChange const& change) {
		std::filesystem::path dirOrFile(directory()->name() / change.fileName);
		_invalidateNode(dirOrFile, change.lastWriteTime);
	}

	void SourceFileRepository::_handleRename(FileChange const& change) {
		FileChange remove = change;
		remove.fileName = change.oldFileName;
		_handleRemove(remove);
		_handleAdd(change);
	}

	void SourceFileRepository::_handleOverflow() {
		std::vector<std::shared_ptr<Node>> nodesInRepo;
		std::filesystem::path const& repoDir = directoryName();
		auto includeNode = Delegate<bool, std::shared_ptr<Node> const&>::CreateLambda(
			[&repoDir](std::shared_ptr<Node> const& node) {
				return isNodeInRepo(node.get(), repoDir);
			});
		_context->nodes().find(includeNode, nodesInRepo);
		for (auto node : nodesInRepo) {
		    node->setState(Node::State::Dirty);
	    }
	}

	std::shared_ptr<Node> SourceFileRepository::_invalidateNode(
		std::filesystem::path const& path,
		std::chrono::time_point<std::chrono::utc_clock> const& lastWriteTime
	) {
		std::shared_ptr<Node> node = _context->nodes().find(path);
		if (node == nullptr) {
			// happens when path was excluded from repo or when
			// _directory has not yet been executed
			;
		} else {
			bool dirty = true;
			auto fileNode = dynamic_pointer_cast<FileNode>(node);
			if (fileNode != nullptr) {
				dirty = fileNode->lastWriteTime() != lastWriteTime;
			} else {
				auto dirNode = dynamic_pointer_cast<DirectoryNode>(node);
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

	void SourceFileRepository::clear() {
		if (!_readOnly) {
			_context->nodes().remove(_directory);
			_directory->clear();
			_directory->setState(Node::State::Dirty);
		}
	}

	void SourceFileRepository::_invalidateNodeRecursively(std::filesystem::path const& path) {
		std::shared_ptr<Node> node = _context->nodes().find(path);
		if (node != nullptr) _invalidateNodeRecursively(node);
	}

	void SourceFileRepository::_invalidateNodeRecursively(std::shared_ptr<Node> const& node) {
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