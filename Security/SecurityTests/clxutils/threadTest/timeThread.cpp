/* quick time() and gmtime() tester. */
/* As of Dec. 20, 2000, this test demonstrates that stdlib time() is thread-safe, but
 * gmtime() is NOT thread safe. */
#include <time.h>
#include <stdio.h>
#include "testParams.h"
#include <security_utilities/threading.h>
#include <CoreFoundation/CFDate.h>

#define DO_CF_TIME		1
#define DO_TIME_LOCK	1

#if		DO_CF_TIME
int timeInit(TestParams *tp)
{
	return 0;
}

#define INNER_LOOPS		100

int timeThread(TestParams *tp)
{
	CFAbsoluteTime cfTimes[INNER_LOOPS];
	
	for(unsigned dex=0; dex<tp->numLoops; dex++) {
		if(tp->verbose) {
			printf("timeThread loop %u\n", dex);
		}
		else if(!tp->quiet) {
			printChar(tp->progressChar);
		}
		
		for(unsigned innerLoop=0; innerLoop<INNER_LOOPS; innerLoop++) {
			cfTimes[innerLoop] = CFAbsoluteTimeGetCurrent();
		}
	}
	return 0;
}

#else

/* process-wide time base */
static time_t baseTime = 0;
static tm baseTm;
static Mutex timeLock;

/* init time base first time thru */
int timeInit(TestParams *tp)
{
	if(baseTime != 0) {
		return 0;
	}
	baseTime = time(NULL);
	baseTm = *gmtime(&baseTime);
	return 0;
}

int timeThread(TestParams *tp)
{
	unsigned dex;
	
	for(dex=0; dex<(100 * tp->numLoops); dex++) {
		time_t nowTime;
		struct tm nowTm;
		nowTime = time(NULL);
		#if DO_TIME_LOCK
		timeLock.lock();
		#endif
		nowTm = *gmtime(&nowTime);
		#if	DO_TIME_LOCK
		timeLock.unlock();
		#endif
		if(nowTime < baseTime) {
			printf("\n***time() went backwards: base %d  now %d\n",
				(int)baseTime, (int)nowTime);
			return 1;
		}
		if((nowTm.tm_year < baseTm.tm_year) ||
		   (nowTm.tm_mon  < baseTm.tm_mon) ||
		   (nowTm.tm_mday  < baseTm.tm_mday) ||
		   /* careful, this overflows at midnight */
		   (nowTm.tm_hour  < baseTm.tm_hour)) {
			printf("\n***gmtime() went backwards\n");
			printf(" baseTm y:%d m:%d d:%d h:%d m:%d\n",
				baseTm.tm_year, baseTm.tm_mon, baseTm.tm_mday, 
				baseTm.tm_hour, baseTm.tm_min);
			printf(" nowTm  y:%d m:%d d:%d h:%d m:%d\n",
				nowTm.tm_year, nowTm.tm_mon, nowTm.tm_mday, 
				nowTm.tm_hour, nowTm.tm_min);
			return 1;
		}
		if(((dex % 100) == 0) && !tp->quiet) {
			printChar(tp->progressChar);
		}
	}
	return 0;
}

#endif	/* DO_CF_TIME */
