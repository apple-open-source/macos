/*
 * @OSF_FREE_COPYRIGHT@
 * 
 */

#include <mach/mach.h>
#include <mach/mach_init.h>
#include <stdio.h>
#include <stdarg.h>

int (*vprintf_stderr_func)(const char *format, va_list ap);


/* This function allows the writing of a mach error message to an
 * application-controllable output method, the default being to
 * use printf if no other method is specified by the application.
 *
 * To override, set the global (static) function pointer vprintf_stderr to
 * a function which takes the same parameters as vprintf.
 */

int fprintf_stderr(const char *format, ...)
{
        va_list args;
	int retval;

	va_start(args, format);
	if (vprintf_stderr_func == NULL)
		retval = vprintf(format, args);
	else
		retval = (*vprintf_stderr_func)(format, args);
	va_end(args);

	return retval;
}
