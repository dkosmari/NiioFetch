#include <cstdarg>
#include <cstdio>

#include "ansi.hpp"


namespace ansi {

    int width = 80;
    int height = 25;


    void
    enable_auto_newline()
    {
        std::fputs("\e[20h", stdout);
    }

    void
    set_pos(int x,
            int y)
    {
        std::printf("\e[%d;%dH", y, x);
    }


    void
    set_col(int x)
    {
        std::printf("\e[%dG", x);
    }


    void
    hide_cursor()
    {
        std::fputs("\e[?25l", stdout);
    }


    void
    show_cursor()
    {
        std::fputs("\e[?25h", stdout);
    }


    void
    erase_line_forward()
    {
        std::fputs("\e[0K", stdout);
    }


    void
    clear_screen(int mode)
    {
        std::printf("\e[%dJ", mode);
    }


    int
    printf_xy(int x,
              int y,
              const char* fmt,
              ...)
    {
        set_pos(x, y);
        va_list args;
        va_start(args, fmt);
        int r = std::vprintf(fmt, args);
        va_end(args);
        return r;
    }


    void
    centered(const char* fmt,
             ...)
    {
        va_list args;
        va_start(args, fmt);
        int w = std::vsnprintf(nullptr, 0, fmt, args);
        va_end(args);
        if (w <= 0)
            return;
        int pos_x = (width - w) / 2;
        set_col(pos_x);
        va_start(args, fmt);
        std::vprintf(fmt, args);
        va_end(args);
    }


    void
    centered(int y,
             const char* fmt,
             ...)
    {
        va_list args;
        va_start(args, fmt);
        int w = std::vsnprintf(nullptr, 0, fmt, args);
        va_end(args);
        if (w <= 0)
            return;
        int pos_x = (width - w) / 2;
        set_pos(pos_x, y);
        va_start(args, fmt);
        std::vprintf(fmt, args);
        va_end(args);
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
    set_fg(uint8_t r,
           uint8_t g,
           uint8_t b)
    {
        std::printf("\e[38;2;%u;%u;%um", r, g, b);
    }

    void
    set_bg(uint8_t r,
           uint8_t g,
           uint8_t b)
    {
        std::printf("\e[48;2;%u;%u;%um", r, g, b);
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
