/**
 * Utility functions.
 *
 * @author Connor Imes
 * @date 2017-03-16
 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "powercap-sc.h"
#include "powercap-sc-util.h"

psc* psc_alloc_init(double perf_target, double min_power, double max_power, double current_power, double k_i,
                    uint32_t lb_length, const char* log_filename) {
  psc* state;
  psc_log_buffer* lb = NULL;
  FILE* lf = NULL;
  int err_save;

  // create the psc struct
  state = malloc(sizeof(psc));
  if (state == NULL) {
    return NULL;
  }
  
  // allocate log buffer
  if (lb_length > 0) {
    lb = malloc(lb_length * sizeof(psc_log_buffer));
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

  if (psc_init(state, perf_target, min_power, max_power, current_power, k_i) || psc_set_logging(state, lb, lb_length, lf)) {
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

int psc_destroy_free(psc* state) {
  int ret = 0;
  if (state != NULL) {
    psc_destroy(state);
    if (state->ls.lf != NULL) {
      ret |= fclose(state->ls.lf);
    }
    free(state->ls.lb);
    free(state);
  }
  return ret;
}
