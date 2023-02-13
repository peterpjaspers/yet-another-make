#include "DirectoryWatcher.h"

#if defined( _WIN32 )
    #include "DirectoryWatcherWin32.h"
#define FW_IMPL_CLASS DirectoryWatcherWin32
#elif
    #error "platform is not supported"
#endif

namespace YAM
{
	DirectoryWatcher::DirectoryWatcher(
		std::filesystem::path const& directory,
		bool recursive,
		Delegate<void, FileChange const&> const& changeHandler)
		: IDirectoryWatcher(directory, recursive, changeHandler)
	{
		_impl = std::make_shared<FW_IMPL_CLASS>(_directory, _recursive, _changeHandler);
	}
	
	void DirectoryWatcher::stop() {
		_impl->stop();
	}
}