#ifndef CACHE_H
#define CACHE_H

#include <pcre.h>

#include "libhash/libhash.h"

/**
 * @brief TODO: Retrieves a regex from the cache, or creates (and caches) one if
 * not extant.
 *
 * @param pattern
 * @return char*
 */
pcre *regex_cache_get(h_table *regex_cache, char *pattern);

#endif /* CACHE_H */
