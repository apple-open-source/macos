//
// AC plugin transition layer.
// This file was automatically generated. Do not edit on penalty of futility!
//
#ifndef _H_ACABSTRACTSESSION
#define _H_ACABSTRACTSESSION

#include <security_cdsa_plugin/pluginsession.h>
#include <security_cdsa_utilities/cssmdata.h>


namespace Security {


//
// A pure abstract class to define the AC module interface
//
class ACAbstractPluginSession {
public:
	virtual ~ACAbstractPluginSession();
  virtual void AuthCompute(const CSSM_TUPLEGROUP &BaseAuthorizations,
         const CSSM_TUPLEGROUP *Credentials,
         uint32 NumberOfRequestors,
         const CSSM_LIST &Requestors,
         const CSSM_LIST *RequestedAuthorizationPeriod,
         const CSSM_LIST &RequestedAuthorization,
         CSSM_TUPLEGROUP &AuthorizationResult) = 0;
  virtual void PassThrough(CSSM_TP_HANDLE TPHandle,
         CSSM_CL_HANDLE CLHandle,
         CSSM_CC_HANDLE CCHandle,
         const CSSM_DL_DB_LIST &DBList,
         uint32 PassThroughId,
         const void *InputParams,
         void **OutputParams) = 0;
};

} // end namespace Security

#endif //_H_ACABSTRACTSESSION
