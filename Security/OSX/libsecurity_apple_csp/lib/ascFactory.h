/*
 * Copyright (c) 2001,2011,2014 Apple Inc. All Rights Reserved.
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


#ifdef	ASC_CSP_ENABLE

#ifndef _ASC_ALG_FACTORY_H_
#define _ASC_ALG_FACTORY_H_

#include <security_cdsa_plugin/CSPsession.h>
#include "AppleCSP.h"

class AppleCSPSession;

/* Algorithm factory */
class AscAlgFactory : public AppleCSPAlgorithmFactory {
public:
	
    AscAlgFactory(
		Allocator *normAlloc, 
		Allocator *privAlloc);
	~AscAlgFactory() { }
	
    bool setup(
		AppleCSPSession &session,
		CSPFullPluginSession::CSPContext * &cspCtx, 
		const Context &context);

};


#endif 	/*_ASC_ALG_FACTORY_H_ */
#endif	/* ASC_CSP_ENABLE */
