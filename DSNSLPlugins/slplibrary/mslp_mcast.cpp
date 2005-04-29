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
 * mslp_mcast.c : Minimal SLP v2 multicast utilities used by mslpd and mslplib.
 *
 * Version: 1.4
 * Date:    03/07/99
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

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"

EXPORT SLPInternalError join_group(SOCKET sdUDP, struct in_addr iaMC, struct in_addr iaInterf);
EXPORT int		mcast_join( SOCKET sockfd, const struct sockaddr* sa, int salen, const char* ifname );
EXPORT int 		mcast_set_if(int sockfd, const char *ifname);

EXPORT SLPInternalError set_multicast_sender_interf(SOCKET sd) 
{
    struct in_addr iaMC;
    struct in_addr ia;
    int iErr;
    SLPInternalError err = SLP_OK;
    unsigned char	ttl;
    u_char			loop = 1;		// enable;
    char*		endPtr = NULL;
    
    ttl =  (SLPGetProperty("net.slp.multicastTTL"))?(unsigned char) strtol(SLPGetProperty("net.slp.multicastTTL"), &endPtr, 10):255;
    iErr = setsockopt( sd, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl) );
    
#ifdef ENABLE_SLP_LOGGING
    if (iErr < 0)
        mslplog(SLP_LOG_DEBUG,"set_multicast_sender_interf: Could not set multicast TTL",strerror(errno));
#endif

    iErr = setsockopt( sd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop) );
    
#ifdef ENABLE_SLP_LOGGING
    if (iErr < 0)
        mslplog(SLP_LOG_DEBUG,"set_multicast_sender_interf: Could not set ip multicast loopback option",strerror(errno));
#endif

    iErr = GetOurIPAdrs( &ia, NULL );

    if (iErr < 0) 
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "an error occurred trying to get our IP Address" );
#endif
        err = SLP_NETWORK_INIT_FAILED;
    } 
    else 
    {
        memset(&iaMC, 0, sizeof iaMC);
        iaMC.s_addr = SLP_MCAST;
        
        err = join_group( 	sd,
                            iaMC,               /* group to send to */
                            ia);  				/* interf to send on */

        if ( err == SLP_OK )
        {
            if ((iErr= setsockopt(sd,IPPROTO_IP,IP_MULTICAST_IF,
                        (char*)&ia, sizeof(struct in_addr))) != 0) 
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DEBUG, "an error occurred calling setsockopt default mc interface",strerror(iErr));
#endif
                err = SLP_NETWORK_INIT_FAILED;
            }
        }
    }
    
    return err;
}

int mcast_set_if(int sockfd, const char *ifname)
{
    struct in_addr		inaddr;
    struct ifreq		ifreq;

    if (ifname != NULL) 
    {
        strncpy(ifreq.ifr_name, ifname, IFNAMSIZ);

        if (ioctl(sockfd, SIOCGIFADDR, &ifreq) < 0)
            return(-1);
            
        memcpy(&inaddr, &((struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr, sizeof(struct in_addr));
    }
    else
        inaddr.s_addr = htonl(INADDR_ANY);	/* remove prev. set default */

    return(setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF,
                        &inaddr, sizeof(struct in_addr)));
}

EXPORT SLPInternalError join_group(SOCKET sdUDP, struct in_addr iaMC, struct in_addr iaInterf) 
{
    struct ip_mreq 	mreq;
	char*			endPtr = NULL;
    unsigned char	ttl =  (SLPGetProperty("net.slp.multicastTTL"))?(unsigned char) strtol(SLPGetProperty("net.slp.multicastTTL"), &endPtr, 10):255;
    int 			err = 0;
    SLPInternalError		returnErr = SLP_OK;
    
    memset(&mreq,0,sizeof mreq);
    mreq.imr_multiaddr = iaMC;
    mreq.imr_interface = iaInterf;
    err = setsockopt(sdUDP,IPPROTO_IP,IP_ADD_MEMBERSHIP, (char*)&mreq,sizeof(mreq));
#ifdef ENABLE_SLP_LOGGING
    if (err < 0) 
    {        
        SLP_LOG( SLP_LOG_DEBUG, "Received an error trying to setsockopt IP_ADD_MEMBERSHIP" );
    }
#endif    
    if ( returnErr == SLP_OK )
    {
        err = setsockopt(sdUDP,IPPROTO_IP,IP_MULTICAST_TTL,(char*)&ttl,sizeof(ttl));
        if (err < 0) 
        {
#ifdef ENABLE_SLP_LOGGING
            mslplog(SLP_LOG_DEBUG, "join_group: Could not set multicast TTL",strerror(errno));
#endif
            returnErr = SLP_NETWORK_INIT_FAILED;
        }
    }
    
    if ( returnErr == SLP_OK )
    {
        u_char	loop = 1;	// enable
        err = setsockopt( sdUDP, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop) );
        
#ifdef ENABLE_SLP_LOGGING
        if (err < 0)
        {
            mslplog(SLP_LOG_DEBUG,"join_group: Could not set ip multicast loopback option",strerror(errno));
        }
#endif
    }
    
    return returnErr;
}

int mcast_join( SOCKET sockfd, const struct sockaddr* sa, int salen, const char* ifname )
{
    switch (sa->sa_family)
    {
        case AF_INET:
        {
            struct ip_mreq 	mreq;
            struct ifreq	ifreq;
            
            bzero( &ifreq, sizeof(ifreq) );
            memcpy( &mreq.imr_multiaddr, &((struct sockaddr_in*)sa)->sin_addr, sizeof(struct in_addr) );
            
            if ( ifname != NULL )
            {
                strncpy( ifreq.ifr_name, ifname, IFNAMSIZ );

                if ( ioctl( sockfd, SIOCGIFADDR, &ifreq ) < 0 )
                {
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_DEBUG, "Error trying to set interface name %s for multicasting: %s", ifname, strerror(errno) );
#endif
                    return -1;
                }
                    
                memcpy( &mreq.imr_interface, &((struct sockaddr_in*) &ifreq.ifr_addr)->sin_addr, sizeof(struct in_addr) );
            }
            else
                mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        
            return ( setsockopt( sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq) ) );
        }
        
        case AF_INET6:
        default:
            errno = EPROTONOSUPPORT;
            return (-1);
    }
}
