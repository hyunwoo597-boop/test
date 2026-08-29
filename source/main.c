#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define RE5_TITLE_ID "010018100CD46000"
#define PATCH_ROOT "sdmc:/atmosphere/contents/" RE5_TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive"
#define CRASH_DST "sdmc:/atmosphere/exefs_patches/RE5_Manual_CrashFix/C517ECBB79DE97338E98147A6B5B5F2B.ips"

typedef struct { const char *name; const char *src; const char *dst; } PatchFile;
static const PatchFile g_files[] = {
    {"CoreResource.arc", "romfs:/payload/atmosphere/contents/" RE5_TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/CoreResource.arc", PATCH_ROOT "/CoreResource.arc"},
    {"GuiTextResource.arc", "romfs:/payload/atmosphere/contents/" RE5_TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/GuiTextResource.arc", PATCH_ROOT "/GuiTextResource.arc"},
    {"Msg2Resource_e.arc", "romfs:/payload/atmosphere/contents/" RE5_TITLE_ID "/romfs/nativeNXx64/ImgNX/Archive/Msg2Resource_e.arc", PATCH_ROOT "/Msg2Resource_e.arc"},
    {"MANUAL CrashFix IPS", "romfs:/payload/atmosphere/exefs_patches/RE5_Manual_CrashFix/C517ECBB79DE97338E98147A6B5B5F2B.ips", CRASH_DST},
};

static int mkdir_p(const char *path) {
    char tmp[512];
    size_t n=strlen(path);
    if(n>=sizeof(tmp)) return -1;
    strcpy(tmp,path);
    for(char *p=tmp+1; *p; ++p) {
        if(*p=='/') { *p='\0'; if(mkdir(tmp,0777)<0 && errno!=EEXIST) return -1; *p='/'; }
    }
    if(mkdir(tmp,0777)<0 && errno!=EEXIST) return -1;
    return 0;
}

static int ensure_parent(const char *file) {
    char tmp[512];
    if(strlen(file)>=sizeof(tmp)) return -1;
    strcpy(tmp,file);
    char *slash=strrchr(tmp,'/');
    if(!slash) return 0;
    *slash='\0';
    return mkdir_p(tmp);
}

static int copy_file(const char *src, const char *dst) {
    if(ensure_parent(dst)<0) return -1;
    FILE *in=fopen(src,"rb"); if(!in) return -2;
    FILE *out=fopen(dst,"wb"); if(!out){ fclose(in); return -3; }
    unsigned char *buf=malloc(256*1024); if(!buf){fclose(in);fclose(out);return -4;}
    int rc=0; size_t n;
    while((n=fread(buf,1,256*1024,in))>0) if(fwrite(buf,1,n,out)!=n){rc=-5;break;}
    if(ferror(in)) rc=-6;
    fflush(out); fclose(out); fclose(in); free(buf); return rc;
}

static long file_size(const char *p){ FILE *f=fopen(p,"rb"); if(!f)return -1; fseek(f,0,SEEK_END); long s=ftell(f); fclose(f); return s; }

static bool verify_one(const PatchFile *f){ long a=file_size(f->src), b=file_size(f->dst); return a>0 && a==b; }

static bool show_manual_gallery(void) {
    Result rc=romfsInit();
    if(R_FAILED(rc)) return true;
    Framebuffer fb;
    framebufferCreate(&fb, nwindowGetDefault(), 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&fb);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);
    int page=0; bool redraw=true, done=false;
    uint16_t *raw=malloc(1280*720*2);
    if(!raw){ framebufferClose(&fb); romfsExit(); return true; }

    while(appletMainLoop() && !done) {
        padUpdate(&pad);
        u64 down=padGetButtonsDown(&pad);
        if(down & HidNpadButton_L){ page=(page+4)%5; redraw=true; }
        if(down & HidNpadButton_R){ page=(page+1)%5; redraw=true; }
        if(down & HidNpadButton_A) done=true;
        if(down & HidNpadButton_Plus) { free(raw); framebufferClose(&fb); romfsExit(); return false; }
        if(redraw){
            char path[64]; snprintf(path,sizeof(path),"romfs:/manual/page%d.rgb565",page+1);
            FILE *f=fopen(path,"rb");
            if(f){ size_t got=fread(raw,1,1280*720*2,f); fclose(f);
                if(got==1280*720*2){
                    u32 stride=0; u32 *dst=(u32*)framebufferBegin(&fb,&stride);
                    for(int y=0;y<720;y++) for(int x=0;x<1280;x++){
                        uint16_t v=raw[y*1280+x];
                        u32 r=((v>>11)&31)*255/31, g=((v>>5)&63)*255/63, b=(v&31)*255/31;
                        dst[y*(stride/4)+x]=0xFF000000u | (b<<16) | (g<<8) | r;
                    }
                    framebufferEnd(&fb);
                }
            }
            redraw=false;
        }
    }
    free(raw); framebufferClose(&fb); romfsExit(); return true;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if(!show_manual_gallery()) return 0;

    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);

    Result r=romfsInit();
    if(R_FAILED(r)){ printf("RomFS init failed: 0x%08X\n", r); consoleUpdate(NULL); while(appletMainLoop()){padUpdate(&pad); if(padGetButtonsDown(&pad)&HidNpadButton_Plus)break;} consoleExit(NULL); return 1; }
    Result sd=fsdevMountSdmc();
    (void)sd; /* sdmc may already be mounted; stdio path is what matters */

    printf("RE5 Korean Patch Installer\n\n");
    printf("Manual: L/R page, A installer, + exit\n\n");
    printf("A : Install / repair Korean patch\n");
    printf("X : Verify installed files\n");
    printf("Y : Remove installed patch files\n");
    printf("+ : Exit\n\n");

    while(appletMainLoop()){
        padUpdate(&pad); u64 down=padGetButtonsDown(&pad);
        if(down&HidNpadButton_Plus) break;
        if(down&HidNpadButton_A){
            printf("\nInstalling...\n");
            int ok=0;
            for(size_t i=0;i<sizeof(g_files)/sizeof(g_files[0]);i++){
                int rc=copy_file(g_files[i].src,g_files[i].dst);
                bool v=(rc==0)&&verify_one(&g_files[i]);
                printf("%s: %s\n",g_files[i].name,v?"OK":"FAIL"); if(v)ok++;
            }
            printf("Result: %d/%zu OK\n",ok,sizeof(g_files)/sizeof(g_files[0]));
        }
        if(down&HidNpadButton_X){
            printf("\nVerify:\n");
            for(size_t i=0;i<sizeof(g_files)/sizeof(g_files[0]);i++) printf("%s: %s\n",g_files[i].name,verify_one(&g_files[i])?"OK":"MISSING/DIFFERENT");
        }
        if(down&HidNpadButton_Y){
            printf("\nRemoving only files installed by this app...\n");
            for(size_t i=0;i<sizeof(g_files)/sizeof(g_files[0]);i++){ int rc=remove(g_files[i].dst); printf("%s: %s\n",g_files[i].name,(rc==0||errno==ENOENT)?"OK":"FAIL"); }
        }
        consoleUpdate(NULL);
    }
    romfsExit(); fsdevUnmountDevice("sdmc"); consoleExit(NULL); return 0;
}
