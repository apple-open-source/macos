/* APPLE LOCAL 64bit long long constant */
/* { dg-do run } */
/* { dg-options "-O2 -mpowerpc64" } */

#include <stdio.h>

extern void abort();

int main (int argc, const char * argv[]) {
	long long k,p=0;
	
	for(k=0; k<10000000000LL; k++)
		p++;
	if (p != k)
	  abort();
	return 0;
}


