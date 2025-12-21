#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <dirent.h>
#include "monitor.h"

uint8_t calc_checksum(const uint8_t *data, int len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

int test_ddc_device(const char *i2c_device, int *brightness_out) {
    int fd = open(i2c_device, O_RDWR);
    if (fd < 0) return 0;
    
    if (ioctl(fd, I2C_SLAVE, DDC_ADDR) < 0) {
        close(fd);
        return 0;
    }

    uint8_t cmd[] = {0x51, 0x82, 0x01, VCP_BRIGHTNESS, 0x00};
    cmd[4] = calc_checksum(cmd, 4);
    
    if (write(fd, cmd, sizeof(cmd)) != sizeof(cmd)) {
        close(fd);
        return 0;
    }
    
    // usleep(50000);
    
    uint8_t response[12];
    int len = read(fd, response, sizeof(response));
    close(fd);
    
    if (len < 10) return 0;
    
    if (brightness_out) {
        *brightness_out = response[9];
    }
    return 1;
}

int read_edid_from_i2c(const char *i2c_device, uint8_t *edid_out, size_t edid_size) {
    int fd = open(i2c_device, O_RDWR);
    if (fd < 0) return 0;
    
    if (ioctl(fd, I2C_SLAVE, EDID_ADDR) < 0) {
        close(fd);
        return 0;
    }
    
    uint8_t offset = 0x00;
    if (write(fd, &offset, 1) != 1) {
        close(fd);
        return 0;
    }
    

    int bytes_read = read(fd, edid_out, edid_size);
    close(fd);
    
    return bytes_read > 0;
}

int read_edid_from_drm(const char *connector_path, uint8_t *edid_out, size_t edid_size) {
    char edid_path[600];
    snprintf(edid_path, sizeof(edid_path), "%s/edid", connector_path);
    
    int fd = open(edid_path, O_RDONLY);
    if (fd < 0) return 0;
    
    int bytes_read = read(fd, edid_out, edid_size);
    close(fd);
    
    return bytes_read > 0;
}

int edid_match(const uint8_t *edid1, const uint8_t *edid2) {
    return memcmp(edid1 + 8, edid2 + 8, 10) == 0;
}

int map_all_monitors(void) {
    int found_count = 0;
    
    // Get all connected DRM connectors
    typedef struct {
        char name[MAX_MONITOR_NAME];
        uint8_t edid[128];
        bool has_edid;
    } DRMConnector;
    
    DRMConnector connectors[10];
    int connector_count = 0;
    
    DIR *dir = opendir("/sys/class/drm");
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && connector_count < 10) {
            if (strncmp(entry->d_name, "card", 4) != 0) continue;
            if (!strchr(entry->d_name, '-')) continue;
            
            char drm_path[512];
            snprintf(drm_path, sizeof(drm_path), "/sys/class/drm/%s", entry->d_name);
            
            // Check if connected
            char status_path[600];
            snprintf(status_path, sizeof(status_path), "%s/status", drm_path);
            FILE *fp = fopen(status_path, "r");
            if (!fp) continue;
            
            char status[32];
            if (!fgets(status, sizeof(status), fp)) {
                fclose(fp);
                continue;
            }
            fclose(fp);
            
            if (strncmp(status, "connected", 9) != 0) continue;
            
            // Extract connector name
            const char *dash = strchr(entry->d_name, '-');
            if (!dash) continue;
            
            strncpy(connectors[connector_count].name, dash + 1, MAX_MONITOR_NAME - 1);
            connectors[connector_count].name[MAX_MONITOR_NAME - 1] = '\0';
            
            // Read EDID
            connectors[connector_count].has_edid = 
                read_edid_from_drm(drm_path, connectors[connector_count].edid, 128);
            
            if (connectors[connector_count].has_edid) {
                connector_count++;
            }
        }
        closedir(dir);
    }
    
    // Scan I2C devices and match with DRM connectors
    for (int bus = 0; bus < 20; bus++) {
        char i2c_device[32];
        snprintf(i2c_device, sizeof(i2c_device), "/dev/i2c-%d", bus);
        
        int brightness;
        if (!test_ddc_device(i2c_device, &brightness)) {
            continue;
        }
        
        // Read EDID from I2C device
        uint8_t i2c_edid[128];
        if (!read_edid_from_i2c(i2c_device, i2c_edid, 128)) {
            continue;
        }
        
        // Match with DRM connectors
        bool matched = false;
        for (int i = 0; i < connector_count; i++) {
            if (connectors[i].has_edid && edid_match(i2c_edid, connectors[i].edid)) {
                MonitorCache cache;
                strncpy(cache.monitor_name, connectors[i].name, sizeof(cache.monitor_name) - 1);
                cache.monitor_name[sizeof(cache.monitor_name) - 1] = '\0';
                strncpy(cache.i2c_device, i2c_device, sizeof(cache.i2c_device) - 1);
                cache.i2c_device[sizeof(cache.i2c_device) - 1] = '\0';
                cache.brightness = brightness;
                cache.unsupported = false;
                
                char cache_file[MAX_PATH];
                snprintf(cache_file, sizeof(cache_file), "%s/%s.cache", CACHE_DIR, connectors[i].name);
                
                FILE *fp = fopen(cache_file, "w");
                if (fp) {
                    fprintf(fp, "%s %s %d %d", 
                            cache.monitor_name, 
                            cache.i2c_device, 
                            cache.brightness,
                            0);
                    fclose(fp);
                }
                
                found_count++;
                matched = true;
                break;
            }
        }
    }
    return found_count > 0;
}

int set_brightness_to_device(const char *i2c_device, int brightness_value) {
    int fd = open(i2c_device, O_RDWR);
    if (fd < 0) return -1;
    
    if (ioctl(fd, I2C_SLAVE, DDC_ADDR) < 0) {
        close(fd);
        return -1;
    }

    uint8_t cmd[] = {
        0x51, 0x84, 0x03, VCP_BRIGHTNESS, 0x00, brightness_value, 0x00
    };
    cmd[6] = calc_checksum(cmd, 6);
    
    if (write(fd, cmd, sizeof(cmd)) != sizeof(cmd)) {
        close(fd);
        return -1;
    }
    
    close(fd);
    return 0;
}
