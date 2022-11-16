#include "RegexSet.h"

namespace YAM
{
	RegexSet::RegexSet(std::initializer_list<std::string> regexStrings) {
		for (auto & re : regexStrings) add(re);
	}

	bool RegexSet::matches(std::string const & s) const {
		for (auto& re : _regexes) {
			if (std::regex_search(s, re)) return true;
		}
		return false;
	}

	std::vector<std::string> const & RegexSet::regexStrings() const {
		return _regexStrings;
	}

	void RegexSet::clear() {
		_regexStrings.clear();
		_regexes.clear();
	}
	void RegexSet::add(std::string const& regexString) {
		_regexStrings.push_back(regexString);
		_regexes.push_back(std::regex(regexString));
	}

	void RegexSet::remove(std::string const& regexString) {
		auto it = std::find(_regexStrings.begin(), _regexStrings.end(), regexString);
		if (it != _regexStrings.end()) {
			std::size_t index = it - _regexStrings.begin();
			_regexStrings.erase(it);
			_regexes.erase(_regexes.begin() + index);
		}
	}
}