/*
 * mslplib_opt.h : Optional feature definitions.
 *
 * These definitions are used by portions of the implementation which
 * provide over-the-minimal features (service type and attribute request).
 *
 * Version:  1.1
 * Date:     03/07/99
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
 * generate_srvtyperqst
 *
 */
TESTEXPORT SLPInternalError generate_srvtyperqst(char *pcSendBuf, int *piSendSz,
	const char *pcLangTag,
        const char *pcNamingAuth, const char *pcScopeList);

SLPInternalError handle_srvtyperply_in(const char *pcSendBuf, const char *pcRecvBuf,
	int iRecvSz, void *pvUser, void *pvCallback, CBType cbCallbackType);

/*
 * generate_attrrqst
 *
 */
TESTEXPORT SLPInternalError generate_attrrqst(char *pcSendBuf, int *piSendSz,
	const char *pcLangTag, const char *pcService,
	const char *pcScopeList, const char *pcAttrList);

SLPInternalError handle_attrrply_in(const char *pcSendBuf, const char *pcRecvBuf,
	int iRecvSz, void *pvUser, void *pvCallback, CBType cbCallbackType);







