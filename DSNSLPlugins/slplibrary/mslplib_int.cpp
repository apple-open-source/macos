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
 * mslplib_int.c : Minimal SLP v2 User Agent API implementation.
 *
 * Version: 1.17
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
#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "mslp_sd.h"     /* System dep. / source compatibility   */
#include "slp.h"         /* SLP API supported features           */
#include "mslp.h"        /* Definitions for mslp, shared         */
#include "mslp_dat.h"    /* Definitions for mslp_dat             */
#include "mslplib.h"     /* Definitions specific to the mslplib  */
#include "mslplib_opt.h" /* Definitions for optional msg support */
#include "CNSLTimingUtils.h"

/*
 * ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! !
 *  ! ! GLOBAL - used for configuration properties  ! !
 * ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! !
 */
static MslpHashtable *pmhConfig = NULL;


/*
 * ----------------------------------------------------------------------------
 *  Functions used internally to this module.
 */

static SLPInternalError get_reply(char *pcSend, int iSize,
			  const char *pcScope,
			  UA_State    *puas,
			  void *pvCallback, void *pvUser, CBType cbt );

EXPORT void EncodeChar( const char c, char* newEscapedBufPtr );
EXPORT char DecodeChar( const char* encodedTriplet  );

/*
 * ----------------------------------------------------------------------------
 *  Implemented interfaces of the SLP API
 */
EXPORT SLPInternalError SLPOpen(const char *pcLang, SLPBoolean isAsync, SLPHandle *phSLP, CFRunLoopRef runLoopRef ) 
{
    SLPInternalError err = SLP_OK;
    UA_State *puas;
    

    int piTimes[5] = { 3000, 3000, 3000, 3000, 3000 };

    *phSLP = NULL;
    
    if (isAsync) 
        return SLP_NOT_IMPLEMENTED;
    
    SLP_LOG( SLP_LOG_DEBUG, "SLPOpen called" );
    if (pmhConfig == NULL) 
        pmhConfig = mslp_hash_init();
    
    #ifdef EXTRA_MSGS
    if (!SLPGetProperty("com.sun.slp.regfile"))
        SLPSetProperty("com.sun.slp.regfile",SDDefaultRegfile());
    if (!SLPGetProperty("com.sun.slp.tempfile"))
        SLPSetProperty("com.sun.slp.tempfile",SDDefaultTempfile());
    #endif /* EXTRA_MSGS */
    
    if (!SLPGetProperty("net.slp.multicastMaximumWait"))
        SLPSetProperty("net.slp.multicastMaximumWait","15000");	// default is supposed to be 15 sec!
    if (!SLPGetProperty("net.slp.multicastTTL"))
        SLPSetProperty("net.slp.multicastTTL",MCAST_TTL);
    if (!SLPGetProperty("net.slp.MTU"))
        SLPSetProperty("net.slp.MTU",SENDMTU);
    if (!SLPGetProperty("com.apple.slp.port"))
        SLPSetProperty("com.apple.slp.port","427");
    if (!SLPGetProperty("com.sun.slp.minRefreshInterval"))  
        SLPSetProperty("com.sun.slp.minRefreshInterval","10800");
    if (!SLPGetProperty("net.slp.locale")) 
    {
        if (pcLang) 
            SLPSetProperty("net.slp.locale",pcLang);
        else
            SLPSetProperty("net.slp.locale","en");
    }

    if (getenv("SLP_CONF_FILE") != NULL)
        mslp_hash_read(pmhConfig,getenv("SLP_CONF_FILE"));
    
    InitializeSLPSystemConfigurator(runLoopRef);
    puas = (UA_State*) safe_malloc(sizeof(UA_State), NULL, 0); 
    
	char*	endPtr = NULL;
    puas->pcSendBuf = safe_malloc(strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10),NULL,0);
    assert( puas->pcSendBuf );
    
    puas->pcRecvBuf = safe_malloc(RECVMTU, NULL, 0);
    puas->iRecvSz   = RECVMTU;
    
    /* set up base configuration */
    
    memset(&(puas->config),0,sizeof(puas->config));
    
    puas->config.pi_net_slp_multicastTimeouts = (int*)
    safe_malloc(5*sizeof(int),(char*) piTimes,5*sizeof(int));
    puas->config.pi_net_slp_DADiscoveryTimeouts = (int*)
    safe_malloc(5*sizeof(int),(char*) piTimes,5*sizeof(int));
    
    *phSLP = (SLPHandle) puas;
    /* initialize networking */
    
    err = mslplib_init_network(&(puas->sdSend),&(puas->sdTCP),&(puas->sdMax));
    
    if ( !err && !OnlyUsePreConfiguredDAs() )
    {
        if (SLPGetProperty("net.slp.isBroadcastOnly") && !(SDstrcasecmp(SLPGetProperty("net.slp.isBroadcastOnly"),"true"))) 
        {
            int i = 1;
            int iErr = setsockopt( puas->sdSend, SOL_SOCKET, SO_BROADCAST, (char*)&i, sizeof(int) );
            
            if (iErr) 
            {
                err = SLP_NETWORK_INIT_FAILED;
                mslplog(SLP_LOG_DEBUG,"SLPOpen could not set broadcast interface",strerror(errno));
            }
            
            puas->sinSendTo.sin_addr.s_addr = BROADCAST;
        } 
        else 
        {
            err = set_multicast_sender_interf(puas->sdSend);
            
            if ( err ) 
            {
                SLP_LOG( SLP_LOG_DROP,"SLPOpen could not set multicast interface: %s", strerror(errno) );
            }
            else
                puas->sinSendTo.sin_addr.s_addr = SLP_MCAST;
        }
    }
    
    if ( err == SLP_OK )
    {
        puas->sinSendTo.sin_port = htons(SLP_PORT);
        puas->sinSendTo.sin_family = AF_INET;
        /* set up fd set here! */
        puas->tv.tv_sec  = WAIT_MSEC;
        puas->tv.tv_usec = (WAIT_MSEC % 1000) * 1000;

        if (err == SLP_OK && runLoopRef) 
            err = StartSLPDALocator( (void*)mslplib_daadvert_callback, runLoopRef, NULL );			// this will fire off a DA Discovery thread
    
        #ifdef EXTRA_MSGS
        /* Initialize client lock for reg file unless this library is
        * being used by the mslpd itself.
        */
        if (SLPGetProperty("com.sun.slp.isSA") == NULL)
            puas->pvMutex = SDGetMutex(MSLP_CLIENT);
        #endif /* EXTRA_MSGS */
    }
    
    SLP_LOG( SLP_LOG_DEBUG, "SLPOpen finished" );

    return err;
}

