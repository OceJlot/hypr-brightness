#ifndef HYPRLAND_H
#define HYPRLAND_H

#include <stddef.h>

void get_active_monitor_socket(char* buffer, size_t size);
int get_monitor_name(char* result_buffer, size_t max_len);

#endif
