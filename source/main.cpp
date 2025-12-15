#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <di/di.h>
#include <gccore.h>
#include <ogc/machine/processor.h>
#include <ogc/pad.h>
#include <ogc/system.h>
#include <wiiuse/wpad.h>

#include <png.h>

#include "ansi.hpp"
#include "ios.h"

#include "dolphin-image_png.h"
#include "wii-image_png.h"
#include "wii-family-image_png.h"
#include "wii-mini-image_png.h"
#include "wiiu-image_png.h"

// #define USE_LIBOGC2
#ifdef USE_LIBOGC2
#define VIDEO_GetVideoScanMode VIDEO_GetScanMode
#endif


#define AHBPROT_DISABLED (*(vu32*)0xcd800064 == 0xFFFFFFFF)

void *xfb = NULL;
GXRModeObj *rmode = NULL;

using ansi::printf_xy;

#define VER "dko"

const std::array languages = {
    "Japanese",
    "English",
    "German",
    "French",
    "Italian",
    "Dutch",
    "Chinese (Simplified)",
    "Chinese (Traditonal)",
    "Korean",
};

const std::array regions = {
    "Japan",
    "USA",
    "Europe",
    "NULL",
    "Korea",
    "China",
};

enum class ConsoleType {
    Wii,
    WiiFamily,
    WiiMini,
    WiiU,
    Dolphin,
};

std::unordered_map<std::string, std::string> settings;

std::string_view
trimmed(const std::string_view& s)
{
    const char* spaces = " \t\r\n";
    auto start = s.find_first_not_of(spaces);
    if (start == std::string::npos)
        return s;
    auto finish = s.find_last_not_of(spaces) + 1;
    return s.substr(start, finish - start);
}

bool
getline(std::string_view& input,
        std::string_view& line)
{
    if (input.empty())
        return false;
    const char* eol = "\n\r";
    auto pos = input.find_first_of(eol);
    if (pos == std::string_view::npos) {
        line = {};
        input = {};
        return false;
    } else {
        line = input.substr(0, pos);
        pos = input.find_first_not_of(eol, pos);
        if (pos != std::string_view::npos)
            input.remove_prefix(pos);
        else
            input = {};
    }
    return true;
}

void
load_settings()
{
    std::array<char, 0x100> settings_buf alignas(32);
    int fd = IOS_Open("/title/00000001/00000002/data/setting.txt", IPC_OPEN_READ);
    if (fd < 0)
        return;
    int r = IOS_Read(fd, settings_buf.data(), settings_buf.size());
    IOS_Close(fd);
    if (r < 0)
        return;

    u32 key = 0x73B5DBFA;
    for (auto& c : settings_buf) {
        c ^= key & 0xff;
        key = (key << 1) | (key >> 31);
    }

    settings.clear();

    std::string_view input(settings_buf.data(), r);
    std::string_view line;
    while (getline(input, line)) {
        line = trimmed(line);
        if  (line.empty())
            continue;
        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;
        bool bad_line = false;
        auto key = line.substr(0, pos);
        for (auto c : key)
            if (c & 0x80) { // key should only contain ASCII characters
                bad_line = true;
                break;
            }
        if (bad_line)
            continue;
        auto value{line.substr(pos + 1)};
        for (auto c : value)
            if (c & 0x80) { // value should only contain ASCII characters
                bad_line = true;
                break;
            }
        if (bad_line)
            continue;
        settings[std::string{key}] = std::string{value};
    }
}

u16 get_tmd_version(u64 title) { // From the homebrew channel
    u8 tmdbuf[1024] alignas(32);
    u32 tmd_view_size = 0;
    s32 res;

    res = ES_GetTMDViewSize(title, &tmd_view_size);

    if (res < 0) return 0;

    if (tmd_view_size > 1024) return 0;

#ifdef USE_LIBOGC2
    ES_GetTMDView(title, tmdbuf, tmd_view_size);
#else
    ES_GetTMDView(title, (tmd_view*)tmdbuf, tmd_view_size);
#endif

    if (res < 0) return 0;

    return (tmdbuf[88] << 8) | tmdbuf[89];
}

