#include "../core/DirectoryWatcher.h"

#include <iostream>

using namespace YAM;

std::filesystem::path watchedDirectory("D:\\Peter");

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
     std::cout 
         << action 
         << " file=" << watchedDirectory / change.fileName 
         << " oldFile=" << watchedDirectory / change.oldFileName
         <<  std::endl;
}

int main(int argc, char** argv) {
    if (argc > 1) {
        watchedDirectory = std::filesystem::path(argv[1]);
    }
    std::cout << "watching " << watchedDirectory << std::endl;
    auto handler = Delegate<void, FileChange const&>::CreateStatic(handle);
    DirectoryWatcher watcher(watchedDirectory, true, handler);
    std::string input;    
    while (input.empty()) std::cin >> input;

}
