/*
 * mslpd_parse.c : Parses messages in and out of the mini slpv2 SA.
 *
 * Version: 1.7
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
#include <assert.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslp_dat.h"
#include "mslpd_store.h"
#include "mslpd.h"
#include "mslpd_mask.h"
#include "mslpd_parse.h"

/* ------------------------------------------------------------------------- */
/* 
* parse the request in
*/

SLPInternalError srvrqst_in(Slphdr *pslphdr, const char *pcInBuf, int iInSz,
                           char **ppcPRList, char **ppcSrvType, char **ppcScopes,
                           char **ppcPredicate) {
  
  SLPInternalError err;
  int offset  = HDRLEN + strlen(pslphdr->h_pcLangTag);
  *ppcPRList  = NULL;
  *ppcSrvType = NULL;
  *ppcScopes  = NULL;
  if ((err = get_string(pcInBuf, iInSz, &offset, ppcPRList)) < 0) 
    goto srvrqst_in_fail;
  if ((err = get_string(pcInBuf, iInSz, &offset, ppcSrvType)) < 0)
    goto srvrqst_in_fail;
  if ((err = get_string(pcInBuf, iInSz, &offset, ppcScopes)) < 0)  
    goto srvrqst_in_fail;
  if ((err = get_string(pcInBuf, iInSz, &offset, ppcPredicate)) < 0)
    goto srvrqst_in_fail;
  /* ignore SLP SPI */
  /* ignore options */
  return SLP_OK;
  
srvrqst_in_fail:
  if (*ppcPRList) SLPFree((void*)ppcPRList); *ppcPRList = NULL;
  if (*ppcSrvType) SLPFree((void*)ppcSrvType); *ppcSrvType = NULL;
  if (*ppcScopes) SLPFree((void*)ppcScopes); *ppcScopes = NULL;
  return err;
}


/*
* srvrply_out
*
*   Parse the reply out, into a SRVRPLY.  Use the result mask to determine
*   which services were matched by the request.  The request's header is
*   used to set the fields in the reply header.
*
*     ps          A pointer to the service store.
*     errcode     The error code of the reply.  If nonzero, just return this.
*     pm          The mask of results.
*     pslphdr     The header of the request.
*     ppcOutBuf   A pointer to the buffer to allocate.
*     piOutSz     A pointer to the size of the buffer that was allocated.
*     piGot       A pointer to the number of results serialized out.
*
* Return:
*   SLPInternalError result.  The ppcOutBuf is set to a new buffer allocated by
*   this function.  The piOutSz parameter is set to the size of this buffer.
*
* Side Effects:
*   The caller must SLPFree the ppcOutBuf.  It is wise to do mask_reset on
*   pm if it is used again as it might have been iterated upon here.
*
*/ 
SLPInternalError srvrply_out(SAStore *ps, SLPInternalError err, Mask *pm, 
		     Slphdr *pslphdr, char **ppcOutBuf, int *piOutSz,
		     int *piGot) {
  
  char*	endPtr	= NULL;
  int iMTU      = strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10);
  int iOverflow = 0;
  int iIsMcast  = (pslphdr->h_usFlags & MCASTFLAG)?1:0;
  int hdrsz     = HDRLEN + strlen(pslphdr->h_pcLangTag);
  int offset    = 0;
  int count     = 0; /* number of urlentries to return*/
  *piOutSz      = hdrsz + 4; /* error code and number of results fields */
  *piGot        = 0;

  if (err == SLP_OK) {
    int item;
    mask_reset(pm);
    while((item = mask_next(pm,1)) >= 0) {
      count++;
      /* 6 is from reserved (1) + lifetime (2) + url len (2) + # auths (1) */
      *piOutSz +=  6 + strlen(ps->url[item]);
    }
  }
  
  /* in the case of overflow in a unicast request, send 0 results & flag */
  if (iIsMcast && *piOutSz >= iMTU) { 
    *piOutSz = hdrsz + 4;
    iOverflow = 1;
  } 

  *piGot     = count;
  *ppcOutBuf = safe_malloc(*piOutSz, 0, 0);  
  assert( *ppcOutBuf );

  /* parse header out */
  SETVER(*ppcOutBuf,2);
  SETFUN(*ppcOutBuf,SRVRPLY);
  SETLEN(*ppcOutBuf,*piOutSz);
  SETLANG(*ppcOutBuf,pslphdr->h_pcLangTag);
  SETXID(*ppcOutBuf,pslphdr->h_usXID);

  if (iOverflow) {
    SETFLAGS(*ppcOutBuf,OVERFLOWFLAG);
    return SLP_OK;
  }
  
  /* parse payload out */
  offset = hdrsz;  
  SETSHT(*ppcOutBuf,api2slp(err),offset);  offset += 2;
  
  if (err == SLP_OK) {
    SETSHT(*ppcOutBuf,count,offset); offset += 2;
    
    mask_reset(pm);
    while (count--) { /* if there are no URLs (ie. on error), skip this */
      int item = mask_next(pm,1);
      offset++; /* skip reserved */
      if ((err = add_sht(*ppcOutBuf,*piOutSz,
			 (0xFFFF & ps->life[item]),&offset)) < 0 ||
	  (err = add_string(*ppcOutBuf,*piOutSz,ps->url[item],&offset)) < 0) {
	return err;
      }
      offset++; /* skip # auths field, none will be supplied */
    }
  }
  return SLP_OK;
}  

