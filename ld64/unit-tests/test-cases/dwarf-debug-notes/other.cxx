
#include "header.h"

int uninit;
int init = 1;
static int suninit;
static int sinit=0;

int bar(int x)
{
	static int bar_uninit;
	static int bar_init=3;
	bar_uninit = x;
	return 20 + suninit + sinit +
		bar_init + bar_uninit + foo(x);
}


