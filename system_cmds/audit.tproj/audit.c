/*
 * Program to trigger the audit daemon with a message that is either:
 *    - Open a new audit log file
 *    - Read the audit control file and take action on it
 *    - Close the audit log file and exit
 *
 */

#include <mach/mach.h>
#include <servers/netname.h>
#include <mach/message.h>
#include <mach/port.h>
#include <mach/mach_error.h>
#include <mach/host_special_ports.h>
#include <servers/bootstrap.h>

#include <auditd_control.h>
#include <auditd.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

mach_port_t     serverPort;
mach_port_t     bootstrapPort;

void init();
void process(int flags);

/*
 * Main routine to process command line options.
 */
int main(int argc, char **argv)
{
	char ch;
	int flags = 0;
	while ((ch = getopt(argc, argv, "nst")) != -1) {
		switch(ch) {

		case 'n':
			flags = OPEN_NEW;
			break;

		case 's':   
			flags = READ_FILE;
			break;

		case 't':
			flags = CLOSE_AND_DIE;
			break;

		case '?':
		default:
			(void)fprintf(stderr,
				"usage: audit -n | -s | -t \n");
			exit(1);
		}
	}
	init();
	process(flags);
	return 0;
}

/*
 * Program initialization:
 *   Look up the server port and store it away.
 */
void init()
{
	if(host_get_audit_control_port(mach_host_self(), &serverPort) != KERN_SUCCESS) {
		fprintf(stderr, "Cannot get auditd_control\n");
		exit(1);
	}

	printf("Server port is %d\n", serverPort);
}

/*
 * Do all the real work.
 * Send a message to the audit daemon and check the return code.
 */
void process(int flags)
{
	kern_return_t retcode;
	retcode = auditd_control(serverPort, flags);
	if(retcode != KERN_SUCCESS) {
		mach_error("error doing IPC: ", retcode);
		exit(1);
	}
	printf("Client call successful\n");
}