EXPORT void SLPClose(SLPHandle slpc) {
  UA_State *puas = (UA_State*) slpc;
  if (puas) {

#ifdef EXTRA_MSGS
    if (SLPGetProperty("com.sun.slp.isSA") == NULL && puas->pvMutex)
      SDFreeMutex(puas->pvMutex, MSLP_CLIENT);
#endif /* EXTRA_MSGS */

    if (puas->pcSendBuf) free(puas->pcSendBuf);
    if (puas->pcRecvBuf) free(puas->pcRecvBuf);
    if (puas->config.pi_net_slp_DADiscoveryTimeouts)
      free(puas->config.pi_net_slp_DADiscoveryTimeouts);
    if (puas->config.pi_net_slp_multicastTimeouts)
      free(puas->config.pi_net_slp_multicastTimeouts);
    if (puas->config.pc_net_slp_locale)
      free(puas->config.pc_net_slp_locale);
    CLOSESOCKET(puas->sdSend);
    CLOSESOCKET(puas->sdTCP);
    free(puas);
  }
  CLOSE_NETWORKING;
}

/*
 * SLPFindSrvs
 *
 * Obtains up to the number of service URLs as passed into the function.
 * Uses SAs or DAs transparently to the caller.
 *
 *   conn           state of the UA
 *   pcScope        scope for the request
 *   pcSrvType      type for the request
 *   pcQuery        query for the request
 *   pSrvURL        an array of SLPSrvURL to fill in with results
 *   lSize          the size of the array of SLPSrvURLs
 *
 * Returns:
 *   0 if OK, error code otherwise (all are negative #s).
 */
EXPORT SLPInternalError SLPFindSrvs(SLPHandle hSLP,
                        const char *pcSrvType,
                        const char *pcScope,
                        const char *pcSearchFilter, 
                        SLPSrvURLCallback callback,
                        void       *pvUser) 
{                       

    SLPInternalError		err = SLP_OK;
    UA_State *puas = (UA_State*) hSLP;
	char*	endPtr = NULL;
	
    int iSize = (SLPGetProperty("net.slp.MTU"))?strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10):1400;
    const char *pcLocale = SLPGetProperty("net.slp.locale");
    char *pcTCPbuf = NULL;  /* this is only used if a request is > the MTU */
    
    if (hSLP == NULL || !pcScope || !pcSrvType || !callback)
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"SLPFindSrvs bad parameter",SLP_PARAMETER_BAD);
    
    memset(puas->pcSendBuf,0,iSize);

    if ( strcmp( pcScope, SLP_DEFAULT_SA_ONLY_SCOPE ) != 0 )        
        err = generate_srvrqst( puas->pcSendBuf, &iSize, pcLocale, pcScope, pcSrvType, pcSearchFilter);
    else
    {
		err = generate_srvrqst( puas->pcSendBuf, &iSize, pcLocale, SLP_DEFAULT_SCOPE, pcSrvType, pcSearchFilter);
    }
    #ifdef SLPTCP
    /*
    * The following code allocates a request which is larger than net.slp.MTU
    * to be sent to a DA, using TCP.
    */
    if (err == SLP_BUFFER_OVERFLOW) 
    {
        /* this is something of a hack - compute the buffer size and allocate it */
        iSize = HDRLEN + strlen(pcLocale) + 2 + /* header+lang tag+zero pr list */
        2+strlen(pcScope) +2+strlen(pcSrvType) +2+strlen(pcSearchFilter) +
        10; /* safety room for picky boundary checking */
        
        pcTCPbuf = safe_malloc(iSize,NULL,0); /* get a well sized send buffer */
        
        if ( strcmp( pcScope, SLP_DEFAULT_SA_ONLY_SCOPE ) != 0 )        
            err = generate_srvrqst(pcTCPbuf, &iSize, pcLocale, pcScope, pcSrvType, pcSearchFilter);
        else
            err = generate_srvrqst(pcTCPbuf, &iSize, pcLocale, SLP_DEFAULT_SCOPE, pcSrvType, pcSearchFilter);

        if (err != SLP_OK) 
        {
            SLPFree(pcTCPbuf);
            LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"SLPFindSrvs generate overflowed request failed",err);
        }
    }
    #endif
        
    if ( strcmp( pcScope, SLP_DEFAULT_SA_ONLY_SCOPE ) != 0 )
        GetGlobalDATableForRequester();			// we are ignoring the return value because we don't care - just want to make sure
                                                // the DA Search is far enough along
    err = get_reply((pcTCPbuf)?pcTCPbuf:puas->pcSendBuf,iSize,
            pcScope, puas, (void*) callback, pvUser, SLPSRVURL_CALLBACK);
    
    SLPFree(pcTCPbuf); /* Free the buffer, if it was allocated. */

  return err;
}

