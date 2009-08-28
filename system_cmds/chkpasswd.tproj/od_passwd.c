/* 
 * Copyright (c) 1998-2008 Apple Inc.  All rights reserved.
 * Portions Copyright (c) 1988 by Sun Microsystems, Inc.
 * Portions Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
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
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yppasswd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <errno.h>

#include <OpenDirectory/OpenDirectory.h>

//-------------------------------------------------------------------------------------
//	od_check_passwd
//-------------------------------------------------------------------------------------

int od_check_passwd(const char *uname, const char *domain)
{
	int	authenticated = 0;
	
	ODSessionRef	session = NULL;
	ODNodeRef		node = NULL;
	ODRecordRef		rec = NULL;
	CFStringRef		user = NULL;
	CFStringRef		location = NULL;
	CFStringRef		password = NULL;

	if (uname) user = CFStringCreateWithCString(NULL, uname, kCFStringEncodingUTF8);
	if (domain) location = CFStringCreateWithCString(NULL, domain, kCFStringEncodingUTF8);

	if (user) {
		printf("Checking password for %s.\n", uname);
		char* p = getpass("Password:");
		if (p) password = CFStringCreateWithCString(NULL, p, kCFStringEncodingUTF8);
	}

	if (password) {
		session = ODSessionCreate(NULL, NULL, NULL);
		if (session) {
			if (location) {
				node = ODNodeCreateWithName(NULL, session, location, NULL);
			} else {
				node = ODNodeCreateWithNodeType(NULL, session, kODNodeTypeAuthentication, NULL);
			}
			if (node) {
				rec = ODNodeCopyRecord(node, kODRecordTypeUsers, user, NULL, NULL);
			}
			if (rec) {
				authenticated = ODRecordVerifyPassword(rec, password, NULL);
			}
		}
	}
	
	if (!authenticated) {
		fprintf(stderr, "Sorry\n");
		exit(1);
	}

	return 0;
}




 
 
