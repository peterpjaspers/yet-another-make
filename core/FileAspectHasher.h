#pragma once

#include "FileAspect.h"
#include "Delegates.h"
#include "..\xxhash\xxhash.h"

#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include <regex>
#include <mutex>

namespace YAM
{
	class __declspec(dllexport) FileAspectHasher
	{
	public:	
		FileAspectHasher() = default;
		FileAspectHasher(const FileAspectHasher & other) = default;

		// Construct a hasher capable of hashing the given aspect of a file.
		// Use the given hash function to hash the aspect.
		// An example of a file aspect is the code aspect of a C++ file.
		// The hash function will only hash the code sections of the file, i.e. 
		// it will exclude the comment sections from being hashed.
		FileAspectHasher(
			FileAspect const & aspect,
			Delegate<XXH64_hash_t, std::filesystem::path const &> const & hashFunction);

		FileAspect const& aspect() const;

		// Pre: this.aspect().applicableFor(fileName)
		XXH64_hash_t hash(std::filesystem::path const & fileName) const;

	private:
		FileAspect _aspect;
		Delegate<XXH64_hash_t, std::filesystem::path const &> _hashFunction;
	};

	// A set of FileAspectHashers with unique aspect names.
	// The set is not thread-safe. Thread-safe access can be implemented by 
	// cooperative locking of mutex(). Always lock before access, also when
	// only calling const member functions.
	//
	class __declspec(dllexport) FileAspectHasherSet
	{
	public:
		FileAspectHasherSet() = default;

		// Return the mutex to be used to lock the hasher set.
		std::mutex& mutex();

		// Add given hasher to the set.
		// Return whether newHasher was added. newHasher cannot be added when
		// newHasher.aspect.name() already exists in the set.
		bool add(FileAspectHasher const& newHasher);

		// Remove hasher with given aspect name from the set.
		// Return whether hasher was found (and removed).
		bool remove(std::string const& aspectName);

		void clear();

		// Find hasher with given aspect name and return it.
		// Return hasher for FileAspect::entireFile() when given aspect is
		// not found.
		FileAspectHasher const & find(std::string const& aspectName) const;

		bool contains(std::string const& aspectName) const;

	private:
		std::mutex _mutex;

		// aspect name => hasher
		std::map<std::string, FileAspectHasher> _hashers;
	};

}
