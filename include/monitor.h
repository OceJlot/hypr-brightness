#ifndef MONITOR_H
#define MONITOR_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// DDC and EDID constants
#define DDC_ADDR 0x37
#define EDID_ADDR 0x50
#define VCP_BRIGHTNESS 0x10

#define CACHE_DIR "/tmp/hypr_brightness"
#define MAX_PATH 512
#define MAX_MONITOR_NAME 128

typedef struct {
    char monitor_name[MAX_MONITOR_NAME];
    char i2c_device[32];
    int brightness;
    bool unsupported;
} MonitorCache;


#endif