/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
#include <unistd.h>
#include <stdarg.h>
#include <sys/sem.h>
#include <sys/syscall.h>

/*
 * Stub function to account for the differences in the ipc_perm structure,
 * while maintaining binary backward compatibility.
 */
int
semctl(int semid, int semnum, int cmd, ...)
{
#ifdef __DARWIN_UNIX03
	va_list			ap;
	struct __semid_ds_new	*ds;

	va_start(ap, cmd);
	ds = va_arg(ap, struct __semid_ds_new *);
	va_end(ap);

	return syscall(SYS_semctl, semid, semnum, cmd, ds);
#else	/* !__DARWIN_UNIX03 */
	va_list			ap;
	struct __semid_ds_old	*ds_old;
	struct __semid_ds_new	ds;
	struct __semid_ds_new	*ds_new = &ds;
	int			rv;

	va_start(ap, cmd);
	ds_old = va_arg(ap, struct __semid_ds_old *);
	va_end(ap);

#define	_UP_CVT(x)	ds_new-> x = ds_old-> x
#define	_DN_CVT(x)	ds_old-> x = ds_new-> x

	if (cmd == IPC_SET) {
		/* convert before call */
		_UP_CVT(sem_perm.uid);
		_UP_CVT(sem_perm.gid);
		_UP_CVT(sem_perm.cuid);
		_UP_CVT(sem_perm.cgid);
		_UP_CVT(sem_perm.mode);
		ds_new->sem_perm._seq = ds_old->sem_perm.seq;
		ds_new->sem_perm._key = ds_old->sem_perm.key;
		_UP_CVT(sem_base);
		_UP_CVT(sem_nsems);
		_UP_CVT(sem_otime);
		_UP_CVT(sem_pad1);	/* binary compatibility */
		_UP_CVT(sem_ctime);
		_UP_CVT(sem_pad2);	/* binary compatibility */
		_UP_CVT(sem_pad3[0]);	/* binary compatibility */
		_UP_CVT(sem_pad3[1]);	/* binary compatibility */
		_UP_CVT(sem_pad3[2]);	/* binary compatibility */
		_UP_CVT(sem_pad3[3]);	/* binary compatibility */
	}

	rv = syscall(SYS_semctl, semid, semnum, cmd, &ds);

	if (cmd == IPC_STAT) {
		/* convert after call */
		_DN_CVT(sem_perm.uid);	/* warning!  precision loss! */
		_DN_CVT(sem_perm.gid);	/* warning!  precision loss! */
		_DN_CVT(sem_perm.cuid);	/* warning!  precision loss! */
		_DN_CVT(sem_perm.cgid);	/* warning!  precision loss! */
		_DN_CVT(sem_perm.mode);
		ds_new->sem_perm.seq = ds_old->sem_perm._seq;
		ds_new->sem_perm.key = ds_old->sem_perm._key;
		_DN_CVT(sem_base);
		_DN_CVT(sem_nsems);
		_DN_CVT(sem_otime);
		_DN_CVT(sem_pad1);	/* binary compatibility */
		_DN_CVT(sem_ctime);
		_DN_CVT(sem_pad2);	/* binary compatibility */
		_DN_CVT(sem_pad3[0]);	/* binary compatibility */
		_DN_CVT(sem_pad3[1]);	/* binary compatibility */
		_DN_CVT(sem_pad3[2]);	/* binary compatibility */
		_DN_CVT(sem_pad3[3]);	/* binary compatibility */
	}

	return (rv);
#endif	/* !__DARWIN_UNIX03 */
}
