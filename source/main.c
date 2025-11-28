#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <di/di.h>
#include <gccore.h>
#include <ogc/machine/processor.h>
#include <ogc/system.h>
#include <wiiuse/wpad.h>

#include <png.h>

#include "ios.h"

#include "dolphin-logo_png.h"
#include "wii-logo_png.h"
#include "wii-mini-logo_png.h"
#include "wiiu-logo_png.h"

#define AHBPROT_DISABLED (*(vu32*)0xcd800064 == 0xFFFFFFFF)

DI_DriveID DI_id;

void *xfb = NULL;
GXRModeObj *rmode = NULL;

extern int __CONF_GetTxt(const char *name, char *buf, int length);

#define VER "1.3"

const char *languages[] = {
    "Japanese",
    "English",
    "German",
    "French",
    "Italian",
    "Dutch",
    "Chinese (Simplified)",
    "Chinese (Traditonal)",
    "Korean"
};

const char *regions[] = {
    "Japan",
    "USA",
    "Europe",
    "NULL",
    "Korea",
    "China"
};

enum consoletypes {
    WII,
    vWII,
    mWII,
    dolphin
};

u16 get_tmd_version(u64 title) { // From the homebrew channel
    STACK_ALIGN(u8, tmdbuf, 1024, 32);
    u32 tmd_view_size = 0;
    s32 res;

    res = ES_GetTMDViewSize(title, &tmd_view_size);

    if (res < 0) return 0;

    if (tmd_view_size > 1024) return 0;

    ES_GetTMDView(title, (tmd_view*)tmdbuf, tmd_view_size);

    if (res < 0) return 0;

    return (tmdbuf[88] << 8) | tmdbuf[89];
}

float GetSysMenuNintendoVersion(u32 sysVersion) { // From SysCheck
    float ninVersion = 0.0;

    switch (sysVersion) {
        case 33:
            ninVersion = 1.0f;
            break;

        case 97:
        case 128:
        case 130:
            ninVersion = 2.0f;
            break;

        case 162:
            ninVersion = 2.1f;
            break;

        case 192:
        case 193:
        case 194:
            ninVersion = 2.2f;
            break;

        case 224:
        case 225:
        case 226:
            ninVersion = 3.0f;
            break;

        case 256:
        case 257:
        case 258:
            ninVersion = 3.1f;
            break;

        case 288:
        case 289:
        case 290:
            ninVersion = 3.2f;
            break;

        case 352:
        case 353:
        case 354:
        case 326:
            ninVersion = 3.3f;
            break;

        case 384:
        case 385:
        case 386:
            ninVersion = 3.4f;
            break;

        case 390:
            ninVersion = 3.5f;
            break;

        case 416:
        case 417:
        case 418:
            ninVersion = 4.0f;
            break;

        case 448:
        case 449:
        case 450:
        case 454:
        case 54448:
        case 54449:
        case 54450:
        case 54454:
            ninVersion = 4.1f;
            break;

        case 480:
        case 481:
        case 482:
        case 486:
            ninVersion = 4.2f;
            break;

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
            ninVersion = 4.3f;
            break;
    }

    return ninVersion;
}

char GetSysMenuRegion(u32 sysVersion) { // From SysCheck
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
            break;

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
            break;

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
            break;

        case 326: //3.3K
        case 390: //3.5K
        case 454: //4.1K
        case 54454: // mauifrog 4.1K
        case 486: //4.2K
        case 518: //4.3K
            return 'K';
            break;
    }
    return 'X';
}

