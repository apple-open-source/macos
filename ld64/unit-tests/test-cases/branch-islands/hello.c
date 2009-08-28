#include <stdio.h>

extern void foo();

int main()
{
	fprintf(stdout, "hello\n");
    foo();
}

