/*
 *  goto.c
 *  testObjects
 *
 *  Created by Blaine Garst on 10/17/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */
 
// CONFIG rdar://6289031

#include <stdio.h>

int main(int argc, char *argv[])
{
	__block int val = 0;

	^{ val = 1; }();

	if (val == 0) {
		goto out_bad; // error: local byref variable val is in the scope of this goto
	}

        printf("%s: Success!\n", argv[0]);
	return 0;
out_bad:
        printf("%s: val not updated!\n", argv[0]);
	return 1;
}
