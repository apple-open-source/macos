
/*
 * mslpd_query.c : Handles service requests coming in to the mini SLPv2 SA.
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslp_dat.h"
#include "mslpd_store.h"
#include "mslpd.h"
#include "mslpd_mask.h"
#include "mslpd_stack.h"
#include "mslpd_parse.h"
#include "mslplib.h"  /* mslplib defs local to the library */

/* ------------------------------------------------------------------------- */

typedef enum { BOOL_TOK = TYPE_BOOL, INT_TOK = TYPE_INT, STR_TOK = TYPE_STR,
OPQ_TOK = TYPE_OPAQUE, KEY_TOK = TYPE_KEYWORD,
INPAREN_TOK = 17, OUTPAREN_TOK = 18, NOT_TOK = 19, OR_TOK = 20,
AND_TOK = 21, TAG_TOK = 22, EQ_TOK = 23, LE_TOK = 24,
GE_TOK = 25, IS_TOK = 26, TERM_TOK = 27, ERR_TOK = 28,
INIT_TOK = 29, APPROX_TOK = 30 /* initially */ }
MSLPQToktype;

typedef struct mslpqtoken {
  MSLPQToktype type;
  union {
    int     i;  /* 0 or 1 if BOOL_TOK, errcode if ERR_TOK, int if INT_TOK */
    char   *pc; /* str if STR_TOK, opaque encoding if OPQ_TOK , op if OP_TOK */
  } val;
} MSLPQToken;

typedef struct ServiceLocationHeader{
			 char	byte1;
			 char	byte2;
			 char	byte3;
			 char	byte4;
			 char	byte5;
			 char	byte6;
			 char	byte7;
			 char	byte8;
			 char	byte9;
			 char	byte10;
			 char	byte11;
			 char	byte12;
			 char	byte13;
			 char	byte14;
			 char	byte15;
			 char	byte16;
} ServiceLocationHeader, *ServiceLocationHeaderPtr;

#define QERR(s,err)  { SLP_LOG( SLP_LOG_DROP,(s)); return NULL; }

/* ------------------------------------------------------------------------- */
void CheckPRListAgainstOurKnownDAs( const char* prList );

SLPInternalError CheckIfRequestAlreadyHandled( const char* buffer, int length, char* ourHostIPAddr );
char* MakePluginInfoMessage( SAState* psa, const char* buffer );
short	GetSLPHeaderSize( ServiceLocationHeader* header )
;

static SLPInternalError use_mask(SAStore *ps, 
  const char *pcSAScopeList, const char *pcRqstScopeList, 
  const char *pcSrvtype, const char *pcLangTag, const char *pcQuery,
  Mask **ppMask);

static SLPInternalError handle_query(SAStore *ps, const char *pcSASList,
                             const char *pcRqstSList, const char *pcSrvtype,
                             const char *pcLangTag, const char *pcQuery,
                             Mask **ppMask);
static void next_token(MSLPQToken tokPrev, MSLPQToken *ptok,
                       const char *pcQuery, int *piIndex);
static SLPInternalError handle_term( SAStore *ps, MSLPQState state, Mask *pmUseMask,
                            Mask *pmResultMask, MSLPQToken tagtok,
                            MSLPQToken optok, MSLPQToken valtok);
static int op(SAStore *ps,int i,char *pcTag,MSLPQToktype ttOp,MSLPQToken tok);

static MSLPQToktype delim2Toktype(char c, const char *pcNext);

/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */

/*
 * store_request
 *
 *   This routine takes a Service Request and returns a buffer with 
 *   a reply (or drops the request).  If the request is dropped,
 *   the return buffer is set to NULL and the output size is 0.
 *   Otherwise a full reply will be constructed in the output buffer.
 *
 * Args:
 *
 *   psa       The SAState, all constants, config and a handle to the store
 *   pslphdr   The header of the request - already parsed
 *   pcInBuf   The incoming buffer 
 *   iInSz     The size of the incoming buffer (may be different than hdr 'len'
 *   ppcOutBuf The generated reply buffer will be allocated and passed back
 *   piOutSz   The length of the reply buffer will be set and passed back
 *   piGot     The number of results obtained.
 *
 * Returns: int
 *
 *   If at all possible, a result (possibly error) is returned.
 *   In the case of a fatal error, -1 is returned and buffers are cleaned up.
 *
 * Side Effects:
 *
 *   The reply buffer is allocated here and must be SLPFreed by the caller.
 */
int store_request(SAState *psa, SLPBoolean viaTCP, Slphdr *pslphdr, const char *pcInBuf,
                   int iInSz, char **ppcOutBuf, int *piOutSz, int *piGot)
{
    char *pcPRList=NULL, *pcSrvtype = NULL, *pcSList = NULL, *pcPred = NULL;
    int err;
    int result = 0;
    assert(psa && pslphdr && pcInBuf && ppcOutBuf && piOutSz);
    *ppcOutBuf = NULL;  /* for now, just drop the request! */
    *piOutSz   = 0;
    *piGot     = 0;

	if ((err = srvrqst_in(pslphdr,pcInBuf,iInSz,&pcPRList, &pcSrvtype,&pcSList,&pcPred))<0)
    {
        SLP_LOG( SLP_LOG_DROP,"store_request: drop request due to parse in error" );
    }
    else if (on_PRList(psa,pcPRList))
    {
        SLP_LOG( SLP_LOG_DROP,"store_request: drop request which is on my PRList" );
        result = -1;
    }
    else
    { /* not on PRList */
        if ( !SDstrcasecmp(pcSrvtype,"service:directory-agent") )
        {
            if ( pcPRList && pcPRList[0] != '\0' )
                CheckPRListAgainstOurKnownDAs( pcPRList );
    #ifdef MAC_OS_X
            if ( AreWeADirectoryAgent() )	/* if we are running as a DA, then we need to return our DAAdvert */
            {
                LOG( SLP_LOG_DA,"handling a service:directory-agent type request" );
                err = daadvert_out( psa, viaTCP, pslphdr, ppcOutBuf, piOutSz );

                if (err == SLP_OK)
                    *piGot = 1;
            }
            else
            {
                result = -1;		// don't just return, fall out and free our memory!
            }
    #else
            return -1; /* drop DADiscovery requests */
    #endif	/*  MAC_OS_X */

        }
        else if ( !SDstrcasecmp(pcSrvtype,"service:service-agent") )
        {
        #ifdef EXTRA_MSGS
            LOG(SLP_LOG_SA,"store_request: handling a service:service-agent type request");
            /* handle a sa discovery request here */
            err = saadvert_out(psa,pslphdr,ppcOutBuf, piOutSz);
    
            if (err == SLP_OK)
            {
                *piGot = 1;
            }
        #else
    
            result = -1; /* sa disc is not supported, no result! */

        #endif /* EXTRA_MSGS */
    
        }
        else if ( !SDstrcasecmp(pcSrvtype,"service:service-agent") )
        {
        }
        else
        {
            Mask *pmask;
            SLPInternalError serr =
                handle_query(&(psa->store), SLPGetProperty("net.slp.useScopes"),
                    pcSList,pcSrvtype,pslphdr->h_pcLangTag,pcPred, &pmask);
        
            /* it is ok to generate a reply if the above had an error -
                this is how the error result gets propogated back to the
                requester */
            err = srvrply_out(&(psa->store), serr, pmask, pslphdr, ppcOutBuf, 
                piOutSz, piGot);
        
                if (err != SLP_OK) 
                    result = -1; 
            
            mask_delete(pmask);
        }
    }
    
    if (pcSrvtype) 
        SLPFree((void*)pcSrvtype);
    
    if (pcPRList) 
        SLPFree((void*)pcPRList);
    
    if (pcSList) 
        SLPFree((void*)pcSList);
    
    if (pcPred) 
        SLPFree((void*)pcPred);
    
    return result;
  
}

