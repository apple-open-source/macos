/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * These are routines typically defined in libc
 * that are replaced by NetInfo.
 *
 * Copyright (c) 1995,  NeXT Computer Inc.
 */

#ifndef _LU_OVERRIDES_H_
#define	_LU_OVERRIDES_H_

#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS
struct passwd *_old_getpwnam __P((const char *));
struct passwd *_old_getpwuid __P((uid_t));
struct passwd *_old_getpwent __P((void));
int _old_setpwent __P((void));
void _old_endpwent __P((void));
int _old_putpwpasswd(); /*XXX*/

struct group *_old_getgrnam __P((const char *));
struct group *_old_getgrgid __P((gid_t));
int _old_setgrent __P((void));
struct group *_old_getgrent __P((void));
void _old_endgrent __P((void));

struct hostent *_old_gethostbyname __P((const char *));
struct hostent *_old_gethostbyaddr __P((const char *, int, int));
void _old_sethostent __P((int));
struct hostent *_old_gethostent __P((void));
void _old_endhostent __P((void));
void _old_sethostfile __P((const char *));

struct netent *_old_getnetbyname();
struct netent *_old_getnetbyaddr();
void _old_setnetent();
struct netent *_old_getnetent();
void _old_endnetent();

struct servent *_old_getservbyname __P((const char *, const char *));
struct servent *_old_getservbyport __P((int, const char *));
void _old_setservent __P((int));
struct servent *_old_getservent __P((void));
void _old_endservent __P((void));

struct protoent *_old_getprotobyname __P((const char *));
struct protoent *_old_getprotobynumber __P((int));
void _old_setprotoent __P((int));
struct protoent *_old_getprotoent __P((void));
void _old_endprotoent __P((void));;

struct rpcent *_old_getrpcbyname();
struct rpcent *_old_getrpcbynumber();
void _old_setrpcent();
struct rpcent *_old_getrpcent();
void _old_endrpcent();

struct fstab *_old_getfsent __P((void));
struct fstab *_old_getfsspec __P((const char *));
struct fstab *_old_getfsfile __P((const char *));
int _old_setfsent __P((void));
void _old_endfsent __P((void));

struct prdb_ent *_old_prdb_getbyname __P((const char *));
void _old_prdb_set __P((void));
struct prdb_ent *_old_prdb_get __P((void));
void _old_prdb_end __P((void));

struct aliasent *_old_alias_getbyname __P((const char *));
void _old_alias_setent __P((void));
struct aliasent *_old_alias_getent __P((void));
void _old_alias_endent __P((void));

int _old_innetgr __P((const char *,const char *,const char *,const char *));
void _old_setnetgrent __P((const char *));
struct netgrent *_old_getnetgrent __P((void));
void _old_endnetgrent __P((void));

int _old_initgroups();
__END_DECLS

#endif /* !_LU_OVERRIDES_H_ */