u32 RGB2YCBCR(u8 r1, u8 g1, u8 b1) {
    u8 r2 = r1; u8 g2 = g1; u8 b2 = b1;
    if (r1 < 16) r1 = 16;
    if (g1 < 16) g1 = 16;
    if (b1 < 16) b1 = 16;
    if (r2 < 16) r2 = 16;
    if (g2 < 16) g2 = 16;
    if (b2 < 16) b2 = 16;

    if (r1 > 240) r1 = 240;
    if (g1 > 240) g1 = 240;
    if (b1 > 240) b1 = 240;
    if (r2 > 240) r2 = 240;
    if (g2 > 240) g2 = 240;
    if (b2 > 240) b2 = 240;

    u8 Y1 = ( 77 * r1 + 150 * g1 + 29 * b1) / 256;
    u8 Y2 = ( 77 * r2 + 150 * g2 + 29 * b2) / 256;
    u8 Cb = (112 * (b1 + b2) -  74 * (g1 + g2) - 38 * (r1 + r2)) / 512 + 128;
    u8 Cr = (112 * (r1 + r2) - 94 * (g1 + g2) - 18 * (b1 + b2)) / 512 + 128;

    return Y1 << 24 | Cb << 16 | Y2 << 8 | Cr;
}

void writetoxfb(void* videoBuffer, u32 offset, u32 length, u32 color)
{
    u32 *pixels = ((u32*)videoBuffer) + offset;
    for (u32 i = 0; i < length; i++) {
        *pixels++ = color;
    }
}

u32 clamp_u32(u32 x, u32 low, u32 high)
{
    if (x < low)
        return low;
    if (x > high)
        return high;
    return x;
}

void convert_row(const u8* src_rgb, unsigned src_width, u32* dst_yuv422)
{
    for (unsigned x = 0; x < src_width; x += 2) {
        u32 r1 = clamp_u32(src_rgb[3*x + 0], 16, 240);
        u32 g1 = clamp_u32(src_rgb[3*x + 1], 16, 240);
        u32 b1 = clamp_u32(src_rgb[3*x + 2], 16, 240);

        u32 r2 = 16, g2 = 16, b2 = 16;
        if (x + 1 < src_width) {
            r2 = clamp_u32(src_rgb[3*x + 3], 16, 240);
            g2 = clamp_u32(src_rgb[3*x + 4], 16, 240);
            b2 = clamp_u32(src_rgb[3*x + 5], 16, 240);
        }

        u32 Y1 = (( 77u * r1 + 150u * g1 + 29u * b1) / 256u) & 0xffu;
        u32 Y2 = (( 77u * r2 + 150u * g2 + 29u * b2) / 256u) & 0xffu;
        u32 Cb = ((112u * (b1 + b2) - 74u * (g1 + g2) - 38u * (r1 + r2)) / 512u + 128u) & 0xffu;
        u32 Cr = ((112u * (r1 + r2) - 94u * (g1 + g2) - 18u * (b1 + b2)) / 512u + 128u) & 0xffu;

        dst_yuv422[x/2] = (Y1 << 24u) | (Cb << 16u) | (Y2 << 8u) | Cr;
    }
}

void blit_png(const u8* data, size_t size)
{
    const unsigned screen_x = 10;
    const unsigned screen_y = 80;

    int r;
    u8* pixels = NULL;
    png_image img;
    unsigned max_width = rmode->fbWidth - screen_x;
    unsigned max_height = rmode->xfbHeight - screen_y;
    memset(&img, 0, sizeof img);
    img.version = PNG_IMAGE_VERSION;
    r = png_image_begin_read_from_memory(&img, data, size);
    if (!r) {
        png_image_free(&img);
        return;
    }
    img.format = PNG_FORMAT_RGB;
    pixels = calloc(img.width * img.height, 3);
    png_image_finish_read(&img, NULL, pixels, 3 * img.width, NULL);

    if (img.height < max_height)
        max_height = img.height;
    if (img.width < max_width)
        max_width = img.width;
    for (unsigned y = 0; y < max_height; ++y) {
        const u8* src_row = pixels + 3 * (y * img.width);
        u32* dst_row = ((u32*)xfb) + ((y + screen_y) * rmode->fbWidth + screen_x) / 2;
        convert_row(src_row, max_width, dst_row);
    }

    free(pixels);
    png_image_free(&img);
}

void printlogo(u8 dev) {
    switch (dev) {
        case WII:
            blit_png(wii_logo_png, wii_logo_png_size);
            break;

        case mWII:
            blit_png(wii_mini_logo_png, wii_mini_logo_png_size);
            break;

        case vWII:
            blit_png(wiiu_logo_png, wiiu_logo_png_size);
            break;

        case dolphin:
            blit_png(dolphin_logo_png, dolphin_logo_png_size);
            break;

        default:
            break;
    }
}