SLPInternalError HandlePluginInfoRequest( SAState* psa, const char* buffer, int length, char** reply, int *replySize )
{
	char*			serviceReply = NULL;
	SLPInternalError		error = SLP_OK;
	
// let's check to see if we have already replied to this
	if ( error == SLP_OK )
		error = CheckIfRequestAlreadyHandled( buffer, length, psa->pcSANumAddr );
	
	if ( error == SLP_OK )
		serviceReply = MakePluginInfoMessage( psa, buffer );
		
	if ( error == SLP_OK )
	{
    	*reply = serviceReply;
//        *replySize = GETLEN( *reply ); 
        *replySize = GETLEN( (*reply) );	// Need ()'s around this MACRO!!!
    }
    
	if ( error && error != SLP_REQUEST_ALREADY_HANDLED )
		LOG_SLP_ERROR_AND_RETURN( SLP_LOG_DEBUG, "HandlePluginInfoRequest returning error: %d", error );
	
	return error;
}

char*	globalDAList = NULL;
char*	globalUnknownList = NULL;

void CheckPRListAgainstOurKnownDAs( const char* prList )
{
#ifdef DEBUG_DA_LIST
    // this is primarly a dugging tool at the moment, we want to see how many DA's are known by
    // other people that we don't know about
    char*		pcTemp = NULL;
    char		cDelim;
    int 		offset = 0;
    int			result = 0;
    int			i;
    int			listLen;
    DATable*	pdat = GetGlobalDATable();	// ignore what they pass in, only reference the globaly defined table
    
    if ( !pdat || !prList || prList[0] == '\0' )
        return;
        
    if ( globalDAList && globalDAList[0] == '\0' )
    {
        free(globalDAList);
        globalDAList = NULL;
    }
    
    if ( !globalDAList )
    {
        globalDAList = (char*)malloc( strlen(prList) + 1 );
        strcpy( globalDAList, prList );
        
//        sprintf( msg, "Create new globalDAList: %s", globalDAList );
//        SLP_LOG( SLP_LOG_ERR, msg );
    }
    else
    {
        listLen = strlen(globalDAList);
        list_merge( prList, &globalDAList, &listLen, 1 );
    }
    
    LockGlobalDATable();
    
    for (i = 0; i < pdat->iSize; i++)
    {
        int		pcTempLen;
        
        if ( pcTemp )
        {
            pcTempLen = strlen(pcTemp);
            list_merge( inet_ntoa(pdat->pDAE[i].sin.sin_addr), &pcTemp, &pcTempLen, 0 );
        }
        else
        {
            pcTemp = (char*)malloc( strlen(inet_ntoa(pdat->pDAE[i].sin.sin_addr))+1 );
            strcpy( pcTemp, inet_ntoa(pdat->pDAE[i].sin.sin_addr) );
        }
        
        if ( globalDAList && list_intersection( globalDAList, inet_ntoa(pdat->pDAE[i].sin.sin_addr) ) )
        {
            char* oldList = globalDAList;
            char* newList = list_remove_element( globalDAList, inet_ntoa(pdat->pDAE[i].sin.sin_addr) );
            
            globalDAList = newList;
            free( oldList );
            
//            sprintf( msg, "removed element %s from our globalDAList", inet_ntoa(pdat->pDAE[i].sin.sin_addr) );
//            SLP_LOG( SLP_LOG_ERR, msg );
        }
    }
    
    if ( globalDAList && globalDAList[0] && pcTemp )
    {
        SLP_LOG( SLP_LOG_DA, "Unknown DA's: %s", globalDAList );
        
        SLP_LOG( SLP_LOG_DA, "Known DA's (%d):%s", pdat->iSize, pcTemp );
    }
    
    UnlockGlobalDATable();
#endif    
}

SLPInternalError CheckIfRequestAlreadyHandled( const char* buffer, int length, char* ourHostIPAddr )
{
	const char*			curPtr = buffer+GetSLPHeaderSize( (ServiceLocationHeader*)buffer);	// point to the length of the prev responders list
    int				preRespondersLength = *(short*)curPtr;
	SLPInternalError		error = SLP_OK;
	short			len;
	
	{
		curPtr += 2;	// advance past 2 bit length
		
		while ( curPtr < buffer+GetSLPHeaderSize( (ServiceLocationHeader*)buffer)+preRespondersLength )
		{
			len = strlen(ourHostIPAddr);
			if ( ( strncmp(ourHostIPAddr, curPtr, len) == 0) && ((curPtr[len] == ',') || (curPtr[len] == '\0')) )
			{
				error = SLP_REQUEST_ALREADY_HANDLED;
				break;
			}
            
            // now advance to the next comma
			while ( curPtr < buffer+length && *curPtr != ',' )
				curPtr++;
			
			curPtr++;	// advance past comma
		}
	}
	
	return error;
}
	
char* MakePluginInfoMessage( SAState* psa, const char* buffer )
{
	char	siURL[1024];
	char*	replyPtr;
	char*	newReply = NULL;
	char*	curPtr;
	short	replyLength, newReplyLength, urlEntryLength;
	
	// this is a new reply, fill out empty header + room for error code and url entry count
	replyLength = GetSLPHeaderSize( (ServiceLocationHeader*)buffer)+4;
	
	replyPtr = (char*)malloc( replyLength );
	
    SETVER(replyPtr,2);
    SETFUN(replyPtr, SRVRPLY);
    SETLEN(replyPtr, replyLength);
    SETLANG(replyPtr,"en");
    SETXID(replyPtr, GETXID(buffer));						// might need to check if this is appropriate for DA

	*((short*)(replyPtr+GetSLPHeaderSize( (ServiceLocationHeader*)buffer))) = 0;						// set the error
	*((short*)((char*)replyPtr+GetSLPHeaderSize( (ServiceLocationHeader*)buffer)+2)) = 0;		// set the entry count to zero

	replyLength = GETLEN( replyPtr );
	
//	serviceInfo->GetURL( siURL );
	sprintf( siURL, "service:x-MacSLPInfo://%s/Version=%s;OSVersion=X;NumRegServices=%d", psa->pcSANumAddr, SLPD_VERSION, psa->store.size );
	
	urlEntryLength = 1 + 2 + 2 + strlen(siURL) + 1;				// reserved, lifetime, url len, url, # url authentication blocks
	
	newReplyLength = replyLength + urlEntryLength;

	curPtr = replyPtr + GetSLPHeaderSize( (ServiceLocationHeader*)buffer);
	*((short*)curPtr) = 0;
	
	curPtr += 2;	// advance past the error to the urlEntry count;
	
	*((short*)curPtr) += 1;	// increment the urlEntry count;
	
	newReply = (char*)malloc( newReplyLength );
		
	memcpy( newReply, replyPtr, replyLength );
	free( replyPtr );
	
	curPtr = newReply+replyLength;						// now we should be pointing at the end of old data, append new url entry
	
	*curPtr = 0;										// zero out the reserved bit
	curPtr++;
	
	*((short*)curPtr) = 0;								// set lifetime
	curPtr += 2;
	
	*((short*)curPtr) = strlen(siURL);					// set url length
	curPtr += 2;
	
	memcpy( curPtr, siURL, strlen(siURL) );
	curPtr += strlen(siURL);
	
	*curPtr = 0;						// this is for the url auth block (zero of them)
	
	replyPtr = newReply;
	
	SETLEN( replyPtr, newReplyLength );
	
	return replyPtr;
}

short	GetSLPHeaderSize( ServiceLocationHeader* header )
{
	// now the length of the header is dependant on the length of the language
	// tag (which usually will be 2 bytes but it could be longer).
	short		headerSize = sizeof( ServiceLocationHeader );		// always at least this
	short		languageTagLength;
    
	if ( header == NULL )
		return 0;
		
	languageTagLength = *((short*)&(header->byte13));
	
	headerSize += ( languageTagLength - 2 );						// we already assumed 2 bytes, compensate

	return headerSize;
}

/* ------------------------------------------------------------------------- */

