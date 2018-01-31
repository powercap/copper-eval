/**
 * RAPLCap lifecycle and utilities.
 *
 * @author Connor Imes
 * @date 2016-06-21
 */
#ifndef _COPPER_EVAL_RAPLCAP_H_
#define _COPPER_EVAL_RAPLCAP_H_

#ifdef __cplusplus
extern "C" {
#endif

int copper_eval_raplcap_init(void);

int copper_eval_raplcap_finish(void);

int copper_eval_raplcap_apply(double powercap);

double copper_eval_raplcap_get_current_powercap(void);

#ifdef __cplusplus
}
#endif

#endif
