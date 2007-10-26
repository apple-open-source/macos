/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/cdefs.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/errno.h>
#include <sys/msgbuf.h>

#include "libproc.h"

int __proc_info(int callnum, int pid, int flavor, uint64_t arg, void * buffer, int buffersize);



int 
proc_listpids(uint32_t type, uint32_t typeinfo, void *buffer, int buffersize) 
{
	int retval;
	
	if ((type == PROC_ALL_PIDS) || (type == PROC_PGRP_ONLY)  || (type == PROC_TTY_ONLY) || (type == PROC_UID_ONLY) || (type == PROC_RUID_ONLY)) {
		if ((retval = __proc_info(1, type, typeinfo,(uint64_t)0, buffer, buffersize)) == -1)
			return(0);
	} else {
		errno = EINVAL;
		retval = 0;
	}
	return(retval);
}


int 
proc_pidinfo(int pid, int flavor, uint64_t arg,  void *buffer, int buffersize)
{
	int retval;

	if ((retval = __proc_info(2, pid, flavor,  arg,  buffer, buffersize)) == -1)
		return(0);
		
	return(retval);
}


int	
proc_pidfdinfo(int pid, int fd, int flavor, void * buffer, int buffersize)
{
	int retval;

	if ((retval = __proc_info(3, pid,  flavor, (uint64_t)fd, buffer, buffersize)) == -1)
		return(0);
		
	return (retval);
}



int
proc_name(int pid, void * buffer, uint32_t buffersize)
{
	int retval = 0, len;
	struct proc_bsdinfo pbsd;
	
	
	if (buffersize < 2*MAXCOMLEN) {
		errno = ENOMEM;
		return(0);
	}

	retval = proc_pidinfo(pid, PROC_PIDTBSDINFO, (uint64_t)0, &pbsd, sizeof(struct proc_bsdinfo));
	if (retval != -1) {
		bcopy(&pbsd.pbi_name, buffer, 2* 2*MAXCOMLEN);
		len = strlen(&pbsd.pbi_name[0]);
		return(len);
	}
	return(0);
}

int 
proc_regionfilename(int pid, uint64_t address, void * buffer, uint32_t buffersize)
{
	int retval = 0, len;
	struct proc_regionwithpathinfo reginfo;
	
	if (buffersize < MAXPATHLEN) {
		errno = ENOMEM;
		return(0);
	}
	
	retval = proc_pidinfo(pid, PROC_PIDREGIONPATHINFO, (uint64_t)address, &reginfo, sizeof(struct proc_regionwithpathinfo));
	if (retval != -1) {
		len = strlen(&reginfo.prp_vip.vip_path[0]);
		if (len != 0) {
			if (len > MAXPATHLEN)
				len = MAXPATHLEN;
			bcopy(&reginfo.prp_vip.vip_path[0], buffer, len);
			return(len);
		}
		return(0);
	}
	return(0);
			
}

int
proc_kmsgbuf(void * buffer, uint32_t  buffersize)
{
	int retval;

	if ((retval = __proc_info(4, 0,  0, (uint64_t)0, buffer, buffersize)) == -1)
		return(0);
	return (retval);
}

int
proc_pidpath(int pid, void * buffer, uint32_t  buffersize)
{
	int retval, len;

	if (buffersize < PROC_PIDPATHINFO_SIZE) {
		errno = ENOMEM;
		return(0);
	}
	if (buffersize >  PROC_PIDPATHINFO_MAXSIZE) {
		errno = EOVERFLOW;
		return(0);
	}

	retval = __proc_info(2, pid, PROC_PIDPATHINFO,  (uint64_t)0,  buffer, buffersize);
	if (retval != -1) {
		len = strlen(buffer);
		return(len);
	}
	return (0);
}


int proc_libversion(int *major, int * minor)
{

	if (major != NULL)
		*major = 1;
	if (minor != NULL)
		*minor = 1;
	return(0);
}