/*
* match_langtag
*
*   Match the languages, ignoring the dialect field
*
* Returns:
*
*   0 on no match, 1 on match, SLP_PARSE_ERROR if langtags missing.
*/
int match_langtag(const char *pc1, const char *pc2) 
{
    char c;
    int i1 = 0, i2 = 0, retval = 0;
    char *pcNoDialect1 = NULL;
    char *pcNoDialect2 = NULL;

    if ( !pc1 || !pc2 )
        retval = SLP_PARSE_ERROR;
    else
    {    
        pcNoDialect1 = get_next_string("-",pc1,&i1,&c);
        pcNoDialect2 = get_next_string("-",pc2,&i2,&c);
        
        if (!pcNoDialect1 || !pcNoDialect2)
            retval = SLP_PARSE_ERROR;
        else if (!SDstrcasecmp(pcNoDialect1,pcNoDialect2)) 
            retval = 1;
        
        if (pcNoDialect1)
            SLPFree((void*)pcNoDialect1);

        if (pcNoDialect2)
            SLPFree((void*)pcNoDialect2);
    }
    
    return retval;
}

/*
* match_srvtype
*
*   Match the request by the item in the store.  The match may be by
*   abstract service type.
*
* Return:
*
*   0 on no match, 1 on match, SLP_PARSE_ERROR for missing fields.
*/
int match_srvtype(const char *pcstRqst, const char *pcstStore) {
  int retval = 0;
  assert(pcstStore);
  if (!pcstRqst) retval = SLP_PARSE_ERROR;
  else if (!SDstrcasecmp(pcstRqst,pcstStore)) retval = 1;
  else {
    int iLen = strlen(pcstRqst);
    if (!SDstrncasecmp(pcstRqst,pcstStore,iLen) && pcstStore[iLen] == ':')
      retval = 1; /* abstract service types match */
  }
  return retval;
}

/*
* use_mask
*
*  This routine checks the store to generate a mask which includes only
*  those services with the right service type, scope list and lang tag.
*
*    ps          Pointer to the store.
*    pcSASList   The scope list of the SA.
*    pcRqstSList The scope list of the request.
*    pcSrvtype   The service type of the request.
*    pcLangTag   The lang tag of the request.
*    pcQuery     The search filter of the request.  If it is NULL ignore lang.
*    ppMask      A pointer to the mask which this function will allocate.
*
* Return:
*  SLPInternalError.
*  ppMask is allocated, filled and returned.
*
* Side effects:
*  ppMask allocation will have to be SLPFreed by the caller.
*/
static SLPInternalError use_mask(SAStore *ps, 
                         const char *pcSASList, const char *pcRqstSList, 
                         const char *pcSrvtype, const char *pcLangTag, 
                         const char *pcQuery, Mask **ppMask)
{
  
    int		i, err = 0;

#ifdef TRACE_MASK
    if ( getenv("SLPTRACE") )
        printf("trace: mask size = %d.  Mask is initially:\n",ps->size);
#endif
  
    *ppMask = mask_create(ps->size);
    
    if (!list_intersection(pcRqstSList, pcSASList))
    {
        SLP_LOG( SLP_LOG_DROP, "use_mask returning SLP_SCOPE_NOT_SUPPORTED, requestScope: %s SAScopeList: %s", pcRqstSList?pcRqstSList:"", pcSASList?pcSASList:"" );
            
        return SLP_SCOPE_NOT_SUPPORTED;
    }
  
    for (i = 0; i < ps->size; i++) 
    {
        mask_set(*ppMask,i,0);
        
        if (pcQuery)
        {
            if ((err = match_langtag(pcLangTag,ps->lang[i]))==0)
            {
                SLP_LOG( SLP_LOG_DROP, "use_mask match_langtag no match, pcLangTag: %s ps->lang[%d]: %s", pcLangTag?pcLangTag:"", i, ps->lang[i]?ps->lang[i]:"" );
                
                continue;
            }
        }
        
        if (err<0 || ((err=match_srvtype(pcSrvtype,ps->srvtype[i]))==0))
        { 
            SLP_LOG( SLP_LOG_DROP, "use_mask err: %d, or match_srvtype no match, pcSrvtype: %s ps->srvtype[%d]: %s", err, pcSrvtype?pcSrvtype:"", i, ps->srvtype[i]?ps->srvtype[i]:"" );
            continue;
        }
        
        /* some services may be registered with a subset of the SA's scopes */
        if (!list_intersection(pcRqstSList,ps->scope[i]))
        {
            SLP_LOG( SLP_LOG_DROP, "use_mask scope: %s doesn't match ps->scope[%d]: %s", pcRqstSList?pcRqstSList:"", i, ps->scope[i]?ps->scope[i]:"" );
            continue;
        }
        
        if (err>=0)
            mask_set(*ppMask,i,1);
    }
    
    #ifdef TRACE_MASK
    mask_show(*ppMask);
    #endif
    
    if (err >= 0)
        return SLP_OK;
    else
        return SLP_PARSE_ERROR;
}

/*
* on_PRList
*
* Return:
*   0 if not on PRList, 1 if on PRList
*/
int on_PRList(SAState *psa, char *pcPRList) 
{  
    char *pcTemp = NULL, cDelim;
    int offset = 0, result = 0;
    
    if ( psa && pcPRList )
    {
        while ( (pcTemp = get_next_string(",",pcPRList,&offset,&cDelim)) != NULL )  
        {
            if ( !SDstrcasecmp(pcTemp,psa->pcSAHost) || !SDstrcasecmp(pcTemp,psa->pcSANumAddr) )
            {
                result = 1;
                break;
            }
            
            SLPFree((void*)pcTemp);
            pcTemp = NULL;
        }
        
        if (pcTemp)
            SLPFree((void*)pcTemp);
    }
    
    return result;
}


/* ------------------------------------------------------------------------- */

