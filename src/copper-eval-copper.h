/**
 * CoPPer lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#ifndef _COPPER_EVAL_COPPER_H_
#define _COPPER_EVAL_COPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

int copper_eval_copper_init(double perf_target, double min_power, double max_power, double start_power);

int copper_eval_copper_finish(void);

double copper_eval_copper_iteration(uint64_t iteration, double perf);

double copper_eval_copper_get_curr_powercap(void);

#ifdef __cplusplus
}
#endif

#endif
