/**
 * DVFS Simple Controller lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2017-03-07
 */
#ifndef _COPPER_EVAL_DSC_H_
#define _COPPER_EVAL_DSC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

int copper_eval_dsc_init(double perf_target, uint32_t* freqs, uint32_t nfreqs, uint32_t current_freq);

int copper_eval_dsc_finish(void);

uint32_t copper_eval_dsc_iteration(uint64_t iteration, double perf);

uint32_t copper_eval_dsc_get_curr_freq(void);

#ifdef __cplusplus
}
#endif

#endif
