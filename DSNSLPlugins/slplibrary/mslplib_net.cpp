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
 * mslplib_net.c : Minimal SLP v2 User Agent network functions implementation.
 *
 * Version: 1.14
 * Date:    10/05/99
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
#include <unistd.h>		// for _POSIX_THREADS
#include <pthread.h>	// for pthread_*_t

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslp_dat.h"
#include "mslplib.h"

static SLPBoolean time_remaining(long lTimeStart, time_t tMaxwait, 
  long *plWaits, int *piSend, int iNumSent, struct timeval *ptv);

/*
 * mslplib will need -
 *   tcp sender (bound on this side to inaddr_any) unless NOTCP
 *   udp sender (broadcast enabled, if config indicates)
 *   list of interfaces to bind to, to multicast off all interfaces
 */

SLPInternalError  mslplib_init_network(SOCKET *psdUDP, SOCKET *psdTCP, SOCKET *psdMax)
{
    int iErr;
    unsigned char ttl = 255;
    u_char	loop = 1;	// enable
    char*	endPtr = NULL;
	
    if ( SLPGetProperty("net.slp.multicastTTL") )
		ttl = (unsigned char) strtol(SLPGetProperty("net.slp.multicastTTL"),&endPtr,10);
    
    if (OPEN_NETWORKING() < 0) return SLP_NETWORK_INIT_FAILED;
    
    *psdUDP = socket(AF_INET, SOCK_DGRAM, 0);
    *psdTCP = socket(AF_INET, SOCK_STREAM, 0);
    
    if (*psdUDP == INVALID_SOCKET || *psdTCP == INVALID_SOCKET)
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "Could not allocate a socket" );
#endif
        return SLP_NETWORK_INIT_FAILED;
    }
    
    iErr = setsockopt(*psdUDP, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));
#ifdef ENABLE_SLP_LOGGING
    if (iErr < 0)
        mslplog(SLP_LOG_DEBUG,"mslplib_init_network: Could not set multicast TTL",strerror(errno));
#endif    
    iErr = setsockopt( *psdUDP, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop) );
    
#ifdef ENABLE_SLP_LOGGING
    if (iErr < 0)
    {
        mslplog(SLP_LOG_DEBUG,"mslplib_init_network: Could not set ip multicast loopback option",strerror(errno));
    }
#endif

    if (*psdUDP > *psdTCP) {
        *psdMax = *psdUDP;
    } else {
        *psdMax = *psdTCP;
    }
    
    return SLP_OK;
}

/*
 * mslplib_daadvert_callback
 *
 *    Simply does the registration when the callback is made for 
 *    incoming DAAdverts.  Each incoming DA is added to the DATable.
 *    This is used for ACTIVE DA DISCOVERY. 
 *
 *    NOTE:  This is different than the mslpd_daadvert_callback.
 *
 *      hSLP         This is ignored by this routine
 *      iErrorCode   This is ignored for now.
 *      sin          The sin of the DA
 *      pcScopeList  The scopes supported by the DA 
 *      lBootTime    The boot time of the DA
 *      pvUser       This is actually the DATable
 *
 * Side effect:  Will update the DATable.
 */
void mslplib_daadvert_callback(SLPHandle hSLP, 
			       int iErrCode,
			       struct sockaddr_in sin,
			       const char *pcScopeList,
			       const char *pcAttrList,
			       long lBootTime,
			       void *pvUser) {
  DATable *pdat = (DATable *) pvUser;
  int iErr;

  /* we don't need these parameters yet, but they're supplied by interf */
  hSLP = hSLP;
  iErrCode = iErrCode;
  pcAttrList = pcAttrList;
  
  if ((iErr=dat_daadvert_in(pdat, sin, pcScopeList, lBootTime)) < 0) {
#ifdef ENABLE_SLP_LOGGING
    SLPInternalError slperr = (SLPInternalError) iErr;
    mslplog(SLP_LOG_DA,"mslplib_daadvert_callback: dat_daadvert_in failed",
	slperror(slperr));
#endif
  }
}

/*
 * generate_srvrqst 
 *
 *   piSendSz  IN: max size to send, OUT: size actually used
 */
