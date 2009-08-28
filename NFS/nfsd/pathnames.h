/*
 * Copyright (c) 1999-2009 Apple Inc.  All rights reserved.
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
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Herb Hasler and Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <paths.h>

#define _PATH_EXPORTS		"/etc/exports"
#define _PATH_NFS_CONF		"/etc/nfs.conf"
#define _PATH_RMOUNTLIST	"/var/db/mountdtab"
#define _PATH_MOUNTEXPLIST	"/var/db/mountdexptab"
#define _PATH_MOUNTD_PID	"/var/run/mountd.pid"
#define _PATH_NFSD_PID		"/var/run/nfsd.pid"
#define _PATH_LOCKD_PID		"/var/run/lockd.pid"
#define _PATH_RQUOTAD_PID	"/var/run/rquotad.pid"
#define _PATH_STATD_PID		"/var/run/statd.pid"
#define _PATH_STATD_NOTIFY_PID	"/var/run/statd.notify.pid"

#define _PATH_NFSD_PLIST	"/System/Library/LaunchDaemons/com.apple.nfsd.plist"
#define _PATH_LAUNCHCTL		"/bin/launchctl"
#define _PATH_RQUOTAD		"/usr/libexec/rpc.rquotad"

/* not really pathnames, but... */
#define _NFSD_SERVICE_LABEL		"com.apple.nfsd"
#define _LOCKD_SERVICE_LABEL		"com.apple.lockd"
#define _STATD_NOTIFY_SERVICE_LABEL	"com.apple.statd.notify"
#define _STATD_SERVICE_LABEL		"com.apple.statd"
#define _RQUOTAD_SERVICE_LABEL		"com.apple.rquotad"

