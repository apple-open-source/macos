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
 * mslplib.h : Minimal SLP v2 User Agent internal definitions.
 *
 * Version: 1.10
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

/* configuration parameters conforms to draft-ietf-svrloc-api-05.txt */
typedef struct mslpconfig { 

  /* the following are used */
  int                *pi_net_slp_DADiscoveryTimeouts;
  int                *pi_net_slp_multicastTimeouts;
  char               *pc_net_slp_locale;

} MSLPConfig;

typedef struct UA_State {
  char               *pcLangTag;
  char               *pcSendBuf;
  int                 iSendSz;
  char               *pcRecvBuf;
  int                 iRecvSz;
  char               *pcDAURL;
  SOCKET              sdSend;
  SOCKET              sdTCP;
  SOCKET              sdMax; /* used for select */
  fd_set              fds;
  struct timeval      tv;
  struct sockaddr_in  sinSendTo;
  MSLPConfig          config;
  void               *pvMutex;
} UA_State;

extern EXPORT DATable* GetGlobalDATable( void );
extern EXPORT DATable* GetGlobalDATableForRequester( void );
extern EXPORT void LocateAndAddDA( long addrOfDA );
extern EXPORT void LockGlobalDATable( void );
extern EXPORT void UnlockGlobalDATable( void );
/* ------------------------------------------------------------- mslp_net.c */

extern SLPInternalError mslplib_init_network(SOCKET *psdUDP, SOCKET *psdTCP,
  SOCKET *psdMax);

extern SLPInternalError get_target(UA_State *puas, const char *pcScope,
		      int *pUseDa, struct sockaddr_in *psin);

extern int    get_addrspec_from_url(const char *pcURL, int iURLLen, 
				    struct sockaddr_in *psin);

extern SLPInternalError get_unicast_result( time_t  timeout, SOCKET  sdSend, 
				  char *pcSendBuf, int iSendSz,
				  char *pcRecvBuf, int iRecvSz,
				  int *piInSz, struct sockaddr_in sin);

extern SLPInternalError get_converge_result( time_t tMaxwait, SOCKET sd,
				     char *ppcSendBuf, int iSendSz,
				     char *pcRecvBuf, int iRecvSz,
				     struct sockaddr_in  sin, unsigned char ttl, void *pvUser,
				     SLPHandle slph, void *pvCallback,
				     CBType cbCallbackType);

extern SLPInternalError get_tcp_result(const char *pcSendBuf, int iSendSz,
                               struct sockaddr_in sin,
                               char **ppcInBuf, int *piInSz);

/* the parsing routine needs to be exported to a test module
   but this is only for debugging.  In the release version, 
   the generate_srvrqst function is for internal use only. */

#ifndef NDEBUG
extern EXPORT SLPInternalError generate_srvrqst( 
				 char *pcSendBuf, int *piSendSz,
				 const char *pcLangTag,  
                                 const char *pcScope, const char *pcSrvType,
				 const char *pcFilter);
#else 
extern SLPInternalError generate_srvrqst( 
				 char *pcSendBuf, int *piSendSz,
				 const char *pcLangTag,  
                                 const char *pcScope, const char *pcSrvType,
				 const char *pcFilter);
#endif /* NDEBUG */

void mslplib_daadvert_callback(SLPHandle hSLP,
			       int iErrcode,
			       struct sockaddr_in sin,
			       const char *pcScopeList,
			       const char *pcDAAttrlist,
			       long lBootTime,
			       void *pvUser);

#ifdef SLPTCP
extern SLPInternalError get_overflow_result( time_t  timeout, SOCKET  sdSend, 
				  char *pcSendBuf, int iSendSz,
				  char *pcRecvBuf, int iRecvSz,
				  int *piInSz, struct sockaddr_in sin);
#endif /* SLPTCP */

/* -------------------------------------------------------------- mslplib.c */
extern TESTEXPORT SLPInternalError process_reply(const char *pcSendBuf,
			      const char *pcRecvBuf, int iRecvSz,
			      int *piLastOne, void *pvUser, SLPHandle hSLP,
			      void *pvCallback, CBType cbCallbackType);
			    
/* ------------------------------------------------------- mslplib_prlist.c */
extern void prlist_modify(char **ppcList, struct sockaddr_in sin);
extern int recalc_sendBuf(char *pcBuf, int iLen, const char *pcList);

/* ------------------------------------------------------- mslplib_regipc.c */

#ifdef EXTRA_MSGS
extern SLPInternalError mslplib_Reg(UA_State *puas,
			    const char *pcURL, unsigned short usLifetime,
			    const char *pcSrvtype, const char *pcAttrs,
			    SLPBoolean fresh);

extern SLPInternalError mslplib_Dereg(UA_State *puas, const char *pcURL, const char *pcScopes);

extern SLPInternalError mslplib_DelAttrs(const char *pcURL, const char *pcAttrs);

#endif /* EXTRA_MSGS */
