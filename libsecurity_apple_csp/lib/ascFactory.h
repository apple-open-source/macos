//
// ascAlgFactory.h - algorithm factory for ASC
// Written by Doug Mitchell 4/4/2001
//

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