SLPInternalError generate_srvrqst(char *pcSendBuf, int *piSendSz, 
        const char *pcLangTag, 
        const char *pcScope, const char *pcSrvType, const char *pcFilter) {

  int offset = 0;
  SLPInternalError err;
  int iMTU = *piSendSz;

  (void) memset(pcSendBuf, 0, *piSendSz);

  if ((err = add_header(pcLangTag,pcSendBuf,iMTU,SRVRQST,0,&offset))
    != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_srvrqst: could not add header",err);
  }
  
  offset += 2; /* PR list is 0 initially, leave 2 0s here */

  if ((err = add_string(pcSendBuf,iMTU,pcSrvType,&offset)) != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_srvrqst: could not add srvtype",err);    
  }
  
  if ((err = add_string(pcSendBuf,iMTU,pcScope,&offset)) != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_srvrqst: could not add scope",err);    
  }
  
  if ((err = add_string(pcSendBuf,iMTU,pcFilter,&offset)) != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_srvrqst: could not add filter",err);    
  }

  if ((err = add_sht(pcSendBuf, iMTU, 0, &offset)) != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_srvrqst: could not add 0 SLP SPI len", err);
  }
  
  SETLEN(pcSendBuf,offset);
  *piSendSz = offset;

  return SLP_OK;
}

#ifdef SLPTCP

/*
 * get_tcp_result
 *
 *   This function is used to send a message and obtain a result from
 *   a particular address.
 *
 *   The size of the incoming buffer is unknown initially.  The header
 *   of the reply is inspected and the appropriate size buffer is
 *   allocated.
 *
 *   A new TCP socket is created and connected for each call to this
 *   routine.  An optimization could hold connections open for reuse.
 *
 * Parameters:
 *   pcSendBuf The outgoing message.
 *   iSendSz   The size of the outgoing message.
 *   sin       The address of the host to request a response from.
 *   ppcInBuf  The OUT parameter of the buffer received.  This must
 *             be freed by the caller.
 *   piInSz    The OUT parameter indicating the length of the InBuf
 *
 * Results:
 *   SLP_OK on success.
 *   SLP_NETWORK_ERROR if the socket setup, read or write fails.
 */
SLPInternalError get_tcp_result(const char *pcSendBuf, int iSendSz, struct sockaddr_in sin, char **ppcInBuf, int *piInSz) 
{    
    SOCKET sd;
    int iResult;
    char  pcHeader[HDRLEN];
    
    /* Initialize */
    
    if ((sd = socket(AF_INET,SOCK_STREAM,0)) < 0) 
    {
#ifdef ENABLE_SLP_LOGGING
        mslplog(SLP_LOG_DROP,"get_tcp_result could not init tcp socket",strerror(errno));
#endif
        return SLP_NETWORK_ERROR;
    }
    
    if (connect(sd,(struct sockaddr*) &sin, sizeof(sin)) < 0) 
    {
        CLOSESOCKET(sd);
#ifdef ENABLE_SLP_LOGGING
        mslplog(SLP_LOG_DROP,"get_tcp_result could not connect to addr",strerror(errno));
#endif
        return SLP_NETWORK_ERROR;
    }
    
    /* Send the request */
    iResult = writen(sd,(void*)pcSendBuf,iSendSz);
    if (iResult < 0) 
    {
        CLOSESOCKET(sd);
#ifdef ENABLE_SLP_LOGGING
        mslplog(SLP_LOG_DROP,"get_tcp_result could not writen",strerror(errno));
#endif
        return SLP_NETWORK_ERROR;
    }
    
    /* Get the reply */
    iResult = readn(sd,pcHeader,HDRLEN);
    if (iResult < HDRLEN) 
    {
        CLOSESOCKET(sd);
#ifdef ENABLE_SLP_LOGGING
        mslplog(SLP_LOG_DROP,"get_tcp_result could not readn header",strerror(errno));
#endif
        return SLP_NETWORK_ERROR;
    }
    
    *piInSz = GETLEN(pcHeader);
    *ppcInBuf = safe_malloc(*piInSz,pcHeader,HDRLEN);
    if( !*ppcInBuf ) return SLP_NETWORK_ERROR;
    
    if ((iResult = readn(sd,&(*ppcInBuf)[HDRLEN],(*piInSz)-HDRLEN)) < 0) 
    {
        SLPFree(*ppcInBuf);
        CLOSESOCKET(sd);
#ifdef ENABLE_SLP_LOGGING
        mslplog(SLP_LOG_DROP,"get_tcp_result could not readn message",strerror(errno));
#endif
        return SLP_NETWORK_ERROR;
    } 
    
    /* Clean up */
    CLOSESOCKET(sd);
    return SLP_OK;
}                       

