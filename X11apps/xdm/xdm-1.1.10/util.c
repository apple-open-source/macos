/*

Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * util.c
 *
 * various utility routines
 */

#include   "dm.h"
#include   "dm_error.h"

#include <X11/Xmu/SysUtil.h>	/* for XmuGetHostname */

#ifdef X_POSIX_C_SOURCE
# define _POSIX_C_SOURCE X_POSIX_C_SOURCE
# include <signal.h>
# undef _POSIX_C_SOURCE
#else
# if defined(X_NOT_POSIX) || defined(_POSIX_SOURCE)
#  include <signal.h>
# else
#  define _POSIX_SOURCE
#  include <signal.h>
#  undef _POSIX_SOURCE
# endif
#endif

#ifndef HAVE_ASPRINTF
# include <stdarg.h>
/* Allocating sprintf found in many newer libc's
 * Since xdm is single threaded, assumes arguments don't change
 * between initial length calculation and copy to result buffer.
 */
int
Asprintf(char ** ret, const char *restrict format, ...)
{
    va_list ap;
    int len;
    char buf[256];

    va_start(ap, format);
    len = vsnprintf(buf, sizeof(buf), format, ap);
    if (len >= 0) {
	*ret = malloc(len + 1);
	if (*ret) {
	    if (len < sizeof(buf)) {
		memcpy(*ret, buf, len + 1);
	    } else {
		vsnprintf(*ret, len + 1, format, ap);
	    }
	} else {
	    len = -1;
	}
    } else {
	*ret = NULL;
    }
    va_end(ap);

    return len;
}
#endif /* !HAVE_ASPRINTF */

void
printEnv (char **e)
{
	while (*e)
		Debug ("%s\n", *e++);
}

static char *
makeEnv (char *name, char *value)
{
	char	*result;

	asprintf(&result, "%s=%s", name, value);

	if (!result) {
		LogOutOfMem ("makeEnv");
		return NULL;
	}
	return result;
}

char *
getEnv (char **e, char *name)
{
	int	l = strlen (name);

	if (!e) return NULL;

	while (*e) {
		if ((int)strlen (*e) > l && !strncmp (*e, name, l) &&
			(*e)[l] == '=')
			return (*e) + l + 1;
		++e;
	}
	return NULL;
}

char **
setEnv (char **e, char *name, char *value)
{
	char	**new, **old;
	char	*newe;
	int	envsize;
	int	l;

	l = strlen (name);
	newe = makeEnv (name, value);
	if (!newe) {
		LogOutOfMem ("setEnv");
		return e;
	}
	if (e) {
		for (old = e; *old; old++)
			if ((int)strlen (*old) > l && !strncmp (*old, name, l) && (*old)[l] == '=')
				break;
		if (*old) {
			free (*old);
			*old = newe;
			return e;
		}
		envsize = old - e;
		new = (char **) realloc ((char *) e,
				(unsigned) ((envsize + 2) * sizeof (char *)));
	} else {
		envsize = 0;
		new = (char **) malloc (2 * sizeof (char *));
	}
	if (!new) {
		LogOutOfMem ("setEnv");
		free (newe);
		return e;
	}
	new[envsize] = newe;
	new[envsize+1] = NULL;
	return new;
}

char **
putEnv(const char *string, char **env)
{
    char *v, *b, *n;
    int nl;

    if ((b = strchr(string, '=')) == NULL)
	return NULL;
    v = b + 1;

    nl = b - string;
    if ((n = malloc(nl + 1)) == NULL)
    {
	LogOutOfMem ("putAllEnv");
	return NULL;
    }

    strncpy(n, string,nl + 1);
    n[nl] = 0;

    env = setEnv(env,n,v);
    free(n);
    return env;
}

void
freeEnv (char **env)
{
    char    **e;

    if (env)
    {
    	for (e = env; *e; e++)
	    free (*e);
    	free (env);
    }
}

#define isblank(c)	((c) == ' ' || c == '\t')

char **
parseArgs (char **argv, char *string)
{
	char	*word;
	char	*save;
	char    **newargv;
	int	i;

	i = 0;
	while (argv && argv[i])
		++i;
	if (!argv) {
		argv = (char **) malloc (sizeof (char *));
		if (!argv) {
			LogOutOfMem ("parseArgs");
			return NULL;
		}
	}
	word = string;
	for (;;) {
		if (!*string || isblank (*string)) {
			if (word != string) {
				newargv = (char **) realloc ((char *) argv,
					(unsigned) ((i + 2) * sizeof (char *)));
				save = malloc ((unsigned) (string - word + 1));
				if (!newargv || !save) {
					LogOutOfMem ("parseArgs");
					free ((char *) argv);
					if (save)
						free (save);
					return NULL;
				} else {
				    argv = newargv;
				}
				argv[i] = strncpy (save, word, string-word);
				argv[i][string-word] = '\0';
				i++;
			}
			if (!*string)
				break;
			word = string + 1;
		}
		++string;
	}
	argv[i] = NULL;
	return argv;
}

void
freeArgs (char **argv)
{
    char    **a;

    if (!argv)
	return;

    for (a = argv; *a; a++)
	free (*a);
    free ((char *) argv);
}

void
CleanUpChild (void)
{
	setsid();
#ifdef SIGCHLD
	(void) Signal (SIGCHLD, SIG_DFL);
#endif
	(void) Signal (SIGTERM, SIG_DFL);
	(void) Signal (SIGPIPE, SIG_DFL);
	(void) Signal (SIGALRM, SIG_DFL);
	(void) Signal (SIGHUP, SIG_DFL);
	CloseOnFork ();
}

static char localHostbuf[256];
static int  gotLocalHostname;

char *
localHostname (void)
{
    if (!gotLocalHostname)
    {
	XmuGetHostname (localHostbuf, sizeof (localHostbuf) - 1);
	gotLocalHostname = 1;
    }
    return localHostbuf;
}

SIGVAL (*Signal (int sig, SIGFUNC handler))(int)
{
#ifdef HAVE_SIGACTION
    struct sigaction sigact, osigact;
    sigact.sa_handler = handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(sig, &sigact, &osigact);
    return osigact.sa_handler;
#else /* __UNIXOS2__ */
    return signal(sig, handler);
#endif
}