#ifdef EXTRA_MSGS
int saadvert_out(SAState *ps, Slphdr *pslph, char **ppc, int *piOutSz){

  int offset = 0;
  const char *pcLang = (pslph->h_pcLangTag)?pslph->h_pcLangTag:"en";
  const char *pcScopeList = SLPGetProperty("net.slp.useScopes");
  const char *pcSrvTypeList = SLPGetProperty("com.sun.slp.saSrvTypes");
  char *pcSAAttrs;
  if (!pcScopeList) pcScopeList = "";
  if (!pcSrvTypeList) pcSrvTypeList = "";

  pcSAAttrs = safe_malloc(strlen(pcSrvTypeList)+strlen("(service-types=")+2,
			  "(service-types=",strlen("(service-types="));
  assert( pcSAAttrs );
  
  strcat(pcSAAttrs,pcSrvTypeList);
  strcat(pcSAAttrs,")");
  
  *piOutSz = strlen(ps->pcSAURL) + 2 + strlen(pcScopeList) + 2 + HDRLEN +
    strlen(pcLang) + strlen(pcSAAttrs) + 1;

  *ppc = safe_malloc(*piOutSz,0,0);
  assert( *ppc );
  
  /* parse header out */
  SETVER(*ppc,2);
  SETFUN(*ppc,SAADVERT);
  SETLEN(*ppc,*piOutSz);
  SETLANG(*ppc,pslph->h_pcLangTag);
  SETXID(*ppc,pslph->h_usXID);
  
  /* parse payload out */
  offset = HDRLEN + strlen(pcLang);  
  add_string(*ppc,*piOutSz,ps->pcSAURL, &offset);
  add_string(*ppc,*piOutSz,pcScopeList, &offset);
  add_string(*ppc,*piOutSz,pcSAAttrs, &offset);
  (*ppc)[offset] = '\0'; /* set the # of auth blocks to 0 */

  SLPFree(pcSAAttrs);

  return SLP_OK;

}
#endif /* EXTRA_MSGS */

#ifdef MAC_OS_X
int daadvert_out(SAState *ps, SLPBoolean viaTCP, Slphdr *pslph, char **ppc, int *piOutSz)
{
    const char *	pcAttributeList = "";
    char*			advertMessage = NULL;
    char*			scopeListToAdvertise = (char*)malloc( strlen(SLPGetProperty("com.apple.slp.daScopeList")) + 1 );	// start out with a copy
    SLPBoolean		needToSetOverflow = SLP_FALSE;
    
    SLP_LOG( SLP_LOG_DEBUG, "daadvert_out called");
    
    if ( SLPGetProperty("com.apple.slp.daAttributeList") )
        pcAttributeList = SLPGetProperty("com.apple.slp.daAttributeList");
        
    if ( !viaTCP && SLPGetProperty("com.apple.slp.daPrunedScopeList") && SLPGetProperty("com.apple.slp.daPrunedScopeList") != ""  )
    {
        strcpy( scopeListToAdvertise, SLPGetProperty("com.apple.slp.daPrunedScopeList") );
        needToSetOverflow = SLP_TRUE;
    }
    else if ( SLPGetProperty("com.apple.slp.daScopeList") )
        strcpy( scopeListToAdvertise, SLPGetProperty("com.apple.slp.daScopeList") );
    else
        return -1;		// no Scope list!
        
    advertMessage = MakeDAAdvertisementMessage( pslph, ps->pcDAURL, scopeListToAdvertise, pcAttributeList, GetStatelessBootTime(), piOutSz );            
    if ( needToSetOverflow )
        SETFLAGS(advertMessage,(unsigned char) OVERFLOWFLAG);	// we want clients to make a TCP connection

    *ppc = advertMessage;
    
    if ( strlen(scopeListToAdvertise) < strlen(SLPGetProperty("com.apple.slp.daScopeList")) )
    {
        SLP_LOG( SLP_LOG_DEBUG, "daadvert_out, advertising scopelist of size:%d out of original size:%d", strlen(scopeListToAdvertise), strlen(SLPGetProperty("com.apple.slp.daScopeList")) );
    }        

    if ( scopeListToAdvertise )
        free( scopeListToAdvertise );
        
    return SLP_OK;

}

