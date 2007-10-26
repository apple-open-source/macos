/* Copyright (c) 1993
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 ****************************************************************
 */


/* Implementation of vsnprintf() for systems that don't have it
 * (e.g. Solaris 2.5).  This hasn't been tested much in the context of
 * Mailman; it was ripped from screen 3.7.6's misc.c file which contains
 * the above copyright.
 *
 * This code has been modified slightly:
 *
 * - use prototypes unconditionally
 * - Don't use macros for stdargs calls
 * - Reformat to Python C standard
 *
 * RMS says it's okay to include this code in Mailman but it should be kept
 * in a separate file.
 *
 * TBD: This file needs a security audit.
 */

#ifndef HAVE_VSNPRINTF
#include <strings.h>
#include <stdarg.h>

int vsnprintf(char* s, size_t n, const char* fmt, va_list stack)
{
	char *f, *sf = 0;
	int i, on, argl = 0;
	char myf[10], buf[20];
	char *arg, *myfp;

	on = n;
	f = (char*)fmt;
	arg = 0;
	while (arg || (sf = index(f, '%')) || (sf = f + strlen(f))) {
		if (arg == 0) {
			arg = f;
			argl = sf - f;
		}
		if (argl) {
			i = argl > n - 1 ? n - 1 : argl;
			strncpy(s, arg, i);
			s += i;
			n -= i;
			if (i < argl) {
				*s = 0;
				return on;
			}
		}
		arg = 0;
		if (sf == 0)
			continue;
		f = sf;
		sf = 0;
		if (!*f)
			break;
		myfp = myf;
		*myfp++ = *f++;
		while (((*f >= '0' && *f <='9') || *f == '#')
		       && myfp - myf < 8)
		{
			*myfp++ = *f++;
		}
		*myfp++ = *f;
		*myfp = 0;
		if (!*f++)
			break;
		switch(f[-1])
		{
		case '%':
			arg = "%";
			break;
		case 'c':
		case 'o':
		case 'd':
		case 'x':
			i = va_arg(stack, int);
			sprintf(buf, myf, i);
			arg = buf;
			break;
		case 's':
			arg = va_arg(stack, char *);
			if (arg == 0)
				arg = "NULL";
			break;
		default:
			arg = "";
			break;
		}
		argl = strlen(arg);
	}
	*s = 0;
	return on - n;

	va_end(stack);
}
#endif /* !HAVE_VSNPRINTF */


/*
 * Local Variables:
 * c-file-style: "python"
 * End:
 */
