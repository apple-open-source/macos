
/*
 * mslplib.c : Minimal SLP v2 User Agent implementation.
 *       This file handles parsing requests out and replies back in again.
 *
 * Version: 1.9
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

#include <stdio.h>    /* these definitions are standard C, system indep. */
#include <time.h>
#include <string.h>
#include <errno.h>

#include "mslp_sd.h"  /* system indep. defs mapped to system dep. defs */
#include "slp.h"      /* slp api */
#include "mslp.h"     /* mslp general defs and utilities */
#include "mslp_dat.h" /* DATable is a member of structures in mslplib.h */
#include "mslplib.h"  /* mslplib defs local to the library */

static SLPInternalError process_srvrply(SLPHandle, Slphdr *, const char *,
				int, int, int*, SLPSrvURLCallback *, void *);
static SLPInternalError get_urlentry(char **ppcURL, unsigned short *pusLifetime,
  const char *pcBuf, int maxlen, int *piOffset);

#ifdef EXTRA_MSGS
static SLPInternalError process_attrrply(SLPHandle, Slphdr *, const char *,
				 int, int, int*, SLPAttrCallback *, void *);
static SLPInternalError process_srvtyperply(SLPHandle, Slphdr *, const char *, int,
				    int, int*, SLPSrvTypeCallback *, void *);
#endif /* EXTRA_MSGS */

EXPORT const char * get_fun_str(int i) {
  switch (i) {
    case SRVRQST: return "SRVRQST";
    case SRVRPLY: return "SRVRPLY";
    case SRVREG: return "SRVREG";
    case SRVDEREG: return "SRVDEREG";
    case SRVACK: return "SRVACK";
    case ATTRRQST: return "ATTRRQST";
    case ATTRRPLY: return "ATTRRPLY";
    case DAADVERT: return "DAADVERT";
    case SRVTYPERQST: return "SRVTYPERQST";
    case SRVTYPERPLY: return "SRVTYPERPLY";
    case SAADVERT: return "SAADVERT";
    default: return "unknown";
  }
}

EXPORT int GetSLPPort(void)
{
    char*	endPtr = NULL;
	int	port = strtol(SLPGetProperty("com.apple.slp.port"),&endPtr,10);
    
    return port;
}

/* get_target
 * 
 * puas      - ua state
 * pcScope   - the scope of the request
 *
 * piUseDA   - OUT PARAM: set to indicate whether a DA should be used, or SA
 * pSin      - OUT PARAM: set to the address to use for sending the request
 *
 * returns: SLP_OK unless there is an error getting the target address.
 *
 * side effect:  potentially it sets the multicast flag in the header of
 *               the request payload.
 */

SLPInternalError get_target(UA_State *puas, const char *pcScope, int *piUseDA, struct sockaddr_in *pSin) 
{

    SLPInternalError err = SLP_OK;
        
    if ( strcmp( pcScope, SLP_DEFAULT_SA_ONLY_SCOPE ) != 0 && (err = dat_get_da(GetGlobalDATable(),pcScope,pSin)) != SLP_OK) 
        return err;
    
    if ( strcmp( pcScope, SLP_DEFAULT_SA_ONLY_SCOPE ) != 0 && pSin->sin_addr.s_addr != 0L )
    {
        *piUseDA         = 1;
        return SLP_OK;
    
    } else { /* we do not have a DA to use - so use multicast (or broadcast) */
    
        /*
        * use multicast or broadcast to SAs
        */
        SETFLAGS(puas->pcSendBuf,(unsigned char) MCASTFLAG);
        *piUseDA = 0;
        *pSin = puas->sinSendTo;
        return SLP_OK;
    
    }
}