#endif /*SLPTCP*/

SLPInternalError get_unicast_result(
                                time_t  timeout,        /* the number of seconds till timeout occurs */
                                SOCKET  sdSend,         /* sd to do sendto and recvfrom on           */
                                char   *pcSendBuf,      /* the buffer to send                        */
                                int     iSendSz,        /* the size of the buffer to send            */
                                char   *pcRecvBuf,      /* the buffer received                       */
                                int     iRecvSz,        /* the size of the buffer received           */
                                int    *piInSz,         /* the number of bytes actually read         */
                                struct sockaddr_in sin) 
  {
    /*
    * Send the request to 'sin', retrying 3 times till it timeouts or till 
    * the max timeout is achieved.
    */
    struct sockaddr_in insin;
    socklen_t iInSinSz = sizeof insin;
    time_t tStart = time(0);
    time_t tElapsed;
    int iErr;
    int loop = 0;
    fd_set fds, allset;
    struct timeval tv = { 2, 0 };
    
    *piInSz = 0;
    FD_ZERO(&allset);
    FD_SET(sdSend,&allset);
    
    for (loop = 0; loop < 3; loop++) 
    {
        tElapsed = time(0) - tStart;
        
        /*
        * force successive tries to be no less than 3 seconds apart 
        */
        if (loop > 0 && (tElapsed < (time_t) (loop*3)) && tElapsed < timeout) 
        {
            struct timeval tv2 = {3,0};
            (void) select(0,NULL,NULL,NULL,&tv2); /* wait 3 seconds */
        }
        
        if ((iErr = sendto(sdSend, pcSendBuf, iSendSz, 0, (struct sockaddr*) &sin,sizeof sin)) < 0) 
        {
            LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP, "get_unicast_result sendto",SLP_NETWORK_ERROR); 
        } 
    
        fds = allset;
        
        iErr = select(sdSend+1,&fds,NULL,NULL,&tv);
        
        if (iErr < 0) 
        {
            if ( errno == EINTR )
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DROP, "get_converge_result: select received EINTR");
#endif
                continue;
            }
            else
                LOG_STD_ERROR_AND_RETURN(SLP_LOG_ERR,"get_converge_result: select",SLP_INTERNAL_SYSTEM_ERROR);
        } 
        else if (iErr == 0) 
        {
            continue;
        } 
        else 
        {
            if ((iErr = recvfrom(sdSend, pcRecvBuf, iRecvSz, 0, (struct sockaddr*)&insin, &iInSinSz)) < 0) 
            {
                if ( errno == EINTR )
                {
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_DROP, "get_unicast_result recvfrom received EINTR");
#endif
                    continue;
                }
                else
                    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP, "get_unicast_result recvfrom", SLP_NETWORK_ERROR);
            }
            
            *piInSz = iErr; /* set the return size parameter to the incoming size */
            
            return SLP_OK;    
        }
    }
    
    return SLP_NETWORK_TIMED_OUT;
}

#define NUM_SECS_BETWEEN_REQUESTS 5
#define INTERVALS 15
/*
 * get_converge_result
 *
 *  Sends the buffer created in the UA_State pcSendBuf and receives
 *  a series of buffers which are passed on to the process_reply routine
 *  and sent to the caller using the callback.
 *
 *  In some ways the most complex part of the mslpd implementation,  this
 *  routine insures that a request will be sent INTERVALSE times, evenly
 *  spaced apart - and wait for replies till tMaxwait elapses.  Replies 
 *  are returned immediately, as per the synchronous API.  Multiple reply
 *  values are returned one by one.
 *
 *  Upon receiving a valid reply, the PR list is lengthened.  This means
 *  the packet must be reconstructed, making room for the expanded list
 *  and adjusting length fields.  This is done by recalc_sendBuf().
 *
 *  Bad reply values are logged, but don't cause the routine to fail.
 *  Failures of the networking calls do cause the convergence loop to fail.
 *
 */
pthread_mutex_t	gMulticastTTLLock = PTHREAD_MUTEX_INITIALIZER;;

