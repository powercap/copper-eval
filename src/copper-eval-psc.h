/**
 * Powercap Simple Controller lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2017-03-16
 */
#ifndef _COPPER_EVAL_PSC_H_
#define _COPPER_EVAL_PSC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

int copper_eval_psc_init(double perf_target, double min_power, double max_power, double start_power);

int copper_eval_psc_finish(void);

double copper_eval_psc_iteration(uint64_t iteration, double perf);

double copper_eval_psc_get_curr_powercap(void);

#ifdef __cplusplus
}
#endif

#endif