/*
 * process_reply
 * 
 *   Handles daadvert and srvrply, simply stripping out the URL and the
 *   addrspec it contains into the appropriate fields.
 *
 *   This is a tricky function.  It can be called by different code paths
 *   with different completion functions.  For instance, if it is used
 *   for service requests, a SLPSRVURL_CALLBACK is supplied.  This is a
 *   user supplied function which is called when SrvRply messages are
 *   processed.
 *
 *     SrvRply  -> SLPSRVURL_CALLBACK
 *     DAAdvert -> SLPDAADVERT_CALLBACK
 *     SAAdvert -> SLPSAADVERT_CALLBACK
 *
 *   These callbacks have different prototypes.  If any arriving DAAdvert
 *   were dispatched to the current callback (ie. a SLPSRVURL_CALLBACK)
 *   it could cause the library to blow up.  For this reason, the callback
 *   type is checked before dispatching incoming messages.  Only the type
 *   of message currently expected will be dispatched.
 *
 *    pcSendBuf       The request which was issued.  Used to check the reply.
 *    pcRecvBuf       The reply which was received.
 *    iRecvSz         The length of the reply.
 *    piLastOne       This is set to 1 if no more results are available.
 *                    It is set to 1 by process_reply if the callback
 *                    function indicates it doesn't want any more data.
 *    pvUser          The user supplied opaque data element, an argument of
 *                    the callback function.
 *    hSLP            The SLP handle.
 *    pvCallback      The supplied callback function.
 *    cbCallbackType  The type of the callback function (set by the caller.)
 *
 * Returns:
 *   SLPInternalError - no error is presently returned.  Errors are passed back to
 *   the caller through the callback routines.
 *
 * Side effects:
 *   SLP messages of unknown type, with the wrong callback function registered
 *   are dropped.
 *
 *   piLastOne may be set by a callback function - if the user returns
 *   a SLP_TRUE value is returned.
 */
