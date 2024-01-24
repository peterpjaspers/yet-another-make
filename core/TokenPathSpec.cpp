#include "TokenPathSpec.h"
#include "Glob.h"

#include <stdexcept>
#include <sstream>
#include <filesystem>

namespace
{
	bool isGroupPath(std::string const& str) {
		std::size_t n = str.length();
		if (n < 3) return false;
		if (str[n - 1] != '>') return false;
		auto it = str.rbegin();
		if (*it != '>') return false;
		it++;
		for (; it != str.rend(); ++it) {
			char c = *it;
			if (c == '/') return false;
			if (c == '\\') return false;
			if (c == '<') break;
		}
		return it != str.rend();
	}

	bool isBin(std::string const& str) {
		std::size_t n = str.length();
		return n > 2 && str[0] == '{' && str[n - 1] == '}';
	}
}

namespace YAM
{
    bool TokenPathSpec::match(const char* str, Token& token) const {
		// For input paths: '|' starts orderOnlyInput section, '|>' starts 
		// command section. 
		// For output paths: ':' starts new rule
		// '>' is exluded as first char because a typical typo is to start the
		// command section with '>|'.
		// Note: this does not work if rule does not contain command section.
		// BuildFileParser to verify presence of command section?
		static std::vector<char> ex = { '|', ':', '>'};

		std::string result;
		const char* s = str;
		bool quoted = false;
		if (ex.end() == std::find(ex.begin(), ex.end(), *s)) {
			for (; *s; s++) {
				if (*s == '"') {
					quoted = !quoted;
				} else if (isspace(*s) && !quoted) {
					break;
				} else {
					result.push_back(*s);
				}
			}
			if (quoted) {
				std::stringstream ss;
				ss << "Missing endquote on string: " << str;
				throw std::runtime_error(ss.str());
			}
		}
		if (result.empty()) {
			token.spec = nullptr;
		} else {
			token.spec = this;
			if (isGroupPath(result)) {
				token.type = "group";
			} else if (isBin(result)) {
				token.type = "bin";
			} else if (Glob::isGlob(result)) {
				token.type = "glob";
			} else {
				token.type = "path";
			}
			token.value = result;
			token.consumed = result.length();
		}
		return token.spec != nullptr;
    }

}