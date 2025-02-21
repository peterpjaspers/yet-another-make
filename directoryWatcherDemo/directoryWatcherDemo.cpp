#include "../core/DirectoryWatcher.h"

#include <iostream>

using namespace YAM;

namespace 
{
    std::string toString(FileChange::Action action) {
        std::string actionStr("Bad action");
        if (action == FileChange::Action::Added) actionStr = "Added";
        else if (action == FileChange::Action::Removed) actionStr = "Removed";
        else if (action == FileChange::Action::Modified) actionStr = "Modified";
        else if (action == FileChange::Action::Renamed) actionStr = "Renamed";
        else if (action == FileChange::Action::Overflow) actionStr = "Overflow";
        return actionStr;
    }

     void handle(FileChange const& change) {
         std::string action = toString(change.action);
         std::cout << action << " file=" << change.fileName;
         if (change.action == FileChange::Action::Renamed) {
             std::cout << " oldFile=" << change.oldFileName;
         }
         std::cout <<  std::endl;
    }

}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: directoryWatcherDemo directoriesToWatch" << std::endl;
    }
    auto handler = Delegate<void, FileChange const&>::CreateStatic(handle);
    std::vector<DirectoryWatcher*> watchers;
    for (int a = 1; a < argc; ++a) {
        std::filesystem::path dir(argv[a]);
        auto watcher = new DirectoryWatcher(dir, true, handler);
        watchers.push_back(watcher);
        watcher->start();
        std::cout << "Watching " << dir << std::endl;

    }
    std::string input;    
    while (input.empty()) std::cin >> input;
    for (auto watcher : watchers) {
        watcher->stop();
    }
}
