/* compare CPUTimeRead() vs CFAbsoluteTimeGetCurrent() */

#include <stdio.h>
#include <stdlib.h>
#include "cputime.h"
#include <CoreFoundation/CoreFoundation.h>

#define WAIT_TIME_SEC	1.0

int main()
{
	CPUTime cputimeStart, cputimeEnd;
	CFAbsoluteTime start;

	printf("Resolution on this machine: %f nanoseconds\n",
		CPUTimeDeltaUs(0ULL, 1ULL) * 1000.0);

	start = CFAbsoluteTimeGetCurrent();
	cputimeStart = CPUTimeRead();
	while((CFAbsoluteTimeGetCurrent() - start) < WAIT_TIME_SEC) {
		;
	}
	cputimeEnd = CPUTimeRead();
	
	printf("Waited %f sec; elapsed CPUTime %f s\n",
		WAIT_TIME_SEC, CPUTimeDeltaSec(cputimeStart, cputimeEnd));
	printf("cputimeStart %08X:%08X   cputimeEnd %08X:%08X\n",
		(unsigned)(cputimeStart >> 32),
		(unsigned)(cputimeStart & 0xffffffffULL),
		(unsigned)(cputimeEnd >> 32),
		(unsigned)(cputimeEnd & 0xffffffffULL));
	printf("cputime raw delta %lu\n", 
		(unsigned long)(cputimeEnd - cputimeStart));

	return 0;
}
