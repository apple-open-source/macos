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
 * mslpd_net.c : Minimal SLP v2 Service Agent networking code
 *
 *  All reads, writes, message composing and decomposing are done here.
 *
 * Version: 1.11
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslp_dat.h"
#include "mslplib.h"
#include "mslpd_store.h"
#include "mslpd.h"
#include "mslpd_query.h"
static int get_reply( SLPBoolean viaTCP, SAState *psa, const char *pcInBuf, int iInSz, struct sockaddr_in* sinIn,
                     char **ppcOutBuf, int *piOutSz, int *piNumResults);
static void  generate_error(SLPReturnError iErr, Slphdr s,char *out, int *pI);
int IsDAUs( SAState* psa, struct sockaddr_in sinDA );

/*static SLPInternalError SrvRegDereg_out(	int isReg,
                                    const char *pcLang, 
                                    const char *pcURL, 
                                    const char *pcSrvType, 
                                    const char *pcScope,
                                    const char *pcAttrList, 
                                    int iLifetime,
                                    char **ppcOutBuf, 
                                    int *piOutSz); 
*/

/* -------------------------------------------------------------------- */

/*
 * handle_udp
 *
 *   This routine is called when the select in the main mslpd loop
 *   detects there is a datagram to be read.
 *
 *   All processing of the request is handled here (recv request,
 *   obtaining results, sending reply.)
 *
 * Return:
 *   0 indicates no error.
 *   The result code is returned from the recvfrom and sendto -
 *   NOTE WELL - THIS IS NOT A SLPInternalError - IT IS A SYSTEM LEVEL ERROR
 *   CODE.
 */
int handle_udp( SAState *psa, char* pcInBuf, int inBufSz, struct sockaddr_in sinIn ) 
{ 
	char				*pcOutBuf = NULL;
	int					iOutSz;
//	struct sockaddr_in	sinIn;
	int					err = 0;
	int					iSinInSz = sizeof(sinIn);
    
// handle the connection from outside this function
	int iNumResults;
//		iInSz = err;
	
//		SDLock(psa->pvMutex);
	/* This routine is independent of whether it was a UDP or TCP rqst */
	if ( get_reply( SLP_FALSE, psa, pcInBuf, inBufSz, &sinIn, &pcOutBuf, &iOutSz, &iNumResults ) == 0 && pcOutBuf )
	{
		if ( iNumResults == 0 && ( GETFLAGS(pcInBuf) & MCASTFLAG ) ) 
		{
			SLP_LOG( SLP_LOG_DROP, "handle_udp:  0 result to multicast request - drop it");
		} 
		else 
		{
#ifndef NDEBUG
			char*				endPtr = NULL;
			SOCKET  			sdSend = socket(AF_INET, SOCK_DGRAM, 0);

			if ( iOutSz > strtol(SENDMTU,&endPtr,10) )		// can't send more than this, we better make sure the overflow bit is set
			{
//                    SETFLAGS(pcOutBuf,OVERFLOWFLAG);
				SETLEN(pcOutBuf, strtol(SENDMTU,&endPtr,10));
				iOutSz = strtol(SENDMTU,&endPtr,10);
			}
				
			SLP_LOG( SLP_LOG_MSG, "handle_udp: send %d byte result to: %s", iOutSz, inet_ntoa(sinIn.sin_addr));
#endif
			if ( ( err = sendto( sdSend, pcOutBuf, iOutSz, 0, (struct sockaddr*)&sinIn, iSinInSz ) ) < 0 )
			{
				if ( pcOutBuf ) 
					SLPFree( pcOutBuf );

//                    SDUnlock(psa->pvMutex);	// unlock since LOG_STD_ERROR_AND_RETURN will return
				
				{
					char	logMsg[255];
					
					sprintf( logMsg, "mslpd handle_udp sendto: %s", inet_ntoa(sinIn.sin_addr) );
					SLP_LOG( SLP_LOG_DROP, logMsg, errno );
					CLOSESOCKET(sdSend);
					return errno;
				}
			}

			CLOSESOCKET(sdSend);
		}
		
		if ( pcOutBuf ) 
			SLPFree( pcOutBuf );
	} 
	else 
	{
		char	logMsg[255];
		
		sprintf( logMsg, "handle_udp: drop an unhandled request from %s!", inet_ntoa(sinIn.sin_addr) );
		SLP_LOG( SLP_LOG_DROP, logMsg );
	}
	
	return 0; /* no error */
}

