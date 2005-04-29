
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifndef HAVE_STRERROR
#include <errno.h>

extern int sys_nerr;
extern char *sys_errlist[];

char *
strerror(int errorNumber)
{
        if ( errorNumber > 0 &&  errorNumber < sys_nerr )
        {
                return sys_errlist[errorNumber];
        }
        else
        {
                return "";
        }
}
#endif
