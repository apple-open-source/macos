//
//  main.c
//  gk_reset_check
//
//  Created by Greg on 12/19/14.
//
//

#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>

int main(int argc, const char * argv[]) {
	// Do not override configuration profiles on users machine
	if (CFPreferencesAppValueIsForced(CFSTR("EnableAssessment"), CFSTR("com.apple.systempolicy.control")) == true ||
		CFPreferencesAppValueIsForced(CFSTR("AllowIdentifiedDevelopers"), CFSTR("com.apple.systempolicy.control")) == true) {
		return 1;
	}
    return 0;
}