TESTEXPORT SLPInternalError process_reply(const char *pcSendBuf, 
		       const char *pcRecvBuf, int iRecvSz,
		       int   *piLastOne,
		       void *pvUser, SLPHandle hSLP,
		       void *pvCallback, CBType cbCallbackType) 
{

    SLPInternalError err = SLP_OK;
    int offset = 0;
    Slphdr slph;
    
    memset(&slph,0,sizeof(Slphdr));		// need to zero this out

    if (*piLastOne == 1) 
    {
        switch (cbCallbackType) 
        {
            case SLPSRVURL_CALLBACK: 
            {
                SLPSrvURLCallback *pssuc = (SLPSrvURLCallback*) pvCallback;
                (void)pssuc(hSLP,NULL,0,SLP_LAST_CALL,pvUser);
                return err;
            }
            case SLPATTR_CALLBACK: 
            {
                SLPAttrCallback *psac = (SLPAttrCallback *) pvCallback;
                (void) psac(hSLP,NULL,SLP_LAST_CALL,pvUser);
                return SLP_OK;
            }
            case SLPSRVTYPE_CALLBACK: 
            {
                SLPSrvTypeCallback *psstc = (SLPSrvTypeCallback *) pvCallback;
                (void) psstc(hSLP,NULL,SLP_LAST_CALL,pvUser);
                return SLP_OK;
            }
            case SLPDAADVERT_CALLBACK: return SLP_OK;
            case SLPSAADVERT_CALLBACK: return SLP_OK;
        #ifdef MAC_OS_X
            case SLPSAADVERT_ASYNC_CALLBACK:
            {
                char    *pcURL = NULL, *pcScope = NULL;
                SLPScopeCallback *pssc = (SLPScopeCallback*) pvCallback;
        
                if ( pcSendBuf && pcRecvBuf )	// its ok to have a nil pcSendBuf and pcRecvBuf, it just means the search
                {				// is finished so we don't want to log an error!  // KA 4/19/00
                    err = get_header(pcSendBuf,pcRecvBuf,iRecvSz,&slph,&offset);
                    
                    if ( err != SLP_OK )
                        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"process_reply get_header failed",err);
        
                    if ( err == SLP_OK )
                    {
                        err = get_string(pcRecvBuf,iRecvSz,&offset,&pcURL);
        
                        if ( err != SLP_OK )
                            LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"process_reply get_string url",err);
                    }
        
                    if ( err == SLP_OK )
                    {
                        err = get_string(pcRecvBuf,iRecvSz,&offset,&pcScope);
        
                        if ( err != SLP_OK )
                            LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"process_reply get_string scopes",err);
                    }
                    
                    SLPFree(slph.h_pcLangTag); /* we don't need this field & must free it */  
                    
                    if ( pcScope )
                        SLPFree(pcScope);
        
                    if ( pcURL )
                        SLPFree(pcURL);
                }

                if ( err == SLP_OK /*&& slph.h_ucFun == SAADVERT*/ )
                    (void) pssc (hSLP, pcScope, SLP_LAST_CALL, pvUser);
        
                return err;			// we should really be returning the error here // KA 4/19/00
            }
        #endif	/* MAC_OS_X */
            default:
            LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"process_reply unexpected callback on LAST_ONE",
                    SLP_INTERNAL_SYSTEM_ERROR);
        }
    } 
    
    if ((err = get_header(pcSendBuf,pcRecvBuf,iRecvSz,&slph,&offset)) != SLP_OK)
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"process_reply unable to get header",err);
    
    offset += 2; /* skip past the error value: It was already parsed. */
    
    switch(slph.h_ucFun) 
    {
        #ifdef EXTRA_MSGS
        
        case SAADVERT:
        {
        #ifdef MAC_OS_X
            if ( cbCallbackType != SLPSAADVERT_ASYNC_CALLBACK ) 
            {
                SLP_LOG( SLP_LOG_DROP,"process_reply: got a saadvert without asking for it");
                err = SLP_REPLY_DOESNT_MATCH_REQUEST;
            } 
            else 
            {
                char				c, *pcURL = NULL, *pcScope = NULL, *pcNewScopeList = NULL;
                int					scopeListOffset = 0;
                SLPScopeCallback*			pssc = (SLPScopeCallback*) pvCallback;
                SLPBoolean				slpbDone = SLP_FALSE;
                
                SLP_LOG( SLP_LOG_DEBUG,"process_reply: saadvert in");
                
                offset -= 2; /* there is no error value in a saadvert, go back two bytes. */
            
                if ( !err && ( err = get_string(pcRecvBuf,iRecvSz,&offset,&pcURL) ) != SLP_OK )
                    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"process_reply get_string url",err);
            
                if ( !err && ( err = get_string(pcRecvBuf,iRecvSz,&offset,&pcNewScopeList) ) != SLP_OK )
                    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"process_reply get_string scopes",err);
            
                while( !slpbDone && (pcScope = get_next_string(",",pcNewScopeList,&scopeListOffset,&c)) )	// its valid for us to get a scope list back "apple.com,kev.com"
                {
                    slpbDone = !pssc( hSLP, pcScope, err, pvUser );
                    SLPFree(pcScope);
                }
            
                SLPFree(pcNewScopeList);
                SLPFree(pcURL);
    
                if (slpbDone == SLP_TRUE) 
                {
                    *piLastOne = 1;
                    return SLP_OK;
                }
            }
        #else
            SLP_LOG( SLP_LOG_DEBUG,"process_reply: saadvert in");
        
            if (cbCallbackType != SLPSAADVERT_CALLBACK) 
            {
                SLP_LOG( SLP_LOG_DROP,"process_reply: got a saadvert without asking for it");
            } 
            else 
            {
                err = handle_saadvert_in(pcSendBuf,pcRecvBuf,iRecvSz,
                                        pvUser,hSLP,pvCallback,cbCallbackType);
            }
        #endif /* MAC_OS_X */
        }
        break;
        
        #endif /* EXTRA_MSGS */
        
        case DAADVERT: 
        {
            SLP_LOG( SLP_LOG_DEBUG,"process_reply: received a DAAdvert");
            if (cbCallbackType != SLPDAADVERT_CALLBACK) 
            	SLP_LOG( SLP_LOG_DROP,"process_reply: I'm not expecting a DAAdvert");
            else	
                err = handle_daadvert_in(pcSendBuf, pcRecvBuf,iRecvSz, pvUser,hSLP,pvCallback,cbCallbackType);
        }
        break;
        
        case SRVRPLY: 
        {
            if (cbCallbackType != SLPSRVURL_CALLBACK)
                SLP_LOG( SLP_LOG_DROP,"process_reply: I'm not expecting a SRVRPLY");
            else
                err = process_srvrply( hSLP, &slph, pcRecvBuf, iRecvSz, offset, piLastOne, (SLPSrvURLCallback *) pvCallback, pvUser );
        }
        break;
        
        #ifdef EXTRA_MSGS
        case ATTRRPLY:
        {
            if (cbCallbackType != SLPATTR_CALLBACK)
                SLP_LOG( SLP_LOG_DROP,"process_reply: I'm not expecting an ATTRRPLY");
            else
                err = process_attrrply(hSLP, &slph, pcRecvBuf, iRecvSz, offset, piLastOne, (SLPAttrCallback *) pvCallback, pvUser );
        }
        break;
            
        case SRVTYPERPLY:
        {
            if (cbCallbackType != SLPSRVTYPE_CALLBACK) 
                SLP_LOG( SLP_LOG_DROP,"process_reply: I'm not expecting an SRVTYPRPLY");
            else
           	err = process_srvtyperply(hSLP, &slph, pcRecvBuf, iRecvSz, offset, piLastOne, (SLPSrvTypeCallback *) pvCallback, pvUser );
        }
        break;
            
        #endif /* EXTRA_MSGS */
            
        default:
            mslplog(SLP_LOG_DROP,"process_reply: message type not understood: ", get_fun_str(slph.h_ucFun));
    }
    
    SLPFree(slph.h_pcLangTag); /* we don't need this field & must free it */  
    
    return err;
}

