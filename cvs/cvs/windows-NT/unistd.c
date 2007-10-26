/*
 * windows-NT/unitstd.c
 * POSIX/UNIX functions not provided by Win32 platform
 * and declared in <unistd.h> header file
 */

#include "unistd.h"

#include <stdio.h>
#include <conio.h>

#include <sys/socket.h>  /* This does: #include <windows.h> */

/* Please order functions by name if possible */



char *
getpass (const char *prompt)
{
    static char pwd_buf[128];
    size_t i;

    fputs (prompt, stderr);
    fflush (stderr);
    for (i = 0; i < sizeof (pwd_buf) - 1; ++i)
    {
	pwd_buf[i] = _getch ();
	if (pwd_buf[i] == '\r')
	    break;
    }
    pwd_buf[i] = '\0';
    fputs ("\n", stderr);
    return pwd_buf;
}



/* This is just a call to GetCurrentProcessID */
pid_t
getpid (void)
{
    return (pid_t) GetCurrentProcessId();
}



unsigned int sleep (unsigned seconds)
{
    Sleep (1000*seconds);
    return 0;
}



/*
 * Sleep at least some number of microseconds
 */
int usleep (useconds_t microseconds)
{
    if ( microseconds )
    {
	const useconds_t one_second = 1000000;
	struct timeval tv_delay;

	tv_delay.tv_sec = microseconds / one_second;
	tv_delay.tv_usec = microseconds % one_second;
	return select (0, NULL, NULL, NULL, &tv_delay);
    }
    return 0;
}
