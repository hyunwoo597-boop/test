#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

#define TITLE_ID "010018100CD46000"
#define PATCH_ROOT "sdmc:/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive"
#define MOD_TARGET "sdmc:/atmosphere/contents/" TITLE_ID "/exefs/main"

#define FB_WIDTH 1280
#define FB_HEIGHT 720
#define FRAME_BYTES (FB_WIDTH * FB_HEIGHT * 2)
#define GALLERY_PAGE_COUNT 8
#define GALLERY_CONTROL_HEIGHT 54
#define GALLERY_CONTROL_BYTES (FB_WIDTH * GALLERY_CONTROL_HEIGHT * 2)

typedef struct {
    const char *name;
    const char *src;
    const char *dst;
} PatchFile;

typedef struct {
    const char *name;
    const char *src_plain;
    const char *src_unlock;
} CharacterMod;

static const PatchFile g_files[] = {
    {"CoreResource.arc",     "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/CoreResource.arc",     PATCH_ROOT "/CoreResource.arc"},
    {"GuiTextResource.arc",  "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/GuiTextResource.arc",  PATCH_ROOT "/GuiTextResource.arc"},
    {"Msg2Resource_e.arc",   "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/Msg2Resource_e.arc",   PATCH_ROOT "/Msg2Resource_e.arc"},
    {"NXStrapResource.arc",  "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/NXStrapResource.arc",   PATCH_ROOT "/NXStrapResource.arc"},
    {"MANUAL CrashFix IPS",  "romfs:/payload/atmosphere/exefs_patches/RE5_Manual_CrashFix/C517ECBB79DE97338E98147A6B5B5F2B.ips", "sdmc:/atmosphere/exefs_patches/RE5_Manual_CrashFix/C517ECBB79DE97338E98147A6B5B5F2B.ips"},
};

static const CharacterMod g_mods[] = {
    {"엑셀라",       "romfs:/payload/mods/excella/main",   "romfs:/payload/mods_unlock/excella/main"},
    {"레베카",       "romfs:/payload/mods/rebecca/main",   "romfs:/payload/mods_unlock/rebecca/main"},
    {"질 코스튬 1",  "romfs:/payload/mods/jill_cos1/main", "romfs:/payload/mods_unlock/jill_cos1/main"},
    {"질 코스튬 2",  "romfs:/payload/mods/jill_cos2/main", "romfs:/payload/mods_unlock/jill_cos2/main"},
};

static const char *g_unlock_only = "romfs:/payload/unlock/main";

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

static const char *g_main_screens[7] = {
    "romfs:/ui/main_0.rgb565",
    "romfs:/ui/main_1.rgb565",
    "romfs:/ui/main_2.rgb565",
    "romfs:/ui/main_3.rgb565",
    "romfs:/ui/main_4.rgb565",
    "romfs:/ui/main_5.rgb565",
    "romfs:/ui/main_6.rgb565",
};

static const char *g_unlock_screens[2] = {
    "romfs:/ui/main_2_off.rgb565",
    "romfs:/ui/main_2_on.rgb565",
};

static const char *g_char_screens[5][2] = {
    {"romfs:/ui/char_0_off.rgb565", "romfs:/ui/char_0_on.rgb565"},
    {"romfs:/ui/char_1_off.rgb565", "romfs:/ui/char_1_on.rgb565"},
    {"romfs:/ui/char_2_off.rgb565", "romfs:/ui/char_2_on.rgb565"},
    {"romfs:/ui/char_3_off.rgb565", "romfs:/ui/char_3_on.rgb565"},
    {"romfs:/ui/char_4_off.rgb565", "romfs:/ui/char_4_on.rgb565"},
};

static u16 g_gallery_controls[FB_WIDTH * GALLERY_CONTROL_HEIGHT];

