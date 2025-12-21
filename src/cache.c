#include <stdio.h>
#include <stdbool.h>
#include "monitor.h"

int read_cache(const char *cache_file, MonitorCache *cache) {
    FILE *fp = fopen(cache_file, "r");
    if (!fp) return 0;
    
    int unsupported_flag;
    int result = fscanf(fp, "%127s %31s %d %d", 
                       cache->monitor_name, 
                       cache->i2c_device, 
                       &cache->brightness,
                       &unsupported_flag);
    fclose(fp);
    
    if (result == 4) {
        cache->unsupported = unsupported_flag != 0;
        return 1;
    }
    return 0;
}

int write_cache(const char *cache_file, const MonitorCache *cache) {
    FILE *fp = fopen(cache_file, "w");
    if (!fp) return 0;
    
    fprintf(fp, "%s %s %d %d", 
            cache->monitor_name, 
            cache->i2c_device, 
            cache->brightness,
            cache->unsupported ? 1 : 0);
    fclose(fp);
    return 1;
}