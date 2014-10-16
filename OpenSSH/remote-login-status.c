//
//  remote-login-status.c
//  OpenSSH
//
// Returns the status of the "Remote Login" sharing preference.
//

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <ServiceManagement/ServiceManagement_Private.h>

int
main(int argc, char *argv[])
{
		int quiet = 0;
		Boolean persistent;
		
		int enabled =  SMJobIsEnabled(kSMDomainSystemLaunchd, CFSTR("com.openssh.sshd"), &persistent);
		
		if ((argc == 2) && strncmp(argv[1], "-q", 3)) {
				quiet = 1;
		}
		
		if (!quiet) {
				printf("Remote Login: %s\n", (enabled ? "on": "off"));
		}
		
		return enabled;
}
