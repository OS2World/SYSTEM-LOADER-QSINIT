//
// QSINIT API
//
#ifndef QSINIT_MATH_H
#define QSINIT_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

/* too many funcs to add here, but common FPU handling is ready and 
   supports multi-thread mode */

const double* _std _get_HugeVal_ptr(void);
#define HUGE_VAL (*_get_HugeVal_ptr())

double _std frexp(double value, int *exp);

double _std cos  (double value);
double _std sin  (double value);
double _std tan  (double value);
double _std sqrt (double value);
double _std fabs (double value);
double _std atan (double value);

#ifdef __WATCOMC__
// the only real intrinsic on watcom, anything else wants runtime code
#pragma intrinsic(cos,sin,tan,sqrt,fabs,atan)
#endif

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MATH_H