SLPInternalError get_converge_result(
  time_t              tMaxwait,   /* number of milliseconds to wait, max  */
  SOCKET              sd,         /* socket to sendto and recvfrom with   */
  char               *pcSendBuf,  /* the buffer to send - will change!    */
  int                 iSendSz,    /* the size of the buffer (net.slp.MTU) */
  char               *pcRecvBuf,  /* the buffer to receove results into   */
  int                 iRecvSz,    /* the size of the buffer for receiving */
  struct sockaddr_in  sin,        /* the address to send the requests     */
  unsigned char		  ttl,        /* ttl                                  */
  void               *pvUser,     /* opaque value to use in callbacks     */
  SLPHandle           hSLP,       /* the SLPHandle of the caller          */
  void               *pvCallback, /* the supplied callback function       */
  CBType              cbt)	      /* the type of the callback function    */
{
    int iLast = 0;         /* The client callback may set this and cause  */
                    /* the multicast convergence sequence to stop. */
    int					loop = 0;
    int					sendit=0,numsent=0;
    int					result;            /* for testing results of system calls      */
    int					len;               /* the length of the received buffer        */
    SLPInternalError			err = SLP_OK; /* contains return codes from SLP functions */
    struct sockaddr_in	insin;
    long				lTimeStart = SDGetTime();
    struct timeval		tv;
    fd_set				fds, allset;
    int					numIntervals;
    long				plWaits[INTERVALS];	// INTERVALS is max number of intervals
#ifdef USE_PR_LIST
    char*				pcPRList = NULL;
    char*				pcPrevPRList = NULL;
#endif
    int					iErr;
    div_t				divResult;
    long				tMaxWaitSecs = tMaxwait/1000;
    char				errbuf[120];
	char*				endPtr = NULL;
	bool				prListMaxedOut = false;
	
    divResult = div(tMaxwait,NUM_SECS_BETWEEN_REQUESTS*1000);		// figure out how many intervals
    
    if ( divResult.quot > INTERVALS )
        numIntervals = INTERVALS;
    else
        numIntervals = divResult.quot;
        
    /*
    * Testing mode!
    * This mode has to be turned off by the DA discovery routine and
    * turned back off again.  Otherwise this single statement is enough
    * to disable multicast/convergence.
    */
    if (SLPGetProperty("com.sun.slp.noSA") && !SDstrcasecmp(SLPGetProperty("com.sun.slp.noSA"),"true"))
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG,"get_converge_result: Simulate converge time out");
#endif
        return SLP_OK;
    }

    /*
    * The last item is a sentinal.  Each other item in plWaits indicates
    * the successive timeout interval.
    */
    for (loop = 0; loop < numIntervals-1; loop++)
        plWaits[loop] = (tMaxWaitSecs/(numIntervals-1));

    plWaits[numIntervals-1] = 0;

    FD_ZERO(&allset);
    FD_SET(sd,&allset);
    
    pthread_mutex_lock( &gMulticastTTLLock );
	unsigned char	defaultTTL = 255;
	char			temp[16] = {0};
	
	if (SLPGetProperty("net.slp.multicastTTL"))
		defaultTTL = (unsigned char) strtol(SLPGetProperty("net.slp.multicastTTL"), &endPtr, 10);
		
	sprintf( temp, "%d", ttl );
	SLPSetProperty("net.slp.multicastTTL",temp);
	sprintf( temp, "%d", defaultTTL );

	if ((err = set_multicast_sender_interf(sd)) != SLP_OK) 
    {
        CLOSESOCKET(sd);
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG,"get_converge_result: set_multicast_sender_interf, %s",slperror(err));
#endif
		SLPSetProperty("net.slp.multicastTTL",temp);
		pthread_mutex_unlock( &gMulticastTTLLock );
        return err;
    }
	SLPSetProperty("net.slp.multicastTTL",temp);
    pthread_mutex_unlock( &gMulticastTTLLock );

    while (1)
    {
        if (!time_remaining(lTimeStart,tMaxwait,plWaits,&sendit,numsent,&tv))
        {
            /* signal the calling application that we have timed out */
            iLast = SLP_TRUE;

            break; /* no time left */
        }

        if (sendit /*&& !prListMaxedOut*/) /* only send stuff when we are crossing 'intervals' */
        {
#ifdef USE_PR_LIST
            if (   (pcPRList != NULL && pcPrevPRList == NULL)
					|| (pcPRList != NULL && pcPrevPRList != NULL && SDstrcasecmp(pcPRList,pcPrevPRList) ) )
            {

                /* only recalc the buffer after a sequence of receives */
                char*	endPtr = NULL;
				int		mtu = 1400;
				
				if ( SLPGetProperty("net.slp.MTU") )
					mtu = strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10);
				
				if ( recalc_sendBuf(pcSendBuf,mtu,pcPRList) == 0 )
				{
					SLPFree(pcPrevPRList);
					pcPrevPRList = safe_malloc(strlen(pcPRList)+1,pcPRList,strlen(pcPRList));
				}
				else
					prListMaxedOut = true;
					
				iSendSz = GETLEN(pcSendBuf); /* the length of the buffer to send */
            }
#endif
            numsent++; /* count the number of times sent to find the right delay */

#ifdef ENABLE_SLP_LOGGING
    #ifndef NDEBUG
            if ( getenv("SLPTRACE") )
            {
                sprintf(errbuf,"trace:  SEND IT %d\n",numsent);
                SLP_LOG( SLP_LOG_DEBUG,errbuf);
            }
    #endif
            sprintf(errbuf,"get_converge_result: sent request to [%s]",inet_ntoa(sin.sin_addr));
            SLP_LOG( SLP_LOG_DEBUG,errbuf);
#endif
            if (sendto(sd, pcSendBuf, iSendSz, 0, (struct sockaddr*) &sin, sizeof(struct sockaddr_in)) < 0)
            {
#ifdef ENABLE_SLP_LOGGING
                mslplog(SLP_LOG_DROP,"get_converge_result: multicast sendto",strerror(errno));
#endif
                err = SLP_NETWORK_ERROR;
                break;
            }
        }

        fds = allset;
        result = select(sd+1,&fds,NULL,NULL,&tv);

        if (result < 0)
        {
            if ( errno == EINTR )
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DROP, "get_converge_result: select received EINTR");
#endif
                continue;
            }
            else
            {
#ifdef ENABLE_SLP_LOGGING
                mslplog(SLP_LOG_DEBUG,"get_converge_result: select",strerror(errno));
#endif
                err = SLP_NETWORK_ERROR;
                break;
            }
        }
        else if (result == 0)
        {
            long lElapsed = SDGetTime() - lTimeStart;
            if (lElapsed < 0) lElapsed *= -1; /* in case start time larger */

            if (lElapsed > tMaxwait)
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DEBUG,"get_converge_result: ran out of time");
#endif
                /* signal the calling application that we have timed out */
                iLast = SLP_TRUE;

                break;
            }
            else
            {
                continue;
            }
        }
        else/* select indicates the UDP socket is ready to read */
        {
			socklen_t iInLen = sizeof(struct sockaddr_in);
            iErr = recvfrom(sd,pcRecvBuf,iRecvSz,0,(struct sockaddr*)&insin,&iInLen);

            if (iErr >= 0 && iErr < HDRLEN+4)
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DROP, "get_converge_result: recvd something, smaller than a SLPv2 msg");
#endif
                continue;
            }
            else if (iErr < 0)
            {
#ifdef ENABLE_SLP_LOGGING
                if ( errno == EINTR )
                {
                    SLP_LOG( SLP_LOG_DROP, "get_converge_result: recvfrom received EINTR");
                }
                else
                    mslplog(SLP_LOG_DROP,"get_converge_result: recvfrom failed",strerror(errno));
#endif                
                continue;
            }
            len = iErr; /* result of recvfrom is # of bytes read, if result is > 0 */

