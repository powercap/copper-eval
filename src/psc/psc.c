/*
 * Simple power cappping controller.
 * Assumes that powercap and speedup are analogous.
 *
 * @author Connor Imes
 * @date 2017-03-16
 */
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "powercap-sc.h"

/**
 * If log file and buffer exist, writes "count" entries to the log file from the start of the log buffer.
 */
static void flush_log_file(FILE* lf, const psc_log_buffer* lb, uint32_t count) {
  uint32_t i;
  for (i = 0; lb != NULL && lf != NULL && i < count; i++) {
    fprintf(lf,
            "%-16"PRIu64" %-16"PRIu64" %-16f %-16f %-16f\n",
            lb[i].id, lb[i].user_tag, lb[i].perf_achieved, lb[i].e, lb[i].powercap);
  }
}

/**
 * If the circular log buffer exists, record an entry in it.
 * If the log file exists and the buffer becomes full, the buffer will flush its contents to the file.
 */
static void psc_log(psc* state, uint32_t tag, double perf_achieved, double e) {
  assert(state != NULL);
  uint32_t i;
  psc_log_state* ls = &state->ls;
  psc_log_buffer* lb = ls->lb;

  if (ls->lb_length > 0 && lb != NULL) {
    i = ls->id % ls->lb_length;
    lb[i].id = ls->id;
    lb[i].user_tag = tag;
    lb[i].perf_achieved = perf_achieved;
    lb[i].e = e;
    lb[i].powercap = state->powercap;

    if (i == ls->lb_length - 1) {
      // flush buffer to log file (if used)
      flush_log_file(ls->lf, lb, ls->lb_length);
    }
    ls->id++;
  }
}

int psc_init(psc* state, double perf_target, double min_power, double max_power, double current_powercap, double k_i) {
  if (state == NULL || perf_target <= 0 || min_power <= 0 || max_power <= 0 ||
      min_power > max_power || current_powercap < min_power || current_powercap > max_power) {
    errno = EINVAL;
    return -1;
  }

  state->powercap = current_powercap;

  // set the context
  state->ctx.perf_target = perf_target;
  state->ctx.min_power = min_power;
  state->ctx.max_power = max_power;
  state->ctx.K_I = k_i;

  // no logging by default
  state->ls.id = 0;
  state->ls.lb_length = 0;
  state->ls.lb = NULL;
  state->ls.lf = NULL;

  return 0;
}

void psc_destroy(psc* state) {
  // flush log buffer (if used)
  if (state != NULL && state->ls.lb_length > 0) {
    flush_log_file(state->ls.lf, state->ls.lb, state->ls.id % state->ls.lb_length);
  }
}

static double clamp(double val, double min, double max) {
  return val < min ? min : (val > max ? max : val);
}

double psc_adapt(psc* state, uint64_t tag, double performance) {
  if (state == NULL || performance <= 0.0) {
    errno = EINVAL;
    return 0;
  }

  psc_context* ctx = &state->ctx;
  double powercap_old = state->powercap;

  if (ctx->K_I < 0.0) {
    // initialize
    ctx->K_I = powercap_old / performance;
  }
  double e = ctx->perf_target - performance;
  state->powercap = powercap_old + (ctx->K_I * e);

  // clamp to allowed power range
  state->powercap = clamp(state->powercap, ctx->min_power, ctx->max_power);

  // internal logging
  psc_log(state, tag, performance, e);

  return state->powercap;
}

int psc_set_logging(psc* state, psc_log_buffer* lb, uint32_t lb_length, FILE* lf) {
  if (state == NULL) {
    errno = EINVAL;
    return -1;
  }
  if (lf != NULL) {
    // write header to log file
    if (fprintf(lf,
                "%-16s %-16s %-16s %-16s %-16s\n",
                "ID", "USER_TAG", "CONSTRAINT", "ERROR", "POWERCAP") < 0) {
      return -1;
    }
  }
  // reset id (prevents writing garbage to files when logging is unset/set)
  state->ls.id = 0;
  state->ls.lb_length = lb_length;
  state->ls.lb = lb;
  state->ls.lf = lf;
  return 0;
}

int psc_set_performance_target(psc* state, double target) {
  if (state == NULL || target <= 0) {
    errno = EINVAL;
    return -1;
  }
  state->ctx.perf_target = target;
  return 0;
}