/*
*
*
*
*
* There are two kinds of state, here.  One is pushed onto a frame when a
* new '(' nesting arrives.  These include the transitions below.  Note
* that transitions to and from TERM do not change the stack state settings.
* For this purpose a variable 'state' is kept.  It is initialized to the
* state of the frame after a push or a pop.  It will change in and out of
* 'term' till the concluding ')' comes, then a pop will restore the
* previous state.
*
* State graph
*
*    [INIT] -- '(' '&' --> [AND]
*           -- '(' '|' --> [OR]
*           -- '(' '!' --> [NOT]
*           -- '(' tag --> [TERM]
*           -- ')' --> return result mask, done
*    [AND]  -- '(' '&' push --> [AND]
*           -- '(' '|' push (invert mask) --> [OR]
*           -- '(' '!' push --> [NOT]
*           -- '(' tag --> [TERM]
*           -- ')' --> pop
*    [OR]   -- '(' '&' push --> [AND]
*           -- '(' '|' push (invert mask) --> [OR]
*           -- '(' '!' push --> [NOT]
*           -- '(' tag --> [TERM]
*           -- ')' --> pop
*    [NOT]  -- '(' '&' push --> [AND]
*           -- '(' '|' push (invert mask) --> [OR]
*           -- '(' '!' push --> [NOT]
*           -- '(' tag --> [TERM]
*           -- ')' --> pop (invert mask)
*    [TERM] -- op ')' --> restore current stack state
*           -- op val ')' --> restore current stack state
*
*/
static SLPInternalError handle_query(SAStore *ps, const char *pcSAScopeList,
                             const char *pcRqstScopeList, const char *pcSrvtype,
                             const char *pcLangTag, const char *pcQuery,
                             Mask **ppMask) {
  
  MSLPQToken  tok, prevtok, tagtok;
  int         index = 0;
  Mask       *pmUse;    /* the base mask, either returned or in stack */
  Mask       *pmOnly;   /* the mask of the only possible matching services */
  Mask       *pmTemp;   /* used for mask calculations */
  SLPInternalError err;
  MSLPQState  state = INIT_STATE;
  MSLPQStack *pQS;

  if (pcSAScopeList == NULL) pcSAScopeList = ""; /* guard this parameter */
  
  err = use_mask(ps,pcSAScopeList,pcRqstScopeList,
		 pcSrvtype,pcLangTag,pcQuery,&pmOnly);

  *ppMask = NULL; /* initialize the returned mask to NULL in case of error */
  
  if (err != SLP_OK) {
    *ppMask = pmOnly;  /* the error came from use_mask.  Return '0' mask */
    return err;
  }

  if (pcQuery[0] == '\0') { /* special case: empty query mask */
    *ppMask = pmOnly;
    return SLP_OK;
  }
  pmUse = mask_clone(pmOnly);
  pQS = stack_init(pmUse);
  prevtok.type = INIT_TOK;
  
  for (next_token(prevtok,&tok,pcQuery,&index) ;
  (tok.type != TERM_TOK && tok.type != ERR_TOK);
  next_token(prevtok,&tok,pcQuery,&index)) {
    
    switch(state) {
      
    case INIT_STATE: /* beginning of parse, INIT_STATE frame was 'pushed'
      as part of the initialization of the stack */
      
      if (tok.type != INPAREN_TOK) {
	mask_delete(pmOnly);
	stack_delete(pQS);
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_query: no initial '('",SLP_PARSE_ERROR);
      }
      prevtok = tok;
      next_token(prevtok,&tok,pcQuery,&index); 
      
      /* set the currect state to advance, INIT stack frame already there */
      switch(tok.type) {
      case AND_TOK:
        state = AND_STATE;
        stack_push(pQS,mask_clone(stack_curr(pQS)->pmask),AND_STATE); break;
      case OR_TOK:
        state = OR_STATE;
        pmTemp = mask_invert(stack_curr(pQS)->pmask);
	/* and with the pmOnly mask, so only the allowed services are sought */
	stack_push(pQS,mask_and(pmTemp,pmOnly),OR_STATE);
	mask_delete(pmTemp);
	break;
	
      case NOT_TOK:
        state = NOT_STATE;
        stack_push(pQS,mask_clone(stack_curr(pQS)->pmask),NOT_STATE); break;
      case TAG_TOK:
        state = TERM_STATE; break; /* do not push */
      default:

	mask_delete(pmOnly);
	stack_delete(pQS);	
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_query: bad init term:",SLP_PARSE_ERROR);
      }
      break;
      
      case AND_STATE:  /* following a & */
      case OR_STATE:   /* following a | */
      case NOT_STATE:  /* following a ! */
        
        if (tok.type == OUTPAREN_TOK) { /* pop */
          Mask *pmPopTemp;
          if (stack_curr(pQS)->state == NOT_STATE) {
            pmPopTemp = mask_invert(stack_curr(pQS)->pmask);
	    /* and with the pmOnly mask so only allowed services match */
	    pmTemp = mask_and(pmPopTemp,pmOnly); 
            mask_delete(stack_curr(pQS)->pmask);
	    mask_delete(pmPopTemp);
            stack_curr(pQS)->pmask = pmTemp;
          }

          if (stack_prev(pQS)->state == AND_STATE) {
            pmTemp = mask_and(stack_prev(pQS)->pmask,stack_curr(pQS)->pmask);
          } else if (stack_prev(pQS)->state == OR_STATE) {
            pmTemp = mask_or(stack_prev(pQS)->pmask,stack_curr(pQS)->pmask);
          } else if (stack_prev(pQS)->state == INIT_STATE) {
            pmTemp = mask_clone(stack_curr(pQS)->pmask);
          } else if (stack_prev(pQS)->state == NOT_STATE){
            pmTemp = mask_clone(stack_curr(pQS)->pmask);
          } else {
	    mask_delete(pmOnly);
            stack_delete(pQS);
            LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"handle_query: bad prev state",SLP_PARSE_ERROR);
          }
          
          mask_delete(stack_prev(pQS)->pmask);
          mask_delete(stack_curr(pQS)->pmask);
          stack_prev(pQS)->pmask = NULL;
          stack_curr(pQS)->pmask = NULL;

          if (stack_prev(pQS)->state == INIT_STATE) { /* done: no more frames to pop */
            *ppMask = pmTemp;

#ifdef TRACE_MASK
              printf("at return: \n");
              mask_show(*ppMask);            
#endif
	    mask_delete(pmOnly);
            stack_delete(pQS);
            return SLP_OK;
          } else { /* set the previous frame to have the right updated mask */
            stack_prev(pQS)->pmask = pmTemp;
            if (stack_pop(pQS) == NULL) {
	      LOG(SLP_LOG_ERR,"handle_query: unexpected NULL query stack frame");
	    }
          }
          
        } else if (tok.type == INPAREN_TOK) {
          prevtok = tok;
          next_token(prevtok,&tok,pcQuery,&index);
          
          /* push the current stack frame to advance */
          switch (tok.type) {
          case AND_TOK:
            state = AND_STATE;
            if (stack_curr(pQS)->state == OR_STATE) {
	      pmTemp = mask_invert(stack_curr(pQS)->pmask);
	      /* and with the pmOnly mask to match only allowed services */
              stack_push(pQS,mask_and(pmTemp,pmOnly),AND_STATE);
	      mask_delete(pmTemp);
            } else {
              stack_push(pQS,mask_clone(stack_curr(pQS)->pmask),AND_STATE); 
            }
            break;

          case OR_TOK:
            state = OR_STATE;
            if (stack_curr(pQS)->state != OR_STATE) {
	      pmTemp = mask_invert(stack_curr(pQS)->pmask);
              stack_push(pQS,mask_and(pmTemp,pmOnly),OR_STATE);
	      mask_delete(pmTemp);
            } else {
              stack_push(pQS,mask_clone(stack_curr(pQS)->pmask),OR_STATE);
            }
            break;

          case NOT_TOK:
            state = NOT_STATE;
            stack_push(pQS,mask_clone(stack_curr(pQS)->pmask),NOT_STATE); break;
          case TAG_TOK:
            state = TERM_STATE; break; /* don't push */
          default:
	    mask_delete(pmOnly);
            stack_delete(pQS);
            LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_query: bad init term",SLP_PARSE_ERROR);
          }
        }
        break;
        
      case TERM_STATE: /* inside a terminal */
        
        /* prevtok = tag, tok = op, so we must advance */
        tagtok = prevtok;
        prevtok = tok;
        
        /* tagtok=tag,prevtok=op,tok=val */
        next_token(prevtok,&tok,pcQuery,&index); 
        
        if (tok.type == ERR_TOK) {
	  mask_delete(pmOnly);
	  stack_delete(pQS);
          LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_query: value bad",SLP_PARSE_ERROR);
	}

#ifdef TRACE_MASK
        printf("before handle query: \n");
        mask_show(stack_curr(pQS)->pmask);
#endif

        if ((err = handle_term(ps, stack_curr(pQS)->state, pmUse,  
          stack_curr(pQS)->pmask,tagtok,prevtok,tok)) != SLP_OK) {
	  mask_delete(pmOnly);
	  stack_delete(pQS);
          LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_query: processing terminal failed",err);
        }

#ifdef TRACE_MASK
        printf("after handle query: \n");
        mask_show(stack_curr(pQS)->pmask);
#endif
        
        /* we have already advanced past the outparen.  simply reset
        state to that in the surrounding frame */
        state = stack_curr(pQS)->state;
        
        if (state == INIT_STATE) { /* special case: ONE TERM predicate */
          *ppMask = mask_clone(stack_curr(pQS)->pmask); /* done */
          stack_delete(pQS);
	  mask_delete(pmOnly);
          if (tagtok.val.pc != NULL) SLPFree((void*)tagtok.val.pc);
          if ((tok.type == STR_TOK || tok.type == OPQ_TOK) && tok.val.pc ) {
            SLPFree((void*)tok.val.pc);
          }
          return SLP_OK;	  
        } else {
          tok.type = OUTPAREN_TOK; /* parsing of value ended at OUTPAREN */
        }
        
        break;
        
      default:
        stack_delete(pQS);
	mask_delete(pmOnly);
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,
		  "handle_query: state type?",SLP_INTERNAL_SYSTEM_ERROR);
        
    }

    if (prevtok.type == TAG_TOK && prevtok.val.pc != NULL) 
      SLPFree((void*)prevtok.val.pc);     
    prevtok = tok;
    
  }
  stack_delete(pQS);
  mask_delete(pmOnly);
  if (tok.type == ERR_TOK) return (SLPInternalError) tok.val.i;
  LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_query: unexpected end of parse",SLP_PARSE_ERROR);
}

