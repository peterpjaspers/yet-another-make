#pragma once

namespace YAM
{
    class __declspec(dllexport) IConsole
    {
    public:
        enum Color {
            black = 0,
            dark_blue = 1,
            dark_green = 2,
            light_blue = 3,
            dark_red = 4,
            magenta = 5,
            orange = 6,
            light_gray = 7,
            gray = 8,
            blue = 9,
            green = 10,
            cyan = 11,
            red = 12,
            pink = 13,
            yellow = 14,
            white = 15,
        };

        virtual void textColor(Color c) = 0;
        virtual void backgroundColor(Color c) = 0;
        virtual void colors(Color text, Color background) = 0;
        virtual void restoreDefaultColors() = 0;
	};
}