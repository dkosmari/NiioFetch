#pragma once

namespace ansi {

    void
    enable_auto_newline();

    int
    set_pos(int x,
            int y);

    int
    clear_line_forward();

    int
    clear_screen();

    __attribute__(( __format__(__printf__, 3, 4) ))
    int printf_xy(int x,
                  int y,
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
    };

    void
    set_fg(color c);

    void
    set_bg(color c);

    void
    blink_slow();

    void
    blink_fast();

    void
    blink_off();

} // namespace ansi