#ifdef MAC_OS_X
EXPORT SLPInternalError SLPFindScopesAsync(SLPHandle hSLP,
                        SLPScopeCallback callback,
                        void       *pvUser) 
{                       
	SLPInternalError 		err = SLP_OK;
    DATable*		pdat = NULL;
    
    pdat = GetGlobalDATableForRequester();
    
    if ( pdat && pdat->iSize > 0 && !GlobalDATableCreationCompleted() )
    {
        // then the first lookup is still going on, we will poll the results of this table and send back
        // the current data
        int				lastCheckedSize = 0;

        SLP_LOG( SLP_LOG_DEBUG, "SLPFindScopesAsync is getting scopelist from the currently running DA Discovery" );        
        do {
            SmartSleep(2*USEC_PER_SEC);		// sleep a couple of secs
            
            LockGlobalDATable();
            if ( pdat->iSize > lastCheckedSize && pdat->pDAE[lastCheckedSize].pcScopeList != NULL )
            {
                int 			iListLen = LISTINCR, i, offset=0;
                char 			*pcList=NULL, *pcScan=NULL, *pcScope=NULL, c;
                SLPBoolean		slpbDone = SLP_FALSE;

                // the table has changed, send the content to the client
                iListLen += strlen(pdat->pDAE[lastCheckedSize].pcScopeList);
                pcList = safe_malloc(iListLen, (char*)pdat->pDAE[lastCheckedSize].pcScopeList, iListLen-LISTINCR);

                for (i = lastCheckedSize+1; i < pdat->iSize; i++) 
                {
                    pcScan = list_pack(pdat->pDAE[i].pcScopeList);
                    list_merge(pcScan,&pcList,&iListLen,CHECK);
                    free(pcScan);
                }
               
                while( !slpbDone && (pcScope = get_next_string(",",pcList,&offset,&c)) )	// its valid for us to get a scope list back "apple.com,kev.com"
                {
                    SLP_LOG( SLP_LOG_DEBUG, "SLPFindScopesAsync is returning single scope %s", pcScope );
                    slpbDone = !callback( hSLP, pcScope, err, pvUser );
                    SLPFree(pcScope);
                }
                
                free( pcList );
        
                lastCheckedSize = pdat->iSize;
            }
            UnlockGlobalDATable();
            
        } while ( !GlobalDATableCreationCompleted() );
    }
    else if ( pdat && pdat->iSize > 0 && pdat->pDAE[0].pcScopeList != NULL )
    {
        // just send a merged list of scopes from the DATable
        int 			iListLen = LISTINCR, i, offset=0;
        char 			*pcList=NULL, *pcScan=NULL, *pcScope=NULL, c;
        SLPBoolean		slpbDone = SLP_FALSE;

        SLP_LOG( SLP_LOG_DEBUG, "SLPFindScopesAsync is returning scopelist from the %d DA(s) we know about", pdat->iSize );
        
        LockGlobalDATable();
        iListLen += strlen(pdat->pDAE[0].pcScopeList);
        pcList = safe_malloc(iListLen, (char*)pdat->pDAE[0].pcScopeList, iListLen-LISTINCR);
        
        if (pdat->iSize > 1) 
        {
            for (i = 1; i < pdat->iSize; i++) 
            {
                pcScan = list_pack(pdat->pDAE[i].pcScopeList);
                list_merge(pcScan,&pcList,&iListLen,CHECK);
                free(pcScan);
            }
        }
        
        UnlockGlobalDATable();
        
        SLP_LOG( SLP_LOG_DEBUG, "SLPFindScopesAsync is returning scopelist %s", pcList );
 
        while( !slpbDone && (pcScope = get_next_string(",",pcList,&offset,&c)) )	// its valid for us to get a scope list back "apple.com,kev.com"
        {
            SLP_LOG( SLP_LOG_DEBUG, "SLPFindScopesAsync is returning single scope %s", pcScope );
            slpbDone = !callback( hSLP, pcScope, err, pvUser );
            SLPFree(pcScope);
        }
        
        free( pcList );
	}
    else
    {
        const char *pcTypeHint = SLPGetProperty("net.slp.typeHint");
    
        SLP_LOG( SLP_LOG_DEBUG, "SLPFindScopesAsync is getting scopelist from active sa discovery" );
        
        err = active_sa_async_discovery( hSLP, callback, pvUser, pcTypeHint );
    }
    
    SLP_LOG( SLP_LOG_DEBUG, "SLPFindScopesAsync is finished" );
    
	return err;
}
#endif /* MAC_OS_X */