#ifdef SLPTCP

/*
 * handle_tcp
 *
 *   This routine is called when the select in the main mslpd loop
 *   detects there is a connection ready to accept and read.
 *
 *   All processing of the request is handled here (recv request,
 *   obtaining results, sending reply.)
 *
 *   The socket is NOT held open for subsequent requests!
 *
 * Return:
 *   0 indicates no error.
 *   The result code is returned from the recvfrom and sendto -
 *   NOTE WELL - THIS IS NOT A SLPInternalError - IT IS A SYSTEM LEVEL ERROR
 *   CODE.
 */

int handle_tcp( SAState *psa, SOCKET sdRqst, struct sockaddr_in sinIn ) 
{
	char 					*pcInBuf, *pcOutBuf = NULL;
	int					iInSz, iOutSz;
//	struct sockaddr_in	sinIn;
	int					err = 0;
//	int					iSinInSz = sizeof(struct sockaddr_in);
/*	SOCKET				sdRqst = accept(psa->sdTCP,(struct sockaddr*)&sinIn,&iSinInSz);
	
	if ( sdRqst == SOCKET_ERROR || sdRqst < 0 ) 
	{
		LOG_STD_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_tcp accept",errno);
	} 
	else 
*/	{
		char pcHeader[HDRLEN];
		int iNumResults;
	
		err = readn( sdRqst, pcHeader, HDRLEN );
		if ( err < HDRLEN )
		{
//			CLOSESOCKET( sdRqst );
			
            if ( err < 0 )
			{
                if ( IsProcessTerminated() )
                {
                    return 0;
                }
                else if ( errno == EINTR )
                {
                    SLP_LOG( SLP_LOG_DROP, "SendDataToSLPd readn received EINTR, try again");
                    return 0;
                }
                else
                    LOG_STD_ERROR_AND_RETURN( SLP_LOG_DROP, "handle_tcp readn header", errno );
			}
            
			LOG_STD_ERROR_AND_RETURN( SLP_LOG_DROP, "handle_tcp received < HDRLEN bytes" ,err );
		}
		
		iInSz = GETLEN( pcHeader );
        
        if ( iInSz < MINHDRLEN )
        {
            SLP_LOG( SLP_LOG_ERR, "handle_tcp received a bad message who says its length is %d", iInSz );
            return SLP_PARSE_ERROR;
        }
                
		pcInBuf = safe_malloc( iInSz, pcHeader, HDRLEN ); /* copy header in */
        assert( pcInBuf );
        
		if ( ( err = readn( sdRqst, &pcInBuf[HDRLEN], iInSz-HDRLEN ) ) < 0 ) 
		{
			SLPFree(pcInBuf);
			LOG_STD_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_tcp readn rest",errno);
		}
    
//		SDLock(psa->pvMutex);
		if ( get_reply( SLP_TRUE, psa, pcInBuf, iInSz, &sinIn, &pcOutBuf, &iOutSz, &iNumResults ) == 0 ) 
		{
			err = writen( sdRqst, pcOutBuf, iOutSz ); 
		} 
		else 
		{
			SLP_LOG( SLP_LOG_ERR, "PANIC! handle_tcp get_reply: a dropped reply which is" );
			SLP_LOG( SLP_LOG_ERR, "       impossible.  This occurs if the sender set the" );
			SLP_LOG( SLP_LOG_ERR, "       MCAST RQST flag incorrectly." );
		}

// let the caller close the socket?    
//		CLOSESOCKET( sdRqst ); /* for now don't support persistent client sockets */
		SLPFree( pcOutBuf );
		SLPFree( pcInBuf );

//        SDUnlock(psa->pvMutex);	

		if ( err < 0 ) 
			LOG_STD_ERROR_AND_RETURN( SLP_LOG_DROP, "handle_tcp writen", errno );        
	}
	return 0;
}
#endif /* SLPTCP */