float
GetSysMenuNintendoVersion(u32 sysVersion)
{
    // From SysCheck
    switch (sysVersion) {
        case 33:
            return 1.0f;

        case 97:
        case 128:
        case 130:
            return 2.0f;

        case 162:
            return 2.1f;

        case 192:
        case 193:
        case 194:
            return 2.2f;

        case 224:
        case 225:
        case 226:
            return 3.0f;

        case 256:
        case 257:
        case 258:
            return 3.1f;

        case 288:
        case 289:
        case 290:
            return 3.2f;

        case 352:
        case 353:
        case 354:
        case 326:
            return 3.3f;

        case 384:
        case 385:
        case 386:
            return 3.4f;

        case 390:
            return 3.5f;

        case 416:
        case 417:
        case 418:
            return 4.0f;

        case 448:
        case 449:
        case 450:
        case 454:
        case 54448:
        case 54449:
        case 54450:
        case 54454:
            return 4.1f;

        case 480:
        case 481:
        case 482:
        case 486:
            return 4.2f;

        case 512:
        case 513:
        case 514:
        case 518:
        case 544:
        case 545:
        case 546:
        case 608:
        case 609:
        case 610:
            return 4.3f;

        default:
            return 0.0f;
    }
}

char
GetSysMenuRegion(u32 sysVersion)
{
    // From SysCheck
    switch (sysVersion) {
        case 1:  //Pre-launch
        case 97: //2.0U
        case 193: //2.2U
        case 225: //3.0U
        case 257: //3.1U
        case 289: //3.2U
        case 353: //3.3U
        case 385: //3.4U
        case 417: //4.0U
        case 449: //4.1U
        case 54449: // mauifrog 4.1U
        case 481: //4.2U
        case 513: //4.3U
        case 545:
        case 609:
            return 'U';

        case 130: //2.0E
        case 162: //2.1E
        case 194: //2.2E
        case 226: //3.0E
        case 258: //3.1E
        case 290: //3.2E
        case 354: //3.3E
        case 386: //3.4E
        case 418: //4.0E
        case 450: //4.1E
        case 54450: // mauifrog 4.1E
        case 482: //4.2E
        case 514: //4.3E
        case 546:
        case 610:
            return 'E';

        case 128: //2.0J
        case 192: //2.2J
        case 224: //3.0J
        case 256: //3.1J
        case 288: //3.2J
        case 352: //3.3J
        case 384: //3.4J
        case 416: //4.0J
        case 448: //4.1J
        case 54448: // mauifrog 4.1J
        case 480: //4.2J
        case 512: //4.3J
        case 544:
        case 608:
            return 'J';

        case 326: //3.3K
        case 390: //3.5K
        case 454: //4.1K
        case 54454: // mauifrog 4.1K
        case 486: //4.2K
        case 518: //4.3K
            return 'K';

        default:
            return 'X';
    }
}

const char*
get_odd_date()
{
    static char drive_date[15] = "";
    DI_Init();
    DI_DriveID drive;
    if (!DI_Identify(&drive)) {
        uint32_t y = (drive.rel_date >> 16) & 0xffff;
        uint32_t m = (drive.rel_date >>  8) & 0x00ff;
        uint32_t d = (drive.rel_date >>  0) & 0x00ff;
        snprintf(drive_date, sizeof drive_date, "%04X-%02X-%02X", y, m, d);
    }
    DI_Close();
    return drive_date;
}

