#include "FileWatcher.h"

#if defined( _WIN32 )
    #include "FileWatcherWin32.h"
#define FW_IMPL_CLASS FileWatcherWin32
#elif
    #error "platform is not supported"
#endif

namespace YAM
{
	FileWatcher::FileWatcher(
		std::filesystem::path const& directory,
		bool recursive,
		Delegate<void, FileChange>& changeHandler) 
		: IFileWatcher(directory, recursive, changeHandler)
	{
		_impl = std::make_shared<FW_IMPL_CLASS>(_directory, _recursive, _changeHandler);
	}
}