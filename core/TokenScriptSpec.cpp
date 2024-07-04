#include "TokenScriptSpec.h"

namespace {
	bool startsWithDelimiter(const char* str) {
		const char* s = str;
		if (*s != '|') return false;
		s++;
		if (*s != '>') return false;
		return true;
	}
}

namespace YAM
{
	bool TokenScriptSpec::match(const char* str, Token& token) const {
		token.spec = nullptr;

		std::string result;
		const char* s = str;
		if (!startsWithDelimiter(s)) return false;
		s += 2;
		for (; *s && !startsWithDelimiter(s); s++) {
			result.push_back(*s);
		}
		if (!startsWithDelimiter(s)) return false;

		token.spec = this;
		token.type = "script";
		token.consumed = (s + 2) - str;
		token.value = result;
		return token.spec != nullptr;
	}
}