const char*
get_wifi_mac()
{
    static char result[2*6 + 5 + 1] = "";
    s32 net_heap = iosCreateHeap(1024);
    u8* mac = reinterpret_cast<u8*>(iosAlloc(net_heap, 6));
    memset(mac, 0, 6);
    s32 fd = IOS_Open("/dev/net/wd/command", 3);
    if  (fd >= 0) {
        IOS_IoctlvFormat(net_heap, fd, 0x100e, ":d", mac, 6);
        snprintf(result, sizeof result, "%02X-%02X-%02X-%02X-%02X-%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        IOS_Close(fd);
    }
    iosFree(net_heap, mac);
    return result;
}

void
convert_row(const u8* src_rgb,
            unsigned src_width,
            u32* dst_yuv422)
{
    for (unsigned x = 0; x < src_width; x += 2) {
        u32 r1 = std::clamp<u32>(src_rgb[3*x + 0], 16u, 240u);
        u32 g1 = std::clamp<u32>(src_rgb[3*x + 1], 16u, 240u);
        u32 b1 = std::clamp<u32>(src_rgb[3*x + 2], 16u, 240u);

        u32 r2 = 16, g2 = 16, b2 = 16;
        if (x + 1 < src_width) {
            r2 = std::clamp<u32>(src_rgb[3*x + 3], 16u, 240u);
            g2 = std::clamp<u32>(src_rgb[3*x + 4], 16u, 240u);
            b2 = std::clamp<u32>(src_rgb[3*x + 5], 16u, 240u);
        }

        u32 Y1 = (( 77u * r1 + 150u * g1 + 29u * b1) / 256u) & 0xffu;
        u32 Y2 = (( 77u * r2 + 150u * g2 + 29u * b2) / 256u) & 0xffu;
        u32 Cb = ((112u * (b1 + b2) - 74u * (g1 + g2) - 38u * (r1 + r2)) / 512u + 128u) & 0xffu;
        u32 Cr = ((112u * (r1 + r2) - 94u * (g1 + g2) - 18u * (b1 + b2)) / 512u + 128u) & 0xffu;

        dst_yuv422[x/2] = (Y1 << 24u) | (Cb << 16u) | (Y2 << 8u) | Cr;
    }
}

void
blit_png(const u8* data,
         size_t size)
{
    png_image img;
    std::memset(&img, 0, sizeof img);
    try {
        const unsigned screen_x = 8;
        const unsigned screen_y = 80;
        unsigned max_width = rmode->fbWidth - screen_x;
        unsigned max_height = rmode->xfbHeight - screen_y;

        img.version = PNG_IMAGE_VERSION;
        if (!png_image_begin_read_from_memory(&img, data, size)) {
            png_image_free(&img);
            return;
        }
        img.format = PNG_FORMAT_RGB;
        std::vector<u8> pixels(img.width * img.height * 3);
        png_image_finish_read(&img, NULL, pixels.data(), 3 * img.width, NULL);

        if (img.height < max_height)
            max_height = img.height;
        if (img.width < max_width)
            max_width = img.width;
        for (unsigned y = 0; y < max_height; ++y) {
            const u8* src_row = pixels.data() + 3 * (y * img.width);
            u32* dst_row = ((u32*)xfb) + ((y + screen_y) * rmode->fbWidth + screen_x) / 2;
            convert_row(src_row, max_width, dst_row);
        }

        png_image_free(&img);
    }
    catch (...) {
        png_image_free(&img);
    }
}

void
show_image(ConsoleType t)
{
    switch (t) {
        case ConsoleType::Wii:
            blit_png(wii_image_png, wii_image_png_size);
            break;

        case ConsoleType::WiiFamily:
            blit_png(wii_family_image_png, wii_family_image_png_size);
            break;

        case ConsoleType::WiiMini:
            blit_png(wii_mini_image_png, wii_mini_image_png_size);
            break;

        case ConsoleType::WiiU:
            blit_png(wiiu_image_png, wiiu_image_png_size);
            break;

        case ConsoleType::Dolphin:
            blit_png(dolphin_image_png, dolphin_image_png_size);
            break;
    }
}

float
get_battery_volts(u8 raw)
{
    float m = 0.00522192f;
    float b = 2.154361f;
    return m * raw + b;
}

float
get_battery_volts_board(u8 raw)
{
    float m = 0.04064257f;
    float b = -0.076462994f;
    return m * raw + b;
}

unsigned
get_battery_bars(u8 raw)
{
    if (raw >= 0x55)
        return 4; // (2.593, 3.2]
    if (raw >= 0x44)
        return 3; // (2.504, 2.593]
    if (raw >= 0x33)
        return 2; // (2.415, 2.504]
    if (raw >= 0x03)
        return 1; // (2.164, 2.415]
    return 0; // (0, 2.164]
}

unsigned
get_battery_bars_board(u8 raw)
{
    if (raw >= 0x82)
        return 4;
    if (raw >= 0x7d)
        return 3;
    if (raw >= 0x78)
        return 2;
    if (raw >= 0x6a)
        return 1;
    return 0;
}

const char*
bars_to_string(unsigned b)
{
    switch (b) {
        case 0:
            return "[    }";
        case 1:
            return "[#   }";
        case 2:
            return "[##  }";
        case 3:
            return "[### }";
        case 4:
            return "[####}";
        default:
            return "error";
    }
}

void
print_battery(unsigned bars, bool crit)
{
    if (crit) {
        ansi::set_fg(ansi::color::light_red);
        ansi::blink_fast();
    }
    ansi::set_bg(ansi::color::gray);
    fputs(bars_to_string(bars), stdout);
    ansi::set_bg(ansi::color::reset);
    if (crit) {
        ansi::blink_off();
        ansi::set_fg(ansi::color::reset);
    }
}

float
remap(float x,
      float src_min, float src_max,
      float dst_min, float dst_max)
{
    return dst_min + (x - src_min) * (dst_max - dst_min) / (src_max - src_min);
}

float
get_battery_percent(u8 raw)
{
    // Use a piecewise linar approximation.
    if (raw >= 0x55)
        return remap(raw, 0x55, 0x7c, 75, 100);
    if (raw >= 0x44)
        return remap(raw, 0x44, 0x55, 50, 75);
    if (raw >= 0x33)
        return remap(raw, 0x33, 0x44, 25, 50);
    if (raw >= 0x03)
        return remap(raw, 0x03, 0x33, 0, 25);
    return 0;
}

float
get_battery_percent_board(u8 raw)
{
    // Use a piecewise linear approximation.
    if (raw >= 0x82)
        return remap(raw, 0x82, 0x86, 75, 100);
    if (raw >= 0x7d)
        return remap(raw, 0x7d, 0x82, 50, 75);
    if (raw >= 0x78)
        return remap(raw, 0x78, 0x7d, 25, 50);
    if (raw >= 0x6a)
        return remap(raw, 0x6a, 0x78, 0, 25);
    return 0;
}

std::atomic_bool power_button_pressed;

void power_button_callback()
{
    power_button_pressed = true;
}

void
show_wiimote(int channel)
{
    if (channel < 0)
        return;

    const int cur_x = 48;
    const int cur_y = 19 + channel;
    ansi::set_pos(cur_x, cur_y);
    ansi::erase_line_forward();

    u32 ext;
    if (WPAD_Probe(channel, &ext))
        return;

    static const std::array names = {
        "Wiimote 1",
        "Wiimote 2",
        "Wiimote 3",
        "Wiimote 4",
    };

    static const std::array ext_names = {
        "none",
        "nunchuk",
        "classic",
        "guitar",
    };

    u8 bat = WPAD_BatteryLevel(channel);
    bool crit = WPAD_IsBatteryCritical(channel);

    auto name = static_cast<unsigned>(channel) < names.size() ? names[channel] : "Unknown";
    if (ext == WPAD_EXP_WIIBOARD) {
        auto data = WPAD_Data(channel);
        bat = data->exp.wb.rbat;
        printf("Bal. Board : ");
        print_battery(get_battery_bars_board(bat), crit);
        printf(" %2.0f%% %1.1fV",
               get_battery_percent_board(bat),
               get_battery_volts_board(bat));
        ansi::set_pos(cur_x, cur_y + 1);
        ansi::erase_line_forward();
        printf("weight: %.1f, temp: %u",
               data->exp.wb.weight,
               unsigned{data->exp.wb.rtemp});
    } else if (ext == WPAD_EXP_NONE) {
        printf("%s : ", name);
        print_battery(get_battery_bars(bat), crit);
        printf(" %2.0f%% %1.1fV",
               get_battery_percent(bat),
               get_battery_volts(bat));
    } else {
        auto ename = ext < ext_names.size() ? ext_names[ext] : "?";
        printf("%s (+%s) : ", name, ename);
        print_battery(get_battery_bars(bat), crit);
        printf(" %2.0f%% %1.1fV",
               get_battery_percent(bat),
               get_battery_volts(bat));
    }
}

void
show_colors()
{
    int pos_x = 8;
    int pos_y = 25;

    // Regular palette.
    ansi::set_pos(pos_x, pos_y + 0);
    using ansi::color;
    for (auto c = color::black; c < color::light_black; c = color{static_cast<int>(c) + 1}) {
        ansi::set_bg(c);
        fputs("  ", stdout);
    }

    // Light palette.
    ansi::set_pos(pos_x, pos_y + 1);
    for (auto c = color::light_black; c < color::num_colors; c = color{static_cast<int>(c) + 1}) {
        ansi::set_bg(c);
        fputs("  ", stdout);
    }

    pos_x = 26;

    // Gray scale bar.
    ansi::set_pos(pos_x, pos_y + 0);
    ansi::set_bg(color::reset);
    fputs("[", stdout);
    for (int i = 0; i < 52; ++i) {
        ansi::set_bg(i * 5, i * 5, i * 5);
        fputs(" ", stdout);
    }
    ansi::set_bg(color::reset);
    fputs("]", stdout);

    // Red bar.
    ansi::set_pos(pos_x, pos_y + 1);
    ansi::set_bg(color::reset);
    fputs("[", stdout);
    for (int i = 0; i < 52; ++i) {
        ansi::set_bg(i * 5, 0, 0);
        fputs(" ", stdout);
    }
    ansi::set_bg(color::reset);
    fputs("]", stdout);

    // Green bar.
    ansi::set_pos(pos_x, pos_y + 2);
    ansi::set_bg(color::reset);
    fputs("[", stdout);
    for (int i = 0; i < 52; ++i) {
        ansi::set_bg(0, i * 5, 0);
        fputs(" ", stdout);
    }
    ansi::set_bg(color::reset);
    fputs("]", stdout);

    // Blue bar.
    ansi::set_pos(pos_x, pos_y + 3);
    ansi::set_bg(color::reset);
    fputs("[", stdout);
    for (int i = 0; i < 52; ++i) {
        ansi::set_bg(0, 0, i * 5);
        fputs(" ", stdout);
    }
    ansi::set_bg(color::reset);
    fputs("]", stdout);

    ansi::set_bg(color::reset);
}

//---------------------------------------------------------------------------------
int
main()
{
    disable_ahbprot();
    if (!AHBPROT_DISABLED)
        return -1;

    // Initialise the video system
    VIDEO_Init();

    // Obtain the preferred video mode from the system
    // This will correspond to the settings in the Wii menu
    rmode = VIDEO_GetPreferredMode(NULL);

    // Allocate memory for the display in the uncached region
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    // Set up the video registers with the chosen mode
    VIDEO_Configure(rmode);

    // Tell the video hardware where our display memory is
    VIDEO_SetNextFramebuffer(xfb);

    // Clear the framebuffer
    VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

    // Make the display visible
    VIDEO_SetBlack(false);

    // Flush the video register changes to the hardware
    VIDEO_Flush();

    // Wait for Video setup to complete
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    // Initialise the console, 80x30
    int con_pad_x = 0;
    int con_pad_y = 0;
    CON_Init(xfb,
             con_pad_x,
             con_pad_y,
             rmode->fbWidth - 2 * con_pad_x,
             rmode->xfbHeight - 2 * con_pad_y,
             rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    CON_EnableGecko(1, 0);

    int con_w, con_h;
    CON_GetMetrics(&con_w, &con_h);
    ansi::width = con_w;
    ansi::height = con_h;
    ansi::reset();
    ansi::enable_auto_newline();
    ansi::hide_cursor();
    ansi::clear_screen();
    ansi::clear_screen(3);

    u16 menu_ver = get_tmd_version(0x0000000100000002);

    const char* drive_date = get_odd_date();

    const char* wifi_mac = get_wifi_mac();

    u32 num_titles = 0;
    ES_GetNumTitles(&num_titles);

    u32 boot2_ver = 0;
    ES_GetBoot2Version(&boot2_ver);

    CONF_Init();

    u8 nickname[11] = "";
    CONF_GetNickName(nickname);

    load_settings();
    std::string serial_prefix = settings.at("CODE");
    std::string serial_number = settings.at("SERNO");
    std::string model = settings.at("MODEL");

    // Detect the console type.
    ConsoleType console_type = ConsoleType::Wii;
    if (model.starts_with("RVL-101"))
        console_type = ConsoleType::WiiFamily;
    if (model.starts_with("RVL-201"))
        console_type = ConsoleType::WiiMini;
    if (boot2_ver == 0)
        console_type = ConsoleType::WiiU;
    int dolphin_dev = IOS_Open("/dev/dolphin", 0);
    if (dolphin_dev >= 0) {
        console_type = ConsoleType::Dolphin;
        IOS_Close(dolphin_dev);
    }

    // Start printing.

    ansi::centered(3, "NiioFetch %s", VER);

    show_image(console_type);

    const int cur_x = 48;

    printf_xy(cur_x, 5, "Using IOS : %d", IOS_GetVersion());
    switch (console_type) {
        case ConsoleType::Wii:
        case ConsoleType::WiiFamily:
        case ConsoleType::WiiMini:
            printf_xy(cur_x, 6, "CPU : IBM PowerPC 750CL");
            break;

        case ConsoleType::WiiU:
            printf_xy(cur_x, 6, "CPU : IBM \"Espresso\"");
            break;

        case ConsoleType::Dolphin:
            printf_xy(cur_x, 6, "CPU : Emulated CPU");
            break;
    }
    printf_xy(cur_x, 7, "WiFi MAC : %s", wifi_mac);
    printf_xy(cur_x, 8, "System Menu : %.1f%c",
              GetSysMenuNintendoVersion(menu_ver),
              GetSysMenuRegion(menu_ver));
    printf_xy(cur_x, 9, "Boot2 : v%d", boot2_ver);
    printf_xy(cur_x, 10, "Drive Date : %s", drive_date);
    printf_xy(cur_x, 11, "Hollywood Revision : 0x%X", SYS_GetHollywoodRevision());
    printf_xy(cur_x, 12, "Resolution : %d%c",
              rmode->viHeight,
              VIDEO_GetVideoScanMode() ? 'p' : 'i');

    printf_xy(cur_x, 13, "Nickname : %s", nickname);
    printf_xy(cur_x, 14, "Wii Model : %s", model.data());
    printf_xy(cur_x, 15, "Serial : %s%s", serial_prefix.data(), serial_number.data());

    printf_xy(cur_x, 16, "Region : %s", regions.at(CONF_GetRegion()));
    printf_xy(cur_x, 17, "Language : %s", languages.at(CONF_GetLanguage()));

    printf_xy(cur_x, 18, "Titles installed : %d", num_titles);

    show_colors();

    SYS_SetPowerCallback(power_button_callback);

    PAD_Init();

    WPAD_Init();
    WPAD_SetIdleTimeout(120);

    bool running = true;
    unsigned frames = 0;
    while (running) {

        if ((frames % 20) == 0)
            for (int i = WPAD_CHAN_0; i <= WPAD_CHAN_3; ++i) {
                u32 ext;
                if (!WPAD_Probe(i, &ext)) {
                    switch (ext) {
                        case WPAD_EXP_NONE:
                        case WPAD_EXP_NUNCHUK:
                        case WPAD_EXP_CLASSIC:
                        case WPAD_EXP_GUITARHERO3:
                            WPAD_PadStatus(i);
                            break;
                        case WPAD_EXP_WIIBOARD:
                            // Battery level is in the data report already
                        default:
                            // Wii U Pro also has battery in data report, as bar levels.
                            ;
                    }
                }
            }

        PAD_ScanPads();
        WPAD_ScanPads();

        if ((frames % 20) == 0)
            for (int i = WPAD_CHAN_0; i <= WPAD_CHAN_3; ++i)
                show_wiimote(i);

        if (SYS_ResetButtonDown()) {
            ansi::centered(24, "RESET button pressed, exiting...");
            running = false;
        }

        if (power_button_pressed) {
            ansi::centered(24, "POWER button pressed, exiting...");
            running = false;
        }

        for (int i = WPAD_CHAN_0; i < WPAD_MAX_DEVICES; ++i) {
            if (WPAD_Probe(i, nullptr))
                continue;

            u32 pressed = WPAD_ButtonsDown(i);
            if (pressed & WPAD_BUTTON_HOME) {
                ansi::centered(24, "HOME button pressed, exiting...");
                running = false;
            }

            u32 held = WPAD_ButtonsHeld(i);
            if (held & WPAD_BUTTON_A) {
                WPAD_Rumble(i, 1);
            } else {
                WPAD_Rumble(i, 0);
            }
        }

        for (int i = PAD_CHAN0; i <= PAD_CHAN3; ++i) {
            u16 pressed = PAD_ButtonsDown(i);
            if (pressed & PAD_BUTTON_START) {
                ansi::centered(24, "START button pressed, exiting...");
                running = false;
            }
        }

        fflush(stdout);
        VIDEO_WaitVSync();

        ++frames;
    }

    // Note: libogc's console isn't ignoring the show_cursor code, so we set the color
    // to black, then revert it back to default.
    ansi::set_fg(ansi::color::black);
    ansi::show_cursor();
    ansi::reset();

    ansi::set_pos(1, 1);

#if 0
    ansi::clear_screen();
    ansi::clear_screen(3);
#endif

    WPAD_Shutdown();
}