#ifdef ENABLE_SLP_LOGGING
    #ifndef NDEBUG
            sprintf(errbuf,"  received %d byte message from [%s]\n", len, inet_ntoa(insin.sin_addr));
            SLP_LOG( SLP_LOG_DEBUG,errbuf);
    #endif /* NDEBUG */
#endif
			char*		tcpBuffer = NULL;
			
            if ( GETFLAGS(pcRecvBuf) & OVERFLOWFLAG )
            {
                // the server is telling us that they have more scope info that can fit in a multicast, and that we should 
                // ask it info directly
#ifdef ENABLE_SLP_LOGGING
                sprintf(errbuf,"get_converge_result: overflow bit set, need to resend TCP request to [%s]",inet_ntoa(insin.sin_addr));
                SLP_LOG( SLP_LOG_MSG, errbuf );
#endif
                insin.sin_port   = htons(SLP_PORT);

                if ((err=get_tcp_result(pcSendBuf,iSendSz, insin, &tcpBuffer,&len)) != SLP_OK) 
                {
                    // ug, couldn't get the message, log and continue multicasting
                    sprintf(errbuf,"get_converge_result: error resending TCP request to [%s]: %s",inet_ntoa(insin.sin_addr), slperror(err));
                    SLPLOG(SLP_LOG_ERR,errbuf);
                }
            }
           
			if ( tcpBuffer )
			{
				err=process_reply(pcSendBuf,tcpBuffer,len,&iLast, pvUser,hSLP,pvCallback,cbt);
				free( tcpBuffer );
			}
			else
				err=process_reply(pcSendBuf,pcRecvBuf,len,&iLast, pvUser,hSLP,pvCallback,cbt);
			
            if (iLast == 1)
            {
                break;
            }
            else if (err == SLP_OK && !prListMaxedOut)
            {
#ifdef USE_PR_LIST
				/* on a successful receive and process reply, add to PR list */
				prlist_modify(&pcPRList,insin);
#endif
            }
            else if ( err == SLP_REPLY_DOESNT_MATCH_REQUEST )
            {
#ifdef ENABLE_SLP_LOGGING
                char	errMsg[128];
                sprintf( errMsg, "received wrong reply to our request from: %s", inet_ntoa(insin.sin_addr) );
                SLP_LOG( SLP_LOG_DEBUG, errMsg );
#endif
            }
            else if ( err == SLP_REQUEST_CANCELED_BY_USER )
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DEBUG, "get_converge_result canceled by caller" );
#endif
                err = SLP_OK;		// its not an error
                break;
            }
        } /* something to read */
    } /* we through INTERVAL-1 times... */