SLPInternalError srvreg_out(	const char *pcLang, const char *pcURL, 
                            const char *pcSrvType, const char *pcScope,
                            const char *pcAttrList, int iLifetime,
                            char **ppcOutBuf, int *piOutSz) 
{
    
    SLPInternalError err = SLP_OK;
    int offset = 0;
    
    if (pcAttrList == NULL) pcAttrList = "";
    
    *piOutSz = HDRLEN + strlen(pcLang) +              /* header      */
            3 + 2 + strlen(pcURL) + 1 +                 /* URL entry   */
            2 + strlen(pcSrvType) +                     /* srvtype     */
            2 + strlen(pcScope) +                       /* scope list  */
            2 + strlen(pcAttrList) +                    /* attrlist    */
            1;                                          /* # auths = 0 */
                
    /* generate the registration buffer */
    *ppcOutBuf =  safe_malloc(*piOutSz,0,0); 
    assert( *ppcOutBuf );
    
    if ((err=add_header(pcLang,*ppcOutBuf,*piOutSz,SRVREG,*piOutSz,&offset))<0) {
        SLPFree(*ppcOutBuf);
        LOG(SLP_LOG_ERR,"SrvRegDereg_out: add hdr failed"); 
    }
    SETFLAGS(*ppcOutBuf,FRESHFLAG);
    
    offset++; /* skip reserved byte */
    if ((err=add_sht(*ppcOutBuf,*piOutSz,
            (unsigned short)(iLifetime & 0x0000ffff),&offset)) < 0) {
        SLPFree(*ppcOutBuf);
        LOG(SLP_LOG_ERR,"SrvRegDereg_out: add lifetime failed");  
        return err;
    }
    if ((err = add_string(*ppcOutBuf, *piOutSz, pcURL, &offset)) < 0) {
        SLPFree(*ppcOutBuf);
        LOG(SLP_LOG_ERR,"SrvRegDereg_out: add url failed");
        return err;
    }
    offset++; /* leave # authenticators 0 */
    
    if ((err = add_string(*ppcOutBuf, *piOutSz, pcSrvType, &offset)) < 0) {
        SLPFree(*ppcOutBuf);
        LOG(SLP_LOG_ERR,"SrvRegDereg_out: add srvtype failed");
        return err;
    }
    
    if ((err = add_string(*ppcOutBuf, *piOutSz, pcScope, &offset)) < 0) {
        SLPFree(*ppcOutBuf);
        LOG(SLP_LOG_ERR,"SrvRegDereg_out: add scope failed");
        return err;
    }
    if ((err = add_string(*ppcOutBuf,*piOutSz,pcAttrList,&offset)) < 0) {
        SLPFree(*ppcOutBuf);
        LOG(SLP_LOG_ERR,"SrvRegDereg_out: add attrlist failed");
        return err;
    }
    /* #attr auth blocks, the last byte, is already 0'ed */
    
    return SLP_OK;
}

SLPInternalError srvdereg_out(const char *pcLang, const char *pcURL, 
                            const char *pcSrvType, const char *pcScope,
                            const char *pcAttrList, int iLifetime,
                            char **ppcOutBuf, int *piOutSz) 
{
    SLPInternalError err = SLP_OK;
    int offset = 0;
    
    if (pcAttrList == NULL) pcAttrList = "";
    
    *piOutSz = HDRLEN + strlen(pcLang) +				/* header      */
            2 + strlen(pcScope) +                       /* scope list  */
            3 + 2 + strlen(pcURL) + 1 +                 /* URL entry   */
            2 + strlen(pcAttrList);						/* attrlist    */
                
    /* generate the registration buffer */
    *ppcOutBuf =  safe_malloc(*piOutSz,0,0); 
    assert( *ppcOutBuf );
    
    if ((err=add_header(pcLang,*ppcOutBuf,*piOutSz,SRVDEREG,*piOutSz,&offset))<0) {
        SLPFree(*ppcOutBuf);
        LOG(SLP_LOG_ERR,"SrvRegDereg_out: add hdr failed"); 
    }
    SETFLAGS(*ppcOutBuf,FRESHFLAG);
    
    if ((err = add_string(*ppcOutBuf, *piOutSz, pcScope, &offset)) < 0) {
        SLPFree(*ppcOutBuf);
        LOG(SLP_LOG_ERR,"SrvRegDereg_out: add scope failed");
        return err;
    }
    
    offset++; /* skip reserved byte */
    if ((err=add_sht(*ppcOutBuf,*piOutSz,
            (unsigned short)(iLifetime & 0x0000ffff),&offset)) < 0) {
        SLPFree(*ppcOutBuf);
        LOG(SLP_LOG_ERR,"SrvRegDereg_out: add lifetime failed");  
        return err;
    }
    if ((err = add_string(*ppcOutBuf, *piOutSz, pcURL, &offset)) < 0) {
        SLPFree(*ppcOutBuf);
        LOG(SLP_LOG_ERR,"SrvRegDereg_out: add url failed");
        return err;
    }
//    offset++; /* leave # authenticators 0 */
    
    if ((err = add_string(*ppcOutBuf,*piOutSz,pcAttrList,&offset)) < 0) {
        SLPFree(*ppcOutBuf);
        LOG(SLP_LOG_ERR,"SrvRegDereg_out: add attrlist failed");
        return err;
    }
    
    return SLP_OK;
}

