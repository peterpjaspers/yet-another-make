#pragma once

#include "IConsole.h"
#include <memory>
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>

namespace YAM
{
    class __declspec(dllexport) ConsoleWin32 : public IConsole
    {
    public:
        ConsoleWin32();

        void textColor(IConsole::Color c) override;
        void backgroundColor(IConsole::Color c) override;
        void colors(Color text, Color background) override;
        void restoreDefaultColors() override;

    private:
        void setColors();

        HANDLE _coutHandle;
        WORD _defaultTextColor;
        WORD _defaultBackgroundColor;
        WORD _textColor;
        WORD _backgroundColor;
    };
};

