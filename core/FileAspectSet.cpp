#include "FileAspectSet.h"

namespace YAM
{
	std::vector<FileAspect> const& FileAspectSet::aspects() const {
		return _aspects;
	}

	std::pair<bool, FileAspect const&> FileAspectSet::find(std::string const& aspectName) const {
		for (auto& _a : _aspects) {
			if (_a.name() == aspectName) return { true, _a };
		}
		return { false, FileAspect() };
	}

	FileAspect const & FileAspectSet::findApplicableAspect(std::filesystem::path const & fileName) const {
		FileAspect const * foundAspect = nullptr;
		for (auto& _a : _aspects) {
			if (_a.matches(fileName)) {
				if (foundAspect != nullptr) throw std::runtime_error("fileName must be applicable for one aspect only");
				foundAspect = &_a;
			}
		}
		return foundAspect != nullptr ? *foundAspect : FileAspect::entireFileAspect();
	}

	void FileAspectSet::add(FileAspect const& aspect) {
		const bool alreadyExists = find(aspect.name()).first;
		if (alreadyExists) throw std::runtime_error("aspect must be unique");
		_aspects.push_back(aspect);
	}

	void FileAspectSet::remove(FileAspect const& aspect) {
		for (std::size_t index = 0; index < _aspects.size(); ++index) {
			if (_aspects[index].name() == aspect.name()) {
				_aspects.erase(_aspects.begin() + index);
			    break;
			}
		}		
	}	

	void FileAspectSet::clear() {
		_aspects.clear();
	}
}
