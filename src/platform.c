#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __vita__
#include <psp2/apputil.h>
#include <psp2/ctrl.h>
#include <psp2/io/stat.h>
#include <psp2/power.h>
#include <psp2/system_param.h>
#else
#include <sys/stat.h>
#endif

void platform_init(void)
{
#ifdef __vita__
    SceAppUtilInitParam init;
    SceAppUtilBootParam boot;
    memset(&init, 0, sizeof(init));
    memset(&boot, 0, sizeof(boot));
    sceAppUtilInit(&init, &boot);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
    sceIoMkdir("ux0:data/icytower", 0777);
    sceIoMkdir("ux0:data/icytower/replays", 0777);
#endif
}

const char *game_root(void)
{
#ifdef __vita__
    return "ux0:data/icytower";
#else
    const char *e = getenv("ICYTOWER_DATA");
    return e ? e : "gamedata";
#endif
}

const char *game_path(const char *rel)
{
    static char buf[8][512];
    static int slot;
    char *out = buf[slot++ & 7];
    snprintf(out, 512, "%s/%s", game_root(), rel);
    return out;
}

const char *config_path(void)
{
#ifndef __vita__
    const char *e = getenv("ICYTOWER_CONFIG");
    if (e) return e;
#endif
    return game_path("icytower_vita.cfg");
}

int platform_enter_is_circle(void)
{
#ifdef __vita__
    int v = SCE_SYSTEM_PARAM_ENTER_BUTTON_CROSS;
    sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_ENTER_BUTTON, &v);
    return v == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE;
#else
    return 0;
#endif
}

void platform_mkdir(const char *path)
{
#ifdef __vita__
    sceIoMkdir(path, 0777);
#else
    mkdir(path, 0777);
#endif
}
