#include "FileAspectSet.h"

namespace YAM
{
	std::vector<FileAspect> const& FileAspectSet::aspects() const {
		return _aspects;
	}

	FileAspect const* FileAspectSet::find(std::string const& aspectName) const {
		for (auto& _a : _aspects) {
			if (_a.applicableFor(aspectName)) return &_a;
		}
		return nullptr;
	}

	FileAspect const * FileAspectSet::findApplicableAspect(std::filesystem::path const & fileName) const {
		FileAspect const * found = nullptr;
		for (auto& _a : _aspects) {
			if (_a.applicableFor(fileName)) {
				if (found != nullptr) throw std::runtime_error("filename must be applicable for one aspect only");
				found = &_a;
			}
		}
		return found;
	}

	std::string const& FileAspectSet::findApplicableAspectName(std::filesystem::path const & fileName) const {
		static std::string entireFile("entireFile");
		FileAspect const* found = findApplicableAspect(fileName);
		if (found != nullptr) return found->aspectName();
		return entireFile;
	}

	void FileAspectSet::add(FileAspect const& aspect) {
		if (nullptr != find(aspect.aspectName())) throw std::runtime_error("aspect must be unique");
		_aspects.push_back(aspect);
	}

	void FileAspectSet::remove(FileAspect const& aspect) {
		for (std::size_t index = 0; index < _aspects.size(); ++index) {
			if (_aspects[index].aspectName() == aspect.aspectName()) {
				_aspects.erase(_aspects.begin() + index);
			    break;
			}
		}		
	}	

	void FileAspectSet::clear() {
		_aspects.clear();
	}
}
