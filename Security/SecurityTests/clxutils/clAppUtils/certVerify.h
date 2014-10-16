#ifndef	_CERT_VERIFY_H_
#define _CERT_VERIFY_H_

#include <clAppUtils/BlobList.h>
#include <Security/cssmtype.h>
#include <Security/cssmapple.h>

/* must be C++ since we use BlobList */
extern "C" {

/* Display verify results */
void dumpVfyResult(
	const CSSM_TP_VERIFY_CONTEXT_RESULT *vfyResult);

typedef enum {
	CVP_Basic = 0,
	CVP_SSL,
	CVP_SMIME,
	CVP_SWUpdateSign,		// was CVP_CodeSigning
	CVP_ResourceSigning,
	CVP_iChat,
	CVP_IPSec,
	CVP_PKINIT_Server,
	CVP_PKINIT_Client,
	CVP_AppleCodeSigning,	// the Leopard version
	CVP_PackageSigning
} CertVerifyPolicy;

typedef enum {
	CRP_None = 0,
	CRP_CRL,
	CRP_OCSP,
	CRP_CRL_OCSP	
} CertRevokePolicy;

/* 
 * Since I never stop adding args to certVerify(), most of which have reasonable 
 * defaults, the inputs are now expressed like so.
 */
#define CERT_VFY_ARGS_VERS	5		/* increment every time you change this struct */
typedef struct {
	int						version;		/* must be CERT_VFY_ARGS_VERS */
	CSSM_TP_HANDLE			tpHand;
	CSSM_CL_HANDLE 			clHand;
	CSSM_CSP_HANDLE 		cspHand;
	BlobList				*certs;	
	BlobList				*roots;
	BlobList				*crls;
	char					*vfyTime;
	
	CSSM_BOOL				certNetFetchEnable;
	CSSM_BOOL				useSystemAnchors;
	CSSM_BOOL				useTrustSettings;
	CSSM_BOOL				leafCertIsCA;
	CSSM_BOOL				allowExpiredRoot;
	CSSM_BOOL				implicitAnchors;
	CSSM_DL_DB_LIST_PTR		dlDbList;		// optional
	CertVerifyPolicy		vfyPolicy;
	
	const char				*sslHost;		// optional; SSL policy
	CSSM_BOOL				sslClient;		// normally server side
	const char				*senderEmail;	// optional, SMIME
	CE_KeyUsage				intendedKeyUse;	// optional, SMIME only
	
	/* revocation options */
	CertRevokePolicy		revokePolicy;
	CSSM_BOOL				allowUnverified;	// if false, at least one must succeed

	/* CRL options */
	CSSM_BOOL				requireCrlIfPresent;	
	CSSM_BOOL				requireCrlForAll;	
	CSSM_BOOL				crlNetFetchEnable;
	CSSM_DL_DB_HANDLE_PTR	crlDlDb;		// obsolete: write CRLs here

	/* OCSP options */
	const char				*responderURI;	// optional, OCSP only
	const unsigned char		*responderCert;	// optional, OCSP only
	unsigned				responderCertLen;// optional, OCSP only 
	CSSM_BOOL				disableCache;	// both r and w for now
	CSSM_BOOL				disableOcspNet;
	CSSM_BOOL				requireOcspIfPresent;
	CSSM_BOOL				requireOcspForAll;
	CSSM_BOOL				generateOcspNonce;
	CSSM_BOOL				requireOcspRespNonce;
	
	const char				*expectedErrStr;// e.g.,
											// "CSSMERR_APPLETP_CRL_NOT_TRUSTED"
				
	/* 
	 * expected per-cert errors
	 * format is certNum:errorString
	 * e.g., "1:CSSMERR_APPLETP_CRL_NOT_TRUSTED"
	 */
	unsigned 				numCertErrors;
	const char				**certErrors;	// per-cert status
	
	/*
	 * Expected per-cert status (CSSM_TP_APPLE_EVIDENCE_INFO.StatusBits)
	 * format is certNum:status_in_hex
	 * e.g., "1:0x18", leading 0x optional
	 */
	unsigned				numCertStatus;
	const char				**certStatus;
	CSSM_BOOL				quiet;
	CSSM_BOOL				verbose;

} CertVerifyArgs;

/* perform one cert/crl verification */
int certVerify(CertVerifyArgs *args);

/*
 * A slightly simplified version of certVerify: 
 *		-- no CRLs
 *		-- no DlDbs
 *		-- no net fetch
 *		-- time = now
 * 	  	-- no trust settings
 */
int certVerifySimple(
	CSSM_TP_HANDLE			tpHand, 
	CSSM_CL_HANDLE 			clHand,
	CSSM_CSP_HANDLE 		cspHand,
	BlobList				&certs,
	BlobList				&roots,
	CSSM_BOOL				useSystemAnchors,
	CSSM_BOOL				leafCertIsCA,
	CSSM_BOOL				allowExpiredRoot,
	CertVerifyPolicy		vfyPolicy,
	const char				*sslHost,		// optional, SSL policy
	CSSM_BOOL				sslClient,		// normally server side
	const char				*senderEmail,	// optional, SMIME
	CE_KeyUsage				intendedKeyUse,	// optional, SMIME only
	const char				*expectedErrStr,// e.g.,
	unsigned 				numCertErrors,
	const char 				**certErrors,	// per-cert status
	unsigned				numCertStatus,
	const char				**certStatus,
	CSSM_BOOL				useTrustSettings,
	CSSM_BOOL				quiet,
	CSSM_BOOL				verbose);

/* convert ASCII string in hex to unsigned */
unsigned hexToBin(const char *hex);

}   /* extern "C" */

#endif	/* _DO_VERIFY_H_ */
