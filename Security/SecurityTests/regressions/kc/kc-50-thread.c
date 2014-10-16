#include <stdlib.h>
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <sys/time.h>
#include "testmore.h"
#include "testenv.h"
#include "testleaks.h"
#include <pthread.h>

const double kTestLength = 10.0; // length of a test

#define MAXIMUM_NUMBER_OF_THREADS 100

double GetTimeOfDay()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	
	return (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
}



void* CopyDefaultAndReleaseInSingleThread(void* arg)
{
	OSStatus result;
	
	double endTime = GetTimeOfDay() + kTestLength;
	do
	{
		SecKeychainRef kc;
		result  = SecKeychainCopyDefault(&kc);
		CFRelease(kc);
		
		if (result != noErr)
		{
			return NULL;
		}
		
	} while (GetTimeOfDay() < endTime);
	
	return NULL;
}



int CopyDefaultAndDeleteInMultipleThreadsTest()
{
	const int gMax = MAXIMUM_NUMBER_OF_THREADS;
	pthread_t threads[gMax];
	
	// make the threads
	int i;
	for (i = 0; i < gMax; ++i)
	{
		pthread_create(&threads[i], NULL, CopyDefaultAndReleaseInSingleThread, NULL);
	}
	
	// wait for them to complete
	for (i = 0; i < gMax; ++i)
	{
		pthread_join(threads[i], NULL);
	}
	
	return 1;
}



int main(int argc, char* const argv[])
{
	plan_tests(2);
	ok(CopyDefaultAndDeleteInMultipleThreadsTest(), "CopyDefaultAndDeleteInMultipleThreadsTest");
	ok_leaks("kc-50-thread");
	return 0;
}

