#include "config.h"
#ifndef HAVE_DIFFTIME
#include <sys/types.h>

double difftime( time_t a, time_t b)
{
	return (double)a - (double)b;
}
#endif