#ifdef ENABLE_SLP_LOGGING
    if ( getenv("SLPTRACE") )
        SLP_LOG( SLP_LOG_DEBUG,"trace: no time left\n");
#endif

#ifdef USE_PR_LIST
    SLPFree(pcPrevPRList);
    SLPFree(pcPRList);
#endif

    return err;
}  

static SLPBoolean time_remaining(long lTimeStart, time_t tMaxwait, 
  long *plWaits, int *piSend, int iNumSent, struct timeval *ptv) 
{
    int i;
    long lSum = 0;
    long lSoFar = SDGetTime() - lTimeStart;
    long lTillNext = 0;
    long lDelay = 0;
#ifdef ENABLE_SLP_LOGGING
    char errbuf[120];
#endif	
    *piSend = 0;

    if (lSoFar < 0) lSoFar *= -1; /* time counter wraps on some platforms */
    
    if (lSoFar > tMaxwait) return SLP_FALSE;
    
    for (i = 0; plWaits[i] != 0; i++) 
    {
        lSum += plWaits[i];
    
        lTillNext = lSum - lSoFar;
        
        if (lTillNext < 0) 
        {
#ifdef ENABLE_SLP_LOGGING
#ifndef NDEBUG
            if ( getenv("SLPTRACE") )
            {
                sprintf(errbuf, "trace: add in another wait interval iSum %ld < lSoFar %ld\n", lSum,lSoFar);
                SLP_LOG( SLP_LOG_DEBUG,errbuf);
            }
#endif /* NDEBUG */      
#endif
            continue;
        }
    
        if (iNumSent <= i) 
            *piSend = 1; /* send if we haven't yet this interval */
    
#ifdef ENABLE_SLP_LOGGING
#ifndef NDEBUG
        if ( getenv("SLPTRACE") )
        {
            sprintf(errbuf,
                "trace: lSoFar = %ld lTillNext = %ld "
                "tMaxwait = %ld lSum = %ld: \n",
                lSoFar, lTillNext, tMaxwait, lSum);	    
            SLP_LOG( SLP_LOG_DEBUG,errbuf);
        }
#endif /* NDEBUG */
#endif
        if (lSoFar < lSum) 
        {
            if (tMaxwait < lSum)   /* if the sum would exceed the allowed max */
            {
                lDelay = (tMaxwait - lSoFar); /* return only remaining allowed      */
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DEBUG,"         max < sum.\n");
#endif
                break;
            } 
            else 
            {
                lDelay = lTillNext;           /* otherwise, return piece of time    */

                break;
            }
        }
    }
    
    if (lDelay == 0)
        return SLP_FALSE;  /* we are past all timeouts           */

    ptv->tv_sec = lDelay;
    ptv->tv_usec = 0;  

#ifdef ENABLE_SLP_LOGGING
#ifndef NDEBUG
    if ( getenv("SLPTRACE") )
    {
        sprintf(errbuf,"trace:    delay %ld sec  %ld usec\n",
            (unsigned long)ptv->tv_sec, (unsigned long)ptv->tv_usec);
        SLP_LOG( SLP_LOG_DEBUG,errbuf);
    }
#endif  
#endif
  return SLP_TRUE;
}

