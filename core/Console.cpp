#include "Console.h"

#if defined( _WIN32 )
#include "ConsoleWin32.h"
#define CONSOLE_IMPL_CLASS ConsoleWin32
#elif
#error "platform is not supported"
#endif

namespace YAM
{
	Console::Console() : _impl(std::make_shared<CONSOLE_IMPL_CLASS>())
	{}

	void Console::textColor(IConsole::Color c) {
		_impl->textColor(c);
	}

	void Console::backgroundColor(IConsole::Color c) {
		_impl->backgroundColor(c);
	}

	void Console::colors(Color text, Color background) {
		_impl->colors(text, background);
	}

	void Console::restoreDefaultColors() {
		_impl->restoreDefaultColors();
	}
}