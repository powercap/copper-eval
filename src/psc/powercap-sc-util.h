/**
 * Utility functions to wrap psc.h.
 *
 * @author Connor Imes
 * @date 2017-03-16
 */

#ifndef _POWER_SIMPLE_CONTROLLER_UTIL_H_
#define _POWER_SIMPLE_CONTROLLER_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "powercap-sc.h"

/**
 * Create a controller.
 */
psc* psc_alloc_init(double perf_target, double min_power, double max_power, double current_power, double k_i,
                    uint32_t lb_length, const char* log_filename);

/**
 * Cleanup.
 */
int psc_destroy_free(psc* state);

#ifdef __cplusplus
}
#endif

#endif
