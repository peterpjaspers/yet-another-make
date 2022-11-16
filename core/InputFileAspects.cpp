#include "InputFileAspects.h"

namespace YAM
{
	InputFileAspects::InputFileAspects(
		std::string const& outputFileNamePattern,
		FileAspectSet const& inputAspects)
		: _outputFileNamePattern(outputFileNamePattern)
		, _outputFileRegex(outputFileNamePattern)
		, _inputAspects(inputAspects) {
	}

	std::string const& InputFileAspects::outputFileNamePattern() const {
		return _outputFileNamePattern;
	}
	FileAspectSet & InputFileAspects::inputAspects() {
		return _inputAspects;
	}

	bool InputFileAspects::matches(std::filesystem::path const& outputFileName) const {
		return std::regex_search(outputFileName.string(), _outputFileRegex);
	}
}

namespace YAM
{
	InputFileAspectsSet::InputFileAspectsSet()
		: _entireFileForAll(".*", FileAspectSet())
	{
		FileAspect aspect(FileAspect::entireFileAspect().name(), RegexSet());
		aspect.fileNamePatterns().add(".*");
		_entireFileForAll.inputAspects().add(aspect);
	}

	bool InputFileAspectsSet::add(InputFileAspects const& newAspects) {
		if (contains(newAspects.outputFileNamePattern())) return false;
		_inputFileAspects.push_back(newAspects);
		return true;
	}

	bool InputFileAspectsSet::remove(std::string const& outputFileNamePattern) {
		for (std::size_t index = 0; index < _inputFileAspects.size(); ++index) {
			if (_inputFileAspects[index].outputFileNamePattern() == outputFileNamePattern) {
				_inputFileAspects.erase(_inputFileAspects.begin() + index);
				return true;
			}
		}
		return false;
	}

	void InputFileAspectsSet::clear() {
		_inputFileAspects.clear();
	}

	bool InputFileAspectsSet::contains(std::string const& outputFileNamePattern) const {
		for (auto& a : _inputFileAspects) {
			if (a.outputFileNamePattern() == outputFileNamePattern) return true;
		}
		return false;
	}

	std::pair<bool, InputFileAspects const&> InputFileAspectsSet::find(std::string const& outputFileNamePattern) const {
		for (auto const & a : _inputFileAspects) {
			if (a.outputFileNamePattern() == outputFileNamePattern) return { true, a };
		}
		return { false, InputFileAspects() };
	}

	InputFileAspects const& InputFileAspectsSet::findOutputMatch(std::filesystem::path const& outputFileName) const {
		InputFileAspects const * foundAspects = nullptr;
		for (auto & a : _inputFileAspects) {
			if (a.matches(outputFileName)) {
				if (foundAspects != nullptr) throw std::runtime_error("only one InputFileAspects must match outputFileName");
				foundAspects = &a;
			}
		}
		return (foundAspects != nullptr) ? *foundAspects : _entireFileForAll;
	}

}
