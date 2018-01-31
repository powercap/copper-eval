/**
 * Utilities for evaluating CoPPer.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// dependency headers
#include <heartbeat-acc-pow.h>
#include <heartbeats-simple-classic.h>
// local headers
#include "copper-eval.h"
#include "copper-eval-copper.h"
#include "copper-eval-cpufreq-bindings.h"
#include "copper-eval-dsc.h"
#include "copper-eval-poet.h"
#include "copper-eval-psc.h"
#include "copper-eval-raplcap.h"
#include "copper-eval-util.h"

typedef enum controller_type {
  COPPER,
  POWERCAP_SIMPLE,
  DVFS_POET,
  DVFS_SIMPLE
} controller_type;

static controller_type controller = COPPER;
static hbsc_acc_pow_ctx heart;
static int hb_inited = 0;
static uint64_t work_interval = 1;
static double powercap_last = 0;

static double abs_dbl(double a) {
  return a >= 0 ? a : -a;
}

// a = actual, b = expected
static int equal_dbl(double a, double b) {
  return abs_dbl(a - b) < DBL_EPSILON;
}

int copper_eval_apply_powercap(double powercap) {
  if (getenv(ENV_POWERCAP_DISABLE) != NULL) {
    return 0;
  }
  return copper_eval_raplcap_apply(powercap);
}

double copper_eval_get_powercap(void) {
  return copper_eval_raplcap_get_current_powercap();
}

int copper_eval_set_dvfs_freq(uint32_t freq) {
  if (getenv(ENV_POWERCAP_DISABLE) != NULL) {
    return 0;
  }
  return copper_eval_cpufreq_set(freq);
}

static int copper_eval_init_fail(const char* msg) {
  int err_save = errno;
  if (msg != NULL) {
    perror(msg);
  }
  copper_eval_finish();
  errno = err_save;
  return -1;
}

int copper_eval_init(void) {
  uint32_t nfreqs;
  uint32_t* freqs;
  uint32_t current_freq;
  // first read shared configurations from environment
  double perf_target = getenv_or_dbl(ENV_TARGET_RATE, TARGET_RATE_DEFAULT);
  unsigned int window_size = getenv_or_u32(ENV_WINDOW_SIZE, WINDOW_SIZE_DEFAULT);
  work_interval = getenv_or_u64(ENV_WORK_INTERVAL, WORK_INTERVAL_DEFAULT);
  if (work_interval == 0) {
    fprintf(stderr, "%s must be > 0\n", ENV_WORK_INTERVAL);
    errno = EINVAL;
    return -1;
  }
  printf("perf_target=%f, window_size=%u, work_interval=%"PRIu64"\n", perf_target, window_size, work_interval);

  // now initialize heartbeats to record performance/power behavior
  if (hbsc_acc_pow_init(&heart, window_size, "heartbeat.log")) {
    perror("hbsc_acc_pow_init");
    return -1;
  }
  hb_inited = 1;
  printf("heartbeat init'd\n");

  double min_power = getenv_or_dbl(ENV_MIN_POWER, MIN_POWER_DEFAULT);
  double max_power = getenv_or_dbl(ENV_MAX_POWER, MAX_POWER_DEFAULT);
  double start_power = getenv_or_dbl(ENV_START_POWER, START_POWER_DEFAULT);

  // Now initialize the appropriate controller and its actuation mechanism
  const char* env_controller = getenv(ENV_CONTROLLER);
  if (env_controller == NULL || strncmp(env_controller, CONTROLLER_COPPER, sizeof(CONTROLLER_COPPER)) == 0) {
    printf("Using CoPPer\n");
    controller = COPPER;
    // actuation mechanism
    if (copper_eval_raplcap_init()) {
      return copper_eval_init_fail("copper_eval_raplcap_init");
    }
    // controller
    if (start_power <= 0) {
      start_power = copper_eval_get_powercap();
      if (start_power < 0) {
        return copper_eval_init_fail("copper_eval_get_powercap");
      }
    }
    if (copper_eval_copper_init(perf_target, min_power, max_power, start_power)) {
      return copper_eval_init_fail("copper_eval_copper_init");
    }
    return 0;
  }
  if (strncmp(env_controller, CONTROLLER_POWERCAP_SIMPLE, sizeof(CONTROLLER_POWERCAP_SIMPLE)) == 0) {
    printf("Using Simple Power Capping Controller\n");
    controller = POWERCAP_SIMPLE;
    // actuation mechanism
    if (copper_eval_raplcap_init()) {
      return copper_eval_init_fail("copper_eval_raplcap_init");
    }
    // controller
    if (start_power <= 0) {
      start_power = copper_eval_get_powercap();
      if (start_power < 0) {
        return copper_eval_init_fail("copper_eval_get_powercap");
      }
    }
    if (copper_eval_psc_init(perf_target, min_power, max_power, start_power)) {
      return copper_eval_init_fail("copper_eval_psc_init");
    }
    return 0;
  }
  if (strncmp(env_controller, CONTROLLER_POET, sizeof(CONTROLLER_POET)) == 0) {
    printf("Using POET\n");
    controller = DVFS_POET;
    // actuation mechanism
    if (copper_eval_cpufreq_init()) {
      return copper_eval_init_fail("copper_eval_cpufreq_init");
    }
    // controller
    current_freq = copper_eval_cpufreq_get_cur_freq();
    if (current_freq == 0) {
      return copper_eval_init_fail("copper_eval_cpufreq_get_cur_freq");
    }
    if (copper_eval_poet_init(perf_target, window_size, current_freq)) {
      return copper_eval_init_fail("copper_eval_poet_init");
    }
    return 0;
  }
  if (strncmp(env_controller, CONTROLLER_DVFS_SIMPLE, sizeof(CONTROLLER_DVFS_SIMPLE)) == 0) {
    printf("Using Simple DVFS Controller\n");
    controller = DVFS_SIMPLE;
    // actuation mechanism
    if (copper_eval_cpufreq_init()) {
      return copper_eval_init_fail("copper_eval_cpufreq_init");
    }
    // controller
    nfreqs = copper_eval_cpufreq_get_available_freqs(&freqs);
    if (nfreqs == 0) {
      // shouldn't happen - the values are cached
      return copper_eval_init_fail("copper_eval_cpufreq_get_available_freqs");
    }
    current_freq = copper_eval_cpufreq_get_cur_freq();
    if (current_freq == 0) {
      return copper_eval_init_fail("copper_eval_cpufreq_get_cur_freq");
    }
    if (copper_eval_dsc_init(perf_target, freqs, nfreqs, current_freq)) {
      return copper_eval_init_fail("copper_eval_dsc_init");
    }
    return 0;
  }
  errno = EINVAL;
  fprintf(stderr, "Unknown value for %s: %s\n", ENV_CONTROLLER, env_controller);
  return copper_eval_init_fail(NULL);
}

int copper_eval_iteration(uint64_t iteration, uint64_t accuracy) {
  int ret = 0;
  uint64_t counter;
  double perf;
  double pwr;
  double powercap;
  uint32_t freq;
  // only proceed at desired work intervals to prevent issuing heartbeats and changing system settings too often
  if (iteration % work_interval == 0) {
    if (hbsc_acc_pow(&heart, iteration, work_interval, accuracy)) {
      perror("copper_eval_iteration:hbsc_acc_pow");
      fprintf(stderr, "Trying to proceed anyway...\n");
    }
    if (getenv(ENV_CHARACTERIZATION) != NULL) {
      return 0;
    }
    counter = iteration / work_interval;
    perf = hb_acc_pow_get_window_perf(hbsc_acc_pow_get_hb(&heart));
    switch (controller) {
      case COPPER:
        // only call CoPPer every window period
        if (counter != 0 && counter % hb_acc_pow_get_window_size(hbsc_acc_pow_get_hb(&heart)) == 0) {
          // get new powercap
          powercap = copper_eval_copper_iteration(iteration, perf);
          if (powercap <= 0) {
            perror("copper_eval_copper_iteration");
            ret = -1;
          } else if (!equal_dbl(powercap, powercap_last)) {
            // apply new powercap
            ret = copper_eval_apply_powercap(powercap);
            if (ret) {
              perror("copper_eval_apply_powercap");
            }
            powercap_last = powercap;
          }
        }
        break;
      case POWERCAP_SIMPLE:
        // only call the power capping controller every window period
        if (counter != 0 && counter % hb_acc_pow_get_window_size(hbsc_acc_pow_get_hb(&heart)) == 0) {
          // get new powercap
          powercap = copper_eval_psc_iteration(iteration, perf);
          if (powercap <= 0) {
            perror("copper_eval_psc_iteration");
            ret = -1;
          } else if (!equal_dbl(powercap, powercap_last)) {
            // apply new powercap
            ret = copper_eval_apply_powercap(powercap);
            if (ret) {
              perror("copper_eval_apply_powercap");
            }
            powercap_last = powercap;
          }
        }
        break;
      case DVFS_POET:
        // We must call POET at every heartbeat
        // It computes new DVFS schedules at every window period and handles actuation through a callback function
        pwr = hb_acc_pow_get_window_power(hbsc_acc_pow_get_hb(&heart));
        copper_eval_poet_iteration(counter, perf, pwr);
        break;
      case DVFS_SIMPLE:
        // only call the DVFS controller every window period
        if (counter != 0 && counter % hb_acc_pow_get_window_size(hbsc_acc_pow_get_hb(&heart)) == 0) {
          // get new frequency
          freq = copper_eval_dsc_iteration(iteration, perf);
          if (freq == 0) {
            perror("copper_eval_dsc_iteration");
            ret = -1;
          } else {
            // apply new frequency
            ret = copper_eval_cpufreq_set(freq);
            if (ret) {
              perror("copper_eval_cpufreq_set");
            }
          }
        }
        break;
      default:
        assert(0);
    }
  }
  return ret;
}

int copper_eval_finish(void) {
  int ret = 0;
  switch (controller) {
    case COPPER:
      ret |= copper_eval_copper_finish();
      ret |= copper_eval_raplcap_finish();
      break;
    case POWERCAP_SIMPLE:
      ret |= copper_eval_psc_finish();
      ret |= copper_eval_raplcap_finish();
      break;
    case DVFS_POET:
      copper_eval_poet_finish();
      copper_eval_cpufreq_finish();
      break;
    case DVFS_SIMPLE:
      ret |= copper_eval_dsc_finish();
      copper_eval_cpufreq_finish();
      break;
    default:
      assert(0);
  }
  if (hb_inited) {
    ret |= hbsc_acc_pow_finish(&heart);
    hb_inited = 0;
    printf("heartbeat finished\n");
  }
  return ret;
}
