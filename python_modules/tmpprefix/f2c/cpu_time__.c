/* f77 interface to (f90) cpu_time routine */

#include "f2c.h"
#include <mach/mach_time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef KR_headers
 integer
cpu_time__(t) real *t;
#else
 integer
cpu_time__(real *t)
#endif
{
	static real ratio;
	static int inited = 0;

	if(!inited) {
	    struct mach_timebase_info info;
	    if(mach_timebase_info(&info) != 0) return 1;
	    ratio = (real)info.numer / ((real)info.denom * NSEC_PER_SEC);
	    inited++;
	}
	*t = ratio * mach_absolute_time();
	return 0;
}
#ifdef __cplusplus
}
#endif
