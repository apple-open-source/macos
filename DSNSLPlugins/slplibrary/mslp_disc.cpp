
/*
 * mslp_disc.c : Minimal SLP v2 Active DADiscovery.
 *
 * Version: 1.12
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

#include "mslp_sd.h"
#include "slp.h"
#include "mslp_dat.h" /* needed for the mslplib.h, not used here */
#include "mslp.h"
#include "mslplib.h"     /* needed for the definition of get_converge_result */

/*
 * We are assuming this will only be done synchronously.  Otherwise we'd have
 * a problem that the buffers are being freed too soon.
 */
SLPInternalError active_da_discovery(SLPHandle hSLP, time_t tWait, SOCKET sd, int iMTU,
			     struct sockaddr_in sin, /* could be mc or bc */
		             const char *pcScopeList,
			     void *pvUser, void *pvCallback, CBType cbt) 
{
    SLPInternalError 	err;
    int         iNoSA = 0;
    char		*pcSendBuf = safe_malloc(iMTU,0,0);
    char		*pcRecvBuf = safe_malloc(RECVMTU,0,0);
    const char	*pcNOSA    = SLPGetProperty("com.sun.slp.noSA");
	char*		endPtr = NULL;
	
    if (!(err = generate_srvrqst(pcSendBuf,&iMTU,"en",pcScopeList, "service:directory-agent",""))) 
    {
        if (pcNOSA && !SDstrcasecmp(SLPGetProperty("com.sun.slp.noSA"),"true")) 
        {
            /* temporarily clear this property as it will interfere with DA disc */
            iNoSA = 1;
            SLPSetProperty("com.sun.slp.noSA",NULL);
        }
        
        SETFLAGS(pcSendBuf,(unsigned char) MCASTFLAG);
    
        err = get_converge_result(tWait,sd,pcSendBuf,iMTU,pcRecvBuf,RECVMTU,sin, (unsigned char) strtol(SLPGetProperty("net.slp.multicastTTL"), &endPtr, 10), pvUser,hSLP,pvCallback,cbt);
    
        if (iNoSA) 
        {
            /* restore ths setting of this property */
            SLPSetProperty("com.sun.slp.noSA","true");
        }       
    }
    
    SLPFree(pcSendBuf);
    SLPFree(pcRecvBuf);
    
    if (err != SLP_OK) 
        SLP_LOG(SLP_LOG_DEBUG,"active_da_discovery failed",err);

    return err;
}

/*
 * process DAAdvert and SAAdvert messages
 */



