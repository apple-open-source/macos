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
 * mslpd_opt.c : Handles optional requests (ATTRRQST and SRVTYPERQST)
 *           coming in to the mini SLPv2 SA.
 *
 * Version: 1.3
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

#ifdef EXTRA_MSGS


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
#include "mslpd_mask.h"  /* for the Mask definition used in mslpd_parse.h */
#include "mslpd_parse.h" /* for the srvrqst_in parse function */

static SLPInternalError optrply_out(Slphdr *pslphdr, int replytype, SLPInternalError err,
			    char **ppcOutBuf, int *piOutSz,
			    const char *pcResult, int extra);
static int tag_list_includes(const char *pcTagList, const char *pcTag);
/*
 * srvtyperqst_in
 *
 */
static SLPInternalError srvtyperqst_in(Slphdr *pslph, const char *pcInBuf, int iInSz,
			       char **ppcPRL, char **ppcNA, char **ppcSL) {
  SLPInternalError err;
  int offset = HDRLEN + strlen(pslph->h_pcLangTag); 
  int iNALen = 0;
  *ppcPRL = NULL;
  *ppcNA  = NULL;
  *ppcSL  = NULL;

  if ((err = get_string(pcInBuf, iInSz, &offset, ppcPRL)) < 0) 
    goto srvtyperqst_in_fail;
  if ((err = get_sht(pcInBuf, iInSz, &offset, &iNALen)) < 0)
    goto srvtyperqst_in_fail;
  if (iNALen == 0xffff) {
    /* handle the case where there is a wild NA */
    *ppcNA = safe_malloc(2,"*",1);
    if( !*ppcNA ) return SLP_MEMORY_ALLOC_FAILED;
  } else {
    offset -= 2; /* move back before the string length */
    if ((err = get_string(pcInBuf, iInSz, &offset, ppcNA)) < 0) 
      goto srvtyperqst_in_fail;
  }
  if ((err = get_string(pcInBuf, iInSz, &offset, ppcSL)) < 0) 
    goto srvtyperqst_in_fail;
  /* ignore options */
  return SLP_OK;
  
srvtyperqst_in_fail:
  SLPFree(*ppcPRL); *ppcPRL = NULL;
  SLPFree(*ppcNA);  *ppcNA  = NULL;
  SLPFree(*ppcSL);  *ppcSL  = NULL;  
  return err;
}


/*
 * opt_type_request
 *
 *   This routine takes a Service Type Request and returns a buffer with 
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
 *   In the case of a fatal error, a negative number is returned
 *   and buffers are cleaned up.  Note that this negative number
 *   corresponds to the error code.  This helps for unit testing
 *   but is not used by the mslpd as it simply needs to know whether
 *   to use the buffer or not.
 *
 * Side Effects:
 *
 *   The reply buffer is allocated here and must be SLPFreed by the caller.
 */
