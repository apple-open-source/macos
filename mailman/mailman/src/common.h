/* common.h --- Prototypes for common routines
 *
 * Copyright (C) 1998,1999,2000,2001,2002 by the Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif /* HAVE_SYSLOG_H */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <grp.h>
#include <unistd.h>

/* GETGROUPS_T gets set in the makefile by configure */
#define GID_T GETGROUPS_T

extern void fatal(const char*, int, char*, ...);
extern void check_caller(const char*, const char*);
extern int run_script(const char*, int, char**, char**);

/* Global variable used as a flag. */
extern int running_as_cgi;

/* Extern to reference this global from one of the wrapper mains */
extern const char* logident;

/* Exit codes, so it's easier to distinguish what caused fatal errors when
 * looking at syslogs.
 */
#define GROUP_MISMATCH 2
#define SETREGID_FAILURE 3
#define EXECVE_FAILURE 4
#define MAIL_USAGE_ERROR 5
#define MAIL_ILLEGAL_COMMAND 6
#define ADDALIAS_USAGE_ERROR 7
#define GROUP_NAME_NOT_FOUND 8


/*
 * Local Variables:
 * c-file-style: "python"
 * End:
 */
