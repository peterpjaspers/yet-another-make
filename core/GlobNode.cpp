//#include "GlobNode.h
//
//#include <regex>
//
//namespace
//{
//    using namespace YAM;
//    namespace fs = std::filesystem;
//
//    std::vector<fs::path> filter(
//        std::vector<fs::path> const& names,
//        std::string const& pattern
//    ) {
//        std::vector<fs::path> result;
//        Glob glob(pattern, true);
//        for (auto& name : names) {
//            if (glob.matches(name)) {
//                result.push_back(name);
//            }
//        }
//        return result;
//    }
//
//    bool isHidden(std::string const& pathname) { return pathname[0] == '.'; }
//
//    bool isRecursive(std::string const& pattern) { return pattern == "**"; }
//
//    std::vector<fs::path> iter_directory(const fs::path& dirname, bool dironly) {
//        std::vector<fs::path> result;
//        auto current_directory = dirname;
//        if (current_directory.empty()) {
//            current_directory = fs::current_path();
//        }
//        std::error_code ec;
//        if (fs::is_directory(current_directory, ec)) {
//            for (auto& entry : fs::directory_iterator(
//                current_directory, fs::directory_options::follow_directory_symlink |
//                fs::directory_options::skip_permission_denied)) {
//                if (!dironly || entry.is_directory()) {
//                    if (dirname.is_absolute()) {
//                        result.push_back(entry.path());
//                    } else {
//                        result.push_back(fs::relative(entry.path()));
//                    }
//                }
//            }
//        }
//        return result;
//    }
//
//    // Recursively yields relative pathnames inside a literal directory.
//    std::vector<fs::path> rlistdir(const fs::path& dirname, bool dironly) {
//        std::vector<fs::path> result;
//        auto names = iter_directory(dirname, dironly);
//        for (auto& x : names) {
//            if (!isHidden(x.string())) {
//                result.push_back(x);
//                for (auto& y : rlistdir(x, dironly)) {
//                    result.push_back(y);
//                }
//            }
//        }
//        return result;
//    }
//
//    // This helper function recursively yields relative pathnames inside a literal
//    // directory.
//    std::vector<fs::path> glob2(const fs::path& dirname, [[maybe_unused]] const fs::path& pattern,
//                                bool dironly) {
//      // std::cout << "In glob2\n";
//        std::vector<fs::path> result{ "." };
//        assert(isRecursive(pattern.string()));
//        for (auto& dir : rlistdir(dirname, dironly)) {
//            result.push_back(dir);
//        }
//        return result;
//    }
//
//    // These 2 helper functions non-recursively glob inside a literal directory.
//    // They return a list of basenames.  _glob1 accepts a pattern while _glob0
//    // takes a literal basename (so it only has to check for its existence).
//
//    std::vector<fs::path> glob1(const fs::path& dirname, const fs::path& pattern,
//                                bool dironly) {
//      // std::cout << "In glob1\n";
//        auto names = iter_directory(dirname, dironly);
//        std::vector<fs::path> filtered_names;
//        for (auto& n : names) {
//            if (!isHidden(n.string())) {
//                filtered_names.push_back(n.filename());
//                // if (n.is_relative()) {
//                //   // std::cout << "Filtered (Relative): " << n << "\n";
//                //   filtered_names.push_back(fs::relative(n));
//                // } else {
//                //   // std::cout << "Filtered (Absolute): " << n << "\n";
//                //   filtered_names.push_back(n.filename());
//                // }
//            }
//        }
//        return filter(filtered_names, pattern.string());
//    }
//
//    std::vector<fs::path> glob0(
//        const fs::path& dirname, 
//        const fs::path& basename,
//        bool /*dironly*/
//    ) {
//        std::vector<fs::path> result;
//        if (basename.empty()) {
//            // 'q*x/' should match only directories.
//            if (fs::is_directory(dirname)) {
//                result = { basename };
//            }
//        } else {
//            if (fs::exists(dirname / basename)) {
//                result = { basename };
//            }
//        }
//        return result;
//    }
//
//    std::vector<fs::path> glob(const fs::path& inpath, bool dironly = false) {
//        std::vector<fs::path> result;
//
//        const auto pathname = inpath.string();
//        auto path = fs::path(pathname);
//
//        auto dirname = path.parent_path();
//        const auto basename = path.filename();
//
//        if (Glob::isLiteral(pathname)) {
//            assert(!dironly);
//            if (!basename.empty()) {
//                if (fs::exists(path)) {
//                    result.push_back(path);
//                }
//            } else {
//                // Patterns ending with a slash should match only directories
//                if (fs::is_directory(dirname)) {
//                    result.push_back(path);
//                }
//            }
//            return result;
//        }
//
//        if (dirname.empty()) {
//            if (isRecursive(basename.string())) {
//                return glob2(dirname, basename, dironly);
//            } else {
//                return glob1(dirname, basename, dironly);
//            }
//        }
//
//        std::vector<fs::path> dirs;
//        if (dirname != fs::path(pathname) && !Glob::isLiteral(dirname.string())) {
//            dirs = glob(dirname, true);
//        } else {
//            dirs = { dirname };
//        }
//
//        std::function<std::vector<fs::path>(const fs::path&, const fs::path&, bool)>
//            glob_in_dir;
//        if (!Glob::isLiteral(basename.string())) {
//            if (isRecursive(basename.string())) {
//                glob_in_dir = glob2;
//            } else {
//                glob_in_dir = glob1;
//            }
//        } else {
//            glob_in_dir = glob0;
//        }
//
//        for (auto& d : dirs) {
//            for (auto& name : glob_in_dir(d, basename, dironly)) {
//                fs::path subresult = name;
//                if (name.parent_path().empty()) {
//                    subresult = d / name;
//                }
//                result.push_back(subresult.lexically_normal());
//            }
//        }
//
//        return result;
//    }
//
//    std::vector<fs::path> glob(const std::string& pathname) {
//        return glob(pathname, false);
//    }
//}
//
//namespace YAM
//{
//    namespace fs = std::filesystem;
//        
//    // Return the directory and file nodes that match 'inpath' which is a glob
//    // pattern or a literal path.
//    // When 'dirsOnly' is true: only return matching directory nodes.
//    std::vector<std::shared_ptr<Node>> GlobNode::find(
//        fs::path const& globPath,
//        bool dirsOnly
//    ) {
//        std::vector<std::shared_ptr<Node>> result;
//        return result;
//    }
//
//    // Return a hash of of the hashes of the base directory name, the glob
//    // pattern and of the names of the directories visited during glob
//    // execution. When this hash changes the glob will re-execute.
//    XXH64_hash_t GlobNode::executionHash() {
//        return 0;
//    }
//}