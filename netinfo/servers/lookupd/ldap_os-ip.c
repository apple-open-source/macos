/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 *  Copyright (c) 1995 Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  os-ip.c -- platform-specific TCP & UDP related code
 */

#ifdef NOTDEF
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1995 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include <NetInfo/config.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef NeXT
#include <sys/types.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef _WIN32
#include <io.h>
#include "msdos.h"
#else /* _WIN32 */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif /* _WIN32 */
#ifdef _AIX
#include <sys/select.h>
#endif /* _AIX */
#ifdef VMS
#include "ucx_select.h"
#endif /* VMS */
#include "ldap_portable.h"
#include "lber.h"
#include "ldap_ldap-int.h"
#include "ldap.h"

#ifdef LDAP_REFERRALS
#ifdef USE_SYSCONF
#include <unistd.h>
#endif /* USE_SYSCONF */
#ifdef notyet
#ifdef NEED_FILIO
#include <sys/filio.h>
#else /* NEED_FILIO */
#include <sys/ioctl.h>
#endif /* NEED_FILIO */
#endif /* notyet */
#endif /* LDAP_REFERRALS */

#ifdef MACOS
#define tcp_close( s )		tcpclose( s )
#else /* MACOS */
#ifdef DOS
#ifdef PCNFS
#define tcp_close( s )		close( s )
#endif /* PCNFS */
#ifdef NCSA
#define tcp_close( s )		netclose( s ); netshut()
#endif /* NCSA */
#ifdef WINSOCK
#define tcp_close( s )		closesocket( s ); WSACleanup();
#endif /* WINSOCK */
#else /* DOS */
#define tcp_close( s )		close( s )
#endif /* DOS */
#endif /* MACOS */

#ifdef notdef
/*
 * Let lookupd avoid reentrancy problem.
 * 971218 lukeh
 */
#include <netdb.h>

static struct hostent *ldap_gethostbyname(const char *host, Sockbuf *sb);
static struct hostent  *ldap_gethostbyaddr(const char *addr, int len, int type, Sockbuf *sb);
struct hostent  *_old_gethostbyaddr (const char *addr, int len, int type);
struct hostent  *_old_gethostbyname (const char *name);
struct hostent  *_res_gethostbyaddr (const char *addr, int len, int type);
struct hostent  *_res_gethostbyname (const char *name);

static struct hostent *ldap_gethostbyname(const char *name, Sockbuf *sb)
{
	if (sb->sb_options & LBER_RES_NO_LOOKUPD) {
	    struct hostent *res;
    
	    res = _res_gethostbyname(name);
	    if (res != NULL) {
		    return res;
	    }
	    return _old_gethostbyname(name);
	}
	return gethostbyname(name);
}

static struct hostent  *ldap_gethostbyaddr(const char *addr, int len, int type, Sockbuf *sb)
{
	if (sb->sb_options & LBER_RES_NO_LOOKUPD) {
	    struct hostent *res;
    
	    res = _res_gethostbyaddr(addr, len, type);
	    if (res != NULL) {
		    return res;
	    }
	    return _old_gethostbyaddr(addr, len, type);
	}
	return gethostbyaddr(addr, len, type);
}

#endif

int
connect_to_host( Sockbuf *sb, char *host, unsigned long address,
	int port, int async )
