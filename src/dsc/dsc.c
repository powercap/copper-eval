/*
 * Simple DVFS Controller.
 * Picks the lowest DVFS frequency that should meet the computed speedup.
 * Assumes that frequency and speedup are analogous.
 *
 * @author Connor Imes
 * @date 2017-03-07
 */
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "dvfs-sc.h"

/**
 * If log file and buffer exist, writes "count" entries to the log file from the start of the log buffer.
 */
static void flush_log_file(FILE* lf, const dsc_log_buffer* lb, uint32_t count) {
  uint32_t i;
  for (i = 0; lb != NULL && lf != NULL && i < count; i++) {
    fprintf(lf,
            "%-16"PRIu64" %-16"PRIu64" %-16f %-16f %-16f %-16"PRIu32"\n",
            lb[i].id, lb[i].user_tag, lb[i].perf_achieved, lb[i].e, lb[i].freq_r, lb[i].freq);
  }
}

/**
 * If the circular log buffer exists, record an entry in it.
 * If the log file exists and the buffer becomes full, the buffer will flush its contents to the file.
 */
static void dsc_log(dsc* state, uint32_t tag, double perf_achieved, double freq_r, double e) {
  assert(state != NULL);
  uint32_t i;
  dsc_log_state* ls = &state->ls;
  dsc_log_buffer* lb = ls->lb;

  if (ls->lb_length > 0 && lb != NULL) {
    i = ls->id % ls->lb_length;
    lb[i].id = ls->id;
    lb[i].user_tag = tag;
    lb[i].perf_achieved = perf_achieved;
    lb[i].e = e;
    lb[i].freq_r = freq_r;
    lb[i].freq = state->freq;

    if (i == ls->lb_length - 1) {
      // flush buffer to log file (if used)
      flush_log_file(ls->lf, lb, ls->lb_length);
    }
    ls->id++;
  }
}

int dsc_init(dsc* state, double perf_target, uint32_t* freqs, uint32_t nfreqs, uint32_t current_freq, double k_i) {
  if (state == NULL || perf_target <= 0 || freqs == NULL || nfreqs == 0) {
    errno = EINVAL;
    return -1;
  }

  state->freq = current_freq;

  // set the model
  state->ctx.perf_target = perf_target;
  state->ctx.nfreqs = nfreqs;
  state->ctx.freqs = freqs;
  state->ctx.K_I = k_i;

  // no logging by default
  state->ls.id = 0;
  state->ls.lb_length = 0;
  state->ls.lb = NULL;
  state->ls.lf = NULL;

  return 0;
}

void dsc_destroy(dsc* state) {
  // flush log buffer (if used)
  if (state != NULL && state->ls.lb_length > 0) {
    flush_log_file(state->ls.lf, state->ls.lb, state->ls.id % state->ls.lb_length);
  }
}

// binary search of the frequency array
static uint32_t find_freq_idx(double freq_r, const uint32_t* freqs, uint32_t nfreqs) {
  assert(freqs != NULL);
  assert(nfreqs > 0);
  uint32_t i, l, u;
  if (freq_r <= freqs[0]) {
    // special case
    u = 0;
  } else {
    for (l = 0, u = nfreqs - 1; u - l > 1; ) {
      i = (u + l) / 2;
      if (freqs[i] >= freq_r) {
        u = i;
      } else {
        l = i;
      }
    }
    // check for an exact match at the lower index
    if (freqs[l] == (uint32_t) freq_r) {
      u = l;
    }
  }
  return u;
}

uint32_t dsc_adapt(dsc* state, uint64_t tag, double performance) {
  if (state == NULL || performance <= 0.0) {
    errno = EINVAL;
    return 0;
  }

  dsc_context* ctx = &state->ctx;
  uint32_t freq_old = state->freq;

  if (ctx->K_I < 0.0) {
    // initialize
    ctx->K_I = freq_old / performance;
  }
  double e = ctx->perf_target - performance;
  double freq_r = freq_old + (ctx->K_I * e);

  // search the frequency list to find lowest freq that supports the requested value
  uint32_t idx = find_freq_idx(freq_r, ctx->freqs, ctx->nfreqs);
  state->freq = ctx->freqs[idx];
  
  // internal logging
  dsc_log(state, tag, performance, freq_r, e);

  return state->freq;
}

int dsc_set_logging(dsc* state, dsc_log_buffer* lb, uint32_t lb_length, FILE* lf) {
  if (state == NULL) {
    errno = EINVAL;
    return -1;
  }
  if (lf != NULL) {
    // write header to log file
    if (fprintf(lf,
                "%-16s %-16s %-16s %-16s %-16s %-16s\n",
                "ID", "USER_TAG", "CONSTRAINT", "ERROR", "FREQ_R", "DVFS_FREQUENCY") < 0) {
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

int dsc_set_performance_target(dsc* state, double target) {
  if (state == NULL || target <= 0) {
    errno = EINVAL;
    return -1;
  }
  state->ctx.perf_target = target;
  return 0;
}