/* ------------------------------------------------------------------------- */

static int skipWild(const char *pcQuery, int iQOffset,
		    const char *pcString, int *piSOffset, int iStrLen) {

  char cNext = pcQuery[iQOffset];
  
  if (cNext == '*') {
    return SLP_PARSE_ERROR;
  }
  
  if (cNext == '\0') {
    return 1; /* final '*' always matches */
  }
  
  /* skip over a wild card */
  while (toupper(pcString[*piSOffset]) != toupper(cNext)) {
    if (*piSOffset < iStrLen) {
      (*piSOffset)++;
    } else {
      return 0; /* '*' CHAR doesn't match the pcString */
    }
  }
  return 1; /* we found a the position for the continuing to search */
}

/*
 * isWildMatch
 * 
 *   This match is done in a stateful single pass.  The possible grammar is
 *     ['*'] 1*char *['*' 1*char ] ['*']
 *
 *   The algorithm:
 *
 *     If there is a no initial '*' or substring, it is a 'match nothing'
 *       return 0.
 *
 *     If there is only a '*' it is a presense filter - return 1.
 *
 *     Pass through the query string, delimited by the '*'.  When this
 *       string is exhausted - we had no mismatch - thus we have a match!
 *
 *       If the substring is NULL and the delimiter is '*' we have an
 *         initial '*'.  We do '*' matching - see below:
 *
 *       '*' matching - when the delimiter is '*':
 *         In this case, the first matching character (after the '*'
 *         in the qs) is found in the searched string.  If such a
 *         character doesn't exist, we are done.
 *
 *       If there is a substring found and the delimiter is NULL -
 *         this is a terminal substring.  This means that the terminal
 *         query substring must match the terminal characters of the
 *         searched string.
 *
 *       If there is a substring and the delimiter is not NULL -
 *         perform a substring match (query substring vs searched string)
 *
 *         If this is successful, advance the searched string index and
 *         continue with the pass through the query string. Example:
 *
 *           query string = "xy*yz", searched string = "xyaxyz"
 *           The first query substring will be "xy", matching the first
 *           substring of the searched string.  The searched string
 *           index is advanced and the query string '*' is handled
 *           (which will skip over the 'ax' in the searched string).
 *           
 *         If this is unsuccessful, advance the search string index
 *         by one and try again until either there is not enough
 *         room left in the string or there is a success.  Examples:
 *
 *           query string = "*xy*", searched string = "axbxy"
 *           The '*' handling of the initial '*' will skip 'a'.
 *           The query substring 'xy' does not match the searched
 *           string substring 'xb', so the searched string index
 *           is advanced.  'bx' doesn't match either, but 'xy' does.
 *           The query string is advanced to the final '*'.  In this
 *           case - it matches the remaining search string ("").
 *           
 *           query string = "*xyz*", searched string = "xyxy"
 *           The '*' handling of the initial '*' has no effect.
 *           The substring 'xyz' is matched to 'xyx' which fails,
 *           then against 'yxy' which also fails.  The search aborts
 *           since 'xyz' is length 3 and the index into the searched
 *           string is already 2 of 4.
 *        
 * returns: 
 *   0 for failure, 1 for success, SLP_PARSE_ERROR for malformed query. 
 */         
int isWildMatch(const char *pcQuery, const char *pcString) {

  int   iQOffset = 0;
  int   iSOffset = 0;
  char *pcSubstring;
  char  cDelim;
  int   iQueryLen = strlen(pcQuery);
  int   iStrLen   = strlen(pcString);
  int   retval;
  int   found = 0;
  int   initial_strict = 0; /* if there is an initial non-'*' */
  
  if (iQueryLen == 0) {
    return SLP_PARSE_ERROR;
  }
  
  if (iQueryLen == 1 && pcQuery[0] == '*') {
    return 1; /* presense filter - always true */
  }

  if (pcQuery[0] == '*') { /* handle initial '*' */
    iQOffset++; /* skip the '*' - required by skipWild */
    retval = skipWild(pcQuery, iQOffset, pcString, &iSOffset,iStrLen);
    if (retval != 1) return retval;
  } else {
    initial_strict = 1;
  }
					  
  while ((pcSubstring = get_next_string("*",pcQuery,&iQOffset,&cDelim))) {

    if (pcSubstring != NULL) {

      found = 0;

      if (initial_strict) {
	if (!SDstrncasecmp(pcSubstring,pcString,strlen(pcSubstring))) {
	  iSOffset += strlen(pcSubstring);
	  SLPFree(pcSubstring);	  	  
	  initial_strict = 0;
	  continue;
	} else {
	  SLPFree(pcSubstring);	  
	  return 0; /* initial substring does not match */
	}
      }

      while (iSOffset < iStrLen) {

	int iSLen = strlen(pcSubstring);

	if (iSLen > (iStrLen-iSOffset)) {
	  SLPFree(pcSubstring);	  
	  return 0; /* pcSubstring too long for remaining searched string */
	}

	if (cDelim == '\0') {

	  if (!SDstrcasecmp(pcSubstring,&pcString[iSOffset])) {
	    SLPFree(pcSubstring);
	    return 1; /* final string matched */
	  } 

	} else if (!SDstrncasecmp(pcSubstring,&pcString[iSOffset],iSLen)) {
	  iSOffset += iSLen; /* advance searched string past query substring */
	  found = 1;
	  break;             /* done with the scan through pcString */
	}

	iSOffset++;
	
      }

      SLPFree(pcSubstring);

    }

    if (found == 0) return 0; /* could not find substring match */
    assert(iSOffset <= iStrLen); /* assure iSOffset never > iStrLen */

    if (cDelim == '*') {
      retval = skipWild(pcQuery, iQOffset, pcString, &iSOffset,iStrLen);
      if (retval != 1) return retval;
    }      
  }
  return 1;
}