SLPInternalError handle_daadvert_in(const char *pcSendBuf, /* for testing vs rqst */
			const char *pcRecvBuf, int iRecvSz,
			void *pvUser, SLPHandle hSLP,
			void *pvCallback, CBType cbCallbackType) 
{
    int      offset = 0;
    int      iTemp = 0;
    int      iErrorCode = 0;
    char    *pcURL = NULL, *pcScopes = NULL;
//    DATable *pdat = (DATable *) pvUser;
    DATable *pdat = GetGlobalDATable();
    SLPInternalError err = SLP_OK;
    struct sockaddr_in sinDA;
    long     lBootTime;
    Slphdr   slph;
    char     *pcDAAttrs;
    
    if ( !pdat )
        return SLP_OK;		// don't handle this advertisement until we have a pdat set up
    
    assert(pdat && pcRecvBuf && pvCallback && cbCallbackType == SLPDAADVERT_CALLBACK);

    if ((err= get_header(pcSendBuf, pcRecvBuf, iRecvSz, &slph, &offset)) !=SLP_OK) 
    {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_daadvert_in: get_header failed",err);
    }
    
    if ( slph.h_iOffset > 0 )
    {
        // we have an extension here.  We only support the SCOPE_SPONSER_EXTENSION_ID so we'll
        // see if this is set.  We should be spanning the potential (n) extensions, but since 
        // a) we don't support any other extensions anwyay and
        // b) it isn't likely that others will support this one in the near future (private id) we'll
        // go with just looking at the first.
        int			extOffset = slph.h_iOffset, extID = 0;
        
        if ((err = get_sht(pcRecvBuf,iRecvSz,&extOffset,&extID)) != SLP_OK)
        {
            SLP_LOG( SLP_LOG_ERR, "handle_daadvert_in, get_sht extension id failed: %s", slperror(err) );
        }
        else
        {
            if ( extID == SCOPE_SPONSER_EXTENSION_ID )
            {
                // cool, we have a scope that the DA want's us to use for registrations
                long		extLength;
                char*		scopeToUse = NULL;
                
                if ( extOffset )
                    extLength = extOffset - slph.h_iOffset;
                else
                    extLength = slph.h_ulLen - slph.h_iOffset;
                    
                extLength -= 2;		// subtract length of extension ID
                
                scopeToUse = safe_malloc( extLength+1, pcRecvBuf+extOffset, extLength );
                assert( scopeToUse );
                    
                SLP_LOG( SLP_LOG_DA, "Received manditory scope to use for registration: %s", scopeToUse );
            }
        }        
    }
    
    if ((err = get_sht(pcRecvBuf,iRecvSz,&offset,&iErrorCode)) != SLP_OK) 
    {
        SLPFree(slph.h_pcLangTag);
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_daadvert_in: get_sht errcode",err);
    }
        
    if ((err = get_sht(pcRecvBuf,iRecvSz,&offset,&iTemp)) != SLP_OK) 
    {
        SLPFree(slph.h_pcLangTag);
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_daadvert_in: get_long boot time, sht 1",err);
    }
    lBootTime = (0xffff & iTemp)<<16;
    
    if ((err = get_sht(pcRecvBuf,iRecvSz,&offset,&iTemp)) != SLP_OK) 
    {
        SLPFree(slph.h_pcLangTag);
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_daadvert_in: get_long boot time, sht 1",err);
    }
    lBootTime += (0xffff & iTemp);
        
    if ((err = get_string(pcRecvBuf,iRecvSz,&offset,&pcURL)) != SLP_OK) 
    {
        SLPFree(slph.h_pcLangTag);
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_daadvert_in: get_string sa url",err);
    }
    else if ((err = get_string(pcRecvBuf,iRecvSz,  &offset,&pcScopes)) != SLP_OK) 
    {
        SLPFree(slph.h_pcLangTag);
        SLPFree(pcURL);
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_daadvert_in: get_string scopes",err);
    }
        
    if ((err = get_string(pcRecvBuf,iRecvSz,&offset,&pcDAAttrs)) != SLP_OK) 
    {
        SLPFree(slph.h_pcLangTag);
        SLPFree(pcURL);
        SLPFree(pcScopes);
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_daadvert_in: get_string da attributes",err);
    }
    
    /* ignore the DAAdvert authentication blocks for now! */
        
    if ((err = get_sin_from_url(pcURL,strlen(pcURL),&sinDA)) != SLP_OK) 
    {
        SLPFree(slph.h_pcLangTag);
        SLPFree(pcURL);
        SLPFree(pcScopes); 
        SLPFree(pcDAAttrs);   
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_daadvert_in get_sin da",err);
    }
        
    if (pvCallback != NULL) 
    {
        SLPDAAdvertCallback *psdaac = (SLPDAAdvertCallback *) pvCallback;
        /* the SA will propogate regs to this DA */
        SLP_LOG( SLP_LOG_DEBUG,"handle_daadvert_in calling callback");
        psdaac(hSLP,iErrorCode,sinDA,pcScopes,pcDAAttrs,lBootTime,(void*)pdat);
    }
    else 
        err = (SLPInternalError) iTemp;
    
    SLPFree(slph.h_pcLangTag);
    SLPFree(pcURL); /* we are not using this information */
    SLPFree(pcScopes);
    SLPFree(pcDAAttrs);   

    return SLP_OK;  
}

/*
 * The following code is only for supporting 'active SA discovery'.
 * This is conditionally compiled since it is an optional feature.
 * It is only useful for mslplib.  In the absense of scope configuration
 * from a config file and any DAs, the library will use this function
 * to obtain a list of scopes from SAs present on the network in order
 * to support the SLPFindScopes interface.
 */

