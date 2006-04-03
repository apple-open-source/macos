#include <math.h>

#include "interface.h"


int fpclassifyF(float x)		{ return fpclassify(x); }
int fpclassifyD(double x)		{ return fpclassify(x); }
int fpclassifyL(long double x)	{ return fpclassify(x); }

int isnormalF(float x)			{ return isnormal(x); }
int isnormalD(double x)			{ return isnormal(x); }
int isnormalL(long double x)	{ return isnormal(x); }

int isfiniteF(float x)			{ return isfinite(x); }
int isfiniteD(double x)			{ return isfinite(x); }
int isfiniteL(long double x)	{ return isfinite(x); }

int isinfF(float x)				{ return isinf(x); }
int isinfD(double x)			{ return isinf(x); }
int isinfL(long double x)		{ return isinf(x); }

int isnanF(float x)				{ return isnan(x); }
int isnanD(double x)			{ return isnan(x); }
int isnanL(long double x)		{ return isnan(x); }

int signbitF(float x)			{ return signbit(x); }
int signbitD(double x)			{ return signbit(x); }
int signbitL(long double x)		{ return signbit(x); }
