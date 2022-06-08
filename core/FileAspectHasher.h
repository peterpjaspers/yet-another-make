#pragma once

#include "FileAspect.h"
#include "Delegates.h"
#include "..\xxhash\xxhash.h"

#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include <regex>

namespace YAM
{
	class __declspec(dllexport) FileAspectHasher
	{
	public:	
		// Construct a hasher capable of hashing the given aspect of a file.
		// Use the given hash function to hash the aspect.
		// An example of a file aspect is the code aspect of a C++ file.
		// The hash function will only hash the code sections of the file, i.e. 
		// it will exclude the comment sections from being hashed.
		FileAspectHasher(
			FileAspect const & aspect,
			Delegate<XXH64_hash_t, std::filesystem::path> const & hashFunction);

		FileAspect const& aspect() const;

		// Pre: this.aspect().applicableFor(fileName)
		XXH64_hash_t hash(std::filesystem::path fileName) const;

	private:
		FileAspect _aspect;
		Delegate<XXH64_hash_t, std::filesystem::path> _hashFunction;
	};
}
