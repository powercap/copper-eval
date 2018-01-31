/**
 * Powercap Simple Controller lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2017-03-16
 */
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "copper-eval-psc.h"
#include "psc/powercap-sc.h"
#include "psc/powercap-sc-util.h"

static psc* psc_ctx = NULL;
static double powercap = 0;

int copper_eval_psc_init(double perf_target, double min_power, double max_power, double start_power) {
  psc_ctx = psc_alloc_init(perf_target, min_power, max_power, start_power, -1.0, 1, "psc.log");
  if (psc_ctx == NULL) {
    perror("psc_alloc_init");
    return -1;
  }
  powercap = start_power;
  printf("Simple Power Capping Controller init'd\n");
  return 0;
}

int copper_eval_psc_finish(void) {
  int ret = 0;
  if (psc_ctx != NULL) {
    ret |= psc_destroy_free(psc_ctx);
    psc_ctx = NULL;
  }
  printf("Simple Power Capping Controller destroyed\n");
  return ret;
}

double copper_eval_psc_iteration(uint64_t iteration, double perf) {
  assert(psc_ctx != NULL);
  powercap = psc_adapt(psc_ctx, iteration, perf);
  if (powercap <= 0) {
    perror("psc_adapt");
  }
  return powercap;
}

double copper_eval_psc_get_curr_powercap(void) {
  return powercap;
}