static SLPInternalError srvack_in(const char *pcOutBuf,const char *pcInBuf,int iInSz){

  int offset = 0;
  SLPInternalError err;
  Slphdr slph;

  if ((err = get_header(pcOutBuf,pcInBuf,iInSz,&slph,&offset)) < 0) {
    return err; /* error has already been logged */
  }

  SLPFree(slph.h_pcLangTag);

  return SLP2APIerr(slph.h_usErrCode);    
}

/*
 * propogate_registrations
 *
 *
 */
#define SAFESZ 5
SLPInternalError propogate_registrations(SAState *pstate, struct sockaddr_in sinDA, const char *pcScopes) 
{

    SAStore st = pstate->store;
    int i;
    SLPInternalError err = SLP_OK;  /* for SLP errors  */
    
    if ( st.size == 0 )
        return err;			// no registrations to propigate
        
    SDLock(pstate->pvMutex);
    for(i=0; i < st.size && err == SLP_OK; i++)  /* goes through each item in the store */
    {
        if (list_intersection(st.scope[i],pcScopes)) 
        {
            err = propogate_registration_with_DA( pstate, sinDA, st.lang[i], st.url[i], st.srvtype[i], st.scope[i], st.attrlist[i], st.life[i] );
        } /* if there is a list intersection for the given service item */
        else
        {
            SLP_LOG( SLP_LOG_DEBUG, "Skipping DA[%s] since it has scopeList:%s and we are looking for %s", inet_ntoa(sinDA.sin_addr), pcScopes, st.scope[i]);
        }
    } /* for each service item */
    
    SDUnlock(pstate->pvMutex);
    
    return err;
}

/*
 * propogate_registration_with_DA
 *
 *
 */
#define SAFESZ 5
SLPInternalError propogate_registration_with_DA(SAState *pstate, struct sockaddr_in sinDA, const char *lang, const char *url, const char *srvtype, const char *scope, const char *attrlist, int life ) 
{
    int iErr;
    int connected = 0;
    SLPInternalError err = SLP_OK;  /* for SLP errors  */
    SOCKET sd;
    
    char *pcInBuf=NULL, pcHead[SAFESZ], *pcOutBuf=NULL;
    int iInSz, iOutSz;

	if ( !lang || !url || !srvtype || !scope )
		return SLP_INVALID_REGISTRATION;

    if ((err = srvreg_out(lang, url, srvtype, scope, attrlist, life, &pcOutBuf, &iOutSz)) != SLP_OK) 
    {
        mslplog(SLP_LOG_ERR,"propogate_registration_with_DA: srvreg_out parsing out failed", slperror(err));
    }

    if ( IsDAUs( pstate, sinDA ) )
    {
        HandleRegistration( pcOutBuf, iOutSz, &sinDA );
    }
    else
    {
        sd = socket(AF_INET,SOCK_STREAM,0);
        if (sd == SOCKET_ERROR) 
        {
            pcOutBuf = NULL;
            iOutSz = 0;
            SLP_LOG( SLP_LOG_FAIL, "propogate_registration_with_DA: socket creation %s", strerror(errno));
            return SLP_NETWORK_ERROR;
        }
    
        if ((iErr = connect(sd,(struct sockaddr*)&sinDA, sizeof(struct sockaddr_in))) < 0) 
        {
            mslplog(SLP_LOG_DA,"propogate_registration_with_DA connect",strerror(errno));
            pcOutBuf = NULL;
            iOutSz = 0;
            CLOSESOCKET(sd);
            return SLP_NETWORK_ERROR;
        }
        connected = 1;

        if ((iErr = writen(sd,pcOutBuf,iOutSz)) <iOutSz) 
        {
        /*         int i = WSAGetLastError(); */
            SLPFree(pcOutBuf);
            pcOutBuf = NULL;
            iOutSz = 0;
            mslplog(SLP_LOG_ERR,"propogate_registration_with_DA writen",strerror(errno));
            CLOSESOCKET(sd);
            return SLP_NETWORK_ERROR;
        }
        
        if ((iErr = readn(sd,pcHead,SAFESZ)) < SAFESZ )
        {
            mslplog(SLP_LOG_ERR,"propogate_registration_with_DA readn head",strerror(errno));
            SLPFree(pcOutBuf);
            pcOutBuf = NULL;
            iOutSz = 0;
            CLOSESOCKET(sd);
            return SLP_NETWORK_ERROR;
        }
        
        iInSz = GETLEN(pcHead);
        pcInBuf = safe_malloc(iInSz,pcHead,SAFESZ);
        assert( pcInBuf );
        
        if ((iErr = readn(sd,&pcInBuf[SAFESZ],iInSz-SAFESZ)) < 0) 
        {
            SLPFree(pcInBuf);
            SLPFree(pcOutBuf);
            mslplog(SLP_LOG_ERR,"propogate_registration_with_DA readn msg",strerror(errno));
            CLOSESOCKET(sd);
            return SLP_NETWORK_ERROR;
        } 
        else
        {
            SLP_LOG( SLP_LOG_DA, "Registration propigated to DA: %s", inet_ntoa(sinDA.sin_addr) );
        }

        CLOSESOCKET(sd);
        connected = 0;		// our DA doesn't support persistent connections yet.  open a new one for each item
        
        err = srvack_in(pcOutBuf,pcInBuf,iInSz);
        
        if (err < 0) 
        {
            SLP_LOG( SLP_LOG_DA,"propogate_registration_with_DA srvack had err");
        }
    }
    
    SLPFree(pcInBuf);
    SLPFree(pcOutBuf);
    
    return err;
}


