#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

#define TITLE_ID "010018100CD46000"
#define PATCH_ROOT "sdmc:/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive"

typedef struct {
    const char *name;
    const char *src;
    const char *dst;
} PatchFile;

static const PatchFile g_files[] = {
    {"CoreResource.arc",     "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/CoreResource.arc",     PATCH_ROOT "/CoreResource.arc"},
    {"GuiTextResource.arc",  "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/GuiTextResource.arc",  PATCH_ROOT "/GuiTextResource.arc"},
    {"Msg2Resource_e.arc",   "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/Msg2Resource_e.arc",   PATCH_ROOT "/Msg2Resource_e.arc"},
    {"NXStrapResource.arc",   "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/NXStrapResource.arc",   PATCH_ROOT "/NXStrapResource.arc"},
    {"MANUAL CrashFix IPS",   "romfs:/payload/atmosphere/exefs_patches/RE5_Manual_CrashFix/C517ECBB79DE97338E98147A6B5B5F2B.ips", "sdmc:/atmosphere/exefs_patches/RE5_Manual_CrashFix/C517ECBB79DE97338E98147A6B5B5F2B.ips"},
};

static int mkdir_p(const char *path) {
    char tmp[768];
    size_t len = strnlen(path, sizeof(tmp) - 1);
    if (len == 0 || len >= sizeof(tmp) - 1) return -1;
    memcpy(tmp, path, len + 1);

    for (char *p = tmp + 6; *p; ++p) { // skip "sdmc:/"
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0777) != 0 && errno != EEXIST) return -2;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0777) != 0 && errno != EEXIST) return -3;
    return 0;
}

static int ensure_parent(const char *file_path) {
    char parent[768];
    size_t len = strnlen(file_path, sizeof(parent) - 1);
    if (len == 0 || len >= sizeof(parent) - 1) return -1;
    memcpy(parent, file_path, len + 1);
    char *slash = strrchr(parent, '/');
    if (!slash) return -2;
    *slash = 0;
    return mkdir_p(parent);
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -10;
    if (ensure_parent(dst) != 0) { fclose(in); return -11; }

    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -12; }

    static unsigned char buf[256 * 1024];
    int rc = 0;
    for (;;) {
        size_t n = fread(buf, 1, sizeof(buf), in);
        if (n > 0 && fwrite(buf, 1, n, out) != n) { rc = -13; break; }
        if (n < sizeof(buf)) {
            if (ferror(in)) rc = -14;
            break;
        }
    }
    if (fflush(out) != 0 && rc == 0) rc = -15;
    fclose(out);
    fclose(in);
    return rc;
}

static long file_size(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 ? (long)st.st_size : -1;
}

static int verify_one(const PatchFile *pf) {
    long a = file_size(pf->src);
    long b = file_size(pf->dst);
    return (a >= 0 && b == a) ? 0 : -1;
}


#define FB_WIDTH 1280
#define FB_HEIGHT 720
#define GALLERY_PAGE_COUNT 8
#define GALLERY_FRAME_BYTES (FB_WIDTH * FB_HEIGHT * 2)

static const char *g_gallery_pages[GALLERY_PAGE_COUNT] = {
    "romfs:/gallery/page1.rgb565",
    "romfs:/gallery/page2.rgb565",
    "romfs:/gallery/page3.rgb565",
    "romfs:/gallery/page4.rgb565",
    "romfs:/gallery/page5.rgb565",
    "romfs:/gallery/page6.rgb565",
    "romfs:/gallery/page7.rgb565",
    "romfs:/gallery/page8.rgb565"
};

