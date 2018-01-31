/**
 * cpufreq-bindings lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#include <assert.h>
#include <cpufreq-bindings.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// unistd.h required for definitions used in discover_cpus()
#include <unistd.h>
#include "copper-eval-cpufreq-bindings.h"

static int* setspeed_fds = NULL;
static uint32_t num_cores;
#define MAX_FREQS 50
static uint32_t freqs[MAX_FREQS];
static uint32_t nfreqs;

int copper_eval_cpufreq_set(uint32_t freq) {
  assert(setspeed_fds != NULL);
  assert(num_cores > 0);
  uint32_t i;
  int ret = 0;
#ifdef VERBOSE
  printf("Setting frequency %"PRIu32" on %"PRIu32" cores\n", freq, num_cores);
#endif
  for (i = 0; i < num_cores; i++) {
    ret |= cpufreq_bindings_set_scaling_setspeed(setspeed_fds[i], i, freq) <= 0;
  }
  return ret;
}

static uint32_t discover_cpus(void) {
  long n = 0;
#ifdef _SC_NPROCESSORS_ONLN
  n = sysconf(_SC_NPROCESSORS_ONLN);
#endif
#ifdef _SC_NPROCESSORS_CONF
  if (n < 1) {
    n = sysconf(_SC_NPROCESSORS_CONF);
  }
#endif
  return n < 0 ? 0 : (uint32_t) n;
}

static int cmp_u32(const void* a, const void* b) {
  const uint32_t* ua = (const uint32_t*) a;
  const uint32_t* ub = (const uint32_t*) b;
  return (*ua > *ub) - (*ua < *ub);
}

int copper_eval_cpufreq_init(void) {
  uint32_t i;
  int err_save;
  num_cores = discover_cpus();
  if (num_cores < 1) {
    fprintf(stderr, "Failed to get number of CPU cores");
    errno = ENODEV;
    return -1;
  }

  // get setspeed file descriptors
  setspeed_fds = calloc(num_cores, sizeof(int));
  if (setspeed_fds == NULL) {
    perror("calloc");
    return -1;
  }
  for (i = 0; i < num_cores; i++) {
    setspeed_fds[i] = cpufreq_bindings_file_open(i, CPUFREQ_BINDINGS_FILE_SCALING_SETSPEED, -1);
    if (setspeed_fds[i] < 0) {
      perror("cpufreq_bindings_file_open");
      goto cpufreq_bindings_init_fail;
    }
  }

  // cache the frequencies
  nfreqs = cpufreq_bindings_get_scaling_available_frequencies(-1, 0, freqs, MAX_FREQS);
  if (nfreqs == 0) {
    perror("cpufreq_bindings_get_scaling_available_frequencies");
    goto cpufreq_bindings_init_fail;
  }

  // sort the array for the controllers
  qsort(freqs, nfreqs, sizeof(uint32_t), cmp_u32);

  printf("DVFS actuation init'd\n");
  return 0;

cpufreq_bindings_init_fail:
  err_save = errno;
  copper_eval_cpufreq_finish();
  errno = err_save;
  return -1;
}

void copper_eval_cpufreq_finish(void) {
  uint32_t i;
  if (setspeed_fds != NULL) {
    for (i = 0; i < num_cores; i++) {
      if (setspeed_fds[i] > 0) {
        cpufreq_bindings_file_close(setspeed_fds[i]);
      }
    }
    free(setspeed_fds);
    setspeed_fds = NULL;
  }
  num_cores = 0;
  printf("DVFS actuation finished\n");
}

uint32_t copper_eval_cpufreq_get_available_freqs(uint32_t** _freqs) {
  assert(_freqs != NULL);
  *_freqs = freqs;
  return nfreqs;
}

uint32_t copper_eval_cpufreq_get_cur_freq(void) {
  assert(num_cores > 0);
  uint32_t f;
  uint32_t cur;
  uint32_t i;
  for (i = 0; i < num_cores; i++) {
    cur = cpufreq_bindings_get_scaling_cur_freq(-1, i);
    if (i == 0) {
      f = cur;
    }
    // check that all frequencies are the same
    if (cur == 0 || cur != f) {
      perror("cpufreq_bindings_get_scaling_cur_freq");
      return 0;
    }
  }
  return f;
}
