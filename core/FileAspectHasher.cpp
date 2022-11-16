#include "FileAspectHasher.h"

namespace
{
	using namespace YAM;

	XXH64_hash_t HashFile(std::filesystem::path const& fileName) {
		return XXH64_file(fileName.string().c_str());
	}

	FileAspectHasher entireFileHasher(
		FileAspect(FileAspect::entireFileAspect().name(), RegexSet()),
		Delegate<XXH64_hash_t, std::filesystem::path const&>::CreateStatic(HashFile));
}

namespace YAM
{
	FileAspectHasher::FileAspectHasher(
			FileAspect const& aspect,
			Delegate<XXH64_hash_t, std::filesystem::path const&> const& hashFunction)
		: _aspect(aspect)
		, _hashFunction(hashFunction)
	{}

	FileAspect const& FileAspectHasher::aspect() const {
		return _aspect;
	}

	XXH64_hash_t FileAspectHasher::hash(std::filesystem::path const & fileName) const {
		return _hashFunction.Execute(fileName);
	}
}

namespace YAM
{
	std::mutex & FileAspectHasherSet::mutex() {
		return _mutex;
	}

	bool FileAspectHasherSet::add(FileAspectHasher const& newHasher) {
		bool mustAdd = !_hashers.contains(newHasher.aspect().name());
		if (mustAdd) {
			_hashers.insert({ newHasher.aspect().name() , newHasher });
		}
		return mustAdd;
	}

	bool FileAspectHasherSet::remove(std::string const& aspectName) {
		auto it = _hashers.find(aspectName);
		bool mustRemove = (it != _hashers.end());
		if (mustRemove) {
			_hashers.erase(it);
		}
		return mustRemove;
	}

	void FileAspectHasherSet::clear() {
		std::lock_guard<std::mutex> lock(_mutex);
		_hashers.clear();
	}

	FileAspectHasher const& FileAspectHasherSet::find(std::string const& aspectName) const {
		auto it = _hashers.find(aspectName);
		if (it != _hashers.end()) {
			return it->second;
		}
		return entireFileHasher;
	}

	bool FileAspectHasherSet::contains(std::string const& aspectName) const {
		return _hashers.contains(aspectName);
	}
}