/**
 * CoPPer lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#include <assert.h>
#include <errno.h>
#include <copper.h>
#include <copper-util.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "copper-eval.h"
#include "copper-eval-copper.h"
#include "copper-eval-util.h"

static copper* cop = NULL;
static double powercap = 0;

int copper_eval_copper_init(double perf_target, double min_power, double max_power, double start_power) {
  int err_save;
  const double gain_limit = getenv_or_dbl(ENV_COPPER_GAIN_LIMIT, COPPER_GAIN_LIMIT_DEFAULT);
  printf("min_power=%f, max_power=%f, start_power=%f, gain_limit=%f\n",
         min_power, max_power, start_power, gain_limit);
  // initialize
  cop = copper_alloc_init(perf_target, min_power, max_power, start_power, 1, "copper.log");
  if (cop == NULL) {
    perror("copper_alloc_init");
    return -1;
  }
  // set gain limit
  if (copper_set_gain_limit(cop, gain_limit)) {
    perror("copper_set_gain_limit");
    err_save = errno;
    copper_eval_copper_finish();
    errno = err_save;
    return -1;
  }
  powercap = start_power;
  printf("CoPPer init'd\n");
  return 0;
}

int copper_eval_copper_finish(void) {
  int ret = 0;
  if (cop != NULL) {
    ret |= copper_destroy_free(cop);
  }
  cop = NULL;
  printf("CoPPer destroyed\n");
  return ret;
}

double copper_eval_copper_iteration(uint64_t iteration, double perf) {
  assert(cop != NULL);
  powercap = copper_adapt(cop, iteration, perf);
  if (powercap <= 0) {
    perror("copper_adapt");
  }
  return powercap;
}

double copper_eval_copper_get_curr_powercap(void) {
  return powercap;
}