EXPORT SLPInternalError SLPFindScopes(SLPHandle       hSLP      ,
                              char **         ppcScopeList) {

  const char *pcTypeHint = SLPGetProperty("net.slp.typeHint");

  if (!hSLP || !ppcScopeList) return SLP_PARAMETER_BAD;

  return dat_get_scopes(hSLP, pcTypeHint, GetGlobalDATable(), ppcScopeList);

}

EXPORT void SLPFree(void* pvMem) {
  if (pvMem) free(pvMem);
}

EXPORT SLPBoolean SLPIsReserved(char c) {
  if (c=='(' || c==')' || c==',' || c=='\\' || c=='!' || c=='<' ||
      c=='=' || c=='>' || c=='~' || c<0x20) return SLP_FALSE;
  else return SLP_FALSE;
}

EXPORT const char* SLPGetProperty(const char*  pcName ) 
{
    if (pmhConfig == NULL) 
        pmhConfig = mslp_hash_init();
  
    return mslp_hash_find(pmhConfig,pcName);
}

EXPORT void SLPSetProperty(const char *   pcName  ,
                           const char *   pcValue  ) 
{

    if (!pcName) return;

    /*
    * Simply disallow certain properties from being eliminated.
    * It would be more robust to check the values of the properties,
    * but this is not done.
    *
    * There is an extra check to make sure that an SA cannot 
    * have its net.slp.useScopes property erased.  The isSA
    * property is set in the MSLPD before the config file is
    * read in.  We just want to make sure we don't get mis-
    * configured by accident!  SAs can't use 'user configured'
    * scopes, ie. "net.slp.useScopes="
    */
    if (NULL == pcValue &&
        (!SDstrcasecmp(pcName,"net.slp.locale") ||
        !SDstrcasecmp(pcName,"net.slp.multicastMaximumWait") ||
        !SDstrcasecmp(pcName,"net.slp.multicastTTL") ||
        !SDstrcasecmp(pcName,"net.slp.MTU") ||
        (!SDstrcasecmp(pcName,"net.slp.useScopes") &&
        SLPGetProperty("com.sun.slp.isSA")))) 
    {

        LOG(SLP_LOG_ERR,"SLPSetProperty: attempt to erase a critical property failed");
        return;
    }

    if (pmhConfig == NULL) 
        pmhConfig = mslp_hash_init();
    
    mslp_hash_add(pmhConfig,pcName,pcValue);
}

/*
 * ----------------------------------------------------------------------------
 *  Non standard interface (not part of the SLP API) but useful enough
 *  to export.
 */
EXPORT void SLPReadConfigFile(const char *pcFileName) {

  if (pcFileName == NULL) return;
  
  if (pmhConfig == NULL) pmhConfig = mslp_hash_init();
  
  mslp_hash_read(pmhConfig, pcFileName);
  
}

EXPORT void SLPWriteConfigFile(const char *pcFileName) {

  if (pcFileName == NULL) return;
  
  if (pmhConfig == NULL) pmhConfig = mslp_hash_init();
  
  mslp_hash_write(pmhConfig, pcFileName);
  
}

EXPORT void SLPLogConfigState( void )
{
  if (pmhConfig == NULL) pmhConfig = mslp_hash_init();
  
//	we have a race condition when we do this...
//  mslp_hash_log_values(pmhConfig);
}
/*
 * ----------------------------------------------------------------------------
 *  Conditionally interfaces of the SLP API.
 */

/*
 * Only implement 'fresh == true' behavior.
 */
