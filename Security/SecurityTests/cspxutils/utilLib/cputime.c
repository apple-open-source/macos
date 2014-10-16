#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libc.h>
#include "cputime.h"

/* 
 * This returns the frequency of the TBR in cycles per second.
 */
static double GetTBRFreq(void) {
	mach_timebase_info_data_t tinfo;
	mach_timebase_info(&tinfo);
	
	double machRatio = (double)tinfo.numer / (double)tinfo.denom;
	return machRatio;
}

/*
 * Return TBR Frequency, getting it lazily once. May not be thread safe.
 */
static double TbrFreqLocal = 0.0;		// ration for NANOSECONDS
static double tbrFreq()
{
	if(TbrFreqLocal == 0.0) {
		TbrFreqLocal = GetTBRFreq();
		printf("machRatio %e\n", TbrFreqLocal);
	}
	return TbrFreqLocal;
}

// seconds
double CPUTimeDeltaSec(CPUTime from, CPUTime to)	
{
	CPUTime delta = to - from;
	return (double)delta * (tbrFreq() * (double)1e-9);
}

// milliseconds
double CPUTimeDeltaMs(CPUTime from, CPUTime to)
{
	CPUTime delta = to - from;
	return (double)delta * (tbrFreq() * (double)1e-6);
}

// microseconds
double CPUTimeDeltaUs(CPUTime from, CPUTime to)
{
	CPUTime delta = to - from;
	return (double)delta * (tbrFreq() * (double)1e-3);
}
	
/*
 * Calculate the average of an array of doubles. The lowest and highest values
 * are discarded if there are more than two samples. Typically used to get an
 * average of a set of values returned from CPUTimeDelta*().
 */
double CPUTimeAvg(
	const double *array,
	unsigned arraySize)
{
	double sum = 0;
	double lowest = array[0];
	double highest = array[0];
	
	for(unsigned dex=0; dex<arraySize; dex++) {
		double curr = array[dex];
		sum += curr;
		if(curr < lowest) {
			lowest = curr;
		}
		if(curr > highest) {
			highest = curr;
		}
	}
	if(arraySize > 2) {
		sum -= lowest;
		sum -= highest;
		arraySize -= 2;
	}
	return sum / (double)arraySize;
}
