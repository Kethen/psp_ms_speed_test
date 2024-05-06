#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <psputils.h>
#include <psppower.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>

/* Define the module info section */
PSP_MODULE_INFO("MS speed test", 0, 1, 1);

/* Define the main thread's attribute value (optional) */
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define printf pspDebugScreenPrintf
#define setXY pspDebugScreenSetXY
#define setTextColor pspDebugScreenSetTextColor
#define setBackColor pspDebugScreenSetBackColor


int log_fd = -1;
#define LOG(...) { \
    char _buf[1024]; \
    if(log_fd >= 0){ \
        u32 _len = sprintf(_buf, __VA_ARGS__); \
        sceIoWrite(log_fd, _buf, _len); \
    } \
    printf(__VA_ARGS__); \
} \

void fill_pattern(char* dst, u32 dst_size, char* pattern, u32 pattern_size){
    for(u32 i = 0;i < dst_size; i+=pattern_size){
        memcpy(&dst[i], pattern, pattern_size);
    }
}

int pattern_not_match(char* dst, u32 dst_size, char* pattern, u32 pattern_size){
    for(u32 i = 0;i < dst_size; i += pattern_size){
        if(memcmp(&dst[i], pattern, pattern_size) != 0){
            return -1;
        }
    }
    return 0;
}

int write_file(char * path, u32 block_size, u32 blocks, char *pattern, u32 pattern_size, u32 *out_clock_cycles){
    char block[block_size];
    fill_pattern(block, block_size, pattern, pattern_size);

    clock_t begin_cycles = sceKernelLibcClock();

    int fd = sceIoOpen(path, PSP_O_WRONLY|PSP_O_CREAT, 0666);
    if(fd < 0){
        return fd;
    }

    for(u32 i = 0; i < blocks; i++){
        int w = sceIoWrite(fd, block, block_size);
        if(w != block_size){
            sceIoClose(fd);
            return w;
        }
    }

    *out_clock_cycles = sceKernelLibcClock() - begin_cycles;

    sceIoClose(fd);
    return 0;
}

int read_file(char *path, u32 block_size, u32 blocks, char *pattern, u32 pattern_size, clock_t *out_clock_cycles){
    char block[block_size];

    clock_t begin_cycles = sceKernelLibcClock();

    int fd = sceIoOpen(path, PSP_O_RDONLY, 0666);
    if(fd < 0){
        return fd;
    }

    for(u32 i = 0;i < blocks; i++){
        int r = sceIoRead(fd, block, block_size);
        if(r != block_size){
            sceIoClose(fd);
            return r;
        }
        if(pattern_not_match(block, block_size, pattern, pattern_size)){
            return -1;
        }
    }

    *out_clock_cycles = sceKernelLibcClock() - begin_cycles;

    sceIoClose(fd);
    return 0;
}

int main(void)
{
    pspDebugScreenInit();

    char *fs = "ms0:/%s";

    char *log_path = "ms0:/ms_speed.txt";
    log_fd = sceIoOpen(log_path, PSP_O_WRONLY|PSP_O_CREAT, 0666);
    if(log_fd < 0){
        fs = "ef0:/%s";
        log_path = "ef0:/ms_speed.txt";
        log_fd = sceIoOpen(log_path, PSP_O_WRONLY|PSP_O_CREAT, 0666);
    }
    if(log_fd < 0){
        LOG("failed creating log file\n");
        sceKernelDelayThread(5 * 1000000);
        sceKernelExitGame();
    }

    float clock_speed = scePowerGetCpuClockFrequencyFloat();
    LOG("clock speed is %f\n", clock_speed);

    char path_buf[128];
    sprintf(path_buf, fs, "ms_test_10MB");
    sceIoRemove(path_buf);

    char test_pattern[] = {0, 1, 2, 3, 4, 5, 6, 7};

    clock_t cycles;

    LOG("beginning tests\n");

    int res = write_file(path_buf, 2048, 10 * 1024 * 1024 / 2048, test_pattern, sizeof(test_pattern), &cycles);
    LOG("first write took %f seconds, %s\n", cycles / clock_speed, res ? "failed" : "succeed");

    res = write_file(path_buf, 2048, 10 * 1024 * 1024 / 2048, test_pattern, sizeof(test_pattern), &cycles);
    LOG("second write took %f seconds, %s\n", cycles / clock_speed, res ? "failed" : "succeed");

    res = read_file(path_buf, 2048, 10 * 1024 * 1024 / 2048, test_pattern, sizeof(test_pattern), &cycles);
    LOG("first read took %f seconds, %s\n", cycles / clock_speed, res ? "failed" : "succeed");

    res = read_file(path_buf, 2048, 10 * 1024 * 1024 / 2048, test_pattern, sizeof(test_pattern), &cycles);
    LOG("second read took %f seconds, %s\n", cycles / clock_speed, res ? "failed" : "succeed");

    if(log_fd >= 0){
        sceIoClose(log_fd);
    }

    sceKernelDelayThread(5 * 1000000);
    sceKernelExitGame();
    return 0;
}