#ifdef EXTRA_MSGS
SLPInternalError active_sa_discovery(SLPHandle hSLP, const char *pcTypeHint) 
{
    int         iErr;
    SLPInternalError    err;
    int         iNoSA = 0;
    char*	endPtr = NULL;
//    int         iWait = atoi(SLPGetProperty("net.slp.multicastMaximumWait"));
    int         iWait = strtol(SLPGetProperty("net.slp.multicastMaximumWait"), &endPtr, 10);
//    int         iMTU = atoi(SLPGetProperty("net.slp.MTU"));
    int         iMTU = strtol(SLPGetProperty("net.slp.MTU"), &endPtr, 10);
    char       *pcSendBuf = safe_malloc(iMTU,0,0);
    char       *pcRecvBuf = safe_malloc(RECVMTU,0,0);
    char        pcRqst[120];
    struct sockaddr_in sin;
//    UA_State   *puas = (UA_State*) hSLP;
//    DATable    *pdat = puas->pdat;
    DATable    *pdat = GetGlobalDATable();
    SOCKET      sd = socket(AF_INET, SOCK_DGRAM, 0);
    const char *pcIsBCast = SLPGetProperty("net.slp.isBroadcastOnly");
    const char *pcIsNoSA  = SLPGetProperty("com.sun.slp.noSA");
    
    if (sd < 0 || sd == SOCKET_ERROR)
    {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"active_sa_discovery: socket",SLP_NETWORK_INIT_FAILED);	// LOG_SLP_ERROR_AND_RETURN also returns error
    }
    
    memset(&sin,0,sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port   = htons(SLP_PORT);
    
    if (pcIsBCast && !(SDstrcasecmp(pcIsBCast,"true"))) 
    {
        int f = 1;
        iErr = setsockopt(sd,SOL_SOCKET,SO_BROADCAST,(char*)&f,sizeof(f));
        if (iErr) 
        {
            CLOSESOCKET(sd);
            mslplog(SLP_LOG_FAIL, "mslpd_init_network: set broadcast option",strerror(errno));
            //LOG_SLP_ERROR_AND_RETURN(SLP_LOG_FAIL,"mslpd_init_network: set broadcast option", SLP_NETWORK_INIT_FAILED);
        }
        sin.sin_addr.s_addr = BROADCAST;
    } 
    else 
    {
        sin.sin_addr.s_addr = SLP_MCAST;
        if ((err = set_multicast_sender_interf(sd)) != SLP_OK) 
        {
            CLOSESOCKET(sd);
            LOG_SLP_ERROR_AND_RETURN(SLP_LOG_FAIL,"mslpd_init_network: set_multicast_sender_interf",err);
        }
    }
    
    if ((pcRqst != NULL) && (strlen(pcRqst) > 0)) 
    {
        sprintf(pcRqst,"(service-type=%s)",pcTypeHint);
    } 
    else 
    {
        pcRqst[0] = '\0';
    }
  
    if (!(err = generate_srvrqst(pcSendBuf,&iMTU,"en","", "service:service-agent",pcRqst))) 
    {
        if (pcIsNoSA && !SDstrcasecmp(pcIsNoSA,"true")) 
        {
        /* temporarily clear this property as it will interfere with SA disc */
            iNoSA = 1;
            SLPSetProperty("com.sun.slp.noSA",NULL);
        }
        
        SETFLAGS(pcSendBuf,(unsigned char) MCASTFLAG);
    
        err = get_converge_result(iWait, sd, pcSendBuf, iMTU, pcRecvBuf, RECVMTU, sin,  (unsigned char) strtol(SLPGetProperty("net.slp.multicastTTL"), &endPtr, 10), (void*)pdat, hSLP, (void*) handle_saadvert_in, SLPSAADVERT_CALLBACK);
    
        if (iNoSA) 
        {
            /* restore ths setting of this property */
            SLPSetProperty("com.sun.slp.noSA","true");
        }       
    }
    
    SLPFree(pcSendBuf);
    SLPFree(pcRecvBuf);
    CLOSESOCKET(sd);
    
    if (err != SLP_OK) 
    {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"active_sa_discovery failed",err);
    }
        
    return err;
}

#ifdef MAC_OS_X
/*
 * The following code is only for supporting 'active SA discovery' that
 * needs async behavior by returning discovered scopes via a user supplied
 * callback.
 */

