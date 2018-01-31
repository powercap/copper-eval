/**
 * Local utilities.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#include <inttypes.h>
#include <stdlib.h>
#include "copper-eval-util.h"

const char* getenv_or_str(const char* name, const char* alternative) {
  const char* tmp = getenv(name);
  return tmp == NULL ? alternative : tmp;
}

double getenv_or_dbl(const char* name, double alternative) {
  const char* tmp = getenv(name);
  return tmp == NULL ? alternative : atof(tmp);
}

uint32_t getenv_or_u32(const char* name, uint32_t alternative) {
  const char* tmp = getenv(name);
  return tmp == NULL ? alternative : strtoul(tmp, NULL, 0);
}

uint32_t getenv_or_u64(const char* name, uint64_t alternative) {
  const char* tmp = getenv(name);
  return tmp == NULL ? alternative : strtoull(tmp, NULL, 0);
}