EXPORT SLPInternalError SLPReg(SLPHandle   hSLP,
		       const char  *pcSrvURL,
		       const unsigned short usLifetime,
		       const char  *pcSrvType,
		       const char  *pcAttrs,
		       SLPBoolean  fresh,
		       SLPRegReport callback,
		       void *pvCookie) 
{    
    UA_State *puas = (UA_State*) hSLP;
    SLPInternalError err = SLP_OK;
    SLPRegReport *srr = (SLPRegReport *) callback;
    
    if (!hSLP || !pcSrvURL || usLifetime == 0 || !pcSrvType || !callback) {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"SLPReg bad parameter",SLP_PARAMETER_BAD);
    }
    
    err = mslplib_Reg(puas, pcSrvURL, usLifetime, pcSrvType, pcAttrs, fresh);
    srr(hSLP, err, pvCookie);
    
    return SLP_OK;                  
}

EXPORT SLPInternalError SLPDereg(SLPHandle  hSLP,
			 const char *pcURL,
             const char  *pcScopes,
			 SLPRegReport callback,
			 void *pvCookie) {

#ifdef EXTRA_MSGS

  UA_State *puas = (UA_State*) hSLP;
  SLPInternalError err = SLP_OK;
  SLPRegReport *srr = (SLPRegReport *) callback;
  
  if (!hSLP || !pcURL || !callback) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"SLPDereg bad parameter",SLP_PARAMETER_BAD);
  }

  err = mslplib_Dereg(puas, pcURL, pcScopes);
  srr(hSLP, err, pvCookie);

  return err;		    

#else  

  return SLP_NOT_IMPLEMENTED;                                   

#endif

}

EXPORT SLPInternalError SLPFindAttrs(SLPHandle        hSLP      ,
                             const char *     pcURL     ,
                             const char *     pcScope   ,
                             const char *     pcAttrIds ,
                             SLPAttrCallback  callback  ,
                             void       *     pvUser    ) {

#ifdef EXTRA_MSGS

  SLPInternalError err = SLP_OK;
  UA_State *puas = (UA_State*) hSLP;
  char*	endPtr = NULL;
  int iSize = (SLPGetProperty("net.slp.MTU"))?strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10):1400;
  const char *pcLocale = SLPGetProperty("net.slp.locale");
  char *pcTCPbuf = NULL;  /* this is only used if a request is > the MTU */
  char *pcUseBuf = NULL;
  
  if (hSLP == NULL || !pcURL || !callback) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"SLPFindSrvs bad parameter",SLP_PARAMETER_BAD);
  }

  if (pcAttrIds == NULL) pcAttrIds = "";
  if (pcScope == NULL) pcScope = SLPGetProperty("net.slp.useScopes");
  
  memset(puas->pcSendBuf,0,iSize);
  err = generate_attrrqst( puas->pcSendBuf, &iSize, pcLocale,
			   pcURL, pcScope, pcAttrIds);

#ifdef SLPTCP
  /*
   * The following code allocates a request which is larger than net.slp.MTU
   * to be sent to a DA, using TCP.
   */
  if (err == SLP_BUFFER_OVERFLOW) {
    /* this is something of a hack - compute the buffer size and allocate it */
    iSize = HDRLEN + strlen(pcLocale) + 2 + /* header+lang tag+zero pr list */
      2+strlen(pcScope) +2+strlen(pcURL) +2+strlen(pcAttrIds) +
      10; /* safety room for picky boundary checking */
    
    pcTCPbuf = safe_malloc(iSize,NULL,0); /* get a well sized send buffer */
    err = generate_attrrqst(pcTCPbuf, &iSize, pcLocale,
			    pcURL, pcScope, pcAttrIds);
    if (err != SLP_OK) {
      SLPFree(pcTCPbuf);
      LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"SLPFindAttrs generate overflowed request failed",err);
    }
  }
#endif

  if (pcTCPbuf != NULL) {
    pcUseBuf = pcTCPbuf;
  } else {
    pcUseBuf = puas->pcSendBuf;
  }
  
  err = get_reply(pcUseBuf,iSize, pcScope, puas, (void*)callback, pvUser,
		  SLPATTR_CALLBACK);
  
  SLPFree(pcTCPbuf); /* Free the buffer, if it was allocated. */
  return err;

#else
  return SLP_NOT_IMPLEMENTED;
#endif /* EXTRA_MSGS */  
}

EXPORT SLPInternalError SLPFindSrvTypes(SLPHandle      hSLP              ,
                                const char *   pcNamingAuthority ,
                                const char *   pcScopeList       ,
                                SLPSrvTypeCallback  callback     ,
                                void       *   pvUser            ) {
                                
#ifdef EXTRA_MSGS

  SLPInternalError err = SLP_OK;
  UA_State *puas = (UA_State*) hSLP;
  char* 	endPtr = NULL;
  int iSize = (SLPGetProperty("net.slp.MTU"))?strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10):1400;
  const char *pcLocale = SLPGetProperty("net.slp.locale");

  if (hSLP == NULL || !callback) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"SLPFindSrvTypes bad parameter",SLP_PARAMETER_BAD);
  }

  if (pcScopeList == NULL) pcScopeList = SLPGetProperty("net.slp.useScopes");
  if (pcNamingAuthority == NULL) pcNamingAuthority = "";
  
  memset(puas->pcSendBuf,0,iSize);
  if ((err = generate_srvtyperqst( puas->pcSendBuf, &iSize, pcLocale,
			      pcNamingAuthority, pcScopeList)) != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"SLPFindSrvTypes couldn't generate message",err);
  }
    
  return get_reply(puas->pcSendBuf,iSize, pcScopeList, puas, (void*) callback,
		   pvUser, SLPSRVTYPE_CALLBACK);

