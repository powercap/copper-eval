/**
 * Utility functions.
 *
 * @author Connor Imes
 * @date 2017-03-07
 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "dvfs-sc.h"
#include "dvfs-sc-util.h"

dsc* dsc_alloc_init(double perf_target, uint32_t* freqs, uint32_t nfreqs, uint32_t current_freq, double k_i,
                    uint32_t lb_length, const char* log_filename) {
  dsc* state;
  dsc_log_buffer* lb = NULL;
  FILE* lf = NULL;
  int err_save;

  // create the dsc struct
  state = malloc(sizeof(dsc));
  if (state == NULL) {
    return NULL;
  }
  
  // allocate log buffer
  if (lb_length > 0) {
    lb = malloc(lb_length * sizeof(dsc_log_buffer));
    if (lb == NULL) {
      free(state);
      return NULL;
    }
    // Open log file
    if (log_filename != NULL) {
      lf = fopen(log_filename, "w");
      if (lf == NULL) {
        free(lb);
        free(state);
        return NULL;
      }
    }
  }

  if (dsc_init(state, perf_target, freqs, nfreqs, current_freq, k_i) || dsc_set_logging(state, lb, lb_length, lf)) {
    err_save = errno;
    if (lf != NULL) {
      fclose(lf);
    }
    free(lb);
    free(state);
    errno = err_save;
    return NULL;
  }

  return state;
}

int dsc_destroy_free(dsc* state) {
  int ret = 0;
  if (state != NULL) {
    dsc_destroy(state);
    if (state->ls.lf != NULL) {
      ret |= fclose(state->ls.lf);
    }
    free(state->ls.lb);
    free(state);
  }
  return ret;
}
