#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stdlib.h>

uint8_t calc_checksum(const uint8_t *data, int len);
int test_ddc_device(const char *i2c_device, int *brightness_out);
int read_edid_from_i2c(const char *i2c_device, uint8_t *edid_out, size_t edid_size);
int read_edid_from_drm(const char *connector_path, uint8_t *edid_out, size_t edid_size);
int edid_match(const uint8_t *edid1, const uint8_t *edid2);
int map_all_monitors(void);
int set_brightness_to_device(const char *i2c_device, int brightness_value);

#endif