#else
  return SLP_NOT_IMPLEMENTED;
#endif /* EXTRA_MSGS */  
}


/*
 * ----------------------------------------------------------------------------
 *  Unimplemented interfaces of the SLP API.
 */

EXPORT SLPInternalError SLPDelAttrs(SLPHandle   hSLP,
			    const char  *pcURL,
			    const char  *pcAttrs,
			    SLPRegReport callback,
			    void *pvCookie) {

  return SLP_NOT_IMPLEMENTED;                                   

}


EXPORT SLPInternalError SLPParseSrvURL(const char *    pcSrvURL ,
                               SLPSrvURL  **   ppSrvURL ) {
  return SLP_NOT_IMPLEMENTED;
}

EXPORT SLPInternalError SLPEscape(const char* pcInBuf, char** ppcOutBuf) 
{
	SLPInternalError		status = SLP_OK;
    char*			newTextBuffer = NULL;
	char*			curWritePtr = NULL;
	u_short			writeBufferMaxLen, newTextBufferLen, rawTextLen,i;

	if ( !pcInBuf || !ppcOutBuf )
		status = SLP_PARAMETER_BAD;
	
    rawTextLen = strlen(pcInBuf);
    newTextBufferLen = rawTextLen * 3 + 1;
    newTextBuffer = (char*)malloc( newTextBufferLen );
    curWritePtr = newTextBuffer;
    
	writeBufferMaxLen = newTextBufferLen;
		
	for ( i=0; !status && i<rawTextLen; i++ )
	{
		if ( SLPIsReserved( pcInBuf[i] ) == SLP_TRUE )
		{
			if ( curWritePtr > newTextBuffer + writeBufferMaxLen + 2 )	// big enough to add two new chars?
			{
				status = SLP_INTERNAL_SYSTEM_ERROR;
				break;
			}
		
			EncodeChar( pcInBuf[i], curWritePtr );
			curWritePtr += 3;
		}
		else
		{
			if ( curWritePtr > newTextBuffer + writeBufferMaxLen )
			{
				status = SLP_INTERNAL_SYSTEM_ERROR;
				break;
			}
			
			*curWritePtr = pcInBuf[i];
			curWritePtr++;
		}
	}
	
    // now null terminate
    
	if ( !status )
    {
		newTextBufferLen = curWritePtr - newTextBuffer;
        *ppcOutBuf = (char*)malloc( newTextBufferLen + 1 );
        memcpy( *ppcOutBuf, newTextBuffer, newTextBufferLen );
        (*ppcOutBuf)[newTextBufferLen] = '\0';
	}
    else
	{
		newTextBufferLen = 0;
	}
	
    if ( newTextBuffer )
        free( newTextBuffer );
        
	return status;
}

EXPORT SLPInternalError SLPUnescape(const char* pcInBuf, char** ppcOutBuf )
{
	const char*		curReadPtr = pcInBuf;
	char*			curWritePtr = NULL;
    char*			tempTextBuffer = NULL;
    u_short			newTextBufferLen, encodedTextLen;
    
	SLPInternalError		status = SLP_OK;
	
	if ( !pcInBuf || !ppcOutBuf )
		status = SLP_PARAMETER_BAD;
	
    encodedTextLen = strlen( pcInBuf );
    tempTextBuffer = (char*)malloc( encodedTextLen + 1 );
    curWritePtr = tempTextBuffer;
    
	while ( !status && (curReadPtr <= pcInBuf+encodedTextLen) )
	{
		if ( curWritePtr > tempTextBuffer + encodedTextLen )
		{
			status = SLP_INTERNAL_SYSTEM_ERROR;
			break;
		}
		
		if ( *curReadPtr == 0x5c )
		{
			*curWritePtr = DecodeChar( curReadPtr );
			
            curWritePtr++;
            curReadPtr += 3;
		}
		else
		{
			*curWritePtr = *curReadPtr;
			curWritePtr++;
			curReadPtr++;
		}
	}
	
	if ( !status )
		newTextBufferLen = (curWritePtr-tempTextBuffer);
	else
		newTextBufferLen = 0;
		
	return status;
}

EXPORT void EncodeChar( const char c, char* newEscapedBufPtr )
{
	// Convert ascii to %xx equivalent
	div_t			result;
	short			hexValue = c;
	char 			c1, c2;
	
	if ( hexValue < 0 )
		hexValue -= 0xFF00;	// clear out the high byte
	
	result = div( hexValue, 16 );
	
	if ( result.quot < 0xA )
		c1 = (char)result.quot + '0';
	else
		c1 = (char)result.quot + 'a' - 10;
	
	if ( result.rem < 0xA )
		c2 = (char)result.rem + '0';
	else
		c2 = (char)result.rem + 'a' - 10;
	
	newEscapedBufPtr[0] = 0x5c;		// "back slash"
	newEscapedBufPtr[1] = c1;
	newEscapedBufPtr[2] = c2;
}

