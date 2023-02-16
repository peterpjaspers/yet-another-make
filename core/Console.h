#pragma once

#include "IConsole.h"
#include <memory>

namespace YAM
{
    class __declspec(dllexport) Console : public IConsole
    {
    public:
        Console();

        void textColor(IConsole::Color c) override;
        void backgroundColor(IConsole::Color c) override;
        void colors(IConsole::Color text, IConsole::Color background) override;
        void restoreDefaultColors() override;

    private:
        std::shared_ptr<IConsole> _impl;
    };
};

