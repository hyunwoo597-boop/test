#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

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
    printf("Only the three Korean patch files were targeted.\n");
}

int main(int argc, char **argv) {
    consoleInit(NULL);

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    Result rc = fsdevMountSdmc();
    if (R_FAILED(rc)) {
        printf("SD mount failed: 0x%08X\nPress + to exit.\n", rc);
        goto loop_no_romfs;
    }
    rc = romfsInit();
    if (R_FAILED(rc)) {
        printf("Embedded payload mount failed: 0x%08X\nPress + to exit.\n", rc);
        goto loop_sd;
    }

    printf("RE5 Korean Patch Installer v5\n");
    printf("Target: Resident Evil 5 / %s\n\n", TITLE_ID);
    printf("A  Install / overwrite Korean patch\n");
    printf("X  Verify installed files\n");
    printf("Y  Remove Korean patch files\n");
    printf("+  Exit\n");

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
loop_sd:
    fsdevUnmountDevice("sdmc");
loop_no_romfs:
    consoleExit(NULL);
    return 0;
}
