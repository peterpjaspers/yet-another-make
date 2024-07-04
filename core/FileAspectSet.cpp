#include "FileAspectSet.h"

namespace YAM
{
    FileAspectSet::FileAspectSet(const std::string& name) :
        _name(name) {
    }

    const std::string& FileAspectSet::name() const {
        return _name;
    }

    std::vector<FileAspect> FileAspectSet::aspects() const {
        std::vector<FileAspect> aspects;
        for (auto& _a : _aspects) {
            aspects.push_back(_a.second);
        }
        return aspects;
    }

    std::pair<bool, FileAspect const&> FileAspectSet::find(std::string const& aspectName) const {
        auto const& a = _aspects.find(aspectName);
        if (a != _aspects.cend()) return { true, a->second };
        return { false, FileAspect()};
    }

    FileAspect const & FileAspectSet::findApplicableAspect(std::filesystem::path const & fileName) const {
        FileAspect const * foundAspect = nullptr;
        for (auto const& _a : _aspects) {
            FileAspect const& aspect = _a.second;
            if (aspect.appliesTo(fileName)) {
                if (foundAspect != nullptr) throw std::runtime_error("fileName must be applicable for one aspect only");
                foundAspect = &aspect;
            }
        }
        return foundAspect != nullptr ? *foundAspect : FileAspect::entireFileAspect();
    }

    void FileAspectSet::add(FileAspect const& aspect) {
        if (_aspects.contains(aspect.name())) throw std::runtime_error("aspect name must be unique");
        _aspects.insert({ aspect.name(), aspect });
    }

    void FileAspectSet::remove(FileAspect const& aspect) {
        auto const& a = _aspects.find(aspect.name());
        if (a != _aspects.cend()) _aspects.erase(a->first);
    }

    bool FileAspectSet::contains(std::string const& aspectName) const {
        return _aspects.contains(aspectName);
    }

    void FileAspectSet::clear() {
        _aspects.clear();
    }

    FileAspectSet const& FileAspectSet::entireFileSet() {
        static FileAspectSet entireFileSet("entireFileSet");
        static bool entireFileSetInitialized = false;
        if (!entireFileSetInitialized) {
            entireFileSetInitialized = true;
            entireFileSet.add(FileAspect::entireFileAspect());
        }
        return entireFileSet;
    }
}
