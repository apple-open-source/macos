#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <notify.h>

int main(int argc, char *argv[])
{
	int t, n, i;
	uint32_t status;
	mach_port_t port = MACH_PORT_NULL;
	const char *name = "com.apple.notify.many.dups.test";
	char tbuf[32];
	time_t now;

	n = 50000;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-n")) n = atoi(argv[++i]);
		else name = argv[i];
	}

	status = notify_register_mach_port(name, &port, 0, &t);
	for (i = 1; i < n; i++)
	{
		status = notify_register_mach_port(name, &port, NOTIFY_REUSE, &t);
		if (status != NOTIFY_STATUS_OK)
		{
			fprintf(stderr, "registration status %d on iteration %d\n", status, i);
			return -1;
		}
	}

	now = time(NULL);
	ctime_r(&now, tbuf);
	tbuf[19] = '\0';
	
	printf("%s: registered %d times for name %s\n", tbuf, n, name);
	return 0;
}
