/* $Id: polltest.c,v 1.1.1.1 2003/07/21 16:20:21 zarzycki Exp $ */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "poll.h"

#define MAX	10

main (int argc, char **argv)
{
    int	    arg;
    int	    i;
    struct  pollfd array[MAX];
    int     ready;

    if ((argc == 1) || ((argc % 2) != 1) || (argc > ((MAX * 2) + 1)) )
    {
	fprintf (stderr, "Usage: %s r|w file [r|w file] ...\n", argv[0]);
	exit (1);
    }

    for (arg = 1, i = 0; arg < argc; arg += 2, i++)
    {
	int open_mode;
	short poll_mask;
	char *strMode;

	if (argv[arg][0]  == 'r')
	{
	    open_mode = O_RDONLY;
	    poll_mask = POLLIN;
	    strMode   = "reading";
	}
	else
	{
	    open_mode = O_WRONLY;
	    poll_mask = POLLOUT;
	    strMode   = "writing";
	}

	printf ("%2d: Opening \"%s\" for %s ...\n", i, argv[arg + 1], strMode);
	array[i].revents = 0;
	array[i].events |= poll_mask;
	array[i].fd      = open (argv[arg + 1], open_mode);
	if (array[i].fd == -1)
	{
	    fprintf (stderr, "Can't open \"%s\": errno %d\n",
		     argv[arg + 1], errno);
	    exit (1);
	}
    }

    printf ("Polling on %d file descriptors.  Timeout is indefinite ...\n", i);
    ready = poll (array, (unsigned long) i, -1);
    printf ("poll() returned %d.\n", ready);
    if (ready < 0)
	printf ("Errno = %d\n", errno);

    for (arg = 1, i = 0; arg < argc; arg += 2, i++)
    {
	if (array[i].revents != 0)
	{
	    printf ("%2d (\"%s\"):", i, argv[arg + 1]);

	    if (array[i].revents &  POLLPRI)
		printf (" EXCEPTION");
	    if (array[i].revents &  POLLIN)
		printf (" INPUT");
	    if (array[i].revents &  POLLOUT)
		printf (" OUTPUT");
	    putchar ('\n');
	}
    }

    exit (0);
}
