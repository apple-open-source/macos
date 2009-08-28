
#include "header.h"

int uninit;
int init = 1;
static int custom _asm(".my_non_standard_name") = 1;
static int suninit;
static int sinit=0;
static int scustominit _asm(".my_non_standard_name_static") = 1;

int bar(int x)
{
	static int bar_uninit;
	static int bar_init=3;
	bar_uninit = x;
	scustominit = x;
	custom = x;
	return 20 + suninit + sinit +
		bar_init + bar_uninit + foo(x);
}

extern void disappear() _asm("lbegone");
void disappear() {}

extern void foo() _asm(".my_non_standard_function_name");
void foo() { disappear(); }

