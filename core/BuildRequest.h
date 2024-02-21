#pragma 

#include "IStreamable.h"
#include "BuildOptions.h"

#include <filesystem>
#include <vector>
#include <string>

namespace YAM
{
    class __declspec(dllexport) BuildRequest : public IStreamable
    {
    public:
        BuildRequest();
        BuildRequest(IStreamer* reader);

        // Set/get the root directory of the repository to build.
        void repoDirectory(std::filesystem::path const& directory);
        std::filesystem::path const &repoDirectory();

        // Set/get the name of the repository.
        void repoName(std::string const& newName);
        std::string const &repoName() const;

        void options(BuildOptions const& newOptions);
        BuildOptions const& options() const;

        static void setStreamableType(uint32_t type);
        // Inherited via IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;

    private:
        std::filesystem::path _repoDirectory;
        std::string _repoName;
        BuildOptions _options;
    };
}