/*
 * propogate_deregistration_with_DA
 *
 *
 */
#define SAFESZ 5
SLPInternalError propogate_deregistration_with_DA(SAState *pstate, struct sockaddr_in sinDA, const char *lang, const char *url, const char *srvtype, const char *scope, const char *attrlist, int life ) 
{
    int iErr;
    int connected = 0;
    SLPInternalError err = SLP_OK;  /* for SLP errors  */
    SOCKET sd;
    
	if ( !lang || !url || !srvtype || !scope )
		return SLP_INVALID_REGISTRATION;

    char *pcInBuf=NULL, pcHead[SAFESZ], *pcOutBuf=NULL;
    int iInSz, iOutSz;

    if ((err = srvdereg_out(lang, url, srvtype, scope, attrlist, life, &pcOutBuf, &iOutSz)) != SLP_OK) 
    {
        mslplog(SLP_LOG_DA,"propogate_deregistration_with_DA: srvdereg_out parsing out failed", slperror(err));
    }

    if ( IsDAUs( pstate, sinDA ) )
    {
        if ( AreWeADirectoryAgent() )
        {
            HandleDeregistration( pcOutBuf, iOutSz, &sinDA );
        }
    }
    else
    {
        sd = socket(AF_INET,SOCK_STREAM,0);
        if (sd == SOCKET_ERROR) 
        {
            SLPFree(pcOutBuf);
            SLP_LOG( SLP_LOG_DA, "propogate_deregistration_with_DA: socket creation %s", strerror(errno));
            return SLP_NETWORK_ERROR;
        }
    
        if ((iErr = connect(sd,(struct sockaddr*)&sinDA, sizeof(struct sockaddr_in))) < 0) 
        {
            mslplog(SLP_LOG_DA,"propogate_deregistration_with_DA connect",strerror(errno));
            SLPFree(pcOutBuf);
            CLOSESOCKET(sd);
            return SLP_NETWORK_ERROR;
        }
        connected = 1;

        if ((iErr = writen(sd,pcOutBuf,iOutSz)) <iOutSz) 
        {
        /*         int i = WSAGetLastError(); */
            SLPFree(pcOutBuf);
            mslplog(SLP_LOG_DA,"propogate_deregistration_with_DA writen",strerror(errno));
            CLOSESOCKET(sd);
            return SLP_NETWORK_ERROR;
        }
        
        if ((iErr = readn(sd,pcHead,SAFESZ)) < SAFESZ )
        {
            mslplog(SLP_LOG_DA,"propogate_deregistration_with_DA readn head",strerror(errno));
            SLPFree(pcOutBuf);
            CLOSESOCKET(sd);
            return SLP_NETWORK_ERROR;
        }
        
        iInSz = GETLEN(pcHead);

        if ( iInSz < SAFESZ )
        {
            SLP_LOG( SLP_LOG_DA, "Deregistration propigation attempt to DA: %s returned an invalid reply.", inet_ntoa(sinDA.sin_addr) );
        }
        else
        {
            pcInBuf = safe_malloc(iInSz,pcHead,SAFESZ);
            assert( pcInBuf );
            
            if ((iErr = readn(sd,&pcInBuf[SAFESZ],iInSz-SAFESZ)) < 0) 
            {
                SLPFree(pcInBuf);
                SLPFree(pcOutBuf);
                mslplog(SLP_LOG_DA,"propogate_deregistration_with_DA readn msg",strerror(errno));
                CLOSESOCKET(sd);
                return SLP_NETWORK_ERROR;
            } 
            else
            {
                SLP_LOG( SLP_LOG_DA, "Deregistration propigated to DA: %s", inet_ntoa(sinDA.sin_addr) );
            }
        }
        
        CLOSESOCKET(sd);
        connected = 0;		// our DA doesn't support persistent connections yet.  open a new one for each item
        
        err = srvack_in(pcOutBuf,pcInBuf,iInSz);
        
        if (err < 0) 
        {
            LOG(SLP_LOG_DA,"propogate_deregistration_with_DA srvack had err");
        }
    }
    
    SLPFree(pcInBuf);
    SLPFree(pcOutBuf);
    
    return err;
}

