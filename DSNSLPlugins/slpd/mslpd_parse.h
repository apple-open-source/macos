/*
 * mslpd_parse.h : Definition for parsing messages in and out of
 *            the mini slpv2 SA.
 * Version: 1.4
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

#ifdef __cplusplus
//extern "C" {
#endif

extern SLPInternalError srvrqst_in(Slphdr *pslphdr, const char *pcInBuf, int iInSz,
  char **ppcPRList, char **ppcSrvType, char **ppcScopes, char **ppcPredicate);

extern SLPInternalError srvrply_out(SAStore *ps, SLPInternalError errcode, Mask *pm,
  Slphdr *pslphdr, char **ppcOutBuf, int *piOutSz, int *piNumResults);

extern unsigned char api2slp(SLPInternalError se);

#ifdef EXTRA_MSGS
extern int saadvert_out(SAState *ps, Slphdr *pslph, char **ppc, int *piOutSz);
#endif /* EXTRA_MSGS */

#ifdef MAC_OS_X
extern int daadvert_out(SAState *ps, SLPBoolean viaTCP, Slphdr *pslph, char **ppc, int *piOutSz);
#endif /* MAC_OS_X */

#ifdef __cplusplus
//}
#endif




