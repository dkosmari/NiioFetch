#pragma once

#include <stdint.h>

namespace ansi {

    extern int width;
    extern int height;

    void
    enable_auto_newline();

    void
    hide_cursor();

    void
    show_cursor();

    void
    set_pos(int x,
            int y);

    void
    set_col(int x);

    void
    erase_line_forward();

    void
    clear_screen(int mode = 2);

    __attribute__(( __format__(__printf__, 3, 4) ))
    int
    printf_xy(int x,
              int y,
              const char* fmt,
              ...);

    __attribute__(( __format__(__printf__, 1, 2) ))
    void
    centered(const char* fmt,
             ...);

    __attribute__(( __format__(__printf__, 2, 3) ))
    void
    centered(int y,
             const char* fmt,
             ...);

    void
    reset();

    enum class color : int {
        reset = -1,
        black = 0,
        red = 1,
        green = 2,
        yellow = 3,
        blue = 4,
        magenta = 5,
        cyan = 6,
        white = 7,
        light_black = 8,
        gray = light_black,
        light_red = 9,
        light_green = 10,
        light_yellow = 11,
        light_blue = 12,
        light_magenta = 13,
        light_cyan = 14,
        light_white = 15,
        num_colors = 16,
    };

    void
    set_fg(color c);

    void
    set_bg(color c);

    void
    set_fg(uint8_t r,
           uint8_t g,
           uint8_t b);

    void
    set_bg(uint8_t r,
           uint8_t g,
           uint8_t b);

    void
    blink_slow();

    void
    blink_fast();

    void
    blink_off();

} // namespace ansi
