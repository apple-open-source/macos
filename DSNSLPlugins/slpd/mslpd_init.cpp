/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * mslpd_init.c : Minimal SLP v2 Service Agent - initialization 
 *             for networking.
 *
 * Version: 1.10
 * Date:    03/29/99
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
#include <errno.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslp_dat.h"
#include "mslpd_store.h"
#include "mslpd.h"

#ifdef MAC_OS_X
#include "slpipc.h"
#endif

/*
 * mslpd will need - 
 *   tcp listener (to inaddr_any) unless NOTCP
 *   udp listener (to mcast group on each and every interface!)
 *   fd_set and max socket descriptor.
 */
SLPInternalError  mslpd_init_network(SAState *psa)
{
    int 				iErr, turnOn=1;
    SLPInternalError	err;
    struct sockaddr_in	serv_addr;
    struct in_addr 		ina;
    char 				pcHost[256];
	char*				pc = NULL;
    const char*			pcInterf = SLPGetProperty("net.slp.interfaces");

    if (OPEN_NETWORKING() < 0)
        return SLP_NETWORK_INIT_FAILED;

    psa->sdUDP = socket(AF_INET, SOCK_DGRAM, 0);
    psa->sdTCP = socket(AF_INET, SOCK_STREAM, 0);
    psa->sdReg = socket(AF_INET, SOCK_STREAM, 0);
    if (psa->sdUDP == INVALID_SOCKET || psa->sdTCP == INVALID_SOCKET ||
        psa->sdReg == INVALID_SOCKET)
    {
        SLP_LOG( SLP_LOG_ERR, "Could not allocate a socket" );
        return SLP_NETWORK_INIT_FAILED;
    }
    
    if (psa->sdUDP > psa->sdTCP)
        psa->sdMax = psa->sdUDP +1;
    else
        psa->sdMax = psa->sdTCP + 1;

    /* set the reuse socket option */
    if ( setsockopt(psa->sdTCP, SOL_SOCKET, SO_REUSEADDR, &turnOn, sizeof(turnOn) ) <0 )
    {
        char	msg[256];
        
        sprintf( msg, "Could not set reuse port option on TCP socket: %s", strerror(errno) );
        SLP_LOG( SLP_LOG_ERR, msg );
        return SLP_NETWORK_INIT_FAILED;
    }

    /* I need to get my host name and build in a sin */

    if (pcInterf != NULL)
    {
        strcpy(pcHost,pcInterf);
    }
    else if ((iErr = GetOurIPAdrs( &ina, NULL )) < 0)
    {
        SLP_LOG( SLP_LOG_ERR, "mslpd_init_network: could not get host address" );
        return SLP_NETWORK_INIT_FAILED;
    }

	pc = inet_ntoa(ina);

    psa->pcSAHost = safe_malloc(strlen(pc)+1,pc,strlen(pc));
    {
        errno = 0;
 
        psa->pcSANumAddr = safe_malloc(strlen(pc)+1,pc,strlen(pc));
        psa->sin.sin_addr.s_addr = ina.s_addr;
		psa->sin.sin_family = AF_INET;
        psa->sin.sin_port = htons(SLP_PORT);
    }

    if (pcInterf != NULL)
    {
        serv_addr = psa->sin;
    }
    else
    {
        memset(&serv_addr,0,sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(SLP_PORT);
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

#ifdef SLPTCP
    iErr = bind(psa->sdTCP,(struct sockaddr *)&serv_addr,sizeof(serv_addr));

    if ((iErr < 0)||(iErr == SOCKET_ERROR))
    {
        char	msg[256];
        
        sprintf( msg, "Error binding to TCP socket: %s", strerror(errno) );
        SLP_LOG( SLP_LOG_ERR, msg );
        
        return SLP_NETWORK_INIT_FAILED;
    }
    iErr = listen(psa->sdTCP,5);

    if ((iErr < 0)||(iErr == SOCKET_ERROR))
    {
        char	msg[256];
        
        sprintf( msg, "Error calling listen on TCP socket: %s", strerror(errno) );
        SLP_LOG( SLP_LOG_ERR, msg );

        return SLP_NETWORK_INIT_FAILED;
    }
#endif /* SLPTCP */

   iErr = bind(psa->sdUDP,(struct sockaddr *)&serv_addr,sizeof(serv_addr));

    if ((iErr < 0)||(iErr == SOCKET_ERROR))
    {
        char	msg[256];
        
        sprintf( msg, "Error binding to UDP socket: %s", strerror(errno) );
        SLP_LOG( SLP_LOG_ERR, msg );
        
        return SLP_NETWORK_INIT_FAILED;
    }

 /* set up multicast */
    if ((err = set_multicast_sender_interf(psa->sdUDP)) != SLP_OK)
    {
        SLP_LOG( SLP_LOG_DEBUG,"mslpd_init_network: set_multicast_sender_interf",err);
    }
    else
        SLP_LOG( SLP_LOG_DEBUG, "mslpd_init_network: set_multicast_sender_interf ok" );

  FD_ZERO(&(psa->fds));
  FD_SET(psa->sdUDP,&(psa->fds));
  FD_SET(psa->sdTCP,&(psa->fds));
   
#ifdef EXTRA_MSGS
    {
        char *pcTemp = "service:service-agent://";
        int iLen = strlen(pcTemp);
        psa->pcSAURL = safe_malloc(iLen+strlen(pc)+1,pcTemp,iLen);
        strcat(psa->pcSAURL,pc);
    }
#endif /* EXTRA_MSGS */

#ifdef MAC_OS_X        
    {
        char *pcTemp = "service:directory-agent://";
        int iLen = strlen(pcTemp);
        psa->pcDAURL = safe_malloc(iLen+strlen(pc)+1,pcTemp,iLen);
        strcat(psa->pcDAURL,pc);
    }

    psa->pcSPIList = safe_malloc(1,NULL,0);
    psa->pcSPIList[0] = '\0';	// this is just a null list for now

#endif /* MAC_OS_X */

  return SLP_OK;  
}
