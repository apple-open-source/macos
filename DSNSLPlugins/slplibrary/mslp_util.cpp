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
 * mslp_util.c : Utilities used by the minimal SLP implementation.
 *
 * Version: 1.13
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
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <syslog.h>
#include <sys/un.h>
#include <pthread.h>	// for pthread_*_t
#include <unistd.h>		// for _POSIX_THREADS

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"

/* functions local to this source module */
static int issep(char c, const char *pcSeps);

int	sysLogInitialized = 0;
int	gethostbynameLockInitialized = 0;
pthread_mutex_t	sysLogLock;
pthread_mutex_t	gethostbynameLock;
pthread_mutex_t	gMallocLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	gStrCatLock = PTHREAD_MUTEX_INITIALIZER;

EXPORT void slp_strcat( char* targetStr, const char* strToAppend )
{
    pthread_mutex_lock(&gStrCatLock);
	strcat( targetStr, strToAppend );
    pthread_mutex_unlock(&gStrCatLock);
}

EXPORT SLPReturnError InternalToReturnError( SLPInternalError iErr )
{
    SLPReturnError	returnErr;
    
    switch (iErr)
    {
        case SLP_OK:
            returnErr = NO_ERROR;
        break;

        case SLP_LANGUAGE_NOT_SUPPORTED:
            returnErr = LANGUAGE_NOT_SUPPORTED;
        break;

        case SLP_PARSE_ERROR:
            returnErr = PARSE_ERROR;
        break;

        case SLP_INVALID_REGISTRATION:
            returnErr = INVALID_REGISTRATION;
        break;

        case SLP_SCOPE_NOT_SUPPORTED:
            returnErr = SCOPE_NOT_SUPPORTED;
        break;

        case SLP_AUTHENTICATION_ABSENT:
            returnErr = AUTHENTICATION_ABSENT;
        break;

        case SLP_AUTHENTICATION_FAILED:
            returnErr = AUTHENTICATION_FAILED;
        break;

        default:
            returnErr = INTERNAL_ERROR;
        break;
    }
    
    return returnErr;
}

/* safe_malloc
 * 
 * This is a safe way to allocate and copy buffers in that it will not
 * need checking afterwards and the size is checked before the copy.
 * 
 * size      - number of bytes to allocate
 * pcCpyInto - if not null, copy into buffer
 * iCpyLen   - the length of pcCpyInto bytes to copy into the new buffer
 *
 * returns the new buffer or NULL if size is <= 0
 * will FAIL if memory cannot be allocated.
 */
EXPORT char * safe_malloc(int size, const char *pcCpyInto, int iCpyLen)
{
    char *pcNewBuf;
    
    if (size <= 0)
    {
        SLP_LOG( SLP_LOG_ERR,"SAFE_MALLOC got size <= 0, return NULL");
        return NULL;
    }
    
    if ((unsigned int)size > MAX_REPLY_LENGTH)
    {
        SLP_LOG( SLP_LOG_ERR,"SAFE_MALLOC got size too large (%ld), return NULL", size );
        return NULL;
    }
    
    if (size < iCpyLen)
    {
        SLP_LOG(  SLP_LOG_ERR, "Could not copy into smaller buffer" );
        return NULL;
    }
  
    pthread_mutex_lock(&gMallocLock);

    pcNewBuf = (char*) calloc(1,size);
    if (!pcNewBuf)
    {
        SLP_LOG( SLP_LOG_ERR, "Could not allocate memory" );
        pthread_mutex_unlock(&gMallocLock);
        return NULL;
    }

    if (pcCpyInto && iCpyLen > 0) 
        memcpy(pcNewBuf, pcCpyInto, iCpyLen);
    
    if (!pcCpyInto && iCpyLen > 0) 
    {
        SLP_LOG( SLP_LOG_ERR,"safe_malloc: could not copy from NULL buffer");
    }
    
    pthread_mutex_unlock(&gMallocLock);

    return pcNewBuf;
}

/***************
 * IsIPAddress *
 ***************
 
 Verifies a CString is a legal dotted-quad format. If it fails, it returns the 
 partial IP address that was collected.
 
*/

int IsIPAddress(const char* adrsStr, long *ipAdrs)
{
	short	i,accum,numOctets,lastDotPos;
	long	tempAdrs;
	register char	c;
	char	localCopy[20];					// local copy of the adrsStr
	
	strncpy(localCopy, adrsStr,sizeof(localCopy)-1);
	*ipAdrs = tempAdrs = 0;
	numOctets = 1;
	accum = 0;
	lastDotPos = -1;
	for (i = 0; localCopy[i] != 0; i++)	{	// loop 'til it hits the NUL
		c = localCopy[i];					// pulled this out of the comparison part of the for so that it is more obvious
		if (c == '.')	{
			if (i - lastDotPos <= 1)	return 0;	// no digits
			if (accum > 255) 			return 0;	// only 8 bits, guys
			*ipAdrs = tempAdrs = (tempAdrs<<8) + accum; // copy back result so far
			accum = 0; 
			lastDotPos = i;							
			numOctets++;								// bump octet counter
		}
		else if ((c >= '0') && (c <= '9'))	{
			accum = accum * 10 + (c - '0');				// [0-9] is OK
		}
		else return 0;								// bogus character
	}
	
	if (accum > 255) return 0;						// if not too big...
	tempAdrs = (tempAdrs<<8) + accum;					// add in the last byte
	*ipAdrs = tempAdrs;									// return real IP adrs

	if (numOctets != 4)									// if wrong count
		return 0;									// 	return FALSE;
	else if (i-lastDotPos <= 1)							// if no last byte
		return 0;									//  return FALSE
	else	{											// if four bytes
		return 1;									// say it worked
	}
}

/*
 * get_in_addr_by_name
 */
struct in_addr get_in_addr_by_name(const char* pcAddr) 
{
    struct	hostent *phe;
    struct	in_addr ina;
    long	addr;
    memset(&ina, 0, sizeof ina);

    if ( !IsIPAddress( pcAddr, &addr ) )	// is the address a quad dotted string format
    {
        if ( !gethostbynameLockInitialized )
        {
            pthread_mutex_init( &gethostbynameLock, NULL );
            gethostbynameLockInitialized = 1;
        }
        
        pthread_mutex_lock( &gethostbynameLock );
        phe = gethostbyname(pcAddr);
        pthread_mutex_unlock( &gethostbynameLock );
        
        if (phe == NULL) 
        {
            mslplog(SLP_LOG_DEBUG,"get_in_addr_by_name: could not get host by name",pcAddr);
            return ina;
        }
        
        ina = * (struct in_addr *) phe->h_addr_list[0];
    }
    else
        ina.s_addr = htonl( addr );
        
    return ina;
}

/* get_sin_from_url
 *
 * Translates a url into a sockaddr, stripping out the address
 * and the port number.
 *
 * If the address cannot be stripped for any reason, the return
 * value is left unset: 0.0.0.0, port 0, etc.
 *
 * The port number for service location entities is assigned.
 *
 * TO DO LATER: if there is no port number given, try the service
 * type string, using getservicebyname.  This will return a servent
 * with an assigned port number.  Assign the port # to this value.
 */
EXPORT SLPInternalError get_sin_from_url(const char *pcURL, int iLen,
				 struct sockaddr_in *psin) {


  char *pcBase, *pc;
  const char *pcAddr = NULL;
  char *pcPort = NULL;

  if (!pcURL || !psin || iLen <= 0) return SLP_PARAMETER_BAD;
  
  memset(psin, 0, sizeof(struct sockaddr_in));

  if (    SDstrncasecmp(pcURL,"service:directory-agent:",
                        strlen("service:directory-agent:"))==0 
       || SDstrncasecmp(pcURL,"service:service-agent:",
                        strlen("service:directory-agent:"))==0) {
    psin->sin_port = htons(SLP_PORT);
  }

  pcBase = safe_malloc(strlen(pcURL)+1,pcURL,strlen(pcURL));
  pc = pcBase;
  while(--iLen> 0 && *pc && *pc != '/') pc++; /* skip up to the first / */
  if (pc == NULL || --iLen < 0) {
    free(pcBase);
    return SLP_PARSE_ERROR;
  }
  pc++; /* skip past the / */
  if (*pc != '/' || --iLen < 0) {
    free(pcBase);
    return SLP_PARSE_ERROR;
  }
  pc++; /* skip past the / */
  
  pcAddr = pc; /* we now have the beginning of the address */
  while( --iLen >= 0 && *pc && (*pc != '/') && (*pc != ';') && (*pc != ':')) pc++;

  psin->sin_family = AF_INET;  
  if (*pc == '/' || *pc == ';') {
    
    psin->sin_addr = get_in_addr_by_name(pcAddr);

  } else if (*pc == '\0' || iLen < 0) {
   
    psin->sin_addr = get_in_addr_by_name(pcAddr);

  } else if (*pc == ':') { /* there is a port number, get it */

    *pc = '\0';   
    psin->sin_addr = get_in_addr_by_name(pcAddr);

    if (pc == NULL || --iLen < 0) {
      free(pcBase);
      return SLP_PARSE_ERROR;	
    }
    pc++;
    pcPort = pc;
    while( --iLen >= 0 && *pc && *pc != '/' && *pc != ';') pc++;
    if (pc != pcPort)
	{
      char*	endPtr = NULL;
	  psin->sin_port = (unsigned short)(0xFFFF & strtol(pcPort,&endPtr,10));
	}
  } else {
    free(pcBase);
    LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"could not parse URL", SLP_PARSE_ERROR);
  }
  free(pcBase);
  return SLP_OK;
}

SLPInternalError add_header(const char *pcLangTag, char *pcSendBuf, int iSendBufSz,
	       int iFun, int iLen, int *piLen) 
{
  int iLangLen;
  static unsigned short xid = 0;
  if (!pcLangTag || strlen(pcLangTag) < 2 || !pcSendBuf || !piLen) {
    return SLP_PARAMETER_BAD;
  }

  if (xid == 0) {
    srand( (unsigned) time(NULL) ); 
    xid = (unsigned short) (0xFFFF & rand());
  }

  iLangLen = strlen(pcLangTag);
  if ((HDRLEN + *piLen + iLangLen) > iSendBufSz) 
    return SLP_BUFFER_OVERFLOW;
  memset(pcSendBuf, 0, HDRLEN+iLangLen);

  SETVER(pcSendBuf,SLP_VER);
  SETFUN(pcSendBuf,iFun);
  SETLEN(pcSendBuf,iLen);
  SETXID(pcSendBuf,xid);
  SETLANG(pcSendBuf,pcLangTag);
  xid++;
  *piLen = HDRLEN + iLangLen; 
  return SLP_OK;

}

/*
 * get_header
 *
 *   Reads in the values and puts them into the pslphdr provided.   
 *
 *   Note:  Be careful to keep fields NULL till filled so that when
 *     the resources are freed they will only free valid values.
 *     There are several 'exception' exits to this routine.  The
 *     caller owns the memory (the h_pcLangTag).
 *
 * Returns:  SLPInternalError (buffer too small, etc.)
 */
SLPInternalError get_header(const char *pcSendBuf, const char *pcRecvBuf, int len, Slphdr *pslphdr, int *piLen) 
{
    int iLangLen=0;
    
    memset(pslphdr,0,sizeof(Slphdr)); /* clear all fields */
    
    if (!pcRecvBuf || !pslphdr || !piLen) 
        return SLP_PARAMETER_BAD;
    
    if (len < HDRLEN) 
        return SLP_PARSE_ERROR;
    
    if ( GETVER(pcRecvBuf) != SLP_VER )
    {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"get_header: bad version",SLP_PARSE_ERROR); 
        return SLP_PARSE_ERROR;
    }
        
    iLangLen = GETLANGLEN(pcRecvBuf);
    if ((iLangLen+ *piLen + HDRLEN) > len)  
        return SLP_PARSE_ERROR;
    else if ( iLangLen < 2 )
		return SLP_PARSE_ERROR;
		
    #ifndef IGNORE_XID_FROM_REPLY
    if (pcSendBuf != NULL && (GETXID(pcRecvBuf) != GETXID(pcSendBuf))) 
    {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"get_header: bad reply xid",SLP_PARSE_ERROR); 
    }
    #endif
    
    if (pcSendBuf != NULL 
        && (    (GETLANGLEN(pcSendBuf) != GETLANGLEN(pcRecvBuf))
        || (SDstrncasecmp(&(pcSendBuf[HDRLEN]),&(pcRecvBuf[HDRLEN]),
                    GETLANGLEN(pcSendBuf)) != 0))) 
    {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP, "get_header: lang tag of reply does not match that of request", SLP_PARSE_ERROR);
    }
    
    if ((pslphdr->h_ucVer = GETVER(pcRecvBuf)) != SLP_VER) 
    {
        LOG_SLP_ERROR_AND_RETURN(SLP_LOG_DROP,"get_header: version not supported",SLP_PARSE_ERROR);
    }
    
    pslphdr->h_ucFun = GETFUN(pcRecvBuf);
    pslphdr->h_ulLen = (0xff0000 & GETBYTE(pcRecvBuf,LEN)<<16)  +
        (0x00ff00 & GETBYTE(pcRecvBuf,LEN+1)<<8) +
        (0x0000ff & GETBYTE(pcRecvBuf,LEN+2));
    pslphdr->h_usXID = GETXID(pcRecvBuf);
    
    if (((pslphdr->h_usFlags = GETFLAGS(pcRecvBuf)) & 0x07) != 0) 
    {
        LOG(SLP_LOG_ERR,"get_header: illegal flags set, process anyway");
    }
    
    pslphdr->h_iOffset = GETNEXTOP(pcRecvBuf);
    
    pslphdr->h_pcLangTag = safe_malloc(iLangLen+1,&pcRecvBuf[HDRLEN],iLangLen);
    pslphdr->h_usErrCode = GETSHT(pcRecvBuf,HDRLEN+iLangLen);
    *piLen += HDRLEN + iLangLen;

    return SLP_OK;
}
    
EXPORT SLPInternalError add_sht(char *pcBuf, int iBufSz, int iVal, int *piLen) 
{    
    if (!pcBuf || !piLen) 
        return SLP_PARAMETER_BAD;
        
    if ((*piLen + 2) > iBufSz) 
        return SLP_BUFFER_OVERFLOW;
    
    pcBuf[(*piLen)++] = (unsigned char) ((iVal & 0xFF00) >> 8);
    pcBuf[(*piLen)++] = (unsigned char) (iVal & 0xFF);
    
    return SLP_OK;
}
    
/* get_sht
 *
 * This routine safely reads a 2 byte value, converts it to an unsigned
 * int (0-2^16) and returns it.  If the offset is passed in, it will
 * increment it.  If there is not enough data, it returns SLP_PARSE_ERROR
 * otherwise it returns OK.
 *
 * pcBuf    - the buffer to read from
 * maxlen   - total length
 * piOffset - offset from here (if NULL assume 0 and don't increment)
 * piErr    - set to error if needed
 *
 * return:
 *   Error value.
 */
EXPORT SLPInternalError get_sht(const char *pcBuf, int maxlen, int *piOffset, int *piSht) {

  int offset = 0;

  if (!pcBuf || !piOffset || !piSht) return SLP_PARAMETER_BAD;

  if (piOffset != NULL) { 
    if ((*piOffset+2) > maxlen) return SLP_PARSE_ERROR;
    offset = *piOffset;
    *piOffset += 2;
  }

  *piSht = (int) ((unsigned char)pcBuf[offset] & (unsigned char)0xFF);
  *piSht = *piSht<< 8;
  *piSht += (int) ((unsigned char)pcBuf[offset+1] & (unsigned char)0xFF);
  return SLP_OK;
}

SLPInternalError add_string(char *pcBuf, int iBufSz, const char *pcStr, int *piLen) {
  int iStrLen ;
  SLPInternalError err = SLP_OK;

  if (!pcBuf || !pcStr || !piLen) return SLP_PARAMETER_BAD;
  iStrLen = strlen(pcStr);
  if (iStrLen > 0xFFFF) return SLP_PARSE_ERROR; /* max allowed is 2^16 */
  if ((iStrLen + *piLen + 2) >= iBufSz) return SLP_BUFFER_OVERFLOW;  
  
  if ((err = add_sht(pcBuf, iBufSz, iStrLen, piLen)) != SLP_OK) return err;

  memcpy(&(pcBuf[*piLen]), pcStr, strlen(pcStr));
  *piLen += strlen(pcStr);
  return SLP_OK;
}

/* get_string 
 *
 * Copy the string buffer into a newly allocated one and return it, unless
 * there is not enough room in the buffer...
 *
 * pcBuf     - the pkt buffer to copy out from
 * iMaxLen   - the size of the buffer
 * piOffset  - the current offset, to increment
 * ppcString - OUT: returns the string.
 *
 * Return:
 *   Error code
 */
EXPORT SLPInternalError get_string(const char *pcBuf, int iMaxLen, int *piOffset, char **ppcString) {
    int iLen;
    SLPInternalError err;
    
    if (ppcString) 
        *ppcString = NULL;
    
    if (!ppcString || !pcBuf || !piOffset) 
        return SLP_PARAMETER_BAD;
    
    *ppcString = NULL;
    err = get_sht(pcBuf, iMaxLen, piOffset, &iLen); 
    if (err) 
        return err;
    if ((iLen+*piOffset) > iMaxLen) 
    {
        SLP_LOG( SLP_LOG_ERR, "get_string: tried to parse but got iLen(%d)+*piOffset(%d) > iMaxLen (%d)", iLen, *piOffset, iMaxLen );

        return SLP_PARSE_ERROR;
    }
    
    *ppcString = safe_malloc(iLen+1, &(pcBuf[*piOffset]), iLen);
    *piOffset += iLen;
    return SLP_OK;
}

/*
 * get_next_string
 *
 *   For a string list "a,b,c" this will return "a" then "b" the "c"
 *   then NULL on subsequent calls.  The returned strings must be freed
 *   by the caller.  Seps allows you to search for any separator you
 *   wish.
 *
 *   NOTE:  empty terms are skipped:  "a,,b" will return "a" then "b"
 *
 *     pcSeps           the string of all separators.
 *     pcStringList     the string list, must be NULL terminated!
 *     piIndex          the index, incremented on calls, must initially
 *                      be set to 0.
 *     pcDelim          this is set to the delimiter found (can be NULL!)
 *
 * Returns:
 *   
 *   copy of the next string in the sequence.
 *
 * Side effects:
 *  
 *   The piIndex parameter is changed by the function call.  The return
 *   value is malloc'ed so it will leak if the caller does not free it
 *   eventually.
 *
 *   pcDelim is set to the delimiter found.
 */
EXPORT char * get_next_string(const char *pcSeps, const char *pcStringList,
		       int *piIndex, char *pcDelim) {

  int iLen = 0;
  int iNext = 0;
  int iEndsSep = 0;
  char *pcStr = NULL;

  *pcDelim = '\0'; /* initialize the return parameter for the case
                      where the string does not end in a separator */ 
  
  if (pcStringList == NULL || pcStringList[*piIndex] == '\0') return NULL;

  while(pcStringList[*piIndex] && isspace(pcStringList[*piIndex]))
    *piIndex += 1;

  /* nothing in fld */  
  if (issep(pcStringList[*piIndex],pcSeps)) {
    *pcDelim = pcStringList[*piIndex];
    *piIndex += 1;
    return NULL; 
  }
  
  if (pcStringList[*piIndex] == '\0') return NULL;
  
  while(pcStringList[*piIndex+iLen]) {
    if (issep(pcStringList[*piIndex+iLen],pcSeps)) {
      iEndsSep = 1; /* if item ends with a separator, advance past it */
      *pcDelim = pcStringList[*piIndex+iLen];
      break;
    }
    iLen++;
  }
    
  iNext = iLen + iEndsSep; /* advance past the string item */
  
  /* remove trailing blanks */
  
  while(iLen > 0
	&& (   isspace(pcStringList[*piIndex+iLen-1])
	    || pcStringList[*piIndex+iLen-1] == '\0'
	    || issep(pcStringList[*piIndex+iLen-1],pcSeps)))
    iLen--;

  if (iLen < 1) return NULL;
  
  pcStr = safe_malloc(iLen+1,&pcStringList[*piIndex],iLen);
  *piIndex += iNext; /* advance past the string item */
  return pcStr;
}

static int issep(char c, const char *pcSeps) {
  while (*pcSeps) {
    if (c == *pcSeps) return 1;
    else pcSeps++;
  }
  return 0;
}

EXPORT const char * slp_strerror(SLPInternalError slpe) {
  switch (slpe) {
    case SLP_OK:                       return "SLP_OK";
    case SLP_LANGUAGE_NOT_SUPPORTED:   return "SLP_LANGUAGE_NOT_SUPPORTED";
    case SLP_PARSE_ERROR:              return "SLP_PARSE_ERROR";
    case SLP_INVALID_REGISTRATION:     return "SLP_INVALID_REGISTRATION";
    case SLP_SCOPE_NOT_SUPPORTED:      return "SLP_SCOPE_NOT_SUPPORTED";
    case SLP_AUTHENTICATION_ABSENT:    return "SLP_AUTHENTICATION_ABSENT";
    case SLP_AUTHENTICATION_FAILED:    return "SLP_AUTHENTICATION_FAILED";
    case SLP_INVALID_UPDATE:           return "SLP_INVALID_UPDATE";
    case SLP_NOT_IMPLEMENTED:          return "SLP_NOT_IMPLEMENTED";
    case SLP_BUFFER_OVERFLOW:          return "SLP_BUFFER_OVERFLOW";
    case SLP_NETWORK_TIMED_OUT:        return "SLP_NETWORK_TIMED_OUT";
    case SLP_NETWORK_INIT_FAILED:      return "SLP_NETWORK_INIT_FAILED";
    case SLP_MEMORY_ALLOC_FAILED:      return "SLP_MEMORY_ALLOC_FAILED";
    case SLP_PARAMETER_BAD:            return "SLP_PARAMETER_BAD";
    case SLP_NETWORK_ERROR:            return "SLP_NETWORK_ERROR";
    case SLP_INTERNAL_SYSTEM_ERROR:    return "SLP_INTERNAL_SYSTEM_ERROR";
    case SLP_RECURSIVE_CALLBACK_ERROR: return "SLP_RECURSIVE_CALLBACK_ERROR";
    case SLP_TYPE_ERROR:               return "SLP_TYPE_ERROR";
    case SLP_REPLY_TOO_BIG_FOR_PROTOCOL:   return "SLP_REPLY_TOO_BIG_FOR_PROTOCOL";
    case SERVICE_NOT_REGISTERED:   	   return "SERVICE_NOT_REGISTERED";
    case SERVICE_ALREADY_REGISTERED:   return "SERVICE_ALREADY_REGISTERED";
    case SERVICE_TYPE_NOT_SUPPORTED:   return "SERVICE_TYPE_NOT_SUPPORTED";
    default: return "unknown error";
  }
}

/*
 * mslplog
 *
 *   All logging comes through this routine.  All that needs to be done to
 *   implement system logging is to replace the printf statements below
 *   with syslog or Event Logger commands.
 *
 *   lev      The logging level.
 *   pcMsg    The message generated at the source line where trouble arose.
 *   pcSysMsg This is an additional string for the log line.  It is usually
 *            used to include an error name.  If the parameter is NULL nothing
 *            is added for this parameter.
 *
 */

int gLastLogOption = 0;
            
EXPORT void SLP_LOG(LogLevel lev, const char* format, ...)
{
    va_list ap;
    
    va_start( ap, format );
    newlog( lev, format, ap );
    va_end( ap );
}

EXPORT void newlog(LogLevel lev, const char* format, va_list ap )
{
    char	pcMsg[MAXLINE +1];
    
    vsnprintf( pcMsg, MAXLINE, format, ap );

    mslplog( lev, pcMsg, NULL );
}

EXPORT void mslplog(LogLevel lev, const char *pcMsg, const char *pcSysMsg) 
{
    char *	pcLogName;

    if ( !pcMsg )
        return;
    /*
    * check to see that the log level has been configured - if not don't log.
    * The rules are:
    *    Give up on logging only if net.slp.traceAll has not been set.
    *    If lev is SLP_LOG_DROP, the net.slp.traceDrop property must be set, etc.
    *
    */
    if (/*(lev & SLP_LOG_ERR)  ||*/ /* always log these two log levels */
        /*(lev & SLP_LOG_FAIL) ||*/
        /* if traceAll!=true, don't log*/
        (SLPGetProperty("com.sun.slp.traceAll") && 
            !SDstrcasecmp(SLPGetProperty("com.sun.slp.traceAll"),"true")) ||

        (SLPGetProperty("com.apple.slp.logAll") && 
            !SDstrcasecmp(SLPGetProperty("com.apple.slp.logAll"),"true")) ||

        ((lev & SLP_LOG_STATE) &&          /* if SLP_LOG_STATE & traceState!=true, don't log */
        (SLPGetProperty("net.slp.traceState") &&
            !SDstrcasecmp(SLPGetProperty("net.slp.traceDrop"),"true"))) ||

        ((lev & SLP_LOG_RADMIN) &&          /* if SLP_LOG_RADMIN & traceRAdmin!=true, don't log */
        (SLPGetProperty("net.slp.traceRAdmin") &&
            !SDstrcasecmp(SLPGetProperty("net.slp.traceRAdmin"),"true"))) ||

        ((lev & SLP_LOG_DROP) &&          /* if DROP & traceDrop!=true, don't log */
        (SLPGetProperty("net.slp.traceDrop") &&
            !SDstrcasecmp(SLPGetProperty("net.slp.traceDrop"),"true"))) ||

        ((lev & SLP_LOG_DA) &&            /* if SLP_LOG_DA & traceDA!=true, don't log */
        (SLPGetProperty("net.slp.traceDATraffic") &&
            !SDstrcasecmp(SLPGetProperty("net.slp.traceDATraffic"),"true"))) ||

        ((lev & SLP_LOG_SA) &&            /* if SLP_LOG_SA & traceSA!=true, don't log */
        (SLPGetProperty("com.apple.slp.traceSATraffic") &&
            !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceSATraffic"),"true"))) ||

        ((lev & SLP_LOG_REG) &&         /* if SLP_LOG_REG & traceReg!=true, don't log */
        (SLPGetProperty("net.slp.traceReg") &&
            !SDstrcasecmp(SLPGetProperty("net.slp.traceReg"),"true"))) ||
        
        ((lev & SLP_LOG_MSG) &&         /* if SLP_LOG_MSG & traceMsg!=true, don't log */
        (SLPGetProperty("net.slp.traceMsg") &&
            !SDstrcasecmp(SLPGetProperty("net.slp.traceMsg"),"true"))) ||

        ((lev & SLP_LOG_DEBUG) &&   /* if SLP_LOG_DEBUG & traceDebug!=true, don't log */
        (SLPGetProperty("com.sun.slp.traceDebug") &&
            !SDstrcasecmp(SLPGetProperty("com.sun.slp.traceDebug"),"true"))) ||

        ((lev & SLP_LOG_REG) &&   /* if SLP_LOG_REG & traceRegistrations!=true, don't log */
        (SLPGetProperty("com.apple.slp.traceRegistrations") &&
            !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceRegistrations"),"true"))) ||

        ((lev & SLP_LOG_EXP) &&   /* if SLP_LOG_EXP & traceExpirations!=true, don't log */
        (SLPGetProperty("com.apple.slp.traceExpirations") &&
            !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceExpirations"),"true"))) ||

        ((lev & SLP_LOG_SR) &&   /* if SLP_LOG_SR & traceServiceRequests!=true, don't log */
        (SLPGetProperty("com.apple.slp.traceServiceRequests") &&
            !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceServiceRequests"),"true"))) ||

        ((lev & SLP_LOG_DA) &&   /* if SLP_LOG_DA & traceDAInfoRequests!=true, don't log */
        (SLPGetProperty("com.apple.slp.traceDAInfoRequests") &&
            !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceDAInfoRequests"),"true"))) ||

        ((lev & SLP_LOG_ERR) &&   /* if SLP_LOG_ERR & traceErrors!=true, don't log */
        (SLPGetProperty("com.apple.slp.traceErrors") &&
            !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceErrors"),"true"))) ||

        ((lev & SLP_LOG_NOTIFICATIONS) &&   /* if SLP_LOG_NOTIFICATIONS & traceNotifications!=true, don't log */
        (SLPGetProperty("com.apple.slp.traceNotifications") &&
            !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceNotifications"),"true"))) ||

        ((lev & SLP_LOG_CONFIG) &&   /* if SLP_LOG_CONFIG & traceConfig!=true, don't log */
        (SLPGetProperty("com.apple.slp.traceConfig") &&
            !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceConfig"),"true"))) ||

        ((lev & SLP_LOG_SIGNAL) &&   /* if SLP_LOG_SIGNAL & traceConfig!=true, don't log */
        (SLPGetProperty("com.apple.slp.traceSignals") &&
            !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceSignals"),"true"))) ||

        ((lev & SLP_LOG_DEBUG) &&   /* if SLP_LOG_DEBUG & traceDebug!=true, don't log */
        (SLPGetProperty("com.apple.slp.traceDebug") &&
            !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceDebug"),"true"))))
    {
        int		priority;
        int		logOption = LOG_CONS | LOG_NDELAY;
        
        switch (lev) 
        { 
            case SLP_LOG_FAIL:  
                pcLogName = "FAIL";    
                priority = LOG_CRIT;
                logOption |= LOG_PERROR;		// also log to the standard error output
            break;
            
            case SLP_LOG_ERR:
                pcLogName = "ERR";     
                priority = LOG_ERR;
                logOption |= LOG_PERROR;		// also log to the standard error output
            break;

            case SLP_LOG_DA:    
                pcLogName = "DA"; 
                priority = LOG_ERR;
            break;

            case SLP_LOG_SA:    
                pcLogName = "SA"; 
                priority = LOG_INFO;
            break;

            case SLP_LOG_REG:   
                pcLogName = "REG"; 
                priority = LOG_ERR;
            break;

            case SLP_LOG_MSG:   
                pcLogName = "MSG"; 
                priority = LOG_INFO;
            break;

            case SLP_LOG_NOTIFICATIONS:
                pcLogName = "NOTIFY";
                priority = LOG_INFO;
            break;
            
            case SLP_LOG_DROP:
                pcLogName = "DROP";
                priority = LOG_INFO;
            break;
            
            case SLP_LOG_DEBUG:
                pcLogName = "DEBUG"; 
                priority = LOG_INFO;
            break;

            case SLP_LOG_RADMIN:
                pcLogName = "ServerAdmin"; 
                priority = LOG_ERR;
            break;

            case SLP_LOG_EXP:
                pcLogName = "EXP"; 
                priority = LOG_ERR;
            break;

            case SLP_LOG_SR:
                pcLogName = "SR"; 
                priority = LOG_INFO;
            break;

            case SLP_LOG_STATE:
                pcLogName = "STATE";
                priority = LOG_ERR;
            break;
            
            case SLP_LOG_CONFIG:
                pcLogName = "CONFIG";
                priority = LOG_INFO;
            break;
            
            default:
                pcLogName = "UNKNOWN"; 
                priority = LOG_INFO;
        }
            
        if ( !sysLogInitialized )
        {
            pthread_mutex_init( &sysLogLock, NULL );
            sysLogInitialized = 1;
        }
        
        pthread_mutex_lock( &sysLogLock );
        
        if ( SLPGetProperty("com.apple.slp.identity") && gLastLogOption != logOption )
        {
            if ( gLastLogOption )
                closelog();
                
            openlog( SLPGetProperty("com.apple.slp.identity"), logOption, LOG_DAEMON );
            
            gLastLogOption = logOption;
        }
        
		syslog( priority, "%s: %s\n", pcLogName, pcMsg );

        pthread_mutex_unlock( &sysLogLock );
    } 
}

/*
 * isAttrvalEscapedOK
 *
 *   This routine scans a null terminated buffer for:
 *
 *     - characters which should be escaped but aren't.
 *     - characters which are escaped but shouldn't be.
 *     - opaque values to be sure they start with the correct escape
 *       value.
 *  
 *   Some of the features are ifdef'ed since debug output is verbose
 *   and this interferes with testing.
 *
 *   pcVal    The NULL terminated string buffer.
 *
 * Return:
 *
 *   SLPInternalError value.
 *
 * Side Effects:
 *
 *   None.
 */
EXPORT SLPInternalError isAttrvalEscapedOK(const char *pcVal) 
{
    const char *pc = pcVal;
    int count = 0;
    
    if (pcVal == NULL)
        return SLP_PARAMETER_BAD;
        
    if (pcVal[0] == '\0')
        return SLP_OK; 
    
    for ( ; *pc; pc++)
    { /* determine if the string is properly escaped! */
        count++;
        if (*pc == '\\') 
        {
            /* we have an escaped value - this is only allowed for escapable
            values, which are:  `(' x28 / `)' x29 / `,' x2c / `\' x5c /
            `!' x21 / `<' x3c / `=' x3d / `>' x3e / `~' x7e / CTL x00-x01f */
            char c1,c2;
            pc++;
            if (strlen(pc) < 2) 
            {
            #ifndef READER_TEST      	
                mslplog(SLP_LOG_DEBUG, "isAttrvalEscapedOK: token too large (>2 char)",(pc)?(pc):"");
            #endif	
                return SLP_PARSE_ERROR;
            }

            c1 = *pc++;
            c2 = *pc; /* loop progression will move past this value */
        
            if (count == 1 && (c1=='F' || c1=='f') && (c2=='F' || c2=='f'))
                return SLP_OK; /* could do a lot more checking */
      
            if (   (c1=='2' && (c2 == '1' ||              /*  !  */
                                c2=='8' ||                /*  (  */
                                c2=='9' ||                /*  )  */
                                c2=='A' || c2=='a' ||     /*  *  */
                                c2=='C' || c2=='c'))      /*  ,  */
                || (    (c1=='0' || c1 == '1') &&         /* CTL */
                        ((c2>='0' && c2<='9') ||        
                        (c2>='a' && c2<='f') ||
                        (c2>='A' && c2<='F')))  
                || (c1=='3' && 
                    (c2=='c' || c2=='C' ||                /*  <  */
                    c2=='d' || c2=='D' ||                /*  =  */
                    c2=='e' || c2=='E'))                 /*  >  */
                || (c1=='5' && (c2=='c' || c2=='C' ||     /*  \  */
                                c2=='f' || c2=='F'))      /*  _  */ 
                || (c1=='7' && (c2=='e' || c2=='E')))     /*  ~  */
            {
                continue; /* OK escaped value */
            }
            
        #ifndef READER_TEST      
            SLP_LOG(SLP_LOG_DEBUG,"isAttrvalEscapedOK: illegal escape value: %s from %s", pc, pcVal);
        #endif      

            return SLP_PARSE_ERROR;
        }
        
        if (*pc == '(' || *pc == ')' || *pc == ',' || *pc == '\\' ||
            *pc == '!' || *pc == '<' || *pc == '=' || *pc == '>' ||
            *pc == '~' || (*pc>= 0x00 && *pc<= 0x1F)) 
        {

        #ifndef READER_TEST
            SLP_LOG(SLP_LOG_DEBUG, "isAttrvalEscapedOK: illegal value, needs to be escaped: %s",pc);
        #endif      

            return SLP_PARSE_ERROR;
        }
    }	

    return SLP_OK;
}

/*
 * isURLEscapedOK
 *
 * Currently this checks only if the service type and/or naming
 * authority is OK.  It does not check the rules for URL path or
 * for the address of URLs.
 *
 *   pcVal
 *
 * Return: SLP_OK if properly escaped.
 * Side Effects: None.
 */

EXPORT SLPInternalError isURLEscapedOK(const char *pcVal) {

  const char *pc = pcVal;

  if (pcVal == NULL) return SLP_PARAMETER_BAD;
  if (pcVal[0] == '\0') return SLP_OK;

  /* Iterate over the string till it is either null terminated or
     the address part begins.  Do not keep state to figure out if
     there are too many ':'s or the URL scheme is malformed. */
  for ( ; *pc && *pc != '/' ; pc++) { 

    if (*pc != '%') {
      if ((*pc >= '0' && *pc <= '9') ||
	  (*pc >= 'a' && *pc <= 'z') ||
	  (*pc >= 'A' && *pc <= 'Z') ||
 	   *pc == '.' || *pc == '+'  || *pc == '-' || *pc == ':') {
	continue;
      } else {
	LOG(SLP_LOG_ERR,
	    "isURLEscapedOK: malformed service type or naming authority");
	return SLP_PARSE_ERROR;
      }
    } else {
      /* we have an escaped value - this is only allowed for escapable
	 values, which are:  `(' x28 / `)' x29 / `,' x2c / `\' x5c /
	 `!' x21 / `<' x3c / `=' x3d / `>' x3e / `~' x7e / CTL x00-x01f */
      char c1,c2;
      pc++;
      if (strlen(pc) < 2) {

#ifndef READER_TEST      	
	mslplog(SLP_LOG_DEBUG,
		"isURLEscapedOK: token too large (>2 char)",(pc)?(pc):"");
#endif	
	return SLP_PARSE_ERROR;
      }
      c1 = *pc++;
      c2 = *pc++; /* loop progression will move past this value */

      if (((c1 >= '0' && c1 <= '9') ||
	   (c1 >= 'A' && c1 <= 'F') ||
	   (c1 >= 'a' && c1 <= 'f')) &&
	  ((c2 >= '0' && c2 <= '9') ||
	   (c2 >= 'A' && c2 <= 'F') ||
	   (c2 >= 'a' && c2 <= 'f'))) {
	continue;
      } else {
	SLP_LOG( SLP_LOG_DEBUG, "isURLEscapedOK: bad hex escape value");
	return SLP_PARSE_ERROR;
      }
    }
  }	
  return SLP_OK;
  
}
