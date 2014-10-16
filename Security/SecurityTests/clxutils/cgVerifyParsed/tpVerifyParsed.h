 /*
  * tpVerifyParsed.h - wrapper for CSSM_TP_CertGroupVerify using parsd anchors. 
  */
 
#ifndef	_TP_VERIFY_PARSED_H_
#define _TP_VERIFY_PARSED_H_

#ifdef __cplusplus
extern "C" {
#endif

CSSM_RETURN tpCertGroupVerifyParsed(
	CSSM_TP_HANDLE						tpHand,
	CSSM_CL_HANDLE						clHand,
	CSSM_CSP_HANDLE 					cspHand,
	CSSM_DL_DB_LIST_PTR					dbListPtr,
	const CSSM_OID						*policy,		// optional
	const CSSM_DATA						*fieldOpts,		// optional
	const CSSM_DATA						*actionData,	// optional
	void								*policyOpts,
	const CSSM_CERTGROUP 				*certGroup,
	CSSM_DATA_PTR						anchorCerts,
	unsigned							numAnchorCerts,
	CSSM_TP_STOP_ON						stopOn,			// CSSM_TP_STOP_ON_POLICY, etc.
	CSSM_TIMESTRING						cssmTimeStr,	// optional
	CSSM_TP_VERIFY_CONTEXT_RESULT_PTR	result);		// optional, RETURNED

#ifdef __cplusplus
}
#endif

#endif	/* _TP_VERIFY_PARSED_H_ */