/* -------------------------------------------------------------------- */

/*
 * mslpd_daadvert_callback
 *
 *    Simply does the registration when the callback is made for 
 *    incoming DAAdverts.  Each incoming DA is added to the DATable.
 *    DAs which are 'new' by the 'boot timestamp' rules receive
 *    registration propogation, if the DATable has been initialized.
 *
 *    (On active discovery, we wait till we're done with discovering
 *    all DAs before we start registering.  Otherwise the registrations
 *    would take up precious time and we wouldn't find all the DAs.)
 * 
 *    The 'min-refresh-interval' in the DAAdvert attributes is used
 *    to update the com.sun.slp.minRefreshInterval property.  This
 *    property is used for reregistering with all known DAs.
 *
 *    THIS IS USED FOR PASSIVE DA DISCOVERY.
 *
 *    NOTE:  This is different than the mslplib_daadvert_callback.
 *    The mslplib callback does not do registration propogation.
 *    The mslplib callback is used for ACTIVE DA DISCOVERY.
 *
 *      hSLP         This is actually the SAState
 *      iErrorCode   The error code associated with the request (if active)
 *      sin          The sin of the DA
 *      pcScopeList  The scopes supported by the DA
 *      pcDAAttrs    The attributes of the DA
 *      lBootTime    The boot time of the DA
 *      pvUser       This is actually the DATable
 *
 * Side effect:  Will update the DATable.
 */
void mslpd_daadvert_callback(SLPHandle hSLP,
			     int iErrorCode,
			     struct sockaddr_in sin,
			     const char *pcScopeList,
			     const char *pcDAAttrs,
			     long lBootTime,
			     void *pvUser) 
{
//    DATable *pdat = (DATable *) pvUser;
    DATable *pdat = GetGlobalDATable();
    SAState *psa  = (SAState *) hSLP;
    
    char *pc = "min-refresh-interval=";
    int iSz = strlen(pc);
    int iInterval;
    
    while (*pcDAAttrs != '\0') 
    {
        if (SDstrncasecmp(pc,pcDAAttrs,iSz)) 
        	pcDAAttrs++;
    	else
        {        
            char buf[20];
            int  index = 0;
            
            pcDAAttrs += iSz;
            while (pcDAAttrs != NULL && isdigit(*pcDAAttrs)) 
            {
                buf[index] = *pcDAAttrs;
            }
        
            char*	endPtr = NULL;
			iInterval = strtol(buf,&endPtr,10);
            if (iInterval <= 0 || errno == EINVAL) 
            {
                LOG(SLP_LOG_ERR, "mslpd_daadvert_callback: got a bogus min-refresh-interval");
            } 
            else 
            {
                if (SLPGetRefreshInterval() < iInterval) 
                {
                    SLP_LOG( SLP_LOG_DEBUG, "mslpd_daadvert_callback: increase min-refresh-interval");
                    SLPSetProperty("com.sun.slp.minRefreshInterval",buf);
                }
            }
            break;
        }
    }
    
    /*
    * If the DAAdvert is new and we are not doing the initial active
    * discovery, forward the registrations as appropriate to the DA.
    */
    if (dat_daadvert_in(pdat, sin, pcScopeList, lBootTime) == 1 && pdat->initialized == SLP_TRUE) 
    {
        SLPInternalError	err = SLP_OK;
        
        RegisterAllServicesWithDA( psa, sin, pcScopeList );		// pass this off to our SLPDARegisterer thread
//        err = propogate_registrations(psa, sin, pcScopeList);
        
        if ( err )
        {
            SLP_LOG( SLP_LOG_DA, "Error trying to propogate a registration to a newly detected DA: %s", inet_ntoa(sin.sin_addr) );		// just log this
        }
    }
}