/*
* op
*
*   Actually perform the comparison operation.  We will do it liberally -
*   logging type conflicts, but just returning FALSE not errors.  The
*   absence of a term also indicates 'FALSE' as the result.  If ANY value
*   for an attribute succeeds, the entire request succeeds.
*
*   UNRESOLVED RIGHT NOW:  UTF8 comparisons
*
*     ps    The store, all service entries
*     i     The index of the specific entry to operate against
*     pcTag The attribute ID to operate against
*     ttOp  The operation to perform
*     tok   The value to use in the operation
*     
* Return:
*
*   0 if false, 1 if true, SLPInternalError if an error occurs (negative #)
*/
static int op(SAStore *ps,int i,char *pcTag,MSLPQToktype ttOp,MSLPQToken tok) {
  
  int j,k,cmp;
  
  assert(ps && pcTag);
  assert(i>=0 && i<ps->size); /* make sure index is in range of store */
  
  if (ps->tag[i] == NULL) {
    
    return 0; /* services with no attributes can only be matched with NULL
                 queries.  A query with any terms will not match as the 
                 service has no attributes to operate on */
  }

  for (j = 0; ps->tag[i][j]; j++) {
    if (!SDstrcasecmp(ps->tag[i][j],pcTag)) {
      if (ttOp == IS_TOK) 
        return 1; /* just being here satisfies 'presense' */

      if (ps->values[i][j].type != tok.type) {
        SLP_LOG( SLP_LOG_DROP,
	    "op: rqst where attr value type != registered attr type"); 
        continue;
      }
            
      for (k = 0; k < ps->values[i][j].numvals; k++) {
        switch (tok.type) {
        case OPQ_TOK:
        case STR_TOK:
	  if (strstr(tok.val.pc,"*") != NULL) {
	    if (ttOp == EQ_TOK) {

	      int result =
		isWildMatch(tok.val.pc,ps->values[i][j].pval[k].v_pc);
	      if (result == 1) {
		return 1;
	      } else if (result == 0) {
		break;
	      } else {
		LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"op: illegal query with '*'",
			  (SLPInternalError)result);
	      }
	    }
	  }
	    
          cmp = SDstrcasecmp(ps->values[i][j].pval[k].v_pc,tok.val.pc);
          switch (ttOp) {
            case EQ_TOK: if (cmp == 0) return 1; else break;
            case LE_TOK: if (cmp <= 0) return 1; else break;
            case GE_TOK: if (cmp >= 0) return 1; else break;
	    default: 
	      break;
          }
          break;
	  
          case INT_TOK:
            switch (ttOp) {
            case EQ_TOK: if (ps->values[i][j].pval[k].v_i==tok.val.i) return 1; 
              break;
            case LE_TOK: if (ps->values[i][j].pval[k].v_i<=tok.val.i) return 1;
              break;
            case GE_TOK: if (ps->values[i][j].pval[k].v_i>=tok.val.i) return 1;
              break;
	    default:
	      break;
            }
            break;
	    
            case BOOL_TOK:
              if (ttOp == EQ_TOK && tok.val.i == ps->values[i][j].pval[k].v_i)
                return 1;
              break;
              
            case KEY_TOK:
              if (ttOp == EQ_TOK &&
                SDstrcasecmp(tok.val.pc,ps->values[i][j].pval[k].v_pc))
                return 1;
              break;
              
            default:
              LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"op: unknown data token",SLP_PARSE_ERROR);
        }
      } /* do any of the values match? */
    } /* do the tags and types match the request? */
  } /* each attribute */
  return 0;
}

/*
* handle term
*
*     ps           The store, includes all services and attributes.
*     state        The state (AND, OR, NOT), for optimizations.
*     pmUseMask    The set of possible service entries in reply.
*     pmResultMask The existing mask to update with terminal results.
*     tagtok       The term's tag (ie. attribute id)
*     optok        The term's operator (ie. <=, etc.)
*     valtok       The term's value (ie. TRUE or 999)
*
* Result:  SLPInternalError
*
* Side Effects:
*
*/

static SLPInternalError handle_term( SAStore *ps, MSLPQState state, Mask *pmUseMask,
                            Mask *pmResultMask, MSLPQToken tagtok,
                            MSLPQToken optok, MSLPQToken valtok) {
  
  MSLPQToktype tval = valtok.type;
  int i;
  
  /* parameter checking */
  assert(ps && pmUseMask && pmResultMask );
  if (tagtok.type != TAG_TOK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_term: expected tag_tok",SLP_PARSE_ERROR);
  }
  if (optok.type != EQ_TOK &&  optok.type != LE_TOK  &&
    optok.type != GE_TOK && optok.type != IS_TOK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_term: expected op_tok",SLP_PARSE_ERROR);
  }  
  if (optok.type == IS_TOK && valtok.type != OUTPAREN_TOK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_term: IS_TOK not followed by ')'",
	      SLP_PARSE_ERROR);
  } 
  if (optok.type != IS_TOK && tval!= STR_TOK && tval != OPQ_TOK && 
    tval != INT_TOK && tval != BOOL_TOK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_term: expected val_tok",SLP_PARSE_ERROR);
  }
  if (state != AND_STATE && state != OR_STATE &&
    state != NOT_STATE && state != INIT_STATE) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_term: illegal state",SLP_PARSE_ERROR);
  }
  mask_reset(pmUseMask);
  while ((i = mask_next(pmUseMask,1)) != -1) {
    int iSet = mask_get(pmResultMask,i);
    int iResult;
    
    /* optimization: AND no further if already false, OR no further if true */
    if ((!iSet && state == AND_STATE) || (iSet && state == OR_STATE)) continue;
    
    if ((iResult = op(ps, i, tagtok.val.pc, optok.type, valtok))<0)
      LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"handle_term: op failed",SLP_PARSE_ERROR);
    
    mask_set(pmResultMask,i,iResult);
  }
  
  return SLP_OK;
}

/* ------------------------------------------------------------------------- */

static MSLPQToktype delim2Toktype(char c, const char *pcNext) {
  
  switch (c) {
  case '&': return AND_TOK;
  case '|': return OR_TOK;
  case '!': return NOT_TOK;
  case '=': if (*pcNext != '*') return EQ_TOK;
            else {
              /* 
               * Here we have to be careful!
               * If there's anything nonwhite between the '*' and the end we 
               * have an 'eq' case with a wildcard, not a presense test.
               */
              
              pcNext++;
              while (pcNext && isspace(*pcNext) && *pcNext != ')') {
                pcNext++;
              }

              /* If pcNext is now pointing at the end paren, presense */
              if (*pcNext == ')') {
                return IS_TOK; 
              } else { /* otherwise, we have a comparison test */
                return EQ_TOK;
              }
            }
  case '~': if (*pcNext == '=') return APPROX_TOK; else return ERR_TOK;
  case '<': if (*pcNext == '=') return LE_TOK; else return ERR_TOK;
  case '>': if (*pcNext == '=') return GE_TOK; else return ERR_TOK;
  default:
    SLP_LOG( SLP_LOG_DEBUG,"delim2Toktype: bad delim - should never get here");
    return ERR_TOK; /* should never get here */
  }
}

