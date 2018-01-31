/**
 * POET lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#ifndef _COPPER_EVAL_POET_H_
#define _COPPER_EVAL_POET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

int copper_eval_poet_init(double perf_target, unsigned int window_size, uint32_t freq_start);

void copper_eval_poet_finish(void);

void copper_eval_poet_iteration(uint64_t counter, double perf, double pwr);

uint32_t copper_eval_poet_get_curr_freq(void);

#ifdef __cplusplus
}
#endif

#endif