/*
 * get_reply
 *
 *   Takes a request buffer and returns a reply buffer.
 *   This routine must never 'fail' (unless memory runs out, etc.
 *   in which case the mslpd fails.)
 *
 *     psa          The SAState data for processing requests.
 *     pcInBuf      The buffer with the incoming request.
 *     iInSz        The size of the incoming request.
 *     ppcOutBuf    The buffer (allocated) for the outgoing reply.
 *     piOutSz      Will be set to the size of the outgoing reply.
 *     piNumResults Will be set to the number of results obtained.
 *  
 * return: 0 if there is data to return, -1 otherwise if the 
 *         request is to be dropped.
 *         Note that it is possible to not have a return - when
 *         a DAAdvert is received.
 */
int	numRegErrors = 1;

static int get_reply( SLPBoolean viaTCP, SAState *psa, const char *pcInBuf, int iInSz, struct sockaddr_in* sinIn,
                     char **ppcOutBuf, int *piOutSz, int *piNumResults)
{
    Slphdr slphdr;
    int offset = 0;
    int returnValue = 0;
    SLPReturnError returnSLPError = NO_ERROR;
    SLPInternalError slperr;

    *piOutSz = 0;
    *ppcOutBuf = NULL;
    *piNumResults = 0;

    memset(&slphdr,0,sizeof(Slphdr));

    if (get_header(NULL, pcInBuf,iInSz,&slphdr,&offset) != SLP_OK)
    {
        char	logMsg[1024];
        
        sprintf( logMsg, "couldn't parse incoming message from %s", inet_ntoa(sinIn->sin_addr) );
        SLP_LOG( SLP_LOG_ERR, logMsg );
        
        /* FIX ME do this better ... */
        *piOutSz = HDRLEN + 2 + 2;								// header plus lang tag of 2 bytes (en) and 2 bytes for error code
        *ppcOutBuf = (char*)malloc( *piOutSz );
        
        generate_error(PARSE_ERROR, slphdr, *ppcOutBuf, piOutSz);
        returnValue = 0;   /* always something to return */
        
        
    }
    else if (slphdr.h_ucFun == DAADVERT)
    {
        if ((slperr = handle_daadvert_in(NULL,pcInBuf, iInSz,(void*)(psa->pdat),
                                        (SLPHandle)psa, (void*) mslpd_daadvert_callback,
                                        SLPDAADVERT_CALLBACK)) != SLP_OK)
        
        SLP_LOG( SLP_LOG_DROP, "get_reply: %s, handle_daadvert_in from %s",slperror(slperr), inet_ntoa(sinIn->sin_addr) );
        
        returnValue = -1; /* DO NOT return a reply */

    }
    else if (slphdr.h_ucFun == SRVRQST)
    {
		if ( AreWeADirectoryAgent() )
            returnValue = DAHandleRequest( psa, sinIn, viaTCP, &slphdr,pcInBuf,iInSz,ppcOutBuf,piOutSz, piNumResults );
        else
            returnValue = store_request(psa, viaTCP, &slphdr,pcInBuf,iInSz,ppcOutBuf,piOutSz, piNumResults);

#ifdef EXTRA_MSGS

    }
    else if (slphdr.h_ucFun == SRVTYPERQST)
    {
        returnValue = opt_type_request(psa,&slphdr,pcInBuf,iInSz,ppcOutBuf,piOutSz, piNumResults);
    }
    else if (slphdr.h_ucFun == ATTRRQST)
    {
        returnValue = opt_attr_request(psa,&slphdr,pcInBuf,iInSz,ppcOutBuf,piOutSz, piNumResults);
#endif /* EXTRA_MSGS */
    }
#ifdef MAC_OS_X
	else if ( AreWeADirectoryAgent() && slphdr.h_ucFun == SRVREG )
    {
        returnSLPError = HandleRegistration( pcInBuf, iInSz, sinIn );
        
        *piOutSz   = HDRLEN+GETLANGLEN(pcInBuf)+2;		// error is only 2 bytes
        *ppcOutBuf = safe_malloc(*piOutSz,0,0);
        assert( *ppcOutBuf );
        
        *piNumResults = 0;

        generate_error(returnSLPError, slphdr, *ppcOutBuf, piOutSz);

        returnValue = 0;   /* always something to return */
    }
    else if ( AreWeADirectoryAgent() && slphdr.h_ucFun == SRVDEREG )
    {
        returnSLPError = HandleDeregistration( pcInBuf, iInSz, sinIn );
        
        *piOutSz   = HDRLEN+GETLANGLEN(pcInBuf)+2;
        *ppcOutBuf = safe_malloc(*piOutSz,0,0);
        assert( *ppcOutBuf );
        
        *piNumResults = 0;

        generate_error(returnSLPError, slphdr, *ppcOutBuf, piOutSz);

//        if (returnValue)
//        	SLP_LOG( SLP_LOG_ERR, "HandleDeregistration returned error!" );
        
        returnValue = 0;   /* always something to return */
    }
    else if ( slphdr.h_ucFun == PluginInfoReq )
    {
        SLP_LOG( SLP_LOG_DEBUG, "get_reply processing PluginInfoReq" );
        slperr = HandlePluginInfoRequest( psa, pcInBuf, iInSz, ppcOutBuf, piOutSz );
        
        if ( slperr == SLP_OK )
            *piNumResults = 1;
            
        returnValue = 0;
    }
#endif /* MAC_OS_X */
    else
    {
        *piOutSz   = HDRLEN+GETLANGLEN(pcInBuf)+2;
        *ppcOutBuf = safe_malloc(*piOutSz,0,0);
        assert( *ppcOutBuf );
        
        *piNumResults = 0;

        generate_error(RQST_NOT_SUPPORTED, slphdr, *ppcOutBuf, piOutSz);
        returnValue = 0;   /* always something to return */
    }
    
    SLPFree(slphdr.h_pcLangTag);
    return returnValue;
}

