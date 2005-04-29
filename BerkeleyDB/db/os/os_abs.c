/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2003
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: os_abs.c,v 1.2 2004/03/30 01:23:45 jtownsen Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"

/*
 * __os_abspath --
 *	Return if a path is an absolute path.
 *
 * PUBLIC: int __os_abspath __P((const char *));
 */
int
__os_abspath(path)
	const char *path;
{
	return (path[0] == '/');
}
