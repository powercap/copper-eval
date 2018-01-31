// force assertions
#undef NDEBUG
#include <assert.h>
#include "copper-eval.h"

int main(int argc, char** argv) {
  (void) argc;
  (void) argv;
  assert(copper_eval_init() == 0);
  assert(copper_eval_iteration(0, 0) == 0);
  assert(copper_eval_iteration(1, 0) == 0);
  assert(copper_eval_finish() == 0);
}
