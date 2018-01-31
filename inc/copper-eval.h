/**
 * Utilities for evaluating CoPPer.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#ifndef _COPPER_EVAL_H_
#define _COPPER_EVAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <float.h>
#include <inttypes.h>

/*
 * Common environment variables
 */
// The performance target/goal
#define ENV_TARGET_RATE "HEART_RATE"
#define TARGET_RATE_DEFAULT DBL_MAX
// How many actual work iterations between issuing heartbeats
#define ENV_WORK_INTERVAL "WORK_INTERVAL"
#define WORK_INTERVAL_DEFAULT 1
// The window size/length (number of heartbeats in a window period)
#define ENV_WINDOW_SIZE "WINDOW_SIZE"
#define WINDOW_SIZE_DEFAULT 1

/*
 * Which controller to use
 */
#define ENV_CONTROLLER "CONTROLLER"
// Valid values for ENV_CONTROLLER
#define CONTROLLER_COPPER "COPPER"
#define CONTROLLER_POWERCAP_SIMPLE "POWERCAP_SIMPLE"
#define CONTROLLER_POET "POET"
#define CONTROLLER_DVFS_SIMPLE "DVFS_SIMPLE"

/*
 * CoPPer-specific evaluation environment variables
 */
// Specify CoPPer's cost pole value
#define ENV_COPPER_GAIN_LIMIT "COPPER_GAIN_LIMIT"
#define COPPER_GAIN_LIMIT_DEFAULT 0.0

/*
 * RAPL configuration
 */
// The RAPL zone to use
#define ENV_RAPL_ZONE "RAPL_ZONE"
// Valid values for ENV_RAPL_ZONE
#define RAPL_ZONE_PACKAGE "PACKAGE"
#define RAPL_ZONE_PSYS "PSYS"
#define ENV_RAPL_CONSTRAINT "RAPL_CONSTRAINT"
// Valid values for ENV_RAPL_ZONE
#define RAPL_CONSTRAINT_LONG_TERM "LONG_TERM"
#define RAPL_CONSTRAINT_SHORT_TERM "SHORT_TERM"

// The minimum allowed power
#define ENV_MIN_POWER "MIN_POWER"
#define MIN_POWER_DEFAULT 0.1
// The maximum allowed power
#define ENV_MAX_POWER "MAX_POWER"
#define MAX_POWER_DEFAULT 1000.0
// The starting power (read at runtime if not specified)
#define ENV_START_POWER "START_POWER"
#define START_POWER_DEFAULT 0.0
// Disable actually applying power caps
#define ENV_POWERCAP_DISABLE "POWERCAP_DISABLE"
// A characterization is running - don't run the iteration function at all
#define ENV_CHARACTERIZATION "CHARACTERIZATION"

/**
 * POET-specific configuration
 */
// POET's control config file
#define ENV_POET_CTL_CONFIG "POET_CTL_CONFIG"
#define POET_CTL_CONFIG_DEFAULT "poet_dvfs_control_config"
// POET's cpu config file
#define ENV_POET_CPU_CONFIG "POET_CPU_CONFIG"
#define POET_CPU_CONFIG_DEFAULT "poet_dvfs_cpu_config"

/**
 * Read environment variables and initialize the proper controller and support libraries.
 *
 * @return 0 on success, a negative value on error
 */
int copper_eval_init(void);

/**
 * Called at each work iteration.
 * Controllers will actuate when appropriate (typically at the end of each window period).
 *
 * @param iteration
 *  an iteration identifier, typically used in logging
 * @param accuracy
 *  an accuracy value, generally unused and can be set to any value (e.g., 0)
 * @return 0 on success, a negative value on error
 */
int copper_eval_iteration(uint64_t iteration, uint64_t accuracy);

/**
 * Cleanup.
 *
 * @return 0 on success, a negative value on error
 */
int copper_eval_finish(void);

/**
 * Applies a powercap to the system when using CoPPer.
 * This function is not usually called directly, but is exposed for testing/evaluation purposes.
 *
 * @param powercap
 * @return 0 on success, a negative value on error
 */
int copper_eval_apply_powercap(double powercap);

/**
 * Get the current powercap.
 * This function is not usually called directly, but is exposed for testing/evaluation purposes.
 *
 * @return the current powercap
 */
double copper_eval_get_powercap(void);

/**
 * Apply a DVFS setting to the system.
 * This function is not usually called directly, but is exposed for testing/evaluation purposes.
 *
 * @param freq
 * @return 0 on success, a negative value on error
 */
int copper_eval_set_dvfs_freq(uint32_t freq);

#ifdef __cplusplus
}
#endif

#endif
