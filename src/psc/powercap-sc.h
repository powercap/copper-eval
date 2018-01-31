/**
 * A simple controller to meet performance targets by manipulating power caps.
 *
 * @author Connor Imes
 * @date 2017-03-16
 */

#ifndef _POWER_SIMPLE_CONTROLLER_H_
#define _POWER_SIMPLE_CONTROLLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdio.h>

// Stores user-defined parameters
typedef struct psc_context {
  double perf_target;
  double min_power;
  double max_power;
  double K_I;
} psc_context;

// Log file fields
typedef struct psc_log_buffer {
  uint64_t id;
  uint64_t user_tag;
  double perf_achieved;
  double e;
  double powercap;
} psc_log_buffer;

// Maintains logging config and state
typedef struct psc_log_state {
  uint64_t id;
  uint32_t lb_length;
  psc_log_buffer* lb;
  FILE* lf;
} psc_log_state;

// The top-level context/state struct
typedef struct psc {
  psc_context ctx;
  psc_log_state ls;
  double powercap;
} psc;

/**
 * Initialize the controller.
 * Power values must be > 0 and be ordered properly (e.g. max >= min).
 */
int psc_init(psc* state, double perf_target, double min_power, double max_power, double current_powercap, double k_i);

/**
 * Destroy the controller.
 */
void psc_destroy(psc* state);

/**
 * Get the new power cap to apply.
 * Returns 0 and sets errno to EINVAL if state is NULL or performance <= 0.
 */
double psc_adapt(psc* state, uint64_t tag, double performance);

/**
 * Enable/disable logging.
 */
int psc_set_logging(psc* state, psc_log_buffer* lb, uint32_t lb_length, FILE* lf);

/**
 * Change the performance target s.t.:
 * target > 0
 */
int psc_set_performance_target(psc* state, double target);

#ifdef __cplusplus
}
#endif

#endif