EXPORT char DecodeChar( const char* encodedTriplet  )
{
	char c, c1, c2;
	
	c = *encodedTriplet;

	if ( c == 0x5c )
	{
		// Convert \xx to ascii equivalent
		c1 = tolower(encodedTriplet[1]);
		c2 = tolower(encodedTriplet[2]);
		if (isdigit(c1) && isdigit(c2)) 
		{
			c1 = isdigit(c1) ? c1 - '0' : c1 - 'a' + 10;
			c2 = isdigit(c2) ? c2 - '0' : c2 - 'a' + 10;
			c = (c1 << 4) | c2;
		}
	}
	// else we just return the character as it wasn't encoded
    
	return c;
}

// in SECONDS
EXPORT int SLPGetRefreshInterval() {

  const char *pcRefreshInterval =
    SLPGetProperty("com.sun.slp.minRefreshInterval");

  if (pcRefreshInterval) {
    char*	endPtr = NULL;
    return strtol(pcRefreshInterval,&endPtr,10);
    
  } else {
    
    return MIN_REFRESH_DEFAULT;

  }

}

#define ALL_DONE 1

static void last_one(SLPInternalError slperr, int iLast,
		     SLPHandle slph, void *pvUser,
		     void *pvCallback, CBType cbt) {
  
  if (iLast == ALL_DONE) { /* send last_one as per api */
    (void) process_reply(NULL, NULL, 0, &iLast, pvUser, slph, pvCallback,cbt);
  }

}

/*
 * get_reply
 *
 *   This general interface encapsulates the following features:
 *     - deciding whether to use a DA and which one to use.
 *     - using TCP or UDP to obtain a reply.
 *     - evoking the request/reply mechanism (whether udp or tcp).
 *     - handling overflow.
 *   This allows this same routine to be used by SLPFindSrvs,
 *   SLPFindAttrs and SLPFindSrvTypes.
 *
 *   This routine is defined twice - once if SLP_TCP has been
 *   compiled in, once otherwise.  The two code bases are
 *   sufficiently different that it is cleaner to separate them.
 *
 *   pcSend	The send buffer.  This will be reallocated to be
 *              the right size if greater than the MTU - which
 *              will only work if there is a DA.  The caller is
 *		responsible for freeing this buffer if allocated.
 *   iSize	The size of the send buffer.
 *   pcScope	The scope list.  This is required for DA selection.
 *   puas	The SLPHandle - used to access default buffers and
 *		the DATable.
 *   pvCallback	The callback function to evoke upon getting results.
 *   pvUser	The callback cookie passed in from the interface.
 *   cbt        The callback type used by process_reply.
 *
 * Results:
 *   Returns an error if one the operation fails.  This follows
 *   the semantics of the API for requests.
 *
 * Side effects:
 *   Buffers allocated by this routine are cleaned up.  It is
 *   possible that a non-responding DA will incur 'strikes' against
 *   it, so that the DA will eventually be dropped from the DA
 *   table.
 */