/*
* next_token
*
*   This routine sets the fields in the MSLPQToken, which passes info out
*   about the stream of tokens.  It uses state (previous token) to determine
*   what sorts of legal 'next tokens' there are.  It makes use of the general
*   mslp utility 'get_next_string' to do the real work.
*
*     tokPrev   The previous token is passed in
*     ptok      Will be set to be this token
*     ptokNext  Will be set to be the next token
*     pcQuery   The query string
*     piIndex   The parsing index, increments as the parsing progresses
*
* Results:  None directly - but error codes can be passed out in the ptok
*   (current token) when the token type is set to ERR_TOK.
*
* Side Effects:
*   piIndex is incremented as the query string is parsed through.
*/
static void next_token(MSLPQToken tokPrev, MSLPQToken *ptok,
                       const char *pcQuery, int *piIndex)
{
  char *pc;
  char c;
  MSLPQToktype tt;
  
  switch (tokPrev.type) {
    
    /* "(" then ( '&' '|' '!' <tag>) */
  case INIT_TOK: 
  case AND_TOK:
  case OR_TOK: 
  case NOT_TOK:
    pc = get_next_string("(",pcQuery,piIndex,&c);
    if (pc != NULL || c != '(') {
      SLP_LOG( SLP_LOG_DROP,"next_token: no leading '('");
      ptok->type = ERR_TOK;
      ptok->val.i = SLP_PARSE_ERROR;
    } else {
      ptok->type = INPAREN_TOK;
      if (pc) SLPFree((void*)pc);
    }
    break;
    
    /* ( '&' '|' '!' ) or <tag> then ( <= = ~= >= =* ) 
       Here we get the tag or logical operator.  We will to check the
       next operator, since we get it anyway.  Then we back off from it,
       to allow the TAG_TOK state to handle the operator if it is a leaf. */
  case INPAREN_TOK:
    pc = get_next_string("&|!=<>~",pcQuery,piIndex,&c);
    /* be careful not to overrun pcQuery getting next c */
    tt = delim2Toktype(c,&pcQuery[*piIndex]);
    if (tt == AND_TOK || tt == OR_TOK || tt == NOT_TOK) {
      ptok->type = tt;
    } else if (pc && (tt==EQ_TOK || tt==LE_TOK || tt==GE_TOK || tt==IS_TOK ||
                      tt==APPROX_TOK)) {
      ptok->type = TAG_TOK;
      ptok->val.pc = safe_malloc(strlen(pc)+1,pc,strlen(pc));
      assert( ptok->val.pc );
      
      /* back up from the operator so it will get parsed next */
      *piIndex -= 1; 
    } else { 
      SLP_LOG( SLP_LOG_DROP,"next_token: illegal first tok after '('");
      ptok->type = ERR_TOK;
      ptok->val.i = SLP_PARSE_ERROR;
    }
    SLPFree((void*)pc);
    break;
    
    /* '=' then '*' or ('<' '>' '~') then '=' */
  case TAG_TOK: 
    pc = get_next_string("<=>~",pcQuery,piIndex,&c);
    /* be careful not to overrun pcQuery getting next c */
    tt = delim2Toktype(c,&pcQuery[*piIndex]);
    if (!pc && tt==EQ_TOK ) {
      ptok->type = tt;
    } else if (!pc &&
               (tt==LE_TOK || tt==GE_TOK || tt==IS_TOK || tt==APPROX_TOK)){
      if (tt==APPROX_TOK) tt = EQ_TOK; /* we handle approx as '=' */
      ptok->type = tt;
      *piIndex += 1;
    } else { 
      SLP_LOG( SLP_LOG_DROP,"next_token: illegal operator after 'tag'");
      ptok->type = ERR_TOK;
      ptok->val.i = SLP_PARSE_ERROR;
    }
    SLPFree((void*)pc);
    break;
    
    /* as per AND_TOK or ')' then '(' or ')' then ')' or ')' then zip or zip */
  case OUTPAREN_TOK:
    
    pc = get_next_string("()",pcQuery,piIndex,&c);
    if (!pc && c=='\0') ptok->type = TERM_TOK;
    else if (!pc && c == ')') ptok->type = OUTPAREN_TOK;
    else if (!pc && c == '(') ptok->type = INPAREN_TOK;
    else {
      ptok->type = ERR_TOK;
      ptok->val.i= SLP_PARSE_ERROR;
      if (pc) SLPFree((void*)pc);
    }
    break;

  case IS_TOK:
    {
      pc = get_next_string(")",pcQuery,piIndex,&c);
      if (pc || c != ')') {
        ptok->type = ERR_TOK;
        ptok->val.i = SLP_PARSE_ERROR;
      } else {
        ptok->type = OUTPAREN_TOK;
      }
    }
    break;

    /* <val> then ')' */
  case EQ_TOK:
  case LE_TOK:
  case GE_TOK:
    {
      Values v;
      Val    val;
      int err;
      v.pval = &val;
      pc = get_next_string(")",pcQuery,piIndex,&c); /* advance & test */
      if (c != ')' || pc == NULL) {
        ptok->type = ERR_TOK;
        ptok->val.i = SLP_PARSE_ERROR;
        if (pc) SLPFree((void*)pc);
      } else {
        char *pcFinger = pc;
	int offset = 0;
        err = (SLPInternalError) fill_value(&v,0,pcFinger,&offset);
        ptok->type = (MSLPQToktype) v.type;	    
        if (err) {
          ptok->type = ERR_TOK;
          ptok->val.i= err;
          SLPFree((void*)pc);
        } else if (v.type == TYPE_BOOL || v.type == TYPE_INT) {
          ptok->val.i = v.pval->v_i;
          SLPFree((void*)pc);
        } else if (v.type == TYPE_OPAQUE) {
          ptok->val.pc = v.pval->v_pc;
        } else if (v.type == TYPE_STR) {
          ptok->val.pc = list_pack(v.pval->v_pc); /* elide spaces */
          SLPFree(v.pval->v_pc);
        } else {
          ptok->type = ERR_TOK;
          ptok->val.i= SLP_PARSE_ERROR;
          SLPFree((void*)pc);
        }
      }
    }
    break;
    
    /* ')' must come next */
  case INT_TOK:
  case STR_TOK:
  case BOOL_TOK:
  case OPQ_TOK:
    
    pc = get_next_string(")",pcQuery,piIndex,&c);
    if (pc || c != ')') {
      ptok->type = ERR_TOK;
      ptok->val.i= SLP_PARSE_ERROR;
      if (pc) SLPFree((void*)pc);
    } else {
      ptok->type = OUTPAREN_TOK;
    }
    break;
    
  default:
    SLP_LOG( SLP_LOG_DROP,"next_tok: unexpected previous token");
    ptok->type = ERR_TOK;
    ptok->val.i= SLP_PARSE_ERROR;
    break;
  }
}

/*
 * ------------------------------------------------------------------------
 * Below this point is testing code
 * ------------------------------------------------------------------------ 
 */

#ifdef QUERY_TEST

