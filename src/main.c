#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include "hyprland.h"
#include "i2c.h"
#include "cache.h"
#include "monitor.h"

typedef struct {
    int target_brightness;
    int operation;
    int delta;
    char monitor_name[MAX_MONITOR_NAME];
    bool use_gradual;
    int step;
    int sleep_ms;
} Arguments;

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <brightness>              - Set brightness to absolute value\n", prog_name);
    fprintf(stderr, "  %s +<value>                  - Increase brightness by value\n", prog_name);
    fprintf(stderr, "  %s -<value>                  - Decrease brightness by value\n", prog_name);
    fprintf(stderr, "  -m <monitor_name>           - Use specific monitor (instead of active)\n");
    fprintf(stderr, "  -a [<step>] [<sleep_ms>]    - Gradual adjustment (step: default 2, sleep_ms: default 0)\n");
}

static bool parse_args(int argc, char *argv[], Arguments *args) {
    if (argc < 2) return false;

    // Initialize defaults
    memset(args->monitor_name, 0, sizeof(args->monitor_name));
    args->operation = 0;  // set
    args->delta = 0;
    args->use_gradual = false;
    args->step = 2;
    args->sleep_ms = 0;

    int i = 1;
    
    const char *brightness_arg = argv[i];
    
    if (brightness_arg[0] == '+') {
        // Relative increase
        args->operation = 1;
        args->delta = atoi(brightness_arg + 1);
        if (args->delta < 0 || args->delta > 100) return false;
    } else if (brightness_arg[0] == '-' && strlen(brightness_arg) > 1 && isdigit(brightness_arg[1])) {
        // Relative decrease
        args->operation = 1;
        args->delta = -atoi(brightness_arg + 1);
        if (args->delta > 0 || args->delta < -100) return false;
    } else {
        // Absolute value
        args->operation = 0;
        args->target_brightness = atoi(brightness_arg);
        if (args->target_brightness < 0 || args->target_brightness > 100) return false;
    }

    i++;

    while (i < argc) {
        if (strcmp(argv[i], "-m") == 0) {
            if (i + 1 >= argc) return false;
            i++;
            strncpy(args->monitor_name, argv[i], MAX_MONITOR_NAME - 1);
            args->monitor_name[MAX_MONITOR_NAME - 1] = '\0';
            i++;
        } else if (strcmp(argv[i], "-a") == 0) {
            args->use_gradual = true;
            i++;
            if (i < argc && argv[i][0] != '-') {
                args->step = atoi(argv[i]);
                if (args->step <= 0 || args->step > 100) args->step = 2;
                i++;
                if (i < argc && argv[i][0] != '-') {
                    args->sleep_ms = atoi(argv[i]);
                    if (args->sleep_ms < 0) args->sleep_ms = 0;
                    i++;
                }
            }
        } else {
            return false;
        }
    }

    return true;
}

static int apply_brightness_gradual(int current, int target, int step, int sleep_ms, 
                                     const char *i2c_device) {
    int diff = target - current;
    int direction = (diff > 0) ? 1 : -1;
    int abs_diff = abs(diff);

    if (abs_diff <= step) {
        if (set_brightness_to_device(i2c_device, target) != 0) return -1;
        return 0;
    }

    // Gradually adjust brightness
    int brightness = current;
    while (brightness != target) {
        brightness += (abs_diff > step) ? (step * direction) : direction;
        
        if (direction > 0 && brightness > target) brightness = target;
        if (direction < 0 && brightness < target) brightness = target;

        if (set_brightness_to_device(i2c_device, brightness) != 0) {
            fprintf(stderr, "Failed to set brightness at step %d\n", brightness);
            return -1;
        }

        if (sleep_ms > 0) usleep(sleep_ms * 1000);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    Arguments args;

    // Parse arguments
    if (!parse_args(argc, argv, &args)) {
        print_usage(argv[0]);
        return 1;
    }

    // Create cache directory
    mkdir(CACHE_DIR, 0777);

    // Determine monitor name
    char monitor_name[MAX_MONITOR_NAME];
    if (args.monitor_name[0] != '\0') {
        // Use specified monitor
        strncpy(monitor_name, args.monitor_name, MAX_MONITOR_NAME - 1);
        monitor_name[MAX_MONITOR_NAME - 1] = '\0';
    } else {
        // Get active monitor name
        if (!get_monitor_name(monitor_name, sizeof(monitor_name))) {
            fprintf(stderr, "Failed to get active monitor name\n");
            return 1;
        }
    }

    // Create cache file path
    char cache_file[MAX_PATH];
    snprintf(cache_file, sizeof(cache_file), "%s/%s.cache", CACHE_DIR, monitor_name);

    // Lock the cache file or exit
    int lock_fd = open(cache_file, O_RDWR | O_CREAT, 0644);
    if (lock_fd < 0) {
        fprintf(stderr, "Failed to open cache file: %s\n", strerror(errno));
        return 1;
    }

    if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
        close(lock_fd);
        return 2;
    }

    MonitorCache cache;
    int current_brightness;

    // Try to read cache
    if (read_cache(cache_file, &cache) && 
        strcmp(cache.monitor_name, monitor_name) == 0) {
        if (cache.unsupported) {
            fprintf(stderr, "Monitor %s does not support DDC control\n", monitor_name);
            flock(lock_fd, LOCK_UN);
            close(lock_fd);
            return 1;
        }
        current_brightness = cache.brightness;
    } else {
        if (!map_all_monitors()) {
            fprintf(stderr, "No monitors with DDC support found\n");
            flock(lock_fd, LOCK_UN);
            close(lock_fd);
            return 1;
        }
        
        // Re-read cache for this specific monitor
        if (!read_cache(cache_file, &cache) || cache.unsupported) {
            fprintf(stderr, "Monitor %s does not support DDC control\n", monitor_name);
            flock(lock_fd, LOCK_UN);
            close(lock_fd);
            return 1;
        }
        
        current_brightness = cache.brightness;
    }

    // Calculate new brightness
    int new_brightness;
    if (args.operation == 1) {
        new_brightness = current_brightness + args.delta;
    } else {
        new_brightness = args.target_brightness;
    }
    if (new_brightness > 100) new_brightness = 100;
    if (new_brightness < 0) new_brightness = 0;

    // Update cache
    cache.brightness = new_brightness;
    write_cache(cache_file, &cache);

    // Set brightness
    int result;
    if (args.use_gradual) {
        result = apply_brightness_gradual(current_brightness, new_brightness, 
                                         args.step, args.sleep_ms, cache.i2c_device);
    } else {
        result = set_brightness_to_device(cache.i2c_device, new_brightness);
    }

    if (result != 0) {
        fprintf(stderr, "Failed to set brightness\n");
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
        return 1;
    }

    // Release lock and close
    flock(lock_fd, LOCK_UN);
    close(lock_fd);

    return 0;
}