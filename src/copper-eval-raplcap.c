/**
 * RAPLCap lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <raplcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "copper-eval.h"
#include "copper-eval-raplcap.h"

typedef enum rapl_constraint {
  LONG_TERM,
  SHORT_TERM
} rapl_constraint;

static raplcap rc;
static int raplcap_inited = 0;
static raplcap_zone rc_zone = RAPLCAP_ZONE_PACKAGE;
static rapl_constraint r_constraint = SHORT_TERM;

static int str_to_raplcap_zone(const char* rapl_zone, raplcap_zone* z) {
  assert(z != NULL);
  if (rapl_zone != NULL) {
    if (strncmp(rapl_zone, RAPL_ZONE_PACKAGE, sizeof(RAPL_ZONE_PACKAGE)) == 0) {
      *z = RAPLCAP_ZONE_PACKAGE;
    } else if (strncmp(rapl_zone, RAPL_ZONE_PSYS, sizeof(RAPL_ZONE_PSYS)) == 0) {
      *z = RAPLCAP_ZONE_PSYS;
    } else {
      fprintf(stderr, "Unknown RAPL zone: %s\n", rapl_zone);
      errno = EINVAL;
      return -1;
    }
  }
  return 0;
}

static int str_to_rapl_constraint(const char* constraint, rapl_constraint* c) {
  assert(c != NULL);
  if (constraint != NULL) {
    if (strncmp(constraint, RAPL_CONSTRAINT_LONG_TERM, sizeof(RAPL_CONSTRAINT_LONG_TERM)) == 0) {
      *c = LONG_TERM;
    } else if (strncmp(constraint, RAPL_CONSTRAINT_SHORT_TERM, sizeof(RAPL_CONSTRAINT_SHORT_TERM)) == 0) {
      *c = SHORT_TERM;
    } else {
      fprintf(stderr, "Unknown RAPL constraint: %s\n", constraint);
      errno = EINVAL;
      return -1;
    }
  }
  return 0;
}

int copper_eval_raplcap_init(void) {
  int err_save;
  if (raplcap_init(&rc)) {
    perror("raplcap_init");
    return -1;
  }
  raplcap_inited = 1;
  // which RAPL zone to use
  if (str_to_raplcap_zone(getenv(ENV_RAPL_ZONE), &rc_zone)) {
    goto raplcap_init_fail;
  }
  printf("Using RAPL zone: %s\n", rc_zone == RAPLCAP_ZONE_PSYS ? "Platform" : "Package");
  // check that RAPL zone is supported (socket 0 is good enough)
  if (raplcap_is_zone_supported(&rc, 0, rc_zone) <= 0) {
    if (errno) {
      perror("raplcap_is_zone_supported");
      goto raplcap_init_fail;
    }
    errno = EINVAL;
    perror("RAPL zone not supported");
    goto raplcap_init_fail;
  }
  // which constraint type to use
  if (str_to_rapl_constraint(getenv(ENV_RAPL_CONSTRAINT), &r_constraint)) {
    goto raplcap_init_fail;
  }
  printf("Using RAPL constraint: %s\n", r_constraint == LONG_TERM ? "long_term" : "short_term");
  // get socket count
  uint32_t sockets = raplcap_get_num_sockets(&rc);
  if (sockets == 0) {
    perror("raplcap_get_num_sockets");
    goto raplcap_init_fail;
  }
  // enable RAPL zone for each socket
  uint32_t i;
  for (i = 0; i < sockets; i++) {
    if (raplcap_set_zone_enabled(&rc, i, rc_zone, 1)) {
      perror("raplcap_set_zone_enabled");
      goto raplcap_init_fail;
    }
  }
  printf("RAPLCap init'd\n");
  return 0;

raplcap_init_fail:
  err_save = errno;
  copper_eval_raplcap_finish();
  errno = err_save;
  return -1;
}

int copper_eval_raplcap_finish(void) {
  int ret = 0;
  if (raplcap_inited) {
    ret |= raplcap_destroy(&rc);
    raplcap_inited = 0;
  }
  printf("RAPLCap destroyed\n");
  return ret;
}

int copper_eval_raplcap_apply(double powercap) {
  assert(raplcap_inited);
  raplcap_limit rl;
  uint32_t i;
  uint32_t n = raplcap_get_num_sockets(&rc);
  if (n == 0) {
    perror("raplcap_get_num_sockets");
    return -1;
  }
#ifdef VERBOSE
  printf("Requested power cap %f for %"PRIu32" sockets\n", powercap, n);
#endif
  // share powercap evenly across sockets
  // time window of zero keeps current time window
  rl.seconds = 0.0;
  rl.watts = powercap / (double) n;
  raplcap_limit* limit_long;
  raplcap_limit* limit_short;
  switch (r_constraint) {
    case LONG_TERM:
      limit_long = &rl;
      limit_short = NULL;
      break;
    case SHORT_TERM:
    default:
      limit_long = NULL;
      limit_short = &rl;
      break;
  }
  for (i = 0; i < n; i++) {
#ifdef VERBOSE
    printf("New RAPL config for socket %"PRIu32": time=%f power=%f\n", i, rl.seconds, rl.watts);
#endif
    if (raplcap_set_limits(&rc, i, rc_zone, limit_long, limit_short)) {
      perror("raplcap_set_limits");
      return -1;
    }
  }
  return 0;
}

double copper_eval_raplcap_get_current_powercap(void) {
  assert(raplcap_inited);
  raplcap_limit rl;
  uint32_t i;
  double watts = 0;
  uint32_t n = raplcap_get_num_sockets(&rc);
  if (n == 0) {
    perror("raplcap_get_num_sockets");
    return -1;
  }
  raplcap_limit* limit_long;
  raplcap_limit* limit_short;
  switch (r_constraint) {
    case LONG_TERM:
      limit_long = &rl;
      limit_short = NULL;
      break;
    case SHORT_TERM:
    default:
      limit_long = NULL;
      limit_short = &rl;
      break;
  }
  for (i = 0; i < n; i++) {
    if (raplcap_get_limits(&rc, i, rc_zone, limit_long, limit_short)) {
      perror("raplcap_get_limits");
      return -1;
    }
    // assumes that powercap is the sum of all sockets
    watts += rl.watts;
  }
#ifdef VERBOSE
  printf("Got current power cap: %f\n", watts);
#endif
  return watts;
}
