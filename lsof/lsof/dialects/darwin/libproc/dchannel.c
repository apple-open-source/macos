/*
 * dchannel.c -- Darwin channel processing functions for libproc-based lsof
 */

/*
 * Portions Copyright 2016 Apple Computer, Inc.  All rights reserved.
 *
 * Copyright 2005 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Allan Nathanson, Apple Computer, Inc., and Victor A.
 * Abell, Purdue University.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors, nor Apple Computer, Inc. nor Purdue University
 *    are responsible for any consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either
 *    by explicit claim or by omission.  Credit to the authors, Apple
 *    Computer, Inc. and Purdue University must appear in documentation
 *    and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */


#ifndef lint
static char copyright[] =
"@(#) Copyright 2016 Apple Computer, Inc. and Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dchannel.c,v 1.7 2012/04/10 16:41:04 abe Exp $";
#endif


#include "lsof.h"
#include <string.h>

static const char *
get_nexus_type_string(uint32_t type, char * buf, int buf_size)
{
	const char *	str;

	switch (type) {
	case PROC_CHANNEL_TYPE_USER_PIPE:
		str = "upipe";
		break;
	case PROC_CHANNEL_TYPE_KERNEL_PIPE:
		str = "kpipe";
		break;
	case PROC_CHANNEL_TYPE_NET_IF:
		str = "netif";
		break;
	case PROC_CHANNEL_TYPE_FLOW_SWITCH:
		str = "flowsw";
		break;
	default:
		snprintf(buf, buf_size, "?%d?", type);
		str = buf;
		break;
	}
	return (str);
}

static const char *
get_channel_flags_string(uint32_t flags, char * buf, int buf_size)
{
	int	need_space = 0;

	buf[0] = '\0';
	if ((flags & (PROC_CHANNEL_FLAGS_MONITOR)) != 0) {
		strlcat(buf, "monitor", buf_size);
		if ((flags & (PROC_CHANNEL_FLAGS_MONITOR))
		    == PROC_CHANNEL_FLAGS_MONITOR) {
			strlcat(buf, "-tx-rx", buf_size);
		} else if ((flags & PROC_CHANNEL_FLAGS_MONITOR_TX) != 0) {
			strlcat(buf, "-tx", buf_size);
		} else {
			strlcat(buf, "-rx", buf_size);
		}
		if ((flags & PROC_CHANNEL_FLAGS_MONITOR_NO_COPY) != 0) {
			strlcat(buf, "-no-copy", buf_size);
		}
		need_space = 1;
	}
	if ((flags & PROC_CHANNEL_FLAGS_EXCLUSIVE) != 0) {
		if (need_space) {
			strlcat(buf, " ", buf_size);
		}
		else {
			need_space = 1;
		}
		strlcat(buf, "exclusive", buf_size);
	}
	if ((flags & PROC_CHANNEL_FLAGS_USER_PACKET_POOL) != 0) {
		if (need_space) {
			strlcat(buf, " ", buf_size);
		}
		else {
			need_space = 1;
		}
		strlcat(buf, "user-packet-pool", buf_size);
	}
	return (buf);
}

/*
 * process_channel() -- process channel file
 */

void
process_channel(pid, fd)
	int pid;			/* PID */
	int32_t fd;			/* FD */
{
	struct channel_fdinfo ci;
	char buf[64];
	uuid_string_t instance;
	int nb;
	const char * type_string;
/*
 * Get channel information.
 */
	nb = proc_pidfdinfo(pid, fd, PROC_PIDFDCHANNELINFO, &ci, sizeof(ci));
	if (nb <= 0) {
	    (void) err2nm("channel");
	    return;
	} else if (nb < sizeof(ci)) {
	    (void) fprintf(stderr,
		"%s: PID %d, FD %d: proc_pidfdinfo(PROC_PIDFDCHANNELINFO);\n",
		Pn, pid, fd);
	    (void) fprintf(stderr,
		"      too few bytes; expected %ld, got %d\n",
		sizeof(ci), nb);
	    Exit(1);
	}
/*
 * Enter basic channel values.
 */
	(void) snpf(Lf->type, sizeof(Lf->type), "CHAN");
	Lf->inp_ty = 2;
/*
 * Enter basic file information.
 */
	enter_file_info(&ci.pfi);

/*
 * Enter channel information.
 */
	type_string = get_nexus_type_string(ci.channelinfo.chi_type,
					    buf, sizeof(buf));
	enter_dev_ch((char *)type_string);
	uuid_unparse_upper(ci.channelinfo.chi_instance, instance);
	snpf(Namech, Namechl, "%s[%d] %s",
	     instance,
	     ci.channelinfo.chi_port,
	     get_channel_flags_string(ci.channelinfo.chi_flags,
				      buf, sizeof(buf)));
	enter_nm(Namech);


}
