/**
 * POET lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <poet.h>
#include <poet_config.h>
#include <stdio.h>
#include <stdlib.h>
#include "copper-eval.h"
#include "copper-eval-poet.h"
#include "copper-eval-util.h"

static poet_state* poet = NULL;
static poet_control_state_t* poet_control_states = NULL;
static poet_cpu_state_t* poet_apply_states = NULL;
static uint32_t freq = 0;

// This is much faster than POET's default apply function which uses subprocesses
static void copper_eval_poet_apply_config(void* states,
                                          unsigned int num_states,
                                          unsigned int id,
                                          unsigned int last_id) {
  (void) num_states;
  (void) last_id;
  const poet_cpu_state_t* cpu_states = (poet_cpu_state_t*) states;
#ifdef VERBOSE
  printf("copper_eval_poet_apply_config: Applying state: %u\n", id);
#endif
  // set DVFS frequencies (we never change core allocation)
  freq = cpu_states[id].freq;
  printf("POET freq: %u\n", freq);
  if (copper_eval_set_dvfs_freq(freq)) {
    perror("copper_eval_poet_apply_config:copper_eval_set_dvfs_freq");
  }
}

int copper_eval_poet_init(double perf_target, unsigned int window_size, uint32_t freq_start) {
  unsigned int poet_num_control_states = 0;
  unsigned int poet_num_system_states = 0;
  int err_save;
  // get POET control and cpu states
  const char* poet_ctl_cfg = getenv_or_str(ENV_POET_CTL_CONFIG, POET_CTL_CONFIG_DEFAULT);
  if (get_control_states(poet_ctl_cfg, &poet_control_states, &poet_num_control_states)) {
    perror(poet_ctl_cfg);
    goto poet_init_fail;
  }
  const char* poet_cpu_cfg = getenv_or_str(ENV_POET_CPU_CONFIG, POET_CPU_CONFIG_DEFAULT);
  if (get_cpu_states(poet_cpu_cfg, &poet_apply_states, &poet_num_system_states)) {
    perror(poet_cpu_cfg);
    goto poet_init_fail;
  }
  if (poet_num_control_states != poet_num_system_states) {
    errno = EINVAL;
    perror("POET control and system state counts do not align");
    goto poet_init_fail;
  }
  // finally initialize POET
  poet = poet_init(perf_target,
                   poet_num_system_states, poet_control_states, poet_apply_states,
                   &copper_eval_poet_apply_config, &get_current_cpu_state,
                   window_size, 1, "poet.log");
  if (poet == NULL) {
    perror("Failed to initialize POET");
    goto poet_init_fail;
  }
  freq = freq_start;
  printf("POET init'd\n");
  return 0;

poet_init_fail:
  err_save = errno;
  copper_eval_poet_finish();
  errno = err_save;
  if (!errno) {
    // most likely problem
    errno = EINVAL;
  }
  return -1;
}

void copper_eval_poet_finish(void) {
  if (poet != NULL) {
    poet_destroy(poet);
    poet = NULL;
    printf("POET destroyed\n");
  }
  free(poet_control_states);
  poet_control_states = NULL;
  free(poet_apply_states);
  poet_apply_states = NULL;
}

void copper_eval_poet_iteration(uint64_t counter, double perf, double pwr) {
  assert(poet != NULL);
  poet_apply_control(poet, counter, perf, pwr);
}

uint32_t copper_eval_poet_get_curr_freq(void) {
  return freq;
}
