/**
 * cpufreq-bindings lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#ifndef _COPPER_EVAL_CPUFREQ_BINDINGS_H_
#define _COPPER_EVAL_CPUFREQ_BINDINGS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

int copper_eval_cpufreq_set(uint32_t freq);

int copper_eval_cpufreq_init(void);

void copper_eval_cpufreq_finish(void);

uint32_t copper_eval_cpufreq_get_available_freqs(uint32_t** freqs);

uint32_t copper_eval_cpufreq_get_cur_freq(void);

#ifdef __cplusplus
}
#endif

#endif
