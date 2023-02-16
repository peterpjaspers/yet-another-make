#include "ConsoleWin32.h"
#include <string>
#include <iostream>

namespace YAM
{
	ConsoleWin32::ConsoleWin32()
		: _coutHandle(GetStdHandle(STD_OUTPUT_HANDLE))
		, _defaultTextColor(IConsole::Color::white)
		, _defaultBackgroundColor(IConsole::Color::black)
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (GetConsoleScreenBufferInfo(_coutHandle, &csbi)) {
			_defaultTextColor = csbi.wAttributes & 0x0F;
			_defaultBackgroundColor = csbi.wAttributes & 0x0F0;
		} else {
			DWORD error = GetLastError();
			// error 6: handle is invalid, happens when used in gtext
		}
		_textColor = _defaultTextColor;
		_backgroundColor = _defaultBackgroundColor;
	}

	void ConsoleWin32::setColors() {
		SetConsoleTextAttribute(_coutHandle, _backgroundColor << 4 | _textColor);
	}

	void ConsoleWin32::textColor(IConsole::Color c) {
		_textColor = c;
		setColors();
	}

	void ConsoleWin32::backgroundColor(IConsole::Color c) {
		_backgroundColor = c;
		setColors();
	}

	void ConsoleWin32::colors(Color text, Color background) {
		_textColor = text;
		_backgroundColor = background;
		setColors();
	}

	void ConsoleWin32::restoreDefaultColors() {
		_textColor = _defaultTextColor;
		_backgroundColor = _defaultBackgroundColor;
		setColors();
	}
}