/**
 * A usage example.
 * User will need to set environment variables as appropriate (see copper-eval.h).
 *
 * @author Connor Imes
 * @date 2016-12-14
 */
#include <inttypes.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include "copper-eval.h"

int main(int argc, char** argv) {
  double start, end, total;
  uint64_t iterations;
  uint64_t i;

  if (argc == 2) {
    iterations = strtoull(argv[1], NULL, 0);
  } else {
    fprintf(stderr, "Usage: %s <iterations>\n", argv[0]);
    return 1;
  }

  // init controller
  if (copper_eval_init()) {
    perror("copper_eval_init");
    return 1;
  }

  // measure overhead of execution
  start = omp_get_wtime();
  for (i = 0; i < iterations; i++) {
    if (copper_eval_iteration(i, 0)) {
      perror("copper_eval_iteration");
      return 1;
    }
  }
  end = omp_get_wtime();
  total = end - start;
  fprintf(stdout, "%f us average iteration time\n", (total * 1000000 / iterations));

  // clean up
  if (copper_eval_finish()) {
    perror("copper_eval_finish");
    return 1;
  }

  return 0;
}
