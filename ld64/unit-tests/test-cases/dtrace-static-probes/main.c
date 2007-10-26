
#include <stdio.h>

typedef int weirdType;
typedef int weirdType2;

#include "foo.h"
#include "bar.h"


int deadwood()
{
	BAR_COUNT1(2);
}


int main() {
	int a = 1;

	while(a) {
		FOO_COUNT1(1);
		printf("test\n");
		BAR_COUNT1(2);
		sleep(1);
	}

	return 0;
}