#ifdef EXTRA_MSGS
SLPInternalError active_sa_async_discovery(SLPHandle hSLP, SLPScopeCallback callback, void *pvUser, const char *pcTypeHint) 
{
    int				iErr;
    SLPInternalError		err = SLP_OK;
    int				iNoSA = 0;
    char*			endPtr = NULL;
//    int				iWait = atoi(SLPGetProperty("net.slp.multicastMaximumWait"));
    int				iWait = strtol(SLPGetProperty("net.slp.multicastMaximumWait"), &endPtr, 10);
//    int				iMTU = atoi(SLPGetProperty("net.slp.MTU"));
    int				iMTU = strtol(SLPGetProperty("net.slp.MTU"), &endPtr, 10);
    char			*pcSendBuf = safe_malloc(iMTU,0,0);
    char			*pcRecvBuf = safe_malloc(RECVMTU,0,0);
    char			pcRqst[120];
    struct sockaddr_in sin;
    //  UA_State   *puas = (UA_State*) hSLP;
    SOCKET			sd = socket(AF_INET, SOCK_DGRAM, 0);
    const char		*pcIsBCast = SLPGetProperty("net.slp.isBroadcastOnly");
    const char		*pcIsNoSA  = SLPGetProperty("com.sun.slp.noSA");

    if (sd < 0 || sd == SOCKET_ERROR)
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"active_sa_async_discovery: socket",SLP_NETWORK_INIT_FAILED);
  
    memset(&sin,0,sizeof sin);
    sin.sin_family = AF_INET;
    sin.sin_port   = htons(SLP_PORT);
    
    if (pcIsBCast && !(SDstrcasecmp(pcIsBCast,"true"))) 
    {
        int f = 1;
        iErr = setsockopt(sd, SOL_SOCKET, SO_BROADCAST, (char*)&f, sizeof(f));
        if (iErr) 
        {
            mslplog(SLP_LOG_FAIL, "active_sa_async_discovery: set broadcast option",strerror(errno));
            err = SLP_NETWORK_INIT_FAILED;
        }
        sin.sin_addr.s_addr = BROADCAST;
    } 
    else 
    {
        sin.sin_addr.s_addr = SLP_MCAST;
        if ((err = set_multicast_sender_interf(sd)) != SLP_OK) 
        {
            mslplog(SLP_LOG_ERR,"active_sa_async_discovery: set_multicast_sender_interf",strerror(errno));
        }
    }
  
    if ( !err ) 
    {
    if ((pcRqst != NULL) && (strlen(pcRqst) > 0)) 
    {
        sprintf(pcRqst,"(service-type=%s)",pcTypeHint);
    } 
    else 
    {
        pcRqst[0] = '\0';
    }
    
    if (!(err = generate_srvrqst(pcSendBuf, &iMTU, "en", "", "service:service-agent", pcRqst))) 
    {
        if (pcIsNoSA && !SDstrcasecmp(pcIsNoSA,"true")) 
        {
            /* temporarily clear this property as it will interfere with SA disc */
            iNoSA = 1;
            SLPSetProperty("com.sun.slp.noSA",NULL);
        }
      
        SETFLAGS(pcSendBuf,(unsigned char) MCASTFLAG);
    
        err = get_converge_result(iWait,sd,pcSendBuf,iMTU,pcRecvBuf,RECVMTU,sin,  (unsigned char) strtol(SLPGetProperty("net.slp.multicastTTL"), &endPtr, 10), 
                    (void*) pvUser,hSLP,(void*) callback,
                    SLPSAADVERT_ASYNC_CALLBACK);

        if (iNoSA) 
        {
            /* restore ths setting of this property */
            SLPSetProperty("com.sun.slp.noSA","true");
        }       
    }
    }
    
    SLPFree(pcSendBuf);
    SLPFree(pcRecvBuf);
    CLOSESOCKET(sd);
    
    if (err != SLP_OK)
        SLP_LOG(SLP_LOG_DEBUG,"active_sa_async_discovery failed",err);
    
    return err;
}

#endif
#endif	/* MAC_OS_X */

SLPInternalError handle_saadvert_in(const char *pcSendBuf, /* for testing vs rqst */
			    const char *pcRecvBuf, int iRecvSz,
			    void *pvUser, SLPHandle hSLP,
			    void *pvCallback, CBType cbCallbackType) 
{
  int      offset = 0;
  char    *pcURL = NULL;     /* the SA's URL from the SAAdvert */
  char    *pcScopes = NULL;  /* the scope list from the SAAdvert */
  char    *pcAttrs = NULL;   /* the attr list from the SAAdvert */
  DATable *pdat = (DATable *) pvUser;
  Slphdr   slph;
  SLPInternalError err;
  
  hSLP = hSLP;               /* we don't need this but the callback has it */
  
#ifdef MAC_OS_X
  assert(pdat && pcRecvBuf && pcSendBuf && pvCallback &&
	 (cbCallbackType == SLPSAADVERT_CALLBACK || cbCallbackType == SLPSAADVERT_ASYNC_CALLBACK ) );
#else
    assert(pdat && pcRecvBuf && pcSendBuf && pvCallback &&
	 cbCallbackType == SLPSAADVERT_CALLBACK);
#endif /* MAC_OS_X */
    
  if ((err = get_header(pcSendBuf,pcRecvBuf,iRecvSz,&slph,&offset))!=SLP_OK) 
  {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_saadvert_in get_header failed",err);
  }
  
  if ((err = get_string(pcRecvBuf,iRecvSz,&offset,&pcURL)) != SLP_OK) 
  {
    SLPFree(slph.h_pcLangTag);
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_saadvert_in get_string sa url",err);
  }
  
  if ((err = get_string(pcRecvBuf,iRecvSz,&offset,&pcScopes)) != SLP_OK) 
  {
    SLPFree(slph.h_pcLangTag);
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_saadvert_in get_string scopes",err);
  } 

  if ((err = get_string(pcRecvBuf,iRecvSz,&offset,&pcAttrs)) != SLP_OK) 
  {
    SLPFree(slph.h_pcLangTag);
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_saadvert_in get_string attrs",err);
  } 

    dat_saadvert_in(pdat,pcScopes,pcAttrs);

  /* we have already copied these buffers */
  SLPFree(slph.h_pcLangTag);
  SLPFree(pcURL);
  SLPFree(pcScopes);
  SLPFree(pcAttrs);
  
  return SLP_OK;
}
#endif /* EXTRA_MSGS */