static u64 gallery_rng_next(u64 *state) {
    u64 x = *state;
    if (x == 0) x = 0x9E3779B97F4A7C15ULL;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static void gallery_shuffle(int order[GALLERY_PAGE_COUNT]) {
    for (int i = 0; i < GALLERY_PAGE_COUNT; ++i) order[i] = i;

    u64 seed = armGetSystemTick();
    seed ^= ((u64)(uintptr_t)order << 17);
    for (int i = GALLERY_PAGE_COUNT - 1; i > 0; --i) {
        int j = (int)(gallery_rng_next(&seed) % (u64)(i + 1));
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
}

static int load_gallery_page(int page_index, void *dst) {
    if (page_index < 0 || page_index >= GALLERY_PAGE_COUNT || !dst) return -1;
    FILE *f = fopen(g_gallery_pages[page_index], "rb");
    if (!f) return -2;
    size_t got = fread(dst, 1, GALLERY_FRAME_BYTES, f);
    int err = ferror(f);
    fclose(f);
    if (err || got != GALLERY_FRAME_BYTES) return -3;
    return 0;
}

/*
 * Launch gallery.
 * Images are shuffled once on each launch.
 * L: previous image (wrap)
 * R: next image (wrap)
 * A: continue to installer menu
 * +: exit application
 */

#define GALLERY_CONTROL_HEIGHT 54
#define GALLERY_CONTROL_BYTES (FB_WIDTH * GALLERY_CONTROL_HEIGHT * 2)

static u16 g_gallery_controls[FB_WIDTH * GALLERY_CONTROL_HEIGHT];

static void gallery_load_korean_controls(void) {
    memset(g_gallery_controls, 0, GALLERY_CONTROL_BYTES);
    FILE *f = fopen("romfs:/gallery/controls_ko.rgb565", "rb");
    if (!f) return;
    fread(g_gallery_controls, 1, GALLERY_CONTROL_BYTES, f);
    fclose(f);
}

static bool show_random_gallery(PadState *pad) {
    Framebuffer fb;
    Result rc = framebufferCreate(&fb, nwindowGetDefault(),
                                  FB_WIDTH, FB_HEIGHT,
                                  PIXEL_FORMAT_RGB_565, 2);
    if (R_FAILED(rc)) return true;

    rc = framebufferMakeLinear(&fb);
    if (R_FAILED(rc)) {
        framebufferClose(&fb);
        return true;
    }

    u16 *pagebuf = (u16*)malloc(GALLERY_FRAME_BYTES);
    if (!pagebuf) {
        framebufferClose(&fb);
        return true;
    }

    gallery_load_korean_controls();

    int order[GALLERY_PAGE_COUNT];
    gallery_shuffle(order);

    int pos = 0;
    int loaded_pos = -1;
    bool continue_to_installer = true;

    while (appletMainLoop()) {
        padUpdate(pad);
        u64 down = padGetButtonsDown(pad);

        if (down & HidNpadButton_Plus) {
            continue_to_installer = false;
            break;
        }
        if (down & HidNpadButton_A) break;

        if (down & HidNpadButton_L) {
            pos = (pos + GALLERY_PAGE_COUNT - 1) % GALLERY_PAGE_COUNT;
        }
        if (down & HidNpadButton_R) {
            pos = (pos + 1) % GALLERY_PAGE_COUNT;
        }

        if (loaded_pos != pos) {
            if (load_gallery_page(order[pos], pagebuf) != 0) break;
            loaded_pos = pos;
        }

        u32 stride = 0;
        u8 *fbptr = (u8*)framebufferBegin(&fb, &stride);

        // Clear the whole framebuffer to black.
        for (u32 y = 0; y < FB_HEIGHT; ++y) {
            memset(fbptr + y * stride, 0, FB_WIDTH * 2);
        }

        // Draw the Korean control strip in its own 54px area.
        memcpy(fbptr, g_gallery_controls, GALLERY_CONTROL_BYTES);

        // Fit the complete 1280x720 image below the control strip.
        // 1280x720 -> 1184x666 preserves the 16:9 aspect ratio.
        const int dst_w = 1184;
        const int dst_h = 666;
        const int dst_x = (FB_WIDTH - dst_w) / 2;
        const int dst_y = GALLERY_CONTROL_HEIGHT;

        for (int y = 0; y < dst_h; ++y) {
            int sy = (y * FB_HEIGHT) / dst_h;
            u16 *dstrow = (u16*)(fbptr + (dst_y + y) * stride) + dst_x;
            const u16 *srcrow = pagebuf + sy * FB_WIDTH;
            for (int x = 0; x < dst_w; ++x) {
                int sx = (x * FB_WIDTH) / dst_w;
                dstrow[x] = srcrow[sx];
            }
        }
        framebufferEnd(&fb);
    }

    free(pagebuf);
    framebufferClose(&fb);
    return continue_to_installer;
}


static void install_patch(void) {
    printf("\n[INSTALL] Korean patch -> Atmosphere LayeredFS\n");
    int failures = 0;
    for (unsigned i = 0; i < sizeof(g_files)/sizeof(g_files[0]); ++i) {
        int rc = copy_file(g_files[i].src, g_files[i].dst);
        if (rc == 0 && verify_one(&g_files[i]) == 0)
            printf("  OK   %s\n", g_files[i].name);
        else {
            printf("  FAIL %s (rc=%d)\n", g_files[i].name, rc);
            failures++;
        }
    }
    printf(failures ? "Result: %d file(s) failed.\n" : "Result: install + size verification OK.\n", failures);
    printf("Reboot or restart the game after exiting.\n");
}

static void verify_patch(void) {
    printf("\n[VERIFY]\n");
    int failures = 0;
    for (unsigned i = 0; i < sizeof(g_files)/sizeof(g_files[0]); ++i) {
        long src = file_size(g_files[i].src);
        long dst = file_size(g_files[i].dst);
        bool ok = src >= 0 && src == dst;
        printf("  %s %s (%ld / %ld bytes)\n", ok ? "OK  " : "MISS", g_files[i].name, dst, src);
        if (!ok) failures++;
    }
    printf(failures ? "Result: incomplete.\n" : "Result: all patch files are present.\n");
}

static void remove_patch(void) {
    printf("\n[REMOVE]\n");
    for (unsigned i = 0; i < sizeof(g_files)/sizeof(g_files[0]); ++i) {
        if (remove(g_files[i].dst) == 0)
            printf("  DEL  %s\n", g_files[i].name);
        else if (errno == ENOENT)
            printf("  SKIP %s\n", g_files[i].name);
        else
            printf("  FAIL %s (errno=%d)\n", g_files[i].name, errno);
    }
    printf("Only validated Korean patch files and the MANUAL crash fix were targeted.\n");
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    consoleInit(NULL);

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    // libnx already initializes FS and mounts sdmc before main().
    // Do not mount "sdmc" a second time here.
    Result rc = romfsInit();
    if (R_FAILED(rc)) {
        printf("Embedded payload mount failed: 0x%08X\n", rc);
        printf("Press + to exit.\n");
        consoleUpdate(NULL);
        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
            consoleUpdate(NULL);
        }
        consoleExit(NULL);
        return 1;
    }

    // The text console owns the default window framebuffer.
    // Release it before creating the full-screen gallery framebuffer.
    consoleExit(NULL);

    if (!show_random_gallery(&pad)) {
        romfsExit();
        return 0;
    }

    // Recreate the text console after leaving the full-screen gallery.
    consoleInit(NULL);
    printf("RE5 Korean Patch Installer v15\n");
    printf("Target: Resident Evil 5 / %s\n\n", TITLE_ID);
    printf("A  Install / overwrite Korean patch\n");
    printf("X  Verify installed files\n");
    printf("Y  Remove Korean patch files\n");
    printf("+  Exit\n");
    consoleUpdate(NULL);

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & HidNpadButton_Plus) break;
        if (kDown & HidNpadButton_A) install_patch();
        if (kDown & HidNpadButton_X) verify_patch();
        if (kDown & HidNpadButton_Y) remove_patch();
        consoleUpdate(NULL);
    }

    romfsExit();
    consoleExit(NULL);
    return 0;
}
