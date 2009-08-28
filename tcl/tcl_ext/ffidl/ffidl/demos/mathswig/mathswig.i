%module mathswig

// Grab a typemap for Tcl mode
%include typemaps.i

%init %{
%}

%{
#include <math.h>
%}

#ifdef __cplusplus
extern "C" {
#endif
double acos(double);
double asin(double);
double atan(double);
double atan2(double, double);
double cos(double);
double sin(double);
double tan(double);
double cosh(double);
double sinh(double);
double tanh(double);
double acosh(double);
double asinh(double);
double atanh(double);
double exp(double);
  /* double frexp(double, OUT int *); */
double ldexp(double, int);
double log(double);
double log10(double);
double expm1(double);
double log1p(double);
double logb(double);
  /* double modf(double, OUT double *); */
double pow(double, double);
double sqrt(double);
double cbrt(double);
double ceil(double);
double fabs(double);
double floor(double);
double fmod(double, double);
int isinf(double);
int finite(double);
double copysign(double, double);
double scalbn(double, int);
double drem(double, double);
double significand(double);
int isnan(double);
int ilogb(double);
double hypot(double, double);
double erf(double);
double erfc(double);
double gamma(double);
double j0(double);
double j1(double);
double jn(int, double);
double lgamma(double);
double y0(double);
double y1(double);
double yn(int, double);
  /* double gamma_r(double, OUT int *); */
  /* double lgamma_r(double, OUT int *); */
double rint(double);
double scalb(double, double);
double nextafter(double, double);
double remainder(double, double);

#ifdef __cplusplus
}
#endif