static SLPInternalError get_reply(	char*		pcSend, 
                                int 		iSize,
                                const char*	pcScope,
                                UA_State*	puas,
                                void*		pvCallback, 
                                void*		pvUser, 
                                CBType 		cbt ) 
{
    char*				pcRecvBuf = NULL;
    int					iLast  = 0;   /* used to for ending async callbacks */
    int					use_da = 0;   /* keeps track of whether DAs are used */
    struct sockaddr_in	sin;          /* the address of the DA to use */
    SLPInternalError				err    = SLP_OK;
	char*				endPtr = NULL;
    int					iMTU   = (SLPGetProperty("net.slp.MTU"))?strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10):1400;
    int					len    = 0;   /* This records the reply length. */
    unsigned char		ttl    = (SLPGetProperty("net.slp.multicastTTL"))?(unsigned char) strtol(SLPGetProperty("net.slp.multicastTTL"), &endPtr, 10):255;
	int					multicastMaximumWait = (SLPGetProperty("net.slp.multicastMaximumWait"))?strtol(SLPGetProperty("net.slp.multicastMaximumWait"), &endPtr, 10):15000;

	
	if ( strcmp( pcScope, SLP_DEFAULT_SA_ONLY_SCOPE ) == 0 ) 
		ttl = 1;
		
    while ( 1 )
    {
        /* Gets the target for the request.  Side Effect: sets MCASTRQST flag! */
        if ((err = get_target(puas, pcScope, &use_da, &sin)) != SLP_OK) 
        {
            if ( err == SLP_SCOPE_NOT_SUPPORTED )
                return SLP_OK;
                
            LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DEBUG,"get_reply: Could not get to-address for request", SLP_PARSE_ERROR);
        }
        
        /* Handle larger than MTU sized requests */
        if (iSize >= iMTU) 
        {
            if (use_da == 0) 
            {
                LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DEBUG,"get_reply: Request overflowed MTU", SLP_BUFFER_OVERFLOW);
            } 
            else 
            {
                pcRecvBuf = NULL; /* make sure we're not pointing at puas->pcRecvBuf */
                if ((err=get_tcp_result(pcSend,iSize, sin,&pcRecvBuf,&len))!= SLP_OK)
                {
                    SLPFree(pcRecvBuf);
                    pcRecvBuf = NULL;
                    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DEBUG,"get_reply of overflowed result failed",err);
                }
            
                /* evokes the callback once */
                err = process_reply(pcSend, pcRecvBuf, len, &iLast, pvUser, (SLPHandle) puas, pvCallback,cbt);
                last_one(err, iLast,pvUser,(SLPHandle)puas,pvCallback,cbt);
                
                SLPFree(pcRecvBuf);
                pcRecvBuf = NULL;
                return err;
            }
            /* --- End of special case: sending a request which is > MTU to a DA --- */
        }
        
        /* Handle all other kinds of requests */
        if (use_da) 
        {
            /*
            * Note: TCP will be used in the section below to handle replies
            * which overflow.  In this case, the request must be small enough
            * to fit into the net.slp.MTU limit or else the special case code
            * above would have been used.
            */
            int tcp_used = 0; /* set if overflow occurs and is handled by tcp */
    
            SLP_LOG( SLP_LOG_DEBUG, "get_reply connecting to DA [%s]",inet_ntoa(sin.sin_addr));
        
            pcRecvBuf = (char*)malloc(RECVMTU);
            
            if ((err = get_unicast_result(
                                            MAX_UNICAST_WAIT,
                                            puas->sdSend, 
                                            pcSend, 
                                            iSize, 
                                            pcRecvBuf,
                                            RECVMTU, 
                                            &len, 
                                            sin)) != SLP_OK) 
            {
                SLP_LOG( SLP_LOG_DA, "get_reply could not get_da_results from [%s]...: %s",inet_ntoa(sin.sin_addr), slperror(err) );
                
                dat_strike_da( NULL, sin );		// this DA was bad, give them a strike and when we return an error, the caller can try again
                
                SLPFree(pcRecvBuf);
                pcRecvBuf = NULL;
                continue; 						// try again
            }
            else
            {
                if (GETFLAGS(pcRecvBuf) & OVERFLOWFLAG) 
                { /* the result overflowed ! */
                    SLPFree(pcRecvBuf);
                    pcRecvBuf = NULL;   
                    
                    // set the port to use the SLP port
                    sin.sin_port   = htons(SLP_PORT);
                    err=get_tcp_result(pcSend,iSize, sin, &pcRecvBuf,&len);
                    
                    if (err != SLP_OK) 
                        err=get_tcp_result(pcSend,iSize, sin, &pcRecvBuf,&len);		// try once more

                    if (err != SLP_OK) 
                    {
                        SLPFree(pcRecvBuf);
                        pcRecvBuf = NULL;

                        SLP_LOG(SLP_LOG_DEBUG, "get_reply overflow, tcp failed from [%s] when getting a reply...: %s",inet_ntoa(sin.sin_addr), slperror(err));
                
                        dat_strike_da( NULL, sin );		// this DA was bad, give them a strike and when we return an error, the caller can try again
                
                        continue; 						// try again
                    }
                    else
                        SLP_LOG( SLP_LOG_DEBUG, "get_tcp_result, received %ld bytes from [%s]", len, inet_ntoa(sin.sin_addr) );
                    
                    tcp_used = 1;	
                }
                else
                    SLP_LOG( SLP_LOG_DEBUG, "get_unicast_result, received %ld bytes from [%s]", len, inet_ntoa(sin.sin_addr) );
            }
            /* evokes the callback once */
            if ( !err )
                err = process_reply(pcSend, pcRecvBuf, len, &iLast, pvUser, (SLPHandle) puas, pvCallback, cbt);
        
			if ( pcRecvBuf );
			{
                SLPFree(pcRecvBuf);
                pcRecvBuf = NULL;
            }
        
            /* take care of last call as per api */
            if ( !err )
                last_one(err, ALL_DONE,pvUser,(SLPHandle)puas,pvCallback,cbt);
            return err;
        
        } 
        else 
        { 
            SETFLAGS( pcSend,(unsigned char) MCASTFLAG);

            /* will evoke the callback repeatedly, via process_reply */
            err = get_converge_result(
                                        multicastMaximumWait,
                                        puas->sdSend, 
                                        puas->pcSendBuf, 
                                        iSize, 
                                        puas->pcRecvBuf,
                                        RECVMTU, 
                                        sin, 
										ttl,
                                        pvUser, 
                                        (SLPHandle) puas, 
                                        pvCallback, 
                                        cbt);
        
            return err;
        
        }
    }
}