atomic_bool power_pressed;

static void power_callback(void)
{
    atomic_store(&power_pressed, true);
}

__attribute__(( __format__(__printf__, 3, 4) ))
int printf_xy(int x, int y, const char* fmt, ...)
{
    int r1, r2;
    r1 = printf("\x1b[%d;%dH", y, x);
    if (r1 < 0)
        return r1;
    va_list args;
    va_start(args, fmt);
    r2 = vprintf(fmt, args);
    va_end(args);
    if (r2 < 0)
        return r2;
    return r1 + r2;
}

//---------------------------------------------------------------------------------
int main(void) {
    //---------------------------------------------------------------------------------

    // Initialise the video system
    VIDEO_Init();

    // This function initialises the attached controllers
    WPAD_Init();

    bool ahbprot = disable_ahbprot();

    if (!AHBPROT_DISABLED)
        return 0;

    CONF_Init();

    u16 SMVER = get_tmd_version(0x0000000100000002);

    u8 consoletype = WII;

    s32 test = IOS_Open("/dev/dolphin", 0);

    if (test >= 0) {
        consoletype = dolphin;
    }

    IOS_Close(test);

    u8 nickname[11];
    char drivedate[15];
    char sernumber[11];
    char sernumberprefix[4];
    char model[14];
    u32 boot2ver = 0;
    u32 numoftitles = 0;
    if (ahbprot) { // A wise man once told me that AHBPROT should be absent for homebrew to prosper
        DI_Init();
        if (!DI_Identify(&DI_id)) {
            uint32_t y = (DI_id.rel_date >> 16) & 0xffff;
            uint32_t m = (DI_id.rel_date >>  8) & 0x00ff;
            uint32_t d = (DI_id.rel_date >>  0) & 0x00ff;
            snprintf(drivedate, sizeof drivedate, "%04X/%02X/%02X", y, m, d);
        }
        DI_Close();
    }

    ES_GetNumTitles(&numoftitles);
    ES_GetBoot2Version(&boot2ver);

    if (boot2ver == 0) {
        consoletype = vWII;
    }

    CONF_GetNickName(nickname);
    __CONF_GetTxt("CODE", sernumberprefix, 4);
    __CONF_GetTxt("SERNO", sernumber, 10);
    __CONF_GetTxt("MODEL", model, 13);

    // Obtain the preferred video mode from the system
    // This will correspond to the settings in the Wii menu
    rmode = VIDEO_GetPreferredMode(NULL);

    // Allocate memory for the display in the uncached region
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    // Initialise the console, required for printf
    console_init(xfb,
                 16, 16,
                 rmode->fbWidth - 16,
                 rmode->xfbHeight - 16,
                 rmode->fbWidth * VI_DISPLAY_PIX_SZ);
    //SYS_STDIO_Report(true);

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
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    printf_xy(31, 3, "NiioFetch %s", VER);

    printlogo(consoletype);

    const int cur_x = 48;

    printf_xy(cur_x, 5, "Running on IOS : %d", IOS_GetVersion());
    switch (consoletype) {
        case WII:
        case mWII:
            printf_xy(cur_x, 6, "CPU : IBM PowerPC 750CL");
            break;

        case vWII:
            printf_xy(cur_x, 6, "CPU : IBM \"Espresso\"");
            break;

        case dolphin:
            printf_xy(cur_x, 6, "CPU : Emulated CPU");
            break;

        default:
            break;
    }
    s32 __net_hid = iosCreateHeap(1024);
    u8 *buff = iosAlloc(__net_hid, 6);

    s32 fd = IOS_Open("/dev/net/wd/command", 3);
    IOS_IoctlvFormat(__net_hid, fd, 0x100e, ":d", buff, 6);
    printf_xy(cur_x, 7, "WiFi MAC : %02X-%02X-%02X-%02X-%02X-%02X",
              buff[0], buff[1], buff[2], buff[3], buff[4], buff[5]);
    IOS_Close(fd);
    iosFree(__net_hid, buff);

    printf_xy(cur_x, 8, "System Menu : %.1f%c", GetSysMenuNintendoVersion(SMVER), GetSysMenuRegion(SMVER));
    printf_xy(cur_x, 9, "Boot2 : v%d", boot2ver);
    printf_xy(cur_x, 10, "Drive Date : %s", drivedate);
    printf_xy(cur_x, 11, "Hollywood Revision : 0x%X", SYS_GetHollywoodRevision());
    printf_xy(cur_x, 12, "Resolution : %d%c", rmode->viHeight, VIDEO_GetVideoScanMode() ? 'p' : 'i');

    printf_xy(cur_x, 13, "Nickname : %s", nickname);
    printf_xy(cur_x, 14, "Wii Model : %s", model);
    printf_xy(cur_x, 15, "Serial : %s%s", sernumberprefix, sernumber);

    printf_xy(cur_x, 16, "Region : %s", regions[CONF_GetRegion()]);
    printf_xy(cur_x, 17, "Language : %s", languages[CONF_GetLanguage()]);

    printf_xy(cur_x, 18, "Titles installed  : %d", numoftitles);

    printf_xy(cur_x, 19, "P1 Battery Level : %d%%", WPAD_BatteryLevel(0));

    fflush(stdout);

    for (int i = 400; i < 416; i++) {
        writetoxfb(xfb, 160 + i*320, 12, COLOR_BLACK);
        writetoxfb(xfb, 160 + 12 + i*320, 12,  RGB2YCBCR(192, 0, 0));
        writetoxfb(xfb, 160 + 24 + i*320, 12,  RGB2YCBCR(0, 200, 0));
        writetoxfb(xfb, 160 + 36 + i*320, 12,  RGB2YCBCR(200, 200, 16));
        writetoxfb(xfb, 160 + 48 + i*320, 12,  RGB2YCBCR(16, 32, 192));
        writetoxfb(xfb, 160 + 60 + i*320, 12,  RGB2YCBCR(160, 16, 240));
        writetoxfb(xfb, 160 + 72 + i*320, 12,  RGB2YCBCR(16, 200, 200));
        writetoxfb(xfb, 160 + 84 + i*320, 12,  RGB2YCBCR(190, 190, 190));
    }
    for (int i = 416; i < 432; i++) {
        writetoxfb(xfb, 160 + i*320, 12, RGB2YCBCR(64, 64, 64));
        writetoxfb(xfb, 160 + 12 + i*320, 12,  RGB2YCBCR(255, 0, 0));
        writetoxfb(xfb, 160 + 24 + i*320, 12,  RGB2YCBCR(0, 255, 0));
        writetoxfb(xfb, 160 + 36 + i*320, 12,  RGB2YCBCR(255, 255, 64));
        writetoxfb(xfb, 160 + 48 + i*320, 12,  RGB2YCBCR(16, 64, 255));
        writetoxfb(xfb, 160 + 60 + i*320, 12,  RGB2YCBCR(180, 52, 240));
        writetoxfb(xfb, 160 + 72 + i*320, 12,  RGB2YCBCR(64, 240, 240));
        writetoxfb(xfb, 160 + 84 + i*320, 12,  COLOR_WHITE);
    }

    SYS_SetPowerCallback(power_callback);

    bool running = true;
    while (running) {
        printf_xy(cur_x, 19, "P1 Battery Level : %d%%    ", WPAD_BatteryLevel(0));
        fflush(stdout);

        if (SYS_ResetButtonDown()) {
            printf_xy(24, 22, "RESET pressed, exiting...");
            fflush(stdout);
            running = false;
        }

        if (atomic_load(&power_pressed)) {
            printf_xy(24, 22, "POWER pressed, exiting...");
            fflush(stdout);
            running = false;
        }

        WPAD_ScanPads();
        u32 pressed = WPAD_ButtonsDown(0);

        if (pressed & WPAD_BUTTON_HOME) {
            printf_xy(24, 22, "HOME pressed, exiting...");
            fflush(stdout);
            running = false;
        }

        VIDEO_WaitVSync();
    }
}
