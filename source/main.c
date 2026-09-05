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

#define BGM_PATH "romfs:/audio/iron_and_bone.pcm"
#define BGM_BUFFER_COUNT 4
#define BGM_BUFFER_SIZE 0x40000

static FILE *g_bgm_file = NULL;
static bool g_bgm_ready = false;
static int g_bgm_refill_index = 0;
static AudioOutBuffer g_bgm_out[BGM_BUFFER_COUNT];
static u8 g_bgm_pcm[BGM_BUFFER_COUNT][BGM_BUFFER_SIZE] __attribute__((aligned(0x1000)));

static void bgm_pump(void);

typedef struct {
    const char *name;
    const char *src;
    const char *dst;
} PatchFile;

typedef struct {
    const char *name;
    const char *src;
} CharacterMod;

static const PatchFile g_files[] = {
    {"CoreResource.arc",     "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/CoreResource.arc",     PATCH_ROOT "/CoreResource.arc"},
    {"GuiTextResource.arc",  "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/GuiTextResource.arc",  PATCH_ROOT "/GuiTextResource.arc"},
    {"Msg2Resource_e.arc",   "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/Msg2Resource_e.arc",   PATCH_ROOT "/Msg2Resource_e.arc"},
    {"NXStrapResource.arc",  "romfs:/payload/atmosphere/contents/" TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/NXStrapResource.arc",   PATCH_ROOT "/NXStrapResource.arc"},
    {"MANUAL CrashFix IPS",  "romfs:/payload/atmosphere/exefs_patches/RE5_Manual_CrashFix/C517ECBB79DE97338E98147A6B5B5F2B.ips", "sdmc:/atmosphere/exefs_patches/RE5_Manual_CrashFix/C517ECBB79DE97338E98147A6B5B5F2B.ips"},
};

static const CharacterMod g_mods[] = {
    {"엑셀라",       "romfs:/payload/mods/excella/main"},
    {"레베카",       "romfs:/payload/mods/rebecca/main"},
    {"질 코스튬 1",  "romfs:/payload/mods/jill_cos1/main"},
    {"질 코스튬 2",  "romfs:/payload/mods/jill_cos2/main"},
};

static const char *g_unlock_only = "romfs:/payload/unlock/main";

static const char *g_main_screens[7] = {
    "romfs:/ui/main_0.rgb565",
    "romfs:/ui/main_1.rgb565",
    "romfs:/ui/main_2.rgb565",
    "romfs:/ui/main_3.rgb565",
    "romfs:/ui/main_4.rgb565",
    "romfs:/ui/main_5.rgb565",
    "romfs:/ui/main_6.rgb565",
};

static const char *g_char_screens[5] = {
    "romfs:/ui/char_0.rgb565",
    "romfs:/ui/char_1.rgb565",
    "romfs:/ui/char_2.rgb565",
    "romfs:/ui/char_3.rgb565",
    "romfs:/ui/char_4.rgb565",
};

static bool bgm_fill_buffer(int index) {
    if (!g_bgm_file || index < 0 || index >= BGM_BUFFER_COUNT) return false;

    size_t filled = 0;
    while (filled < BGM_BUFFER_SIZE) {
        size_t n = fread(g_bgm_pcm[index] + filled, 1, BGM_BUFFER_SIZE - filled, g_bgm_file);
        filled += n;

        if (filled == BGM_BUFFER_SIZE) break;
        if (ferror(g_bgm_file)) {
            clearerr(g_bgm_file);
            memset(g_bgm_pcm[index] + filled, 0, BGM_BUFFER_SIZE - filled);
            break;
        }

        if (feof(g_bgm_file)) {
            clearerr(g_bgm_file);
            if (fseek(g_bgm_file, 0, SEEK_SET) != 0) {
                memset(g_bgm_pcm[index] + filled, 0, BGM_BUFFER_SIZE - filled);
                break;
            }
        }
    }

    armDCacheFlush(g_bgm_pcm[index], BGM_BUFFER_SIZE);
    return true;
}

static bool bgm_init(void) {
    Result rc = audoutInitialize();
    if (R_FAILED(rc)) return false;

    g_bgm_file = fopen(BGM_PATH, "rb");
    if (!g_bgm_file) {
        audoutExit();
        return false;
    }

    memset(g_bgm_out, 0, sizeof(g_bgm_out));
    for (int i = 0; i < BGM_BUFFER_COUNT; ++i) {
        g_bgm_out[i].next = NULL;
        g_bgm_out[i].buffer = g_bgm_pcm[i];
        g_bgm_out[i].buffer_size = BGM_BUFFER_SIZE;
        g_bgm_out[i].data_size = BGM_BUFFER_SIZE;
        g_bgm_out[i].data_offset = 0;
        if (!bgm_fill_buffer(i)) goto fail;
    }

    rc = audoutStartAudioOut();
    if (R_FAILED(rc)) goto fail;

    for (int i = 0; i < BGM_BUFFER_COUNT; ++i) {
        rc = audoutAppendAudioOutBuffer(&g_bgm_out[i]);
        if (R_FAILED(rc)) {
            audoutStopAudioOut();
            goto fail;
        }
    }

    g_bgm_refill_index = 0;
    g_bgm_ready = true;
    return true;

fail:
    fclose(g_bgm_file);
    g_bgm_file = NULL;
    audoutExit();
    return false;
}

static void bgm_pump(void) {
    if (!g_bgm_ready) return;

    AudioOutBuffer *released = NULL;
    u32 released_count = 0;
    Result rc = audoutGetReleasedAudioOutBuffer(&released, &released_count);
    (void)released;
    if (R_FAILED(rc) || released_count == 0) return;

    for (u32 i = 0; i < released_count; ++i) {
        int index = g_bgm_refill_index;
        bgm_fill_buffer(index);
        if (R_SUCCEEDED(audoutAppendAudioOutBuffer(&g_bgm_out[index]))) {
            g_bgm_refill_index = (g_bgm_refill_index + 1) % BGM_BUFFER_COUNT;
        }
    }
}

static void bgm_shutdown(void) {
    if (!g_bgm_ready) return;

    g_bgm_ready = false;
    audoutStopAudioOut();
    bool flushed = false;
    audoutFlushAudioOutBuffers(&flushed);
    (void)flushed;

    if (g_bgm_file) {
        fclose(g_bgm_file);
        g_bgm_file = NULL;
    }
    audoutExit();
}

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
        bgm_pump();
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

static bool install_character(int index) {
    // 0 = 기본 셰바. exefs/main을 지워 캐릭터 모드를 해제한다.
    if (index == 0) {
        if (remove(MOD_TARGET) == 0) return true;
        return errno == ENOENT;
    }

    int mod_index = index - 1;
    if (mod_index < 0 || mod_index >= (int)(sizeof(g_mods)/sizeof(g_mods[0]))) return false;
    const char *src = g_mods[mod_index].src;
    if (copy_file(src, MOD_TARGET) != 0) return false;
    return same_size(src, MOD_TARGET);
}

static bool verify_character(int index) {
    if (index == 0) return file_size(MOD_TARGET) < 0;

    int mod_index = index - 1;
    if (mod_index < 0 || mod_index >= (int)(sizeof(g_mods)/sizeof(g_mods[0]))) return false;
    return same_size(g_mods[mod_index].src, MOD_TARGET);
}

static bool install_unlock_only(void) {
    if (copy_file(g_unlock_only, MOD_TARGET) != 0) return false;
    return same_size(g_unlock_only, MOD_TARGET);
}

static bool verify_unlock_only(void) {
    return same_size(g_unlock_only, MOD_TARGET);
}

static bool remove_character_or_unlock(void) {
    if (remove(MOD_TARGET) == 0) return true;
    return errno == ENOENT;
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

static void present_fullscreen(Framebuffer *fb, const u16 *frame) {
    u32 stride = 0;
    u8 *fbptr = (u8*)framebufferBegin(fb, &stride);
    for (u32 y = 0; y < FB_HEIGHT; ++y) {
        memcpy(fbptr + y * stride, frame + y * FB_WIDTH, FB_WIDTH * 2);
    }
    framebufferEnd(fb);
}

static void show_status_screen(Framebuffer *fb, PadState *pad, u16 *screenbuf, bool ok) {
    const char *path = ok ? "romfs:/ui/status_ok.rgb565" : "romfs:/ui/status_fail.rgb565";
    if (load_frame(path, screenbuf) == 0) present_fullscreen(fb, screenbuf);

    while (appletMainLoop()) {
        bgm_pump();
        padUpdate(pad);
        u64 down = padGetButtonsDown(pad);
        if (down & (HidNpadButton_A | HidNpadButton_B | HidNpadButton_Plus)) break;
    }
}

static void show_info_screen(Framebuffer *fb, PadState *pad, u16 *screenbuf) {
    if (load_frame("romfs:/ui/info.rgb565", screenbuf) == 0) present_fullscreen(fb, screenbuf);
    while (appletMainLoop()) {
        bgm_pump();
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
    int char_sel = 0; // 0=기본 셰바, 1=엑셀라, 2=레베카, 3=질1, 4=질2
    bool dirty = true;
    bool quit = false;

    while (appletMainLoop() && !quit) {
        bgm_pump();
        padUpdate(pad);
        u64 down = padGetButtonsDown(pad);

        if (down & HidNpadButton_Plus) break;

        if (mode == MODE_MAIN) {
            if (down & HidNpadButton_Up)   { main_sel = (main_sel + 6) % 7; dirty = true; }
            if (down & HidNpadButton_Down) { main_sel = (main_sel + 1) % 7; dirty = true; }

            if (down & HidNpadButton_X) {
                bool ok = false;
                if (main_sel == 0) ok = verify_patch_action();
                else if (main_sel == 2) ok = verify_unlock_only();
                else if (main_sel == 3) ok = verify_patch_action();
                else { dirty = true; }

                if (main_sel == 0 || main_sel == 2 || main_sel == 3) {
                    show_status_screen(&fb, pad, screenbuf, ok);
                    dirty = true;
                }
            }

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

                    case 2: // 코스튬 + 추가 스토리 전체 언락 단독 설치
                        ok = install_unlock_only();
                        show_status_screen(&fb, pad, screenbuf, ok);
                        dirty = true;
                        break;

                    case 3: // 한글패치 설치 상태 확인
                        ok = verify_patch_action();
                        show_status_screen(&fb, pad, screenbuf, ok);
                        dirty = true;
                        break;

                    case 4: // 모드 제거 / 원상복구
                        ok = remove_patch_action();
                        if (!remove_character_or_unlock()) ok = false;
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

            if (down & HidNpadButton_A) {
                bool ok = install_character(char_sel);
                show_status_screen(&fb, pad, screenbuf, ok);
                dirty = true;
            }

            if (down & HidNpadButton_X) {
                bool ok = verify_character(char_sel);
                show_status_screen(&fb, pad, screenbuf, ok);
                dirty = true;
            }
        }

        if (dirty && !quit) {
            const char *path = (mode == MODE_MAIN)
                ? g_main_screens[main_sel]
                : g_char_screens[char_sel];

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

    bgm_init();

    consoleExit(NULL);
    show_installer_ui(&pad);
    bgm_shutdown();
    romfsExit();
    return 0;
}