char* MakeDAAdvertisementMessage(	Slphdr*	pslph,
									char*	url,
									const char*	scopeList,
									const char*	attributeList,
									long	timeStamp,
                                    int*	outSize )
{
	short	urlLength =strlen( url );
	short	scopeListLength = strlen(scopeList);
	short	attributeListLength = strlen(attributeList);
    const char*	pcLang = (pslph && pslph->h_pcLangTag)?pslph->h_pcLangTag:"en";
	char*	newRequest = NULL;
	char*	curPtr;
	short	sizeofNewMessage 	= HDRLEN + strlen(pcLang) 
								+ 2 													// error code
								+ 4 													// Time stamp
								+ urlLength + sizeof(urlLength) 
								+ scopeListLength + sizeof(scopeListLength) 
								+ attributeListLength + sizeof(attributeListLength)
								+ 2														// SPI List length
								+ 1;													// num Auth blocks
    
    if ( ServerScopeSponsoringEnabled() )
    {
        sizeofNewMessage 	+= 2 + SizeOfServerScopeSponsorData();	// including scope data
    }
    
 	newRequest = (char*)safe_malloc( sizeofNewMessage, 0, 0 );
    assert( newRequest );
	// first fill out the header
    SETVER(newRequest,2);
    SETFUN(newRequest, DAADVERT);
    SETLEN(newRequest, sizeofNewMessage);
    SETLANG(newRequest,pcLang);
    SETXID(newRequest, (pslph)?pslph->h_usXID:0);			// only is zero if this is an unsolicited DAAdvert

	curPtr = newRequest+GETHEADERLEN(newRequest);			// point to beyond the header
	
	*((short*)curPtr) = SLP_OK;	// set the error code
	curPtr += 2;
	
	*(long*)curPtr = timeStamp;	// if we are stateless then we need to remember last time all regs were lost
	curPtr += 4;
	
	*(short*)curPtr = urlLength;
	curPtr += sizeof(urlLength);			// advance past the length bytes
	strcpy( curPtr, url );				// now add the url
	curPtr += urlLength;

	*((short*)curPtr) = scopeListLength;	// set the scope list length
	curPtr += sizeof(scopeListLength);
	
	if ( scopeListLength > 0 )
		strcpy( curPtr, scopeList );
	curPtr += scopeListLength;

	*((short*)curPtr) = attributeListLength;	// set the attributeListLength
	curPtr += sizeof(attributeListLength);
	
	if ( attributeListLength > 0 )
		strcpy( curPtr, attributeList );
	curPtr += attributeListLength;

//	 now we don't support SLP SPI yet
	*((short*)curPtr) = 0;
	curPtr += 2;
	
//	 nor auth blocks so...
	*((char*)curPtr) = 0;
	curPtr += 2;

//	 but perhaps we have it this configured to use SCOPE_SPONSER_EXTENSION_ID
    if ( ServerScopeSponsoringEnabled() )
    {
        memcpy( curPtr, GetServerScopeSponsorData(), SizeOfServerScopeSponsorData() );	// including scope data
        curPtr += SizeOfServerScopeSponsorData();
    }

	*outSize = sizeofNewMessage;
    
	return newRequest;
}

#endif /* MAC_OS_X */

/* ------------------------------------------------------------------------- */

unsigned char api2slp(SLPInternalError se) {
  switch(se) {
    case SLP_OK: return 0;
    case SLP_LANGUAGE_NOT_SUPPORTED:
      return LANGUAGE_NOT_SUPPORTED;
    case SLP_PARSE_ERROR:
      return PARSE_ERROR;
    case SLP_SCOPE_NOT_SUPPORTED:
      return SCOPE_NOT_SUPPORTED;
    default: return INTERNAL_ERROR;
  }
}

