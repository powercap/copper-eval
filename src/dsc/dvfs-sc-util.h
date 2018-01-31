/**
 * Utility functions to wrap dvfs-sc.h.
 *
 * @author Connor Imes
 * @date 2017-03-07
 */

#ifndef _DVFS_SIMPLE_CONTROLLER_UTIL_H_
#define _DVFS_SIMPLE_CONTROLLER_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "dvfs-sc.h"

/**
 * Create a controller.
 */
dsc* dsc_alloc_init(double perf_target, uint32_t* freqs, uint32_t nfreqs, uint32_t current_freq, double k_i,
                    uint32_t lb_length, const char* log_filename);

/**
 * Cleanup.
 */
int dsc_destroy_free(dsc* state);

#ifdef __cplusplus
}
#endif

#endif