int opt_type_request(SAState *psa, Slphdr *pslphdr, const char *pcInBuf,
		     int iInSz, char **ppcOutBuf, int *piOutSz,int *piGot) {
  
  char *pcList = safe_malloc(1,0,0); /* the dynamic result list */  
  char *pcPRList=NULL, *pcNA = NULL, *pcSList = NULL;
  const char *pcSrvtypes = SLPGetProperty("com.sun.slp.saSrvTypes");
  int err = 0;
  if( !pcList || !psa || !pslphdr || !pcInBuf || !ppcOutBuf || !piOutSz) return SLP_PARSE_ERROR;
  *ppcOutBuf = NULL;  /* for now, just drop the request! */
  *piOutSz   = 0;
  *piGot     = 0;

  if (pcSrvtypes == NULL) {
    pcSrvtypes = ""; /* If the SA is not advertising anything, advertise "" */
  }
  
  if ((err=srvtyperqst_in(pslphdr,pcInBuf,iInSz,&pcPRList,&pcNA,&pcSList))<0){
    
    SLP_LOG( SLP_LOG_DROP,"opt_type_request: drop request due to parse in error");

    /* return the error in a srvtype reply message */
    if ((err = optrply_out(pslphdr,SRVTYPERPLY,SLP_PARSE_ERROR,
			   ppcOutBuf,piOutSz,"", 0)) != SLP_OK) {
      LOG(SLP_LOG_ERR,"opt_type_request: could not serialize parse error");
    }
    
    err = SLP_PARSE_ERROR;

  } else if (on_PRList(psa,pcPRList)) {
    
    SLP_LOG( SLP_LOG_DROP,"opt_type_request: drop request which is on my PRList");
    SLPFree(pcList);
    SLPFree(pcPRList);
    SLPFree(pcSList);
    SLPFree(pcNA);
    return SLP_OK;    

  } else if (!pcPRList || !pcNA || !pcSList ) {

    SLP_LOG( SLP_LOG_DROP,"opt_type_request: request is malformed - missing values");

    /* return the error in a srvtype reply message */
    if ((err = optrply_out(pslphdr,SRVTYPERPLY,SLP_PARSE_ERROR,
			   ppcOutBuf,piOutSz,"", 0)) != SLP_OK) {
      LOG(SLP_LOG_ERR,"opt_type_request: could not serialize missing val error");
    }

    err = SLP_PARSE_ERROR;    
    
  } else if (!list_intersection(pcSList,SLPGetProperty("net.slp.useScopes"))){

    SLP_LOG( SLP_LOG_DROP,"opt_type_request: drop request not on my scope list");

    /* return the error in a srvtype reply message */
    if ((err = optrply_out(pslphdr,SRVTYPERPLY,SLP_SCOPE_NOT_SUPPORTED,
			   ppcOutBuf,piOutSz,"", 0)) != SLP_OK) {
      LOG(SLP_LOG_ERR,"opt_type_request: could not serialize scope supp. error");
    }

    err = SLP_SCOPE_NOT_SUPPORTED;
    
  } else { /* calculate which result */

    int  iListLen = 0;                 /* the dynamic result length */
    char *pcSrvtype;                   /* each srvtype */
    char *pcNAVal;                     /* the NA of each advertised service */
    int loop;   
    
    /* get each service type in the list */
    for (loop = 0; loop < psa->store.size; loop++) {

      if (list_intersection(pcSList,psa->store.scope[loop]) == 0) {
	continue; /* the scope in the service type request doesn't
		     match the service table entry. */
      }

      pcSrvtype = psa->store.srvtype[loop];
      /* get the naming authority string */
      pcNAVal = strchr(pcSrvtype,(int) '.');

      if (pcNA[0] == '*') {

	/* wild card naming authority in request */
	list_merge(pcSrvtype,&pcList,&iListLen,CHECK);
	*piGot += 1; /* not necessarily unique, but count anyway */
	
      } else if (pcNAVal != NULL) {
	
	/*
	 * There is a naming authority string:  Check to see it matches.
	 * The matching is a little tricky since we have to be sure that
	 * the naming authority matches the request and that it ends either
	 * a concrete or abstract service type definition. 'na' does not
	 * match service:foo.nambo:// but it does match service:foo.na://
	 */
	pcNAVal++; /* advance to the naming authority itself */
	if (!SDstrncasecmp(pcNAVal, pcNA,strlen(pcNA)) &&
	    (strlen(pcNAVal) == strlen(pcNA) ||
	     pcNAVal[strlen(pcNA)] == ':')) {
	  list_merge(pcSrvtype,&pcList,&iListLen,CHECK);
	  *piGot += 1; /* not necessarily unique, but count anyway */
	}
	
      } else {
	
	/* There is no NA string:  Is this true in the request too? */
	if (pcNA[0] == '\0') {
	  list_merge(pcSrvtype,&pcList,&iListLen,CHECK);
	  *piGot += 1; /* not necessarily unique, but count anyway */
	}
      }
    }

    /* return the list of service types in a srvtype reply message */
    err = optrply_out(pslphdr, SRVTYPERPLY, (SLPInternalError) err,
		    ppcOutBuf, piOutSz, pcList, 0);
  }
    
  
  SLPFree(pcList);
  SLPFree(pcPRList);
  SLPFree(pcSList);
  SLPFree(pcNA);

  /* This routine returns either (0) success or (-1) failure:  This
     indicates whether a reply buffer has been created or not. */
  if (err == 0) return 0;
  else return (int) err;
}

/*
 * opt_attr_request
 *
 *   This routine takes an Attribute Request and returns a buffer with 
 *   a reply (or drops the request).  If the request is dropped,
 *   the return buffer is set to NULL and the output size is 0.
 *   Otherwise a full reply will be constructed in the output buffer.
 *
 *   MslpHashtable and mslp_list functions are used to create a duplicate
 *   suppressed set of attributes and values.
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
 *   If at all possible, a result buffer (possibly error) is returned.
 *   In the case of a fatal error, -1 is returned and buffers are cleaned up.
 *
 * Side Effects:
 *
 *   The reply buffer is allocated here and must be SLPFreed by the caller.
 */
int opt_attr_request(SAState *psa, Slphdr *pslphdr, const char *pcInBuf,
		     int iInSz, char **ppcOutBuf, int *piOutSz, int *piGot) {
  
  char *pcPRList=NULL, *pcSrv = NULL, *pcSList = NULL, *pcTagList = NULL;
  SLPInternalError err = SLP_OK;
  int result = 0;
  if (!psa || !pslphdr || !pcInBuf || !ppcOutBuf || !piOutSz) return SLP_PARSE_ERROR;
  *ppcOutBuf = NULL;  /* for now, just drop the request! */
  *piOutSz   = 0;
  *piGot     = 0;

  
  /* note that srvrqst_in may be used for AttrRqst as well as it is
     formatted identically. */
  if ((err = srvrqst_in(pslphdr,pcInBuf,iInSz,&pcPRList,
    &pcSrv,&pcSList,&pcTagList))<0){

    SLP_LOG( SLP_LOG_DROP,"opt_attr_request: drop request due to parse in error");
    return SLP_OK;

  } else if (on_PRList(psa,pcPRList)) {
    
    SLP_LOG( SLP_LOG_DROP,"opt_attr_request: drop request which is on my PRList");
    return SLP_OK;
    
  }  if (!list_intersection(pcSList,SLPGetProperty("net.slp.useScopes"))){

    SLP_LOG( SLP_LOG_DROP,"opt_attr_request: drop request not on my scope list");
    return SLP_OK;
    
  } else {
    
    MslpHashtable *pmht = mslp_hash_init();
    MslpHashbucket *pb;
    char *pcResult = safe_malloc(1,0,0);
    int   iSz = 0;
    int i, j;
    int iRetval;
 
    if( !pcResult ) return SLP_MEMORY_ALLOC_FAILED;
    
    for (i = 0; (err == SLP_OK) && (i < psa->store.size); i++) {

      /* does the request language fail to match the service's language? */
      if ((iRetval=match_langtag(psa->store.lang[i],pslphdr->h_pcLangTag))==0) {
        continue;  /* does not match */
      } else if (iRetval != 1) {
        mslplog(SLP_LOG_DROP,"opt_attr_request: bad language in request", 
                pslphdr->h_pcLangTag);
        continue;
      } 
      
      /* does the request fail to match the service's scope list? */
      if (!list_intersection(psa->store.scope[i],pcSList)) {
	continue;
      }

      /* does the request fail to match the service's URL or service type? */
      if ((iRetval=match_srvtype(pcSrv,psa->store.url[i]))==0) {
        continue;  /* does not match */
      } else if (iRetval != 1) {
        mslplog(SLP_LOG_DROP,"opt_attr_request: bad servtype in request",pcSrv);
        continue;
      }
      
      for (j = 0; (err == SLP_OK) && (psa->store.tag[i][j]); j++) {

	char *pcVals;
	const char *pcValList;
	int returnval = tag_list_includes(pcTagList,psa->store.tag[i][j]);
	
	if (pcTagList[0] != '\0' && returnval == 0) {
	  continue; /* the attribute was not requested */
	} else if (returnval != SLP_OK && returnval != 1) {
	  /* an error condition */
	  SLP_LOG( SLP_LOG_DEBUG,"opt_attr_request: could not compare tag lists");
	  err = (SLPInternalError) returnval;
	  break;
	} 
	  
	if ((pcVals = serialize_values(&(psa->store),i,j)) == NULL) {
	  continue; /* no values in current item */
	}
	
	if ((pcValList = mslp_hash_find(pmht, psa->store.tag[i][j])) == NULL) {

	  /* this is the first attr */
	  mslp_hash_add(pmht, psa->store.tag[i][j], pcVals);
	  
	} else {
	  int   iValSz  = strlen(pcVals);
	  list_merge(pcValList,&pcVals,&iValSz,CHECK);
	  mslp_hash_add(pmht, psa->store.tag[i][j], pcVals);
	}
	
	SLPFree(pcVals); /* this is copied into the hash table by now */
	
      }
    }

    /*
     * Iterate over each bucket in the hash table and create a list of all
     * attributes for delivery.
     */
    for (result = 0 ; (err == 0) && (result < NUMBUCKETS); result++) {
      char *pcItem;
      pb = pmht->bucket[result];      
      while (pb) {
	
	if (pb->pcVal[0] == '\0') { /* keyword */
	  
	  list_merge(pb->pcKey, &pcResult, &iSz, NO_CHECK);
	  *piGot += 1;
	  
	} else { /* attributes with values */
	  
	  pcItem = safe_malloc(strlen(pb->pcKey) +strlen(pb->pcVal) +4, 0, 0);
      if( !pcItem ) return SLP_PARSE_ERROR;
      
	  sprintf(pcItem,"(%s=%s)",pb->pcKey,pb->pcVal);
	  list_merge(pcItem, &pcResult, &iSz, NO_CHECK);
	  SLPFree(pcItem);
	  *piGot += 1;
	  
	}
	
	pb = pb->pBucketNext;
      }
    }
    
    err = optrply_out(pslphdr, ATTRRPLY, (SLPInternalError) err,
		      ppcOutBuf, piOutSz, pcResult, 1);
    SLPFree(pcResult);
  }
  
  /* This routine returns only success (0) or failure (-1) indicating
     whether a reply buffer has been created. */
  if (err == 0) return 0;
  else return -1;
}

static SLPInternalError optrply_out(Slphdr *pslphdr, int replytype, SLPInternalError err,
			    char **ppcOutBuf, int *piOutSz,
			    const char *pcResult, int extra) {
  
  char*	endPtr = NULL;
  int iMTU      = (SLPGetProperty("net.slp.MTU"))?strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10):1400;
  int iOverflow = 0;
  int iIsMcast  = (pslphdr->h_usFlags & MCASTFLAG)?1:0;
  int hdrsz     = HDRLEN + strlen(pslphdr->h_pcLangTag);
  int offset    = 0;
  *piOutSz      = hdrsz + 2 +  /* error code */
                  strlen(pcResult) + 2 + extra + /* extra allows 0 auth */
                  1;    /* The extra one byte is a measure of safety
			   and is required by string out parsing which
			   does >= buffer size testing, not just > "    */
  
  /* in the case of overflow in a unicast request, send 0 results & flag */
  if (iIsMcast && *piOutSz >= iMTU) { 
    *piOutSz = hdrsz + 2;
    iOverflow = 1;
  } 
  *ppcOutBuf = safe_malloc(*piOutSz, 0, 0);  
  if( !*ppcOutBuf ) return SLP_PARSE_ERROR;
  
  /* parse header out */
  SETVER(*ppcOutBuf,2);
  SETFUN(*ppcOutBuf,replytype);
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

  if ((err=add_string(*ppcOutBuf,*piOutSz,pcResult,&offset))!=SLP_OK) {
    SLP_LOG( SLP_LOG_DEBUG,"optrply_out: could not add_string, should never happen!");
    offset = hdrsz;
    SETSHT(*ppcOutBuf,api2slp(SLP_PARSE_ERROR),offset);
    *piOutSz = hdrsz+2;
    SETLEN(*ppcOutBuf,*piOutSz);
    return SLP_PARSE_ERROR;
  }

  return SLP_OK;
}

/*
 * tag_list_includes
 *
 *   This routine makes use of the isWildMatch routine in order to determine
 *   if the query matches the string.
 *
 *   The tag list is decomposed, term by term, and a match against the tag
 *   of an individual attribute is attempted for each one.
 *
 *     pcTagList     A tag list of items which each can contain wildcards.
 *     pcTag         The tag of an individual attribute.
 *
 * Returns:
 *   1 if the tag (pcTag) matches the constraints of the tag list (pcTagList).
 *   0 if the tag does not match the tag list.
 *   Negative number (SLP_PARSE_ERROR) for error conditions.
 *
 * Side Effects: 
 *   None.
 */
static int tag_list_includes(const char *pcTagList, const char *pcTag) {
  
  char *pcTagQuery;
  char cDelim;
  int offset = 0;
  int result = 0;
  SLPInternalError err = SLP_OK;
  int iErr = 0;

  if (!pcTagList || !pcTag) return SLP_PARSE_ERROR;

  /*
   * Special case: Return true if both tag list and tags are empty.
   */
  if (pcTagList[0] == '\0' && pcTag[0] == '\0') return 1;
  
  while ((pcTagQuery= get_next_string(",",pcTagList,&offset,&cDelim)) != NULL){
    if ((iErr = isWildMatch(pcTagQuery,pcTag)) != 0) {

      if (iErr < 0) {
	err = (SLPInternalError) iErr;
	result = 0;
      } else {
	result = 1;
      }
      
      break;
    }
    SLPFree(pcTagQuery);
    pcTagQuery = NULL; /* prevent double freeing when loop exits */
  }
  SLPFree(pcTagQuery); /* free the tag for correct results, etc. */
  if (err != SLP_OK) return err; /* an error takes priority over result */
  return result;
}

#ifndef NDEBUG

int opt_tag_list_includes(const char *pc1, const char *pc2) {
  return tag_list_includes(pc1,pc2);
}

SLPInternalError opt_optrply_out(Slphdr *pslphdr, int replytype, 
			 SLPInternalError err, char **ppcOutBuf,
			 int *piOutSz, const char *pcResult, int extra) {
  
  return optrply_out(pslphdr,replytype,err,
		     ppcOutBuf,piOutSz,pcResult,extra);
}
#endif /* NDEBUG */

#endif /* EXTRA_MSGS */
