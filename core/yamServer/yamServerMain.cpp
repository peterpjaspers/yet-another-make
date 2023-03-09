#include "../BuildService.h"
#include "../DotYamDirectory.h"
#include "../BasicOStreamLogBook.h"
#include "../BuildServicePortRegistry.h"

#include <filesystem>
#include <iostream>
#include <memory>

using namespace YAM;

int main(int argc, char argv[]) {
    BasicOStreamLogBook logBook(&std::cout);

    DotYamDirectory::initialize(std::filesystem::current_path(), &logBook);
    auto service = std::make_shared<BuildService>();
    BuildServicePortRegistry writer(service->port());
    if (!writer.good()) {
        logBook.add(LogRecord(LogRecord::Aspect::Error, std::string("Failed to write service port registry")));
        return 1;
    }
    service->join();
}