/**
 * DVFS Simple Controller lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2017-03-07
 */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "copper-eval-dsc.h"
#include "dsc/dvfs-sc.h"
#include "dsc/dvfs-sc-util.h"

static dsc* dsc_ctx = NULL;
static uint32_t freq = 0;

int copper_eval_dsc_init(double perf_target, uint32_t* freqs, uint32_t nfreqs, uint32_t current_freq) {
  dsc_ctx = dsc_alloc_init(perf_target, freqs, nfreqs, current_freq, -1.0, 1, "dsc.log");
  if (dsc_ctx == NULL) {
    perror("dsc_alloc_init");
    return -1;
  }
  freq = current_freq;
  printf("DVFS Simple Controller init'd\n");
  return 0;
}

int copper_eval_dsc_finish(void) {
  int ret = 0;
  if (dsc_ctx != NULL) {
    ret |= dsc_destroy_free(dsc_ctx);
    dsc_ctx = NULL;
  }
  printf("DVFS Simple Controller destroyed\n");
  return ret;
}

uint32_t copper_eval_dsc_iteration(uint64_t iteration, double perf) {
  assert(dsc_ctx != NULL);
  freq = dsc_adapt(dsc_ctx, iteration, perf);
  if (freq == 0) {
    perror("dsc_adapt");
  }
  return freq;
}

uint32_t copper_eval_dsc_get_curr_freq(void) {
  return freq;
}
