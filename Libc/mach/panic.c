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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
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
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <stdarg.h>
#include <stdio.h>

static mach_port_t master_host_port;

void
panic_init(mach_port_t port)
{
	master_host_port = port;
}

/*VARARGS1*/
void
panic(const char *s, ...)
{
	va_list listp;

	printf("panic: ");
	va_start(listp, s);
	vprintf(s, listp);
	va_end(listp);
	printf("\n");

#define RB_DEBUGGER	0x1000	/* enter debugger NOW */
	(void) host_reboot(master_host_port, RB_DEBUGGER);
}
