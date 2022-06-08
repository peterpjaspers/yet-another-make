#include "FileAspectHasher.h"

namespace YAM
{
	FileAspectHasher::FileAspectHasher(
		FileAspect const& aspect,
		Delegate<XXH64_hash_t, std::filesystem::path> const& hashFunction)
		: _aspect(aspect)
		, _hashFunction(hashFunction)
	{}

	FileAspect const& FileAspectHasher::aspect() const {
		return _aspect;
	}

	XXH64_hash_t FileAspectHasher::hash(std::filesystem::path fileName) const {
		return _hashFunction.Execute(fileName);
	}
}