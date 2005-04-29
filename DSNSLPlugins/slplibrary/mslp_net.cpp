/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * mslp_net.c : This module contains general networking utility functions
 *       used throughout the mslp implementation.
 *
 * Version: 1.6 
 * Date:    03/27/99
 *
 * Licensee will, at its expense,  defend and indemnify Sun Microsystems,
 * Inc.  ("Sun")  and  its  licensors  from  and  against any third party
 * claims, including costs and reasonable attorneys' fees,  and be wholly
 * responsible for  any liabilities  arising  out  of  or  related to the
 * Licensee's use of the Software or Modifications.   The Software is not
 * designed  or intended for use in  on-line  control  of  aircraft,  air
 * traffic,  aircraft navigation,  or aircraft communications;  or in the
 * design, construction, operation or maintenance of any nuclear facility
 * and Sun disclaims any express or implied warranty of fitness  for such
 * uses.  THE SOFTWARE IS PROVIDED TO LICENSEE "AS IS" AND ALL EXPRESS OR
 * IMPLIED CONDITION AND WARRANTIES, INCLUDING  ANY  IMPLIED  WARRANTY OF
 * MERCHANTABILITY,   FITNESS  FOR  WARRANTIES,   INCLUDING  ANY  IMPLIED
 * WARRANTY  OF  MERCHANTABILITY,  FITNESS FOR PARTICULAR PURPOSE OR NON-
 * INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT WILL SUN BE LIABLE HEREUNDER
 * FOR ANY DIRECT DAMAGES OR ANY INDIRECT, PUNITIVE, SPECIAL, INCIDENTAL
 * OR CONSEQUENTIAL DAMAGES OF ANY KIND.
 *
 * (c) Sun Microsystems, 1998, All Rights Reserved.
 * Author: Erik Guttman
 */
 /*
	Portions Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>		// interface struture ifreq, ifconf
#include <errno.h>
#include <pthread.h>	// for pthread_*_t

#include "SLPSystemConfiguration.h"

#include <sys/types.h>
#include <time.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"

/*
 * Adapted from Stevens, Unix Network Programming, 2nd Edition.
 */

/*
 * Read n bytes from file descriptor fd.
 */
EXPORT int readn(SOCKET fd, void *pv, size_t n)
{
    int    nleft = n;
    int    nread = 0, iErr;
    char   *pc   = (char *) pv;
    fd_set fds, allset;
    struct timeval tv = { 5, 0 };
    
    FD_ZERO(&allset);
    FD_SET(fd,&allset);

    while (nleft > 0) 
    {
        fds = allset;

        iErr = select(fd+1,&fds,NULL,NULL,&tv);
        
        if (iErr < 0) 
        {
            if ( errno == EINTR )
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DROP, "propogate_registrations readn msg received EINTR");
#endif
                continue;
            }
            else
                LOG_STD_ERROR_AND_RETURN(SLP_LOG_DEBUG,"readn: select",errno);
        } 
        else if (iErr == 0) 
        {
            break;
        } 
        else 
        {    
            if ((nread = SDread(fd, pc, nleft)) < 0) 
            {
                if (errno == EINTR) 
                {
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_DROP, "readn SDread received EINTR");
#endif
                    nread = 0; /* and call read() again */
                }
                else 
                    return -1;               /* error                 */
            } 
            else if (nread == 0) 
                break;   /* EOF                   */

            nleft -= nread;
            pc    += nread;
        }
    }
  return (n - nleft);               /* return >= 0           */
}

/*
 * write n bytes to a file descriptor fd.
 */
EXPORT int writen(SOCKET fd, void *pv, size_t n)
{
    int nleft = n;
    int nwritten = 0;
    const char *pc = (char *) pv;
    
    while(nleft > 0) 
    {
        if ((nwritten = SDwrite(fd, pc, nleft)) < 0) 
        {
            if (errno == EINTR)
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DROP, "writen SDwrite received EINTR");
#endif
                nwritten = 0; /* call write() again */
            }
			else
				return -1;                   /* error              */
        }
        nleft -= nwritten;
        pc    += nwritten;
    }
    
    return n;
}

/****************
 * GetOurIPAdrs *
*****************

Determine if TCP is available.Return the IP addr of this machine.
*/
const	short	kMaxIPAddrs = 32;
pthread_mutex_t	sock_ntopLock = PTHREAD_MUTEX_INITIALIZER;;

EXPORT int GetOurIPAdrs( struct in_addr* ourIPAddr, const char** pcInterf )
{
	return SLPSystemConfiguration::TheSLPSC()->GetOurIPAdrs( ourIPAddr, pcInterf );
}

struct in_addr	gCurrentIPAddr = {0};

EXPORT int CalculateOurIPAddress( struct in_addr* ourIPAddr, const char** pcInterf )
{
    int					err = -1;
    CFStringRef			interfaceRef = CopyConfiguredInterfaceToUse();	// are we configured to use a particular interface?
    struct ifi_info		*ifi, *ifihead;
    int					family=AF_INET, doaliases=0;

    if ( pcInterf )
        *pcInterf = NULL;		// set to null by default
                
    if ( !interfaceRef )
    {
        // find out what the current primary active interface is:
        interfaceRef = CopyCurrentActivePrimaryInterfaceName();
        
#ifdef ENABLE_SLP_LOGGING
        if ( interfaceRef && CFStringGetCStringPtr( interfaceRef, CFStringGetSystemEncoding() ) )
            SLP_LOG( SLP_LOG_DEBUG, "Primary Interface is: %s", CFStringGetCStringPtr( interfaceRef, CFStringGetSystemEncoding() ) );
#endif
    }
        
    for ( ifihead = ifi = get_ifi_info(family, doaliases); ifi != NULL; ifi = ifi->ifi_next)
    {
        if ( ifi->ifi_flags & IFF_UP && ifi->ifi_flags & IFF_MULTICAST && !(ifi->ifi_flags & IFF_LOOPBACK) )
        {
            if ( interfaceRef )
            {
                Boolean			skipInterface = false;
                CFStringRef		curInterfaceRef = CFStringCreateWithCString( NULL, ifi->ifi_name, kCFStringEncodingUTF8 );
                
                if ( kCFCompareEqualTo != CFStringCompare( interfaceRef, curInterfaceRef, 0 ) )
                    skipInterface = true;
                    
                CFRelease( curInterfaceRef );
                
                if ( skipInterface )
                    continue;
            }

            // grab the first interface that is up and supports multicast and isn't the loopback address
            // I couldn't figure out the proper way to convert the ifi ptr to the in_addr address so I did it the inefficient way...
            if ( pcInterf )
                *pcInterf = strdup(ifi->ifi_name);

            pthread_mutex_lock( &sock_ntopLock );	// sock_ntop is not reentrant or threadsafe!
            *ourIPAddr = get_in_addr_by_name( sock_ntop(ifi->ifi_addr, sizeof(ifi->ifi_addr)) );
            pthread_mutex_unlock( &sock_ntopLock );

            err = 0;
            
#ifdef ENABLE_SLP_LOGGING
            pthread_mutex_lock( &sock_ntopLock );	// sock_ntop is not reentrant or threadsafe!
            SLP_LOG( SLP_LOG_DEBUG, "Returning our IP Address as: %s, on interface: %s", sock_ntop(ifi->ifi_addr, sizeof(ifi->ifi_addr)), ifi->ifi_name );
            pthread_mutex_unlock( &sock_ntopLock );
#endif
            break;		// we are just grabbing the first one.  If we want all, we should continue and fill an in_addr array
        }
    }
	
	free_ifi_info(ifihead);

    if ( interfaceRef )
        CFRelease( interfaceRef );

    return err;
}

char* sock_ntop( const struct sockaddr* sa, u_char salen )
{
    char		portstr[7];
    static char	str[128] = {0};
    
    switch (sa->sa_family)
    {
        case AF_INET:
        {
            struct sockaddr_in* sin = (struct sockaddr_in*)sa;
            
            if (slp_inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str) ) == NULL)
                return NULL;
            
            if (ntohs( sin->sin_port) != 0)
            {
                snprintf(portstr, sizeof(portstr), ".%d", ntohs(sin->sin_port));
                strcat(str, portstr);
            }
            
            return (str);
        }
        
        case AF_INET6:
        {
            errno = EAFNOSUPPORT;
            return NULL;		// not supported yet
        }
        
        default:
            errno = EAFNOSUPPORT;
            return NULL;
    }
}

const char* slp_inet_ntop( int family, const void* addrptr, char* strptr, size_t len )
{
    const u_char* p = (const u_char*)addrptr;
    
    switch ( family )
    {
        case AF_INET:
        {
            char	temp[INET_ADDRSTRLEN];
            
            snprintf(temp, sizeof(temp), "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
            
            if ( strlen(temp) >= len )
            {
                errno = ENOSPC;
                return (NULL);
            }
            
            strcpy(strptr, temp);
            
            return strptr;
        }
        break;
        
        case AF_INET6:
            errno = EAFNOSUPPORT;
            return NULL;		// not supported yet
        break;
        
        default:
            errno = EAFNOSUPPORT;
            return NULL;
    }
}

struct ifi_info* get_ifi_info( int family, int doaliases )
{
    struct ifi_info		*ifi, *ifihead, **ifipnext;
    int					sockfd, len, lastlen, flags, myflags;
    char				*buf, lastname[IFNAMSIZ], *cptr;
    struct ifconf		ifc;
    struct ifreq		*ifr, ifrcopy;
    struct sockaddr_in	*sinptr;
    
    sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
    
    if ( sockfd < 0 )
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "Couldn't create a network socket: %s while trying to get interface information", strerror(errno) );
#endif        
        return NULL;
    }
    
    lastlen = 0;
    len = 100 * sizeof(struct ifreq);		// initial buffer size guess
    for ( ; ; )
    {
        buf = (char*)malloc(len);
        ifc.ifc_len = len;
        ifc.ifc_buf = buf;
        if ( ioctl(sockfd, SIOCGIFCONF, &ifc) < 0 )
        {
#ifdef ENABLE_SLP_LOGGING
            if ( errno != EINVAL || lastlen != 0 )
                SLP_LOG( SLP_LOG_DEBUG, "System error: %s while trying to determine network information.", strerror(errno));
#endif
            break;	// can't continue
        }
        else
        {
            if ( ifc.ifc_len == lastlen )
                break;						// success, len has not changed
                
            lastlen = ifc.ifc_len;
        }

        len += 10 * sizeof(struct ifreq);	// increment
        free(buf);
    }
    
    ifihead = NULL;
    ifipnext = &ifihead;
    lastname[0] = 0;
    
    #define IFR_NEXT(ifr)   \
                                ((struct ifreq *) ((char *) (ifr) + sizeof(*(ifr)) + \
                                MAX(0, (int) (ifr)->ifr_addr.sa_len - (int) sizeof((ifr)->ifr_addr))))

    for ( ifr = (ifreq*)buf; (char*)ifr < buf + ifc.ifc_len; ifr = IFR_NEXT(ifr) )
    {
        
        if ( ifr->ifr_addr.sa_family != family )
            continue;							// ignore if not desired address family
        
        myflags = 0;
        if ( (cptr = strchr(ifr->ifr_name, ':') ) != NULL )
            *cptr = 0;							// replace colon with NULL
        
        if ( strncmp(lastname, ifr->ifr_name, IFNAMSIZ) == 0 )
        {
            if ( doaliases == 0 )
                continue;						// already processed this interface
            
            myflags = IFI_ALIAS;
        }
        
        memcpy( lastname, ifr->ifr_name, IFNAMSIZ );
        
        ifrcopy = *ifr;
        
        ioctl( sockfd, SIOCGIFFLAGS, &ifrcopy);
        flags = ifrcopy.ifr_flags;
        
        if ( (flags & IFF_UP) == 0 )
            continue;							// ignore if interface not up
            
        ifi = (ifi_info*)calloc( 1, sizeof(struct ifi_info) );
        
        *ifipnext = ifi;						// prev points to this new one
        ifipnext = &ifi->ifi_next;				//pointer to next one goes here
        
        ifi->ifi_flags = flags;					// IFF_xxx values
        ifi->ifi_myflags = myflags;				// IFI_xxx values
        
        memcpy( ifi->ifi_name, ifr->ifr_name, IFI_NAME );
        ifi->ifi_name[IFI_NAME - 1] = '\0';
        
        switch ( ifr->ifr_addr.sa_family )
        {
            case AF_INET:
                sinptr = (struct sockaddr_in *) &ifr->ifr_addr;
                if ( ifi->ifi_addr == NULL )
                {
                    ifi->ifi_addr = (struct sockaddr*)calloc( 1, sizeof(struct sockaddr_in) );
                    memcpy( ifi->ifi_addr, sinptr, sizeof(struct sockaddr_in) );
                    
#ifdef	SIOCGIFBRDADDR
                    if ( flags & IFF_BROADCAST )
                    {
                        ioctl( sockfd, SIOCGIFBRDADDR, &ifrcopy );
                        sinptr = (struct sockaddr_in*) &ifrcopy.ifr_broadaddr;
                        ifi->ifi_brdaddr = (struct sockaddr*)calloc( 1, sizeof(struct sockaddr_in) );
                        memcpy( ifi->ifi_brdaddr, sinptr, sizeof(struct sockaddr_in) );
                    }
#endif
                    
#ifdef	SIOCGIFDSTADDR
                    if ( flags & IFF_POINTOPOINT )
                    {
                        ioctl( sockfd, SIOCGIFDSTADDR, &ifrcopy );
                        sinptr = (struct sockaddr_in*) &ifrcopy.ifr_dstaddr;
                        ifi->ifi_dstaddr = (struct sockaddr*)calloc( 1, sizeof(struct sockaddr_in) );
                        memcpy( ifi->ifi_dstaddr, sinptr, sizeof(struct sockaddr_in) );
                    }
#endif
                }
            
            default:
                break;
        }
    }
    
    free(buf);
    close(sockfd);
    
    return (ifihead);
}

void free_ifi_info( struct ifi_info* ifihead )
{
    struct ifi_info		*ifi, *ifinext;
    
    for ( ifi = ifihead; ifi != NULL; ifi = ifinext )
    {
        if ( ifi->ifi_addr != NULL )
            free( ifi->ifi_addr);
            
        if ( ifi->ifi_brdaddr != NULL )
            free( ifi->ifi_brdaddr);
            
        if ( ifi->ifi_dstaddr != NULL )
            free( ifi->ifi_dstaddr);
            
        ifinext = ifi->ifi_next;			// can't fetch ifi_next after free()
        
        free( ifi );
    }
}









