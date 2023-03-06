#pragma 

#include "IStreamable.h"
#include <filesystem>
#include <vector>
#include <string>

namespace YAM
{
    class __declspec(dllexport) BuildRequest : public IStreamable
    {
    public:
        enum RequestType {
            Init = 0,  // create a build state in directory()/.yam
            Build = 1, // execute dirty nodes in build scope
            Clean = 2  // delete output files of nodes in build scope
        };

        BuildRequest(RequestType type = Build);
        BuildRequest(IStreamer* reader);

        void requestType(RequestType type);
        RequestType requestType() const;

        // Set/get the directory from which the build was started.
        void directory(std::filesystem::path const& directory);
        std::filesystem::path const& directory();

        // Only applicable to Build and Clean requests.
        void addToScope(std::filesystem::path const& path);
        std::vector<std::filesystem::path> const& pathsInScope();

        static void setStreamableType(uint32_t type);
        // Inherited via IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;

    private:
        RequestType _type;
        std::filesystem::path _directory;
        std::vector<std::filesystem::path> _pathsInScope;
    };
}


