/*
 * @OSF_COPYRIGHT@
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *	File:	error_codes.c
 *	Author:	Douglas Orr, Carnegie Mellon University
 *	Date:	Mar, 1988
 *
 *      Generic error code interface
 */

#include <mach/error.h>
#include <mach/errorlib.h>
#include "err_kern.sub"
#include "err_us.sub"
#include "err_server.sub"
#include "err_ipc.sub"
#include "err_mach_ipc.sub"

struct error_system errors[err_max_system+1] = {
	/* 0; err_kern */
	{
		errlib_count(err_os_sub),
		(char *)"(operating system/?) unknown subsystem error",
		err_os_sub,
	},
	/* 1; err_us */
	{
		errlib_count(err_us_sub),
		(char *)"(user space/?) unknown subsystem error",
		err_us_sub,
	},
	/* 2; err_server */
	{
		errlib_count(err_server_sub),
		(char *)"(server/?) unknown subsystem error",
		err_server_sub,
	},
	/* 3 (& 3f); err_ipc */
	{
		errlib_count(err_ipc_sub),
		(char *)"(ipc/?) unknown subsystem error",
		err_ipc_sub,
	},
	/* 4; err_mach_ipc */
	{
		errlib_count(err_mach_ipc_sub),
		(char *)"(ipc/?) unknown subsystem error",
		err_mach_ipc_sub,
	},
};

int error_system_count = errlib_count(errors);
