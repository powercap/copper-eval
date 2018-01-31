/**
 * Local utilities.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#ifndef _COPPER_EVAL_UTIL_H_
#define _COPPER_EVAL_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

#pragma GCC visibility push(hidden)

const char* getenv_or_str(const char* name, const char* alternative);

double getenv_or_dbl(const char* name, double alternative);

uint32_t getenv_or_u32(const char* name, uint32_t alternative);

uint32_t getenv_or_u64(const char* name, uint64_t alternative);

#pragma GCC visibility pop

#ifdef __cplusplus
}
#endif

#endif