/*
 * ----------------------------------------------------------------------
 */

static SLPInternalError process_srvrply(SLPHandle hSLP, Slphdr *pslph,
				const char *pcRecvBuf, int iRecvSz,
				int offset, int *piLastOne,
				SLPSrvURLCallback *pssuc, void *pvUser) {
  int i;
  int iNumResults;
  SLPInternalError err = SLP_OK;

  if (pslph->h_usErrCode != SLP_OK) {
    return SLP2APIerr(pslph->h_usErrCode);
  }
  
  if ((err = get_sht(pcRecvBuf,iRecvSz,&offset,&iNumResults)) == SLP_OK) {
    for (i = 0; i<iNumResults; i++) {
      SLPBoolean      slpbDone;
      char           *pcURLloop = NULL;
      unsigned short  sLifetime = 0;
      
      err = get_urlentry(&pcURLloop,&sLifetime,pcRecvBuf,iRecvSz,&offset);
      
      if (err != SLP_OK) {
	LOG(SLP_LOG_ERR,"process_srvrply: get_urlentry got a bad result");
      }
      
      slpbDone = !pssuc(hSLP,pcURLloop,sLifetime,SLP_OK,pvUser);
      free(pcURLloop);
      
      if (slpbDone  == SLP_TRUE) {
	*piLastOne = 1;
	return SLP_OK;
      }
      
    } /* end of handling of each result */
  } /* no error in the number of results */
  
  return err;
}

#ifdef EXTRA_MSGS

static SLPInternalError process_srvtyperply(SLPHandle hSLP, Slphdr *pslph,
				    const char *pcRecvBuf, int iRecvSz,
				    int offset, int *piLastOne,
				    SLPSrvTypeCallback *psstc, void *pvUser) {
  SLPInternalError    err = SLP_OK;
  char       *pcSrvTypes = NULL;
  SLPBoolean  slpbDone = SLP_FALSE;
  
  /*
   * if (pslph->h_usErrCode != SLP_OK) return SLP2APIerr(pslph->h_usErrCode);
   *
   * Commenting this out means we will propogate errors to the callback.
   * Should we?
   */

  /*
   * No 'srvtyperply_in' function is necessary:  all we are doing is
   * reading in the string payload and returning it.
   */
  if ((err = get_string(pcRecvBuf, iRecvSz, &offset, &pcSrvTypes)) != SLP_OK) {
    return err;
  }
  slpbDone = psstc(hSLP, pcSrvTypes, SLP2APIerr(pslph->h_usErrCode), pvUser);
  SLPFree(pcSrvTypes);
  if (slpbDone == SLP_TRUE) *piLastOne = 1;
  
  return SLP_OK;
}