/*
 * if host == NULL, connect using address
 * "address" and "port" must be in network byte order
 * zero is returned upon success, -1 if fatal error, -2 EINPROGRESS
 * async is only used ifdef LDAP_REFERRALS (non-0 means don't wait for connect)
 * XXX async is not used yet!
 */
{
	int			rc, i, s = -1, connected, use_hp;
	struct sockaddr_in	sin;
	struct hostent		*hp = NULL;
#ifdef notyet
#ifdef LDAP_REFERRALS
	int			status;	/* for ioctl call */
#endif /* LDAP_REFERRALS */
#endif /* notyet */

	Debug( LDAP_DEBUG_TRACE, "connect_to_host: %s:%d\n",
	    ( host == NULL ) ? "(by address)" : host, ntohs( port ), 0 );

	connected = use_hp = 0;

	if ( host != NULL && ( address = inet_addr( host )) == -1 ) {
#ifdef notdef
		if ( (hp = ldap_gethostbyname( host, sb )) == NULL ) {
#else
		if ( (hp = gethostbyname( host )) == NULL ) {
#endif
			errno = EHOSTUNREACH;	/* not exactly right, but... */
			return( -1 );
		}
		use_hp = 1;
	}

	rc = -1;
	for ( i = 0; !use_hp || ( hp->h_addr_list[ i ] != 0 ); i++ ) {
		if (( s = socket( AF_INET, SOCK_STREAM, 0 )) < 0 ) {
			return( -1 );
		}
#ifdef notyet
#ifdef LDAP_REFERRALS
		status = 1;
		if ( async && ioctl( s, FIONBIO, (caddr_t)&status ) == -1 ) {
			Debug( LDAP_DEBUG_ANY, "FIONBIO ioctl failed on %d\n",
			    s, 0, 0 );
		}
#endif /* LDAP_REFERRALS */
#endif /* notyet */
		(void)memset( (char *)&sin, 0, sizeof( struct sockaddr_in ));
		sin.sin_family = AF_INET;
		sin.sin_port = port;
		SAFEMEMCPY( (char *) &sin.sin_addr.s_addr,
		    ( use_hp ? (char *) hp->h_addr_list[ i ] :
		    (char *) &address ), sizeof( sin.sin_addr.s_addr) );

		if ( connect( s, (struct sockaddr *)&sin,
		    sizeof( struct sockaddr_in )) >= 0 ) {
			connected = 1;
			rc = 0;
			break;
		} else {
#ifdef notyet
#ifdef LDAP_REFERRALS
#ifdef EAGAIN
			if ( errno == EINPROGRESS || errno == EAGAIN ) {
#else /* EAGAIN */
			if ( errno == EINPROGRESS ) {
#endif /* EAGAIN */
				Debug( LDAP_DEBUG_TRACE,
					"connect would block...\n", 0, 0, 0 );
				rc = -2;
				break;
			}
#endif /* LDAP_REFERRALS */
#endif /* notyet */

#ifdef LDAP_DEBUG		
			if ( ldap_debug & LDAP_DEBUG_TRACE ) {
				perror( (char *)inet_ntoa( sin.sin_addr ));
			}
#endif
			close( s );
			if ( !use_hp ) {
				break;
			}
		}
	}

	sb->sb_sd = s;

	if ( connected ) {
#ifdef notyet
#ifdef LDAP_REFERRALS
		status = 0;
		if ( !async && ioctl( s, FIONBIO, (caddr_t)&on ) == -1 ) {
			Debug( LDAP_DEBUG_ANY, "FIONBIO ioctl failed on %d\n",
			    s, 0, 0 );
		}
#endif /* LDAP_REFERRALS */
#endif /* notyet */

		Debug( LDAP_DEBUG_TRACE, "sd %d connected to: %s\n",
		    s, inet_ntoa( sin.sin_addr ), 0 );
	}

	return( rc );
}


void
close_connection( Sockbuf *sb )
{
    tcp_close( sb->sb_sd );
}


#ifdef KERBEROS
char *
host_connected_to( Sockbuf *sb )
{
	struct hostent		*hp;
	char			*p;
	int			len;
	struct sockaddr_in	sin;

	(void)memset( (char *)&sin, 0, sizeof( struct sockaddr_in ));
	len = sizeof( sin );
	if ( getpeername( sb->sb_sd, (struct sockaddr *)&sin, &len ) == -1 ) {
		return( NULL );
	}

	/*
	 * do a reverse lookup on the addr to get the official hostname.
	 * this is necessary for kerberos to work right, since the official
	 * hostname is used as the kerberos instance.
	 */
#ifdef notdef
	if (( hp = ldap_gethostbyaddr( (char *) &sin.sin_addr,
	    sizeof( sin.sin_addr ), AF_INET, sb )) != NULL ) {
#else
	if (( hp = gethostbyaddr( (char *) &sin.sin_addr,
	    sizeof( sin.sin_addr ), AF_INET )) != NULL ) {
#endif
		if ( hp->h_name != NULL ) {
			return( strdup( hp->h_name ));
		}
	}

	return( NULL );
}
#endif /* KERBEROS */


#ifdef LDAP_REFERRALS
/* for UNIX */
struct selectinfo {
	fd_set	si_readfds;
	fd_set	si_writefds;
	fd_set	si_use_readfds;
	fd_set	si_use_writefds;
};


void
mark_select_write( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;

	sip = (struct selectinfo *)ld->ld_selectinfo;

	if ( !FD_ISSET( sb->sb_sd, &sip->si_writefds )) {
		FD_SET( sb->sb_sd, &sip->si_writefds );
	}
}


void
mark_select_read( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;

	sip = (struct selectinfo *)ld->ld_selectinfo;

	if ( !FD_ISSET( sb->sb_sd, &sip->si_readfds )) {
		FD_SET( sb->sb_sd, &sip->si_readfds );
	}
}


void
mark_select_clear( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;

	sip = (struct selectinfo *)ld->ld_selectinfo;

	FD_CLR( sb->sb_sd, &sip->si_writefds );
	FD_CLR( sb->sb_sd, &sip->si_readfds );
}


int
is_write_ready( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;

	sip = (struct selectinfo *)ld->ld_selectinfo;

	return( FD_ISSET( sb->sb_sd, &sip->si_use_writefds ));
}


int
is_read_ready( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;

	sip = (struct selectinfo *)ld->ld_selectinfo;

	return( FD_ISSET( sb->sb_sd, &sip->si_use_readfds ));
}


void *
new_select_info()
{
	struct selectinfo	*sip;

	if (( sip = (struct selectinfo *)calloc( 1,
	    sizeof( struct selectinfo ))) != NULL ) {
		FD_ZERO( &sip->si_readfds );
		FD_ZERO( &sip->si_writefds );
	}

	return( (void *)sip );
}


void
free_select_info( void *sip )
{
	free( sip );
}


int
do_ldap_select( LDAP *ld, struct timeval *timeout )
{
	struct selectinfo	*sip;
	static int		tblsize;

	Debug( LDAP_DEBUG_TRACE, "do_ldap_select\n", 0, 0, 0 );

	if ( tblsize == 0 ) {
#ifdef USE_SYSCONF
		tblsize = sysconf( _SC_OPEN_MAX );
#else /* USE_SYSCONF */
		tblsize = getdtablesize();
#endif /* USE_SYSCONF */
	}

	sip = (struct selectinfo *)ld->ld_selectinfo;
	sip->si_use_readfds = sip->si_readfds;
	sip->si_use_writefds = sip->si_writefds;
	
	return( select( tblsize, &sip->si_use_readfds, &sip->si_use_writefds,
	    NULL, timeout ));
}
#endif /* LDAP_REFERRALS */
