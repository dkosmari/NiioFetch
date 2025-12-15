#include <cstdarg>
#include <cstdio>

#include "ansi.hpp"


namespace ansi {

    void
    enable_auto_newline()
    {
        std::fputs("\e[20h", stdout);
    }

    int
    set_pos(int x,
            int y)
    {
        return std::printf("\e[%d;%dH", y, x);
    }


    int
    clear_line_forward()
    {
        return std::fputs("\e[0K", stdout);
    }


    int
    clear_screen()
    {
        return std::fputs("\e[2J", stdout);
    }


    __attribute__(( __format__(__printf__, 3, 4) ))
    int printf_xy(int x,
                  int y,
                  const char* fmt,
                  ...)
    {
        int r1 = set_pos(x, y);
        if (r1 < 0)
            return r1;
        va_list args;
        va_start(args, fmt);
        int r2 = std::vprintf(fmt, args);
        va_end(args);
        if (r2 < 0)
            return r2;
        return r1 + r2;
    }


    void
    reset()
    {
        std::fputs("\e[0m", stdout);
    }


    void
    set_fg(color c)
    {
        if (c == color::reset) {
            std::fputs("\e[39m", stdout);
            return;
        }
        int base = 30;
        int val = static_cast<int>(c);
        if (c >= color::light_black)
            base = 82; // light colors start from 90
        std::printf("\e[%dm", base + val);
    }


    void
    set_bg(color c)
    {
        if (c == color::reset) {
            std::fputs("\e[49m", stdout);
            return;
        }
        int base = 40;
        int val = static_cast<int>(c);
        if (c >= color::light_black)
            base = 92; // light colors start from 100
        std::printf("\e[%dm", base + val);
    }

    void
    blink_slow()
    {
        std::fputs("\e[5m", stdout);
    }

    void
    blink_fast()
    {
        std::fputs("\e[6m", stdout);
    }

    void
    blink_off()
    {
        std::fputs("\e[25m", stdout);
    }

} // namespace ansi