static int mkdir_p(const char *path) {
    char tmp[768];
    size_t len = strlen(path);
    if (len == 0 || len >= sizeof(tmp) - 1) return -1;
    memcpy(tmp, path, len + 1);

    for (char *p = tmp + 6; *p; ++p) {
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
    size_t len = strlen(file_path);
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

static bool same_size(const char *a, const char *b) {
    long sa = file_size(a);
    long sb = file_size(b);
    return sa >= 0 && sb == sa;
}

static bool install_patch_action(void) {
    for (unsigned i = 0; i < sizeof(g_files)/sizeof(g_files[0]); ++i) {
        if (copy_file(g_files[i].src, g_files[i].dst) != 0) return false;
        if (!same_size(g_files[i].src, g_files[i].dst)) return false;
    }
    return true;
}

static bool verify_patch_action(void) {
    for (unsigned i = 0; i < sizeof(g_files)/sizeof(g_files[0]); ++i) {
        if (!same_size(g_files[i].src, g_files[i].dst)) return false;
    }
    return true;
}

static bool remove_patch_action(void) {
    bool ok = true;
    for (unsigned i = 0; i < sizeof(g_files)/sizeof(g_files[0]); ++i) {
        if (remove(g_files[i].dst) != 0 && errno != ENOENT) ok = false;
    }
    return ok;
}

static bool install_selection(int index, bool unlock_enabled) {
    // index 0 = no character mod (default Sheva)
    if (index == 0) {
        if (!unlock_enabled) {
            if (remove(MOD_TARGET) == 0) return true;
            return errno == ENOENT;
        }
        if (copy_file(g_unlock_only, MOD_TARGET) != 0) return false;
        return same_size(g_unlock_only, MOD_TARGET);
    }

    int mod_index = index - 1;
    if (mod_index < 0 || mod_index >= (int)(sizeof(g_mods)/sizeof(g_mods[0]))) return false;

    const char *src = unlock_enabled ? g_mods[mod_index].src_unlock : g_mods[mod_index].src_plain;
    if (copy_file(src, MOD_TARGET) != 0) return false;
    return same_size(src, MOD_TARGET);
}

static bool verify_selection(int index, bool unlock_enabled) {
    if (index == 0) {
        if (!unlock_enabled) return file_size(MOD_TARGET) < 0;
        return same_size(g_unlock_only, MOD_TARGET);
    }

    int mod_index = index - 1;
    if (mod_index < 0 || mod_index >= (int)(sizeof(g_mods)/sizeof(g_mods[0]))) return false;
    const char *src = unlock_enabled ? g_mods[mod_index].src_unlock : g_mods[mod_index].src_plain;
    return same_size(src, MOD_TARGET);
}

static bool remove_character_mod(void) {
    if (remove(MOD_TARGET) == 0) return true;
    return errno == ENOENT;
}

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

static int load_frame(const char *path, void *dst) {
    if (!path || !dst) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -2;
    size_t got = fread(dst, 1, FRAME_BYTES, f);
    int err = ferror(f);
    fclose(f);
    return (!err && got == FRAME_BYTES) ? 0 : -3;
}

static int load_gallery_page(int page_index, void *dst) {
    if (page_index < 0 || page_index >= GALLERY_PAGE_COUNT) return -1;
    return load_frame(g_gallery_pages[page_index], dst);
}

static void gallery_load_korean_controls(void) {
    memset(g_gallery_controls, 0, GALLERY_CONTROL_BYTES);
    FILE *f = fopen("romfs:/gallery/controls_ko.rgb565", "rb");
    if (!f) return;
    fread(g_gallery_controls, 1, GALLERY_CONTROL_BYTES, f);
    fclose(f);
}

static void present_fullscreen(Framebuffer *fb, const u16 *frame) {
    u32 stride = 0;
    u8 *fbptr = (u8*)framebufferBegin(fb, &stride);
    for (u32 y = 0; y < FB_HEIGHT; ++y) {
        memcpy(fbptr + y * stride, frame + y * FB_WIDTH, FB_WIDTH * 2);
    }
    framebufferEnd(fb);
}

static bool show_random_gallery(PadState *pad) {
    Framebuffer fb;
    Result rc = framebufferCreate(&fb, nwindowGetDefault(), FB_WIDTH, FB_HEIGHT,
                                  PIXEL_FORMAT_RGB_565, 2);
    if (R_FAILED(rc)) return true;
    rc = framebufferMakeLinear(&fb);
    if (R_FAILED(rc)) { framebufferClose(&fb); return true; }

    u16 *pagebuf = (u16*)malloc(FRAME_BYTES);
    if (!pagebuf) { framebufferClose(&fb); return true; }

    gallery_load_korean_controls();
    int order[GALLERY_PAGE_COUNT];
    gallery_shuffle(order);
    int pos = 0;
    int loaded_pos = -1;
    bool continue_to_installer = true;

    while (appletMainLoop()) {
        padUpdate(pad);
        u64 down = padGetButtonsDown(pad);

        if (down & HidNpadButton_Plus) { continue_to_installer = false; break; }
        if (down & HidNpadButton_A) break;
        if (down & HidNpadButton_L) pos = (pos + GALLERY_PAGE_COUNT - 1) % GALLERY_PAGE_COUNT;
        if (down & HidNpadButton_R) pos = (pos + 1) % GALLERY_PAGE_COUNT;

        if (loaded_pos != pos) {
            if (load_gallery_page(order[pos], pagebuf) != 0) break;
            loaded_pos = pos;
        }

        u32 stride = 0;
        u8 *fbptr = (u8*)framebufferBegin(&fb, &stride);
        for (u32 y = 0; y < FB_HEIGHT; ++y) memset(fbptr + y * stride, 0, FB_WIDTH * 2);
        memcpy(fbptr, g_gallery_controls, GALLERY_CONTROL_BYTES);

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

static void show_status_screen(Framebuffer *fb, PadState *pad, u16 *screenbuf, bool ok) {
    const char *path = ok ? "romfs:/ui/status_ok.rgb565" : "romfs:/ui/status_fail.rgb565";
    if (load_frame(path, screenbuf) == 0) present_fullscreen(fb, screenbuf);

    while (appletMainLoop()) {
        padUpdate(pad);
        u64 down = padGetButtonsDown(pad);
        if (down & (HidNpadButton_A | HidNpadButton_B | HidNpadButton_Plus)) break;
    }
}

static void show_info_screen(Framebuffer *fb, PadState *pad, u16 *screenbuf) {
    if (load_frame("romfs:/ui/info.rgb565", screenbuf) == 0) present_fullscreen(fb, screenbuf);
    while (appletMainLoop()) {
        padUpdate(pad);
        u64 down = padGetButtonsDown(pad);
        if (down & (HidNpadButton_B | HidNpadButton_A | HidNpadButton_Plus)) break;
    }
}

static void show_installer_ui(PadState *pad) {
    Framebuffer fb;
    Result rc = framebufferCreate(&fb, nwindowGetDefault(), FB_WIDTH, FB_HEIGHT,
                                  PIXEL_FORMAT_RGB_565, 2);
    if (R_FAILED(rc)) return;
    rc = framebufferMakeLinear(&fb);
    if (R_FAILED(rc)) { framebufferClose(&fb); return; }

    u16 *screenbuf = (u16*)malloc(FRAME_BYTES);
    if (!screenbuf) { framebufferClose(&fb); return; }

    enum { MODE_MAIN, MODE_CHARACTER } mode = MODE_MAIN;
    int main_sel = 1;
    int char_sel = 0;          // 0=기본 셰바, 1=엑셀라, 2=레베카, 3=질1, 4=질2
    bool unlock_enabled = true;
    bool dirty = true;
    bool quit = false;

    while (appletMainLoop() && !quit) {
        padUpdate(pad);
        u64 down = padGetButtonsDown(pad);

        if (down & HidNpadButton_Plus) break;

        if (mode == MODE_MAIN) {
            if (down & HidNpadButton_Up)   { main_sel = (main_sel + 6) % 7; dirty = true; }
            if (down & HidNpadButton_Down) { main_sel = (main_sel + 1) % 7; dirty = true; }

            if (down & HidNpadButton_A) {
                bool ok = true;
                switch (main_sel) {
                    case 0: // 한글패치 설치
                        ok = install_patch_action();
                        show_status_screen(&fb, pad, screenbuf, ok);
                        dirty = true;
                        break;

                    case 1: // 캐릭터 모드
                        mode = MODE_CHARACTER;
                        dirty = true;
                        break;

                    case 2: // 코스튬 + 추가 스토리 언락 옵션
                        unlock_enabled = !unlock_enabled;
                        dirty = true;
                        break;

                    case 3: // 설치 상태 확인
                        ok = verify_patch_action() && verify_selection(char_sel, unlock_enabled);
                        show_status_screen(&fb, pad, screenbuf, ok);
                        dirty = true;
                        break;

                    case 4: // 모드 제거 / 원상복구
                        ok = remove_patch_action();
                        if (!remove_character_mod()) ok = false;
                        show_status_screen(&fb, pad, screenbuf, ok);
                        dirty = true;
                        break;

                    case 5: // 설정 / 기타
                        show_info_screen(&fb, pad, screenbuf);
                        dirty = true;
                        break;

                    case 6: // 종료
                        quit = true;
                        break;
                }
            }
        } else {
            if (down & HidNpadButton_B) {
                mode = MODE_MAIN;
                main_sel = 1;
                dirty = true;
            }

            if (down & HidNpadButton_Up) {
                char_sel = (char_sel + 4) % 5;
                dirty = true;
            }

            if (down & HidNpadButton_Down) {
                char_sel = (char_sel + 1) % 5;
                dirty = true;
            }

            if (down & HidNpadButton_Y) {
                unlock_enabled = !unlock_enabled;
                dirty = true;
            }

            if (down & HidNpadButton_A) {
                bool ok = install_selection(char_sel, unlock_enabled);
                show_status_screen(&fb, pad, screenbuf, ok);
                dirty = true;
            }

            if (down & HidNpadButton_X) {
                bool ok = verify_selection(char_sel, unlock_enabled);
                show_status_screen(&fb, pad, screenbuf, ok);
                dirty = true;
            }
        }

        if (dirty && !quit) {
            const char *path = NULL;

            if (mode == MODE_MAIN) {
                if (main_sel == 2) path = g_unlock_screens[unlock_enabled ? 1 : 0];
                else path = g_main_screens[main_sel];
            } else {
                path = g_char_screens[char_sel][unlock_enabled ? 1 : 0];
            }

            if (load_frame(path, screenbuf) == 0) present_fullscreen(&fb, screenbuf);
            dirty = false;
        }
    }

    free(screenbuf);
    framebufferClose(&fb);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    consoleInit(NULL);

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

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

    consoleExit(NULL);

    if (show_random_gallery(&pad)) {
        show_installer_ui(&pad);
    }

    romfsExit();
    return 0;
}
