/*
 * mslplib_opt.c : Optional feature parsing and API handling functions.
 *
 * These definitions are used by portions of the implementation which
 * provide over-the-minimal features (service type and attribute request).
 *
 * Version:  1.3
 * Date:     10/05/99
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

#ifdef EXTRA_MSGS


#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslp_dat.h"
#include "mslplib.h"
#include "mslplib_opt.h"

/*
 * generate_srvtyperqst
 *
 */
TESTEXPORT SLPInternalError 
generate_srvtyperqst(char *pcSendBuf, int *piSendSz,
  const char *pcLangTag, const char *pcNamingAuth, const char *pcScope) {

  int offset = 0;
  SLPInternalError err;
  int iMTU = *piSendSz;

  if (!pcNamingAuth || !pcScope) {
    return SLP_PARAMETER_BAD;
  }

  /* If the NA string isn't a wildcard, it has to abide by the URL
     escape conventions. */
  if (!(strlen(pcNamingAuth) == 1 && pcNamingAuth[0] == '*') &&
      (err = isURLEscapedOK(pcNamingAuth)) != SLP_OK) 
  {
    LOG_SLP_ERROR_AND_RETURN( SLP_LOG_ERR,"generate_srvtyperqst: bad naming auth string",err);
  }
  
  if ((err = add_header(pcLangTag,pcSendBuf,iMTU,SRVTYPERQST,0,&offset))
    != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_srvtyperqst: could not add header",err);
  }
  
  offset += 2; /* PR list is 0 initially, leave 2 0s here */

  if (pcNamingAuth[0] == '*' && pcNamingAuth[1] == '\0') {
    if ((err = add_sht(pcSendBuf,iMTU,0xffff,&offset)) != SLP_OK) {
      LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_srvtyperqst: couldn't add wildcard NA",err);
    }
  } else if ((err = add_string(pcSendBuf,iMTU,pcNamingAuth,&offset))
	     != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_srvtyperqst: couldn't add naming auth",err);
  }
  
  if ((err = add_string(pcSendBuf,iMTU,pcScope,&offset)) != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_srvtyperqst: could not add scope list",err);
  }
  
  SETLEN(pcSendBuf,offset);
  *piSendSz = offset;
  return SLP_OK;
}

/*
 * generate_attrrqst
 *
 */
TESTEXPORT SLPInternalError 
generate_attrrqst(char *pcSendBuf, int *piSendSz,
	const char *pcLangTag, const char *pcService,
	const char *pcScope, const char *pcTagList) {

  int offset = 0;
  SLPInternalError err;
  int iMTU = *piSendSz;

  if (!pcScope || !pcLangTag || !pcService || !pcSendBuf || !piSendSz) {
    return SLP_PARAMETER_BAD;
  }

  (void) memset(pcSendBuf, 0, *piSendSz);
  
  if ((err = add_header(pcLangTag,pcSendBuf,iMTU,ATTRRQST,0,&offset))
    != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_attrrqst: could not add header",err);
  }
  
  offset += 2; /* PR list is 0 initially, leave 2 0s here */

  if ((err = add_string(pcSendBuf,iMTU,pcService,&offset)) != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_attrrqst: could not add url or type",err); 
  }
  
  if ((err = add_string(pcSendBuf,iMTU,pcScope,&offset)) != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_attrrqst: could not add scopes",err);    
  }
  
  if ((err = add_string(pcSendBuf,iMTU,pcTagList,&offset)) != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_attrrqst: could not add tag list",err);    
  }

  if ((err = add_sht(pcSendBuf, iMTU, 0, &offset)) != SLP_OK) {
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_ERR,"generate_attrrqst: could not add 0 SLP SPI len", err);
  }
  
  SETLEN(pcSendBuf,offset);
  *piSendSz = offset;
  return SLP_OK;
}

#else

/* nothing to do */

#endif /* EXTRA_MSGS */