/*
  try it with escaped characters
  case insensitivity
  syntax errors
*/
void test_wildmatch() {

  /* those which should match */
  
  if (isWildMatch("*","string") != 1) printf("wm1.0 wild should match\n");

  if (isWildMatch("str","str") != 1) printf("wm2.0 unwild should match\n");

  if (isWildMatch("*x","x") != 1) printf("wm3.0 '*x' matches x\n");
  if (isWildMatch("*x","yx") != 1) printf("wm3.1 '*x' matches yx\n");
  if (isWildMatch("*x","xyx") != 1) printf("wm3.2 '*x' matches xyx\n");

  if (isWildMatch("a*","a") != 1) printf("wm4.0 'a*' matches a\n");
  if (isWildMatch("a*","ab") != 1) printf("wm4.1 'a*' matches ab\n");
  if (isWildMatch("a*","aba") != 1) printf("wm4.2 'a*' matches aba\n");

  if (isWildMatch("m*m","mm") != 1) printf("wm5.0 'm*m' matches mm\n");
  if (isWildMatch("m*m","m x m") != 1) printf("wm5.1 'm*m' matches m x m\n");
  if (isWildMatch("m*m","m mvm") != 1) printf("wm5.2 'm*m' matches m mvm\n");

  if (isWildMatch("*a*","a") != 1) printf("wm6.0 '*a*' matches a\n");
  if (isWildMatch("*a*","bad") != 1) printf("wm6.1 '*a*' matches bad\n");
  if (isWildMatch("*a*","foo a") != 1) printf("wm6.2 '*a*' matches foo a\n");
  if (isWildMatch("*a*","a foo") != 1) printf("wm6.3 '*a*' matches a foo\n");
  if (isWildMatch("*a*","aaa") != 1) printf("wm6.4 '*a*' matches aaa\n");
  
  if (isWildMatch("*a*b","ab")!=1) printf("wm7.0 '*a*b' matches ab \n");
  if (isWildMatch("*a*b","xab")!=1) printf("wm7.1 '*a*b' matches xab\n");
  if (isWildMatch("*a*b","axb")!=1) printf("wm7.2 '*a*b' matches axb \n");  
  if (isWildMatch("*a*b","xaxb")!=1) printf("wm7.3 '*a*b' matches xaxb\n");  
  if (isWildMatch("*a*b","axbxb")!=1) printf("wm7.4 '*a*b' matches axbxb\n");  
  if (isWildMatch("*a*b","xaxbxb")!=1) printf("wm7.5 '*a*b' matches xaxbxb\n");

  if (isWildMatch("c*d*","cd")!=1) printf("wm8.0 'c*d*' matches cd \n");  
  if (isWildMatch("c*d*","cxd")!=1) printf("wm8.1 'c*d*' matches cxd \n");  
  if (isWildMatch("c*d*","cxdx")!=1) printf("wm8.2 'c*d*' matches cxdx \n");
  if (isWildMatch("c*d*","ccxdd")!=1) printf("wm8.3 'c*d*' matches ccxdd \n");

  if (isWildMatch("*e*f*","ef")!=1) printf("wm9.0 '*e*f*' matches ef\n");
  if (isWildMatch("*e*f*","xef")!=1) printf("wm9.1 '*e*f*' matches xef\n");  
  if (isWildMatch("*e*f*","xexf")!=1) printf("wm9.2 '*e*f*' matches xexf\n");  
  if (isWildMatch("*e*f*","fefx")!=1) printf("wm9.3 '*e*f*' matches fefx\n");  
  if (isWildMatch("*e*f*","xefx")!=1) printf("wm9.4 '*e*f*' matches xexf\n");  
  if (isWildMatch("*e*f*","xexfx")!=1) printf("wm9.5 '*e*f*' matches xexfx\n");
  if (isWildMatch("*e*f*","exf")!=1) printf("wm9.6 '*e*f*' matches exf\n");  
  if (isWildMatch("*e*f*","efx")!=1) printf("wm9.7 '*e*f*' matches efx\n");  
  if (isWildMatch("*e*f*","exfx")!=1) printf("wm9.8 '*e*f*' matches exfx\n");  
  if (isWildMatch("*e*f*","ff ee ff ee")!=1)
    printf("wm9.9 '*e*f*' mathches ff ee ff ee\n");  

  if (isWildMatch("*xy*yz*","axayaxyayza") != 1)
    printf("wm10.0 complex false start match\n");

  /* those which should not match */

  if (isWildMatch("xx","xy")!=0) printf("wm11.0 xx doesn't match xy\n");    

  if (isWildMatch("*x","xy")!=0) printf("wm12.0 *x fails on xy\n");
  if (isWildMatch("*x","")!=0) printf("wm12.1 *x fails on \"\"\n");
  if (isWildMatch("*x","yy")!=0) printf("wm12.2 *x fails on yy\n");
  if (isWildMatch("*x","yxy")!=0) printf("wm12.3 *x fails on yxy\n");  
      
  if (isWildMatch("z*","yz")!=0) printf("wm13.0 z* fails on yz\n");
  if (isWildMatch("z*","y")!=0) printf("wm13.1 z* fails on y\n");
  if (isWildMatch("z*","yzy")!=0) printf("wm13.2 z* fails on yzy\n");
 
  if (isWildMatch("o*o","o")!=0) printf("wm14.0 o*o fails on o\n");
  if (isWildMatch("o*o","oop")!=0) printf("wm14.1 o*o fails on oop\n");
  if (isWildMatch("o*o","poo")!=0) printf("wm14.2 o*o fails on poo\n");
  if (isWildMatch("o*o","opop")!=0) printf("wm14.3 o*o fails on opop\n");
  if (isWildMatch("o*o","popo")!=0) printf("wm14.4 o*o fails on popo\n");  

  if (isWildMatch("*m*","nope")!=0) printf("wm15.0 *m* fails on nope\n");
  if (isWildMatch("*m*","")!=0)  printf("wm15.1 *m* fails on \"\" \n");

  if (isWildMatch("u*v*","vu") == 1) printf("wm16.0 u*v* fails on vu\n");
  if (isWildMatch("u*v*","uu") == 1) printf("wm16.1 u*v* fails on uu\n");
  if (isWildMatch("u*v*","vv") == 1) printf("wm16.2 u*v* fails on vv\n");
  if (isWildMatch("u*v*","") == 1) printf("wm16.3 u*v* fails on \"\"\n");

  if (isWildMatch("*f*g","f") == 1) printf("wm17.0 *f*g fails on f\n");
  if (isWildMatch("*f*g","g") == 1) printf("wm17.1 *f*g fails on g\n");
  if (isWildMatch("*f*g","fgf") == 1) printf("wm17.2 *f*g fails on fgf\n");
  if (isWildMatch("*f*g","gf") == 1) printf("wm17.3 *f*g fails on gf\n");

  if (isWildMatch("*p*q*","qp") == 1) printf("wm18.0 *p*q* fails on qp\n");
  if (isWildMatch("*p*q*","p") == 1) printf("wm18.1 *p*q* fails on p\n");
  if (isWildMatch("*p*q*","q") == 1) printf("wm18.2 *p*q* fails on q\n");
  if (isWildMatch("*p*q*","qqqp") == 1) printf("wm18.3 *p*q* fails on qqqp\n");

  if (isWildMatch("*xy*yz*","axayaxyaz") == 1)
    printf("wm19.0 complex query almost but not quite matches\n");

  /* syntax error */

  if (isWildMatch("boo**","booxxx") != SLP_PARSE_ERROR)
    printf("wm20.0 Warning: double wildcard error not found\n");
}

SLPInternalError qtest(SAStore *ps, const char *pcSAScopeList,
               const char *pcRqstScopeList, const char *pcSrvtype,
               const char *pcLangTag, const char *pcQuery,
               Mask **ppMask) {
  return handle_query(ps,pcSAScopeList,
		      pcRqstScopeList,pcSrvtype,pcLangTag,pcQuery,ppMask);
}

void test_query_features() {
  
  /* test some internal stuff in this file, like onPRList */
  
  /* test PRList */
  SAState st;
  st.pcSAHost = "foo";
  st.pcSANumAddr = "128.127.106.34";
  
  /* tpr1.1 NULL prlist */
  if (on_PRList(&st, NULL)==1)
    printf("tpr1.1.1 null PR list should never be 1\n");
  if (on_PRList(&st, NULL)!=0)
    printf("tpr1.1.2 incorrect 'not on list' return value\n");
  
  /* tpr1.2 prlist does not have value */
  if (on_PRList(&st, "snort,feng,34.23.23.23") == 1)
    printf("tpr1.2.1 different PR list should not match\n");
  if (on_PRList(&st, "snort,feng,34.23.23.23") != 0)
    printf("tpr1.2.2 incorrect 'not on list' return value\n");
  
  /* tpr1.3 prlist of 1 has host name */
  if (on_PRList(&st, "foo") == 0)
    printf("tpr1.3.1 host name as only elt of PR list failed to match\n");
  if (on_PRList(&st, "foo") != 1)
    printf("tpr1.3.2 incorrect 'on list' return value\n");
  
  /* tpr1.4 prlist, long, has host name first */
  if (on_PRList(&st, "foo,fem,33.5.2.3,snort,gloot,99.33.22.11,geeb,zogo")!= 1)
    printf("tpr1.4 host name first elt of PR list failed to match\n");
  
  /* tpr1.5 prlist, long, has host name middle */  
  if (on_PRList(&st, "fem,sno,33.5.2.3,gloot,foo,geeb,99.33.22.11,swinger")!=1)
    printf("tpr1.5 host name middle elt of PR list failed to match\n");
  
  /* tpr1.6 prlist, long, has host name end */  
  if (on_PRList(&st, "fem,sno,33.5.2.3,gloot,geeb,zogo,99.33.22.11,foo") != 1)
    printf("tpr1.6 host name last elt of PR list failed to match\n");
  
  /* tpr1.7 prlist of 1 has numerical address */
  if (on_PRList(&st, "128.127.106.34")!=1)
    printf("tpr1.7 addr as sole elt of PR list failed to match\n");
  
  /* tpr1.8 prlist, long, has numerical address at beginning */
  if (on_PRList(&st,
    "128.127.106.34,foo,fem,33.5.2.3,ot,99.33.22.11,ge,zoo")!= 1)
    printf("tpr1.8 addr as first elt of PR list failed to match\n");
  
  /* tpr1.9 prlist, long, has numerical address at middle */
  if (on_PRList(&st,
    "fem,sno,33.5.2.3,gloot,128.127.106.34,foo,geeb,99.33.22.11,swinger")!=1)
    printf("tpr1.9 addr as middle elt of PR list failed to match\n");
  
  /* tpr1.10 prlist, long, has numerical address at end */
  if (on_PRList(&st, 
    "fem,sno,33.5.2.3,gloot,geeb,zogo,99.33.22.11,foo,128.127.106.34") != 1)
    printf("tpr1.10 addr as last elt of PR list failed to match\n");
  
  
}

#endif
