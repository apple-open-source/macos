/*
 * $XFree86: xc/programs/xdm/dm_auth.h,v 1.4 2003/10/27 21:39:06 herrb Exp $
 */

/************************************************************

Copyright 1998 by Thomas E. Dickey <dickey@clark.net>

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

********************************************************/

#ifndef _DM_AUTH_H_
#define _DM_AUTH_H_ 1

#include "dm.h"	/* for struct display */

extern void	MitInitAuth (unsigned short name_len, char *name);
extern Xauth	*MitGetAuth (unsigned short namelen, char *name);

#ifdef HASXDMAUTH
extern void	XdmInitAuth (unsigned short name_len, char *name);
extern Xauth	*XdmGetAuth (unsigned short namelen, char *name);
#ifdef XDMCP
extern void	XdmGetXdmcpAuth (
    struct protoDisplay	*pdpy,
    unsigned short	authorizationNameLen,
    char		*authorizationName);
extern int XdmCheckAuthentication (struct protoDisplay *pdpy, 
    ARRAY8Ptr displayID, ARRAY8Ptr authenticationName, 
    ARRAY8Ptr authenticationData);
#else
#define XdmGetXdmcpAuth NULL
#endif
#endif

#ifdef SECURE_RPC
extern void	SecureRPCInitAuth (unsigned short name_len, char *name);
extern Xauth	*SecureRPCGetAuth (unsigned short namelen, char *name);
#endif

#ifdef K5AUTH
extern void	Krb5InitAuth (unsigned short name_len, char *name);
extern Xauth	*Krb5GetAuth (unsigned short namelen, char *name);
#endif

/* auth.c */
extern int ValidAuthorization (unsigned short name_length, char *name);


#ifdef XDMCP

extern void
SetProtoDisplayAuthorization (
    struct protoDisplay	*pdpy,
    unsigned short	authorizationNameLen,
    char		*authorizationName);

#endif /* XDMCP */

extern int SaveServerAuthorizations (struct display *d, Xauth **auths, int count);
extern void CleanUpFileName (char *src, char *dst, int len);
extern void RemoveUserAuthorization (struct display *d, struct verify_info *verify);
extern void SetAuthorization (struct display *d);
extern void SetLocalAuthorization (struct display *d);
extern void SetUserAuthorization (struct display *d, struct verify_info *verify);

/* genauth.c */
extern int GenerateAuthData (char *auth, int len);
#if !defined(ARC4_RANDOM)
extern void AddPreGetEntropy (void);
extern void AddOtherEntropy (void);
extern void AddTimerEntropy (void);
#endif

/* prngc.c */
extern int get_prngd_bytes(char *, int, unsigned short, char *);

#endif /* _DM_AUTH_H_ */
