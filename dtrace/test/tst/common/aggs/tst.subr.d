/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)tst.subr.d	1.1	06/08/28 SMI"

#define INTFUNC(x)			\
	BEGIN				\
	/*DSTYLED*/			\
	{				\
		subr++;			\
		@[(long)x] = sum(1);	\
	/*DSTYLED*/			\
	}

#define STRFUNC(x)			\
	BEGIN				\
	/*DSTYLED*/			\
	{				\
		subr++;			\
		@str[x] = sum(1);	\
	/*DSTYLED*/			\
	}

#define VOIDFUNC(x)			\
	BEGIN				\
	/*DSTYLED*/			\
	{				\
		subr++;			\
	/*DSTYLED*/			\
	}

INTFUNC(rand())
INTFUNC(copyin(NULL, 1))
STRFUNC(copyinstr(NULL, 1))
INTFUNC(speculation())
INTFUNC(progenyof($pid))
INTFUNC(strlen("fooey"))
VOIDFUNC(copyout)
VOIDFUNC(copyoutstr)
INTFUNC(alloca(10))
VOIDFUNC(bcopy)
VOIDFUNC(copyinto)
INTFUNC(getmajor(0))
INTFUNC(getminor(0))
STRFUNC(strjoin("foo", "bar"))
STRFUNC(lltostr(12373))
STRFUNC(basename("/var/crash/systemtap"))
STRFUNC(dirname("/var/crash/systemtap"))
STRFUNC(cleanpath("/var/crash/systemtap"))
STRFUNC(strchr("The SystemTap, The.", 't'))
STRFUNC(strrchr("The SystemTap, The.", 't'))
STRFUNC(strstr("The SystemTap, The.", "The"))
STRFUNC(strtok("The SystemTap, The.", "T"))
STRFUNC(substr("The SystemTap, The.", 0))
INTFUNC(index("The SystemTap, The.", "The"))
INTFUNC(rindex("The SystemTap, The.", "The"))

#define DIF_SUBR_MAX                    24      /* max subroutine value minus 10 Darwin omissions*/

BEGIN
/subr == DIF_SUBR_MAX + 1/
{
	exit(0);
}

BEGIN
{
	printf("found %d subroutines, expected %d\n", subr, DIF_SUBR_MAX + 1);
	exit(1);
}
