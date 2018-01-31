/**
 * A simple controller to meet performance targets by manipulating DVFS frequencies.
 *
 * @author Connor Imes
 * @date 2017-03-07
 */

#ifndef _DVFS_SIMPLE_CONTROLLER_H_
#define _DVFS_SIMPLE_CONTROLLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdio.h>

// Stores user-defined parameters
typedef struct dsc_context {
  double perf_target;
  uint32_t nfreqs;
  uint32_t* freqs;
  double K_I;
} dsc_context;

// Log file fields
typedef struct dsc_log_buffer {
  uint64_t id;
  uint64_t user_tag;
  double perf_achieved;
  double freq_r;
  double e;
  uint32_t freq;
} dsc_log_buffer;

// Maintains logging config and state
typedef struct dsc_log_state {
  uint64_t id;
  uint32_t lb_length;
  dsc_log_buffer* lb;
  FILE* lf;
} dsc_log_state;

// The top-level context/state struct
typedef struct dsc {
  dsc_context ctx;
  dsc_log_state ls;
  uint32_t freq;
} dsc;

/**
 * Initialize the controller.
 * freqs must be an array sorted by ascending frequency value.
 */
int dsc_init(dsc* state, double perf_target, uint32_t* freqs, uint32_t nfreqs, uint32_t current_freq, double k_i);

/**
 * Destroy the controller.
 */
void dsc_destroy(dsc* state);

/**
 * Get the new DVFS frequency to apply.
 * Returns 0 and sets errno to EINVAL if state is NULL or performance <= 0.
 */
uint32_t dsc_adapt(dsc* state, uint64_t tag, double performance);

/**
 * Enable/disable logging.
 */
int dsc_set_logging(dsc* state, dsc_log_buffer* lb, uint32_t lb_length, FILE* lf);

/**
 * Change the performance target s.t.:
 * target > 0
 */
int dsc_set_performance_target(dsc* state, double target);

#ifdef __cplusplus
}
#endif

#endif
