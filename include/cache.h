#ifndef CACHE_H
#define CACHE_H

#include "monitor.h"

int read_cache(const char *cache_file, MonitorCache *cache);
int write_cache(const char *cache_file, const MonitorCache *cache);

#endif