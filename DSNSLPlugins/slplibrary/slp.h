
/*
 * slp.h : Minimal SLP v2 API definitions
 *
 * These definitions are consistent with draft-ietf-svrloc-api-06.txt.
 * The SLPReadConfigFile interface is an additional interface not specified
 * by that document.
 *
 * Version: 1.6
 * Date:    03/27/99
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

#ifndef _SLP_H
#define	_SLP_H

#ifdef	__cplusplus
//extern "C" {
#endif

#include <CoreFoundation/CoreFoundation.h>

#ifndef EXPORT
#define EXPORT extern
#endif

typedef void * SLPHandle;

typedef enum {
   
     SLP_LAST_CALL                    	= 1,   
     SLP_OK                           	= 0,
     SLP_LANGUAGE_NOT_SUPPORTED       	= -1,
     SLP_PARSE_ERROR                  	= -2,
     SLP_INVALID_REGISTRATION         	= -3,
     SLP_SCOPE_NOT_SUPPORTED          	= -4,
     SLP_AUTHENTICATION_ABSENT        	= -6,
     SLP_AUTHENTICATION_FAILED        	= -7,
     SLP_INVALID_UPDATE               	= -13,
     SLP_REFRESH_REJECTED            	= -15,
     SLP_NOT_IMPLEMENTED              	= -17,
     SLP_BUFFER_OVERFLOW              	= -18,
     SLP_NETWORK_TIMED_OUT            	= -19,
     SLP_NETWORK_INIT_FAILED          	= -20,
     SLP_MEMORY_ALLOC_FAILED          	= -21,
     SLP_PARAMETER_BAD                	= -22,
     SLP_NETWORK_ERROR                	= -23,
     SLP_INTERNAL_SYSTEM_ERROR        	= -24,
     SLP_RECURSIVE_CALLBACK_ERROR     	= -25,
     SLP_TYPE_ERROR                   	= -26,
     SLP_REPLY_TOO_BIG_FOR_PROTOCOL	  	= -27,
	 SERVICE_NOT_REGISTERED 		  	= -28,
	 SERVICE_ALREADY_REGISTERED       	= -29,
	 SERVICE_TYPE_NOT_SUPPORTED       	= -30,
     SCOPE_LIST_TOO_LONG			  	= -31,
     SLP_REPLY_DOESNT_MATCH_REQUEST		= -32,
     SLP_PREFERENCES_ERROR				= -33,
     SLP_REQUEST_ALREADY_HANDLED		= -34,
     SLP_REQUEST_CANCELED_BY_USER		= -35,
     SLP_DA_BUSY_NOW					= -36
} SLPInternalError ;

typedef enum {
     SLP_LIFETIME_DEFAULT = 10800,
     SLP_LIFETIME_MAXIMUM = 65535,
     SLP_LIFETIME_PERMANENT = -1
} SLPURLLifetime;

/*typedef enum {
      SLP_FALSE = 0,
      SLP_TRUE = 1

} SLPBoolean;
*/
typedef bool SLPBoolean;
#define SLP_FALSE	false
#define SLP_TRUE	true

typedef struct srvurl {
     char *s_pcSrvType;
     char *s_pcHost;
     int   s_iPort;
     char *s_pcNetFamily;
     char *s_pcSrvPart;
} SLPSrvURL;

typedef void SLPRegReport(
     SLPHandle       hSLP,
     SLPInternalError        errCode,
     void           *pvCookie);
   
typedef SLPBoolean SLPSrvTypeCallback(
     SLPHandle       hSLP,
     const char     *pcSrvTypes,
     SLPInternalError        errCode,
     void           *pvCookie);

typedef SLPBoolean SLPSrvURLCallback(
     SLPHandle       hSLP,
     const char     *pcSrvURL,
     unsigned short  sLifetime,
     SLPInternalError        errCode,
     void           *pvCookie);
#ifdef MAC_OS_X
typedef SLPBoolean SLPScopeCallback(
     SLPHandle       hSLP,
     const char     *pcScope,
     SLPInternalError        errCode,
     void           *pvCookie);
#endif /* MAC_OS_X */

typedef SLPBoolean SLPAttrCallback(
     SLPHandle       hSLP,
     const char     *pcAttrList,
     SLPInternalError        errCode,
     void           *pvCookie);

EXPORT SLPInternalError SLPOpen(
     const char *pcLang, 
     SLPBoolean isAsync, 
     SLPHandle *phSLP,
	 CFRunLoopRef runLoopRef = 0 );

EXPORT void SLPClose(
     SLPHandle hSLP);

EXPORT SLPInternalError SLPReg(
     SLPHandle   hSLP,
     const char  *pcSrvURL,
     const unsigned short sLifetime,
     const char  *pcSrvType,
     const char  *pcAttrs,
     SLPBoolean  fresh,
     SLPRegReport callback,
     void        *pvUser);

EXPORT SLPInternalError SLPDereg(
     SLPHandle  hSLP,
     const char *pURL,
     const char  *pcScopes,
     SLPRegReport callback,
     void       *pvUser);
   
EXPORT SLPInternalError SLPDelAttrs(
     SLPHandle   hSLP,
     const char  *pURL,
     const char  *pcAttrs,
     SLPRegReport callback,
     void        *pvUser);

EXPORT SLPInternalError SLPFindSrvTypes(
     SLPHandle    hSLP,
     const char  *pcNamingAuthority,
     const char  *pcScopeList,
     SLPSrvTypeCallback callback,
     void        *pvUser);
     

EXPORT SLPInternalError SLPFindSrvs(
     SLPHandle  hSLP,
     const char *pcServiceType,
     const char *pcScope,
     const char *pcSearchFilter,
     SLPSrvURLCallback callback,
     void       *pvUser);

#ifdef MAC_OS_X
EXPORT SLPInternalError SLPFindScopesAsync(
     SLPHandle  hSLP,
     SLPScopeCallback callback,
     void       *pvUser);
#endif /* MAC_OS_X */

EXPORT SLPInternalError SLPFindAttrs(
     SLPHandle   hSLP,
     const char *pcURL,
     const char *pcScope,
     const char *pcAttrIds,
     SLPAttrCallback callback,
     void       *pvUser);

EXPORT int SLPGetRefreshInterval();

EXPORT SLPInternalError SLPFindScopes(
     SLPHandle hSLP,
     char** ppcScopeList);

EXPORT SLPInternalError SLPParseSrvURL(
     const char *pcSrvURL,
     SLPSrvURL** ppSrvURL);

EXPORT void SLPFree(
     void *pvMem);

EXPORT SLPInternalError SLPEscape(
     const char* pcInBuf,
     char** ppcOutBuf);

EXPORT SLPInternalError SLPUnescape(
     const char* pcInBuf,
     char** ppcOutBuf);

EXPORT const char* SLPGetProperty(
     const char* pcName);

EXPORT void SLPSetProperty(
     const char *pcName,
     const char *pcValue);

EXPORT void SLPReadConfigFile(  /* not in draft-ietf-svrloc-api-08.txt */
     const char *pcFileName);
			      
EXPORT void SLPWriteConfigFile(  /* not in draft-ietf-svrloc-api-08.txt */
     const char *pcFileName);
			      
EXPORT void SLPLogConfigState( void );	/* not in draft-ietf-svrloc-api-08.txt */
#ifdef	__cplusplus
//}
#endif

#endif	/* _SLP_H */

