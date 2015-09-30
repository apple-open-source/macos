//
//  Copyright (c) 2011 Apple. All rights reserved.
//

#include <Security/Security.h>
#include <Security/SecTask.h>
#include <stdio.h>
#include <err.h>

int main (int argc, const char * argv[])
{
	long num = 1000;

	while (num-- > 0) {
		SecTaskRef secTask = SecTaskCreateFromSelf(NULL);
		if (secTask == NULL)
			errx(1, "SecTaskCreateFromSelf");

		CFErrorRef error = NULL;
		CFTypeRef value = SecTaskCopyValueForEntitlement(secTask, CFSTR("com.apple.security.some-entitlement"), &error);
		if (value == NULL)
			errx(1, "SecTaskCopyValueForEntitlement");

		if (num == 1)
			CFShow(value);

		CFRelease(value);
		CFRelease(secTask);
	}

	return 0;
}

