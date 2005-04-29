/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : tokenfactory.h
	    Package: pcsc-lite
            Author : David Corcoran
            Date   : 01/01/00
            Purpose: This handles card abstraction attachment. 
	            
********************************************************************/

#ifndef __cardfactory_h__
#define __cardfactory_h__

#include "mscdefines.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef WIN32
#ifndef MSC_SVC_DROPDIR
#define MSC_SVC_DROPDIR                     TPSvcDropdir()
#define MSC_SVC_DROPDIR_DEFAULT             "/usr/libexec/SmartCardServices/services/"
#define MSC_SVC_DROPDIR_ENV                 "MSC_SVC_DROPDIR"
#endif
#else
#define MSC_SVC_DROPDIR                     "C:\\Program Files\\Muscle\\Services\\"
#endif

	const char *TPSvcDropdir(void);
	MSCLong32 TPLoadToken(MSCLPTokenConnection);
	MSCLong32 TPUnloadToken(MSCLPTokenConnection);
	MSCLong32 TPBindFunctions(MSCLPTokenConnection);
	MSCLong32 TPUnbindFunctions(MSCLPTokenConnection);
	MSCLong32 TPSearchBundlesForAtr(MSCPUChar8 Atr, MSCULong32 Length,
		MSCLPTokenInfo tokenInfo);

#ifdef __cplusplus
}
#endif

#endif							/* __cardfactory_h__ */