static SLPInternalError process_attrrply(SLPHandle hSLP, Slphdr *pslph,
				 const char *pcRecvBuf, int iRecvSz,
				 int offset, int *piLastOne,
				 SLPAttrCallback *psac, void *pvUser) {
  SLPInternalError    err = SLP_OK;
  char       *pcAttrs;
  SLPBoolean  slpbDone = SLP_FALSE;
  
  if (pslph->h_usErrCode != SLP_OK) return SLP2APIerr(pslph->h_usErrCode);

  if ((err = get_string(pcRecvBuf, iRecvSz, &offset, &pcAttrs)) != SLP_OK) {
    return err;
  }
  slpbDone =  psac(hSLP, pcAttrs, SLP_OK, pvUser);
  SLPFree(pcAttrs);
  if (slpbDone == SLP_TRUE) *piLastOne = 1;
  return SLP_OK;
}

#endif /* EXTRA_MSGS */

static SLPInternalError get_urlentry(char **ppcURL, unsigned short *pusLifetime,
  const char *pcBuf, int maxlen, int *piOffset) {
  SLPInternalError err;
  int iShtval;
  int iNumAuths;
  
  if ((*piOffset + 5) >= maxlen) 
    LOG_STD_ERROR_AND_RETURN(SLP_LOG_DROP,"get_urlentry insufficient size",SLP_BUFFER_OVERFLOW);

  *piOffset += 1; /* skip reserved byte */

  if ((err = get_sht(pcBuf, maxlen, piOffset, &iShtval)) != SLP_OK)
    LOG_STD_ERROR_AND_RETURN(SLP_LOG_DROP,"get_urlentry get_sht lifetime",err);
  *pusLifetime = (unsigned short) iShtval;

  if ((err = get_string(pcBuf, maxlen, piOffset, ppcURL)) != SLP_OK)
    LOG_STD_ERROR_AND_RETURN(SLP_LOG_DROP,"get_urlentry get_string url",SLP_PARSE_ERROR);

  iNumAuths = pcBuf[*piOffset];
  *piOffset += 1;
  while (iNumAuths > 0) {  /* TEST ME TEST ME TEST ME */
  
    *piOffset += 2; /* skip bsd */
    if ((err = get_sht(pcBuf, maxlen, piOffset, &iShtval)) != SLP_OK)
      LOG_STD_ERROR_AND_RETURN(SLP_LOG_DROP,"get_urlentry get_sht auth length",err);
    *piOffset += iShtval; /* skip the authentication block length */
    
  }
  return SLP_OK;
}

void free_header(Slphdr *pslph) {
  if (!pslph) return;
  if (pslph->h_pcLangTag) free(pslph->h_pcLangTag);
  free(pslph);
}

EXPORT SLPInternalError SLP2APIerr(unsigned short usErr) {
  switch(usErr) {
    case SLP_OK:                 return SLP_OK;
    case LANGUAGE_NOT_SUPPORTED: return SLP_LANGUAGE_NOT_SUPPORTED;
    case PARSE_ERROR:            return SLP_PARSE_ERROR;
    case INVALID_REGISTRATION:   return SLP_INVALID_REGISTRATION;
    case SCOPE_NOT_SUPPORTED:    return SLP_SCOPE_NOT_SUPPORTED;
    case AUTHENTICATION_ABSENT:  return SLP_AUTHENTICATION_ABSENT;
    case AUTHENTICATION_FAILED:  return SLP_AUTHENTICATION_FAILED;
    case VER_NOT_SUPPORTED:      return SLP_INTERNAL_SYSTEM_ERROR; /* internal */
    case INTERNAL_ERROR:         return SLP_INTERNAL_SYSTEM_ERROR; /* internal */
    case DA_BUSY_NOW:            return SLP_INTERNAL_SYSTEM_ERROR; /* internal */
    case OPTION_NOT_UNDERSTOOD:  return SLP_INTERNAL_SYSTEM_ERROR; /* internal */
    case INVALID_UPDATE:         return SLP_INVALID_UPDATE;
    case RQST_NOT_SUPPORTED:     return SLP_INTERNAL_SYSTEM_ERROR; /* internal */
    default:                     return SLP_PARSE_ERROR;    /* unknown code  */
  }
}