/*
 * generate_error
 *
 *   This routine simply fills in a buffer (which is assumed to be
 *   preallocated) with an error code.  It uses the incoming request's
 *   header to assign the language and XID code.
 *
 *     iErr   The error value to return
 *     s      The header of the request
 *     out    The buffer to return
 *     pI     The length of the buffer to return
 *
 * Side Effect:  Not really a side effect, but the buffer 'out' is
 *   modified to include the error value.  
 */
static void  generate_error(SLPReturnError iErr, Slphdr s,char *out, int *pI) 
{
    int offset = 0;
    int fun;
    SLPInternalError err;
    
    assert(out && pI); /* sanity checks for internal interface */
    
    memset(out,0,*pI);
    
    switch (s.h_ucFun)
    {
        case SRVREG:
        case SRVDEREG:  	fun = SRVACK; 
            break;
       
         case SRVTYPERQST: 	fun = SRVTYPERPLY; 
            break;
        
        case ATTRRQST:    	fun = ATTRRPLY; 
            break;
        
        default: 			fun = SRVRPLY;
    }
    //  *pI = 0;
	char*	endPtr = NULL;
    if ((err = add_header((s.h_pcLangTag)?s.h_pcLangTag:"en",out,
                (SLPGetProperty("net.slp.MTU"))?strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10):1400,
                fun,*pI,&offset))
        != SLP_OK) {
        mslplog(SLP_LOG_ERR,"generate_error: could not create header",slperror(err)); 
    }
    SETXID(out,s.h_usXID);
    SETSHT(out,iErr,offset);
}

int IsDAUs( SAState* psa, struct sockaddr_in sinDA )
{
    char*		daHost = inet_ntoa( sinDA.sin_addr );
    
    if ( strcmp( psa->pcSAHost, daHost ) == 0 )
        return 1;
    else
        return 0;
}
