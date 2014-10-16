/* 
 * ocspTool - simple OCSP request/response generator and parser
 */
#include <Security/Security.h>
#include <Security/SecAsn1Coder.h>
#include <Security/ocspTemplates.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_utils/cuOidParser.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <clAppUtils/CertParser.h>
#include <clAppUtils/timeStr.h>
#include <clAppUtils/identPicker.h>
#include <CommonCrypto/CommonDigest.h>
#include <security_ocspd/ocspExtensions.h>
#include <security_ocspd/ocspdUtils.h>
#include <utilLib/common.h>
#include "ocspUtils.h"
#include "ocspRequest.h"
#include "ocspNetwork.h"
#include "findOcspUrl.h"

static void usage(char **argv) 
{
	printf("Usage: %s cmd [option...]\n", argv[0]);
	printf("cmds:\n");
	printf("   g generate request\n");
	printf("   G parse request\n");
	printf("   r generate reply\n");
	printf("   R parse reply\n");
	printf("   p generate OCSP request, post, get reply (use -p and/or -o)\n");
	printf("Options\n");
	printf("  -c cert_file         -- for generating request\n");
	printf("  -C issuer_cert_file  -- for generating request\n");
	printf("  -i in_file           -- for parsing\n");
	printf("  -o out_file          -- for generating\n");
	printf("  -s status            -- cert status: g(ood)|r(evoked)|u(nknown)\n");
	printf("  -r crlReason         -- integer 0..8\n");
	printf("  -k keychain          -- keychain containing signing cert\n");
	printf("  -p                   -- parse reply from post op\n");
	printf("  -u responderURI      -- OCSP responder here, not from cert's AIA extension\n");
	printf("  -v                   -- verbose; e.g., print certs\n");
	exit(1);
}

void doIndent(int indent)
{
	for(int dex=0; dex<indent; dex++) {
		printf(" ");
	}
}

static void printString(
	const CSSM_DATA *str)
{
	unsigned i;
	char *cp = (char *)str->Data;
	for(i=0; i<str->Length; i++) {
		printf("%c", *cp++);
	}
	printf("\n");
}

static void printDataAsHex(
	const CSSM_DATA *d,
	unsigned maxToPrint = 0)		// optional, 0 means print it all
{
	unsigned i;
	bool more = false;
	uint32 len = d->Length;
	uint8 *cp = d->Data;
	
	if((maxToPrint != 0) && (len > maxToPrint)) {
		len = maxToPrint;
		more = true;
	}	
	for(i=0; i<len; i++) {
		printf("%02X ", ((unsigned char *)cp)[i]);
	}
	if(more) {
		printf("...\n");
	}
	else {
		printf("\n");
	}
}

static void printTaggedItem(
	const NSS_TaggedItem &ti)
{
	switch(ti.tag) {
		case BER_TAG_PRINTABLE_STRING:
		case BER_TAG_T61_STRING:
		case BER_TAG_IA5_STRING:
		case BER_TAG_UTC_TIME:
		case BER_TAG_GENERALIZED_TIME:
			printString(&ti.item);
			break;
		default:
			printDataAsHex(&ti.item, 0);
	}
}

static void printName(
	const NSS_Name &name,
	int indent)
{
	OidParser parser;
	
	unsigned numRdns = ocspdArraySize((const void **)name.rdns);
	for(unsigned rdnDex=0; rdnDex<numRdns; rdnDex++) {
		NSS_RDN *rdn = name.rdns[rdnDex];
		unsigned numATVs = ocspdArraySize((const void **)rdn->atvs);
		for(unsigned atvDex=0; atvDex<numATVs; atvDex++) {
			NSS_ATV *atv = rdn->atvs[atvDex];
			char buf[OID_PARSER_STRING_SIZE];
			parser.oidParse(atv->type.Data, atv->type.Length, buf);
			doIndent(indent);
			printf("%s : ", buf);
			printTaggedItem(atv->value);
		}
	}
}	

static uint8 nullParam[2] = {5, 0};

/*
 * Given the following, create a ResponseData (to be signed by caller).
 *
 *		cert status (CS_{Good,Revoked,Unknown})
 *		cert being verified
 *		issuer cert
 *		this update time
 *		next update time (optional)
 *		nonce (optional)
 */
static int genTbsResp(
	SecAsn1CoderRef coder,			// result in this coder's address space
	CSSM_CL_HANDLE clHand, 
	SecAsn1OCSPCertStatusTag status,
	CE_CrlReason reason,			// for CS_Revoked
	const CSSM_DATA	&subjectCert,
	const CSSM_DATA &issuerCert,
	unsigned thisUpdate,			// required, seconds from now
	unsigned nextUpdate,			// optional, seconds from now, 0 ==> skip
	const CSSM_DATA *nonce,			// optional
	CSSM_DATA &encodedTbs)			// allocated in coder space and RETURNED
{
	char *nextUpdStr = NULL;
	CSSM_DATA nextUpdateData;
	char *thisUpdStr = NULL;
	CSSM_DATA *thisUpdateData;
	SecAsn1OCSPResponseData responseData;
	OCSPNonce *nonceExt = NULL;
	char *producedAt = NULL;
	SecAsn1OCSPSingleResponse singleResp;
	SecAsn1OCSPSingleResponse *respArray[2] = {&singleResp, NULL};
	SecAsn1OCSPResponderID responderID;
	NSS_CertExtension *extenArray[2] = {NULL, NULL};
	
	/* SingleResponse */
	memset(&singleResp, 0, sizeof(singleResp));
	
	/* SingleResponse.CertID */
	SecAsn1OCSPCertID &certId = singleResp.certID;
	CertParser parser(clHand);
	CertParser issuerParser(clHand);
	CSSM_RETURN crtn = parser.initWithData(subjectCert);
	if(crtn) {
		cssmPerror("CertParser.initWithData for subject cert", crtn);
		return -1;
	}
	crtn = issuerParser.initWithData(issuerCert);
	if(crtn) {
		cssmPerror("CertParser.initWithData for issuer", crtn);
		return -1;
	}

	/* algId refers to the hash we'll perform in issuer name and key */
	certId.algId.algorithm = CSSMOID_SHA1;
	certId.algId.parameters.Data = nullParam;
	certId.algId.parameters.Length = sizeof(nullParam);
	
	/* SHA1(issuerName) */
	CSSM_DATA issuerName = {0, NULL};
	issuerName.Data = (uint8 *)parser.fieldForOid(CSSMOID_X509V1IssuerNameStd, 
		issuerName.Length);
	if(issuerName.Data == NULL) {
		printf("***Error fetching issuer name. Aborting.\n");
		return 1;
	}
	uint8 issuerNameHash[CC_SHA1_DIGEST_LENGTH];
	ocspdSha1(issuerName.Data, issuerName.Length, issuerNameHash);
	
	/* SHA1(issuer public key) */
	CSSM_KEY_PTR pubKey = NULL;
	CSSM_SIZE pubKeyLen = sizeof(CSSM_KEY);
	pubKey = (CSSM_KEY_PTR)issuerParser.fieldForOid(CSSMOID_CSSMKeyStruct, pubKeyLen);
	if(pubKey == NULL) {
		printf("***Error fetching public key from issuer cert. Aborting.\n");
		return 1;
	}
	uint8 pubKeyHash[CC_SHA1_DIGEST_LENGTH];
	ocspdSha1(pubKey->KeyData.Data, pubKey->KeyData.Length, pubKeyHash);

	/* serial number */
	CSSM_DATA serialNum = {0, NULL};
	serialNum.Data = (uint8 *)parser.fieldForOid(CSSMOID_X509V1SerialNumber, 
		serialNum.Length);
	if(serialNum.Data == NULL) {
		printf("***Error fetching serial number. Aborting.\n");
		return 1;
	}

	/* build the CertID from those components */
	certId.issuerNameHash.Data = issuerNameHash;
	certId.issuerNameHash.Length = CC_SHA1_DIGEST_LENGTH;
	certId.issuerPubKeyHash.Data = pubKeyHash;
	certId.issuerPubKeyHash.Length = CC_SHA1_DIGEST_LENGTH;	
	certId.serialNumber = serialNum;

	/* SingleResponse.CertStatus - to be encoded on its own */
	SecAsn1OCSPCertStatus certStatus;
	memset(&certStatus, 0, sizeof(certStatus));
	SecAsn1OCSPRevokedInfo revokedInfo;
	char *revokedAt = NULL;
	CSSM_DATA reasonData;
	OSStatus ortn;
	
	if(status == CS_Revoked) { 
		/* cook up SecAsn1OCSPRevokedInfo */
		certStatus.revokedInfo = &revokedInfo;
		revokedAt = appTimeAtNowPlus(-3600, TIME_GEN);
		revokedInfo.revocationTime.Data = (uint8 *)revokedAt; 
		revokedInfo.revocationTime.Length = strlen(revokedAt);
		uint8 theReason = reason;
		reasonData.Data = &theReason;
		reasonData.Length = 1;
		revokedInfo.revocationReason = &reasonData;
		ortn = SecAsn1EncodeItem(coder, &certStatus, 
			kSecAsn1OCSPCertStatusRevokedTemplate, 
			&singleResp.certStatus);
	}
	else {
		ortn = SecAsn1EncodeItem(coder, &certStatus, 
			kSecAsn1OCSPCertStatusGoodTemplate,
			&singleResp.certStatus);
	}
	if(ortn) {
		printf("***Error encoding certStatus\n"); 
		goto errOut;
	}
	
	/* SingleResponse.thisUpdate */
	thisUpdStr = appTimeAtNowPlus(thisUpdate, TIME_GEN);
	thisUpdateData = &singleResp.thisUpdate;
	thisUpdateData->Data = (uint8 *)thisUpdStr;
	thisUpdateData->Length = strlen(thisUpdStr);
	
	/* SingleResponse.nextUpdate, optional */
	if(nextUpdate) {
		nextUpdStr = appTimeAtNowPlus(nextUpdate, TIME_GEN); 
		nextUpdateData.Data = (uint8 *)nextUpdStr;
		nextUpdateData.Length = strlen(nextUpdStr);
		singleResp.nextUpdate = &nextUpdateData;
	}
	
	/* Single Extensions - none for now */
	 
	/* Now up to ResponseData */
	memset(&responseData, 0, sizeof(responseData));
	
	/* skip version */
	
	/* 
	 * ResponseData.responderID: KeyHash (of signer); we already got this for CertID.
	 * WE have to encode this one separately and drop it in as an ASN_ANY. 
	 */
	responderID.byKey = certId.issuerPubKeyHash;
	ortn = SecAsn1EncodeItem(coder, &responderID, 
		kSecAsn1OCSPResponderIDAsKeyTemplate,
		&responseData.responderID);
	if(ortn) {
		printf("***Error encoding responderID\n");
		goto errOut;
	}
	
	/* ResponseData.producedAt = now */
	producedAt = appTimeAtNowPlus(0, TIME_GEN);
	responseData.producedAt.Data = (uint8 *)producedAt;
	responseData.producedAt.Length = strlen(producedAt);
		
	/* ResponseData.responses - one of 'em */
	responseData.responses = respArray;
	
	/* ResponseData.responseExtensions - optionally one, nonce */
	if(nonce) {
		nonceExt = new OCSPNonce(coder, false, *nonce);
		extenArray[0] = nonceExt->nssExt();
		responseData.responseExtensions = extenArray;
	}
	else {
		responseData.responseExtensions = NULL;
	}
	
	/* encode it */
	encodedTbs.Data = NULL;
	encodedTbs.Length = 0;
	ortn = SecAsn1EncodeItem(coder, &responseData, 
		kSecAsn1OCSPResponseDataTemplate,
		&encodedTbs);
	if(ortn) {
		printf("***Error encoding SecAsn1OCSPResponseData\n");
	}
errOut:
	/* free resources */
	if(revokedAt) {
		CSSM_FREE(revokedAt);
	}
	if(thisUpdStr) {
		CSSM_FREE(thisUpdStr);
	}
	if(nextUpdStr) {
		CSSM_FREE(nextUpdStr);
	}
	if(nonceExt) {
		delete nonceExt;
	}
	return ortn;
}

static int genOcspReq(
	CSSM_CL_HANDLE clHand,
	const unsigned char *certFile, 
	unsigned certFileLen, 
	const unsigned char *issuerCertFile, 
	unsigned issuerCertFileLen, 
	unsigned char **outFile,		// RETURNED 
	unsigned *outFileLen)			// RETURNED
{
	CertParser parser(clHand);
	CertParser issuerParser(clHand);
	CSSM_DATA certData = {certFileLen, (uint8 *)certFile};
	CSSM_RETURN crtn;
	crtn = parser.initWithData(certData);
	if(crtn) {
		cssmPerror("CertParser.initWithData for subject cert", crtn);
		return -1;
	}
	certData.Data = (uint8 *)issuerCertFile;
	certData.Length = issuerCertFileLen;
	crtn = issuerParser.initWithData(certData);
	if(crtn) {
		cssmPerror("CertParser.initWithData for issuer", crtn);
		return -1;
	}
	
	/* 
	 * One single request, no extensions
	 */
	SecAsn1OCSPRequest singleReq;
	memset(&singleReq, 0, sizeof(singleReq));
	SecAsn1OCSPCertID &certId = singleReq.reqCert;

	/* algId refers to the hash we'll perform in issuer name and key */
	certId.algId.algorithm = CSSMOID_SHA1;
	certId.algId.parameters.Data = nullParam;
	certId.algId.parameters.Length = sizeof(nullParam);
	
	/* SHA1(issuerName) */
	CSSM_DATA issuerName = {0, NULL};
	issuerName.Data = (uint8 *)parser.fieldForOid(CSSMOID_X509V1IssuerNameStd, 
		issuerName.Length);
	if(issuerName.Data == NULL) {
		printf("***Error fetching issuer name. Aborting.\n");
		return 1;
	}
	uint8 issuerNameHash[CC_SHA1_DIGEST_LENGTH];
	ocspdSha1(issuerName.Data, issuerName.Length, issuerNameHash);
	
	/* SHA1(issuer public key) */
	CSSM_KEY_PTR pubKey = NULL;
	CSSM_SIZE pubKeyLen = sizeof(CSSM_KEY);
	pubKey = (CSSM_KEY_PTR)issuerParser.fieldForOid(CSSMOID_CSSMKeyStruct, pubKeyLen);
	if(pubKey == NULL) {
		printf("***Error fetching public key from issuer cert. Aborting.\n");
		return 1;
	}
	uint8 pubKeyHash[CC_SHA1_DIGEST_LENGTH];
	ocspdSha1(pubKey->KeyData.Data, pubKey->KeyData.Length, pubKeyHash);

	/* serial number */
	CSSM_DATA serialNum = {0, NULL};
	serialNum.Data = (uint8 *)parser.fieldForOid(CSSMOID_X509V1SerialNumber, 
		serialNum.Length);
	if(serialNum.Data == NULL) {
		printf("***Error fetching serial number. Aborting.\n");
		return 1;
	}

	/* build the CertID from those components */
	certId.issuerNameHash.Data = issuerNameHash;
	certId.issuerNameHash.Length = CC_SHA1_DIGEST_LENGTH;
	certId.issuerPubKeyHash.Data = pubKeyHash;
	certId.issuerPubKeyHash.Length = CC_SHA1_DIGEST_LENGTH;	
	certId.serialNumber = serialNum;

	/* 
	 * Build top level request with one entry in requestList, no signature,
	 * one extension (a nonce)
	 */
	SecAsn1OCSPSignedRequest signedReq;
	SecAsn1OCSPRequest *reqArray[2] = { &singleReq, NULL };
	SecAsn1OCSPTbsRequest &tbs = signedReq.tbsRequest;
	memset(&signedReq, 0, sizeof(signedReq));
	uint8 version = 0;
	CSSM_DATA vers = {1, &version};
	tbs.version = &vers;
	tbs.requestList = reqArray;

	/* one extension - the nonce */
	SecAsn1CoderRef coder;
	SecAsn1CoderCreate(&coder);
	uint8 nonceBytes[8] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
	CSSM_DATA nonceData = {8, nonceBytes};
	OCSPNonce *nonce = new OCSPNonce(coder, false, nonceData);
	NSS_CertExtension *extenArray[2] = {nonce->nssExt(), NULL};
	tbs.requestExtensions = extenArray;
	
	/* Encode */
	OSStatus ortn;
	CSSM_DATA encoded = {0, NULL};
	ortn = SecAsn1EncodeItem(coder, &signedReq, kSecAsn1OCSPSignedRequestTemplate,
		&encoded);
	if(ortn) {
		printf("***Error encoding SecAsn1OCSPSignedRequest\n");
	}
	else {
		*outFile = (unsigned char *)malloc(encoded.Length);
		*outFileLen = encoded.Length;
		memmove(*outFile, encoded.Data, encoded.Length);
	}
	SecAsn1CoderRelease(coder);
	return (int)ortn;
}

static void dumpCertID(
	SecAsn1OCSPCertID *certID,
	int indent)
{
	doIndent(indent);
	printf("algId            : ");
	printDataAsHex(&certID->algId.algorithm);
	
	doIndent(indent);
	printf("issuerNameHash   : ");
	printDataAsHex(&certID->issuerNameHash);
	
	doIndent(indent);
	printf("issuerPubKeyHash : ");
	printDataAsHex(&certID->issuerPubKeyHash);
	
	doIndent(indent);
	printf("serialNumber     : ");
	printDataAsHex(&certID->serialNumber);
}

static void printCritical(
	int indent,
	OCSPExtension *ocspExt)
{
	doIndent(indent);
	printf("Critical           : %s\n", ocspExt->critical() ? "true" : "false");
}

static void printOcspExt(
	SecAsn1CoderRef coder,
	NSS_CertExtension *nssExt, 
	int indent)
{
	OCSPExtension *ocspExt = NULL;
	try {
		ocspExt = OCSPExtension::createFromNSS(coder, *nssExt);
	}
	catch(...) {
		doIndent(indent);
		printf("***Error thrown parsing extension\n");
		return;
	}
	switch(ocspExt->tag()) {
		case OET_Unknown: 
			doIndent(indent);
			printf("Extension type: Unknown\n");
			printCritical(indent, ocspExt);
			return;
		case OET_Nonce: 
		{
			doIndent(indent);
			printf("Extension type     : Nonce\n");
			printCritical(indent, ocspExt);
			doIndent(indent);
			OCSPNonce *nonce = dynamic_cast<OCSPNonce *>(ocspExt);
			if(nonce == NULL) {
				printf("***dynamic_cast failure in OCSPNonce!\n");
				return;
			}
			printf("nonce value        : ");
			printDataAsHex(&nonce->nonce());
			break;
		}
		case OET_CrlReference: 
			doIndent(indent);
			printf("Extension type     : CrlReference");
			printCritical(indent, ocspExt);
			/* TBD */
			return;
		case OET_AcceptResponse: 
			doIndent(indent);
			printf("Extension type     : AcceptResponse");
			printCritical(indent, ocspExt);
			/* TBD */
			return;
		case OET_ArchiveCutoff:
			doIndent(indent);
			printf("Extension type     : ArchiveCutoff");
			printCritical(indent, ocspExt);
			/* TBD */
			return;
		case OET_ServiceLocator:
			doIndent(indent);
			printf("Extension type     : ServiceLocator");
			printCritical(indent, ocspExt);
			/* TBD */
			return;
		default:
			/* this code is out of sync with ocspExtensions.{h,cpp} */
			doIndent(indent);
			printf("Extension type     : unrecognized - code sync error");
			printCritical(indent, ocspExt);
			return;
			
	}
}

static int parseOcspReq(
	CSSM_CL_HANDLE clHand, 
	unsigned char *inFile, 
	unsigned inFileLen,
	bool verbose)
{
	SecAsn1CoderRef coder;
	SecAsn1OCSPSignedRequest signedReq;
	SecAsn1OCSPTbsRequest &tbs = signedReq.tbsRequest;
	OSStatus ortn;
	int indent;
	unsigned numExts;
	unsigned numReqs;
	
	SecAsn1CoderCreate(&coder);
	memset(&signedReq, 0, sizeof(signedReq));
	
	ortn = SecAsn1Decode(coder, inFile, inFileLen, kSecAsn1OCSPSignedRequestTemplate,
		&signedReq);
	if(ortn) {
		printf("***Error decoding SecAsn1OCSPSignedRequest\n");
		goto errOut;
	}
	printf("SecAsn1OCSPSignedRequest:\n");
	
	printf("SecAsn1OCSPTbsRequest:\n");
	indent = 2;
	if(tbs.version) {
		doIndent(indent);
		printf("Version : ");
		printDataAsHex(tbs.version);
	}
	if(tbs.requestorName) {
		doIndent(indent);
		printf("NSS_GeneralName found; print it later maybe\n");
	}
	numReqs = ocspdArraySize((const void **)tbs.requestList);
	for(unsigned dex=0; dex<numReqs; dex++) {
		SecAsn1OCSPRequest *req = tbs.requestList[dex];
		doIndent(indent);
		printf("Request List Entry %u\n", dex);
		indent += 2;
		doIndent(indent);
		printf("CertID:\n");
		indent += 2;
		SecAsn1OCSPCertID *certID = &req->reqCert;
		dumpCertID(certID, indent);
		indent -= 2;
		numExts = ocspdArraySize((const void **)req->extensions);
		for(unsigned extDex=0; extDex<numExts; extDex++) {
			doIndent(indent);
			printf("singleExtension[%u]\n", extDex);
			printOcspExt(coder, req->extensions[dex], indent + 2);
		}
		indent -= 2;
	}

	numExts = ocspdArraySize((const void **)tbs.requestExtensions);
	for(unsigned extDex=0; extDex<numExts; extDex++) {
		doIndent(indent);
		printf("requestExtension[%u]\n", extDex);
		printOcspExt(coder, tbs.requestExtensions[extDex], indent + 2);
	}

	indent -= 2;
	
	if(signedReq.signature) {
		printf("SecAsn1OCSPSignature:\n");
		indent += 2;
		doIndent(indent);
		printf("==unparsed for now ==\n");
		/* ... */
		indent -= 2;
	}
errOut:
	SecAsn1CoderRelease(coder);
	return ortn;
}

static int genOcspResp(
	CSSM_CL_HANDLE clHand, 
	SecAsn1OCSPCertStatusTag status,
	CE_CrlReason reason,			// for CS_Revoked
	const unsigned char *subjectCert,
	unsigned subjectCertLen,
	const unsigned char *issuerCert,
	unsigned issuerCertLen,
	SecIdentityRef signer,
	unsigned char **outData,
	unsigned *outDataLen)
{
	SecAsn1CoderRef coder;
	SecAsn1CoderCreate(&coder);

	CSSM_DATA subjectCertData = {subjectCertLen, (uint8 *)subjectCert};
	CSSM_DATA issuerCertData = {issuerCertLen, (uint8 *)issuerCert};
	uint8 nonceBytes[8] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
	CSSM_DATA nonceData = {8, nonceBytes};
	CSSM_DATA tbs;
	CSSM_DATA encoded = {0, NULL};
	SecAsn1OCSPResponse topResponse;
	SecAsn1OCSPResponseBytes responseBytes;
	uint8 responseStatusByte;
	CSSM_DATA resp = {0, NULL};
	CSSM_DATA sig = {0, NULL};
	
	int irtn = genTbsResp(coder, clHand, status, reason, 
		subjectCertData, issuerCertData,
		0,			// thisUpdate
		2600 * 24,	// next update
		&nonceData,
		tbs);
	if(irtn) {
		printf("***Error encoding tbsResp\n");
		return irtn;
	}
	
	/* 
	 * That's the TBSResponseData. Sign it.
	 */
	OSStatus ortn;
	SecAsn1OCSPBasicResponse basicResp;
	memset(&basicResp, 0, sizeof(basicResp));
	ortn = ocspSign(signer, tbs, CSSM_ALGID_SHA1WithRSA, sig);
	if(ortn) {
		printf("***Error signing basicResponse.\n");
		goto errOut;
	}
	basicResp.algId.algorithm = CSSMOID_SHA1WithRSA;
	basicResp.algId.parameters.Data = nullParam;
	basicResp.algId.parameters.Length = sizeof(nullParam);
	basicResp.tbsResponseData = tbs;
	basicResp.sig = sig;
	/* ASN1 encoder needs to know length in bits */
	basicResp.sig.Length *= 8;
	/* no certs for now */
	/* encode SecAsn1OCSPBasicResponse */
	
	ortn = SecAsn1EncodeItem(coder, &basicResp, kSecAsn1OCSPBasicResponseTemplate,
		&encoded);
	if(ortn) {
		printf("***Error encoding SecAsn1OCSPBasicResponse\n");
	}
	
	/* put that into a SecAsn1OCSPResponse */
	responseBytes.responseType = CSSMOID_PKIX_OCSP_BASIC;
	responseBytes.response = encoded;
	responseStatusByte = RS_Success;
	topResponse.responseStatus.Data = &responseStatusByte;
	topResponse.responseStatus.Length = 1;
	topResponse.responseBytes = &responseBytes;
	ortn = SecAsn1EncodeItem(coder, &topResponse, kSecAsn1OCSPResponseTemplate,
		&resp);
	if(ortn) {
		printf("***Error encoding SecAsn1OCSPBasicResponse\n");
		goto errOut;
	}

	/* TA DA */
	*outData = (unsigned char *)malloc(resp.Length);
	*outDataLen = resp.Length;
	memmove(*outData, resp.Data, resp.Length);
errOut:
	SecAsn1CoderRelease(coder);
	if(sig.Data) {
		APP_FREE(sig.Data);
	}
	return ortn;
}

/* decode and parse tbsResponseData, sitting in SecAsn1OCSPBasicResponse as an 
 * ASN_ANY */
static int 	parseResponseData(
	SecAsn1CoderRef coder,
	int indent, 
	const CSSM_DATA &tbsResponseData)
{
	SecAsn1OCSPResponseData	respData;
	SecAsn1OCSPResponderID responderID;
	uint8 tag;
	const SecAsn1Template *templ;
	unsigned numExts;
	
	memset(&respData, 0, sizeof(respData));
	OSStatus ortn = SecAsn1DecodeData(coder, &tbsResponseData,
		kSecAsn1OCSPResponseDataTemplate, &respData);
	if(ortn) {
		printf("***Error decoding ResponseData\n");
		return 1;
	}
	if(respData.version && respData.version->Data) {
		doIndent(indent);
		printf("version: %u\n", respData.version->Data[0]);
	}
	doIndent(indent);
	printf("ResponderID:\n");
	indent += 2;
	memset(&responderID, 0, sizeof(responderID));
	if(respData.responderID.Data == NULL) {
		doIndent(indent);
		printf("***Malformed(empty)***\n");
		return 1;
	}
	
	/* lame-o choice processing */
	tag = respData.responderID.Data[0] & SEC_ASN1_TAGNUM_MASK;
	switch(tag) {
		case RIT_Name: 
			templ = kSecAsn1OCSPResponderIDAsNameTemplate; 
			break;
		case RIT_Key: 
			templ = kSecAsn1OCSPResponderIDAsKeyTemplate; 
			break;
		default:
			doIndent(indent);
			printf("**Unknown tag for ResponderID (%u)\n", tag);
			return 1;
	}
	ortn = SecAsn1DecodeData(coder, &respData.responderID, templ, &responderID);
	if(ortn) {
		doIndent(indent);
		printf("***Error decoding ResponderID\n");
		return 1;
	}
	doIndent(indent);
	switch(tag) {
		case RIT_Name:
			printf("byName:\n");
			printName((NSS_Name &)responderID.byName, indent + 2);
			break;
		case RIT_Key:
			printf("byKey : ");
			printDataAsHex(&responderID.byKey);
			break;
	}
	indent -= 2;		// end of ResponderID
	
	doIndent(indent);
	printf("producedAt: ");
	printString(&respData.producedAt);
	unsigned numResps = ocspdArraySize((const void **)respData.responses);
	doIndent(indent);
	printf("Num responses: %u\n", numResps);
	for(unsigned dex=0; dex<numResps; dex++) {
		SecAsn1OCSPSingleResponse *resp = respData.responses[dex];
		doIndent(indent);
		printf("Response %u:\n", dex);
		indent += 2;
		doIndent(indent);
		printf("CertID:\n");
		dumpCertID(&resp->certID, indent + 2);
		
		doIndent(indent);
		printf("certStatus: ");
		/* lame-o choice processing */
		tag = resp->certStatus.Data[0] & SEC_ASN1_TAGNUM_MASK;
		switch(tag) {
			case CS_Good:
				printf("Good\n");
				break;
			case CS_Unknown:
				printf("Unknown\n");
				break;
			default:
				printf("**MALFORMED cert status tag (%u)\n", tag);
				break;
			case CS_Revoked:
			{
				printf("Revoked\n");
				doIndent(indent);
				SecAsn1OCSPCertStatus certStatus;
				memset(&certStatus, 0, sizeof(certStatus));
				ortn = SecAsn1DecodeData(coder, &resp->certStatus, 
					kSecAsn1OCSPCertStatusRevokedTemplate, &certStatus);
				if(ortn) {
					doIndent(indent);
					printf("***error parsing RevokedInfo\n");
					break;
				}
				if(certStatus.revokedInfo == NULL) {
					doIndent(indent);
					printf("***GAK! Malformed (empty) revokedInfo\n");break;
				}
				printf("RevokedIndfo:\n");
				indent += 2;
				doIndent(indent);
				printf("revocationTime: ");
				printString(&certStatus.revokedInfo->revocationTime);
				if(certStatus.revokedInfo->revocationReason) {
					doIndent(indent);
					printf("reason: %u\n", 
						certStatus.revokedInfo->revocationReason->Data[0]);
				}
				indent -= 2;		// end of RevokedInfo
				break;
			}
		}	/* switch cert status tag */
		
		doIndent(indent);
		printf("thisUpdate: ");
		printString(&resp->thisUpdate);
		
		if(resp->nextUpdate) {
			doIndent(indent);
			printf("nextUpdate: ");
			printString(resp->nextUpdate);
		}

		numExts = ocspdArraySize((const void **)resp->singleExtensions);
		for(unsigned extDex=0; extDex<numExts; extDex++) {
			doIndent(indent);
			printf("singleExtensions[%u]\n", extDex);
			printOcspExt(coder, resp->singleExtensions[extDex], indent + 2);
		}
		
		indent -= 2;		// end of resp[dex]
	}
	
	numExts = ocspdArraySize((const void **)respData.responseExtensions);
	for(unsigned extDex=0; extDex<numExts; extDex++) {
		doIndent(indent);
		printf("responseExtensions[%u]\n", extDex);
		printOcspExt(coder, respData.responseExtensions[extDex], indent + 2);
	}
	return 0;
}

static int parseOcspResp(
	CSSM_CL_HANDLE clHand, 
	unsigned char *inFile, 
	unsigned inFileLen,
	bool verbose)
{
	SecAsn1OCSPResponse topResp;
	SecAsn1CoderRef coder;
	OSStatus ortn;
	int indent = 0;
	const char *str;
	SecAsn1OCSPBasicResponse basicResp;
	unsigned numCerts = 0;
	
	SecAsn1CoderCreate(&coder);	
	memset(&topResp, 0, sizeof(topResp));
	ortn = SecAsn1Decode(coder, inFile, inFileLen, kSecAsn1OCSPResponseTemplate,
		&topResp);
	if(ortn) {
		printf("***Error decoding SecAsn1OCSPResponse\n");
		goto errOut;
	}
	printf("OCSPResponse:\n");
	indent += 2;
	doIndent(indent);
	printf("responseStatus: ");
	if(topResp.responseStatus.Length == 0) {
		printf("**MALFORMED**\n");
	}
	else {
		switch(topResp.responseStatus.Data[0]) {
			case RS_Success: str = "RS_Success"; break;
			case RS_MalformedRequest: str = "RS_MalformedRequest"; break;
			case RS_InternalError: str = "RS_InternalError"; break;
			case RS_TryLater: str = "RS_TryLater"; break;
			case RS_Unused: str = "RS_Unused"; break;
			case RS_SigRequired: str = "RS_SigRequired"; break;
			case RS_Unauthorized: str = "RS_Unauthorized"; break;
			default: str = "MALFORMED (unknown enum)\n"; break;
		}
		printf("%s (%u(d))\n", str, topResp.responseStatus.Data[0]);
	}
	doIndent(indent);
	printf("ResponseBytes: ");
	if(topResp.responseBytes == NULL) {
		printf("empty\n");
		goto errOut;
	}
	printf("\n");
	indent += 2;
	doIndent(indent);
	printf("responseType: ");
	if(appCompareCssmData(&topResp.responseBytes->responseType,
			&CSSMOID_PKIX_OCSP_BASIC)) {
		str = "ocsp-basic";
	}
	else {
		str = "Unknown type\n";
	}
	printf("%s\n", str);
		
	/* decode the BasicOCSPResponse */
	memset(&basicResp, 0, sizeof(basicResp));
	ortn = SecAsn1DecodeData(coder, &topResp.responseBytes->response,
		kSecAsn1OCSPBasicResponseTemplate, &basicResp);
	if(ortn) {
		printf("***Error decoding BasicOCSPResponse\n");
		goto errOut;
	}
	
	doIndent(indent);
	printf("BasicOCSPResponse:\n");
	indent += 2;
	doIndent(indent);
	printf("ResponseData:\n");
	parseResponseData(coder, indent + 2, basicResp.tbsResponseData);
	doIndent(indent);
	printf("sig: ");
	printDataAsHex(&basicResp.sig, 8);
	numCerts = ocspdArraySize((const void **)basicResp.certs);
	doIndent(indent);
	printf("Num Certs: %u\n", numCerts);
	
	if(verbose) {
		for(unsigned dex=0; dex<numCerts; dex++) {
			printf("+++++++++++++++++++++++++ Cert %u +++++++++++++++++++++++++\n", dex);
			printCert(basicResp.certs[dex]->Data, basicResp.certs[dex]->Length, 
				CSSM_FALSE);
			printf("+++++++++++++++++++++++ End Cert %u +++++++++++++++++++++++\n", dex);
		}
	}
	indent -= 2;		// end of BasicOCSPResponse
	indent -= 2;		// end of ResponseBytes
	indent -= 2;		// end of OCSPResponse
errOut:
	SecAsn1CoderRelease(coder);
	return ortn;
}

static int postOcspReq(
	CSSM_CL_HANDLE clHand,
	const unsigned char *certFile, 
	unsigned certFileLen, 
	const unsigned char *issuerCertFile, 
	unsigned issuerCertFileLen, 
	const char *responderURI,
	bool doParse,
	bool verbose,
	unsigned char **outFile,		// RETURNED 
	unsigned *outFileLen)			// RETURNED
{
	auto_ptr<CertParser> subject;
	auto_ptr<CertParser> issuer;
	CSSM_DATA uriData = {0, NULL};
	CSSM_DATA *url = NULL;
	
	try {
		CSSM_DATA cdata = {certFileLen, (uint8 *)certFile};
		subject.reset(new CertParser(clHand, cdata));
	}
	catch(...) {
		printf("***Error parsing subject cert. Aborting.\n");
		return -1;
	}
	try {
		CSSM_DATA cdata = {issuerCertFileLen, (uint8 *)issuerCertFile};
		issuer.reset(new CertParser(clHand, cdata));
	}
	catch(...) {
		printf("***Error parsing issuer cert. Aborting.\n");
		return -1;
	}
	
	SecAsn1CoderRef coder;
	SecAsn1CoderCreate(&coder);
	/* subsequent errors to errOut: */
	int ourRtn = 0;
	const CSSM_DATA *derReq = NULL;
	auto_ptr<OCSPRequest> ocspReq;
	
	if(responderURI != NULL) {
		uriData.Data = (uint8 *)responderURI;
		uriData.Length = strlen(responderURI);
		url = &uriData;
	}
	else {
		/* get OCSP URL from subject cert */
		url = ocspUrlFromCert(*subject, coder);
		if(url == NULL) {
			printf("Sorry, no can do.\n");
			ourRtn = -1;
			goto errOut;
		}
	}
	
	/* create DER-encoded OCSP request for subject */
	try {
		ocspReq.reset(new OCSPRequest(*subject, *issuer, false));
		derReq = ocspReq->encode();
	}
	catch(...) {
		printf("***Error creating OCSP request. Aborting.\n");
		ourRtn = -1;
		goto errOut;
	}
	
	/* do it */
	CSSM_DATA ocspResp;
	CSSM_RETURN crtn;
	crtn = ocspdHttpPost(coder, *url, *derReq, ocspResp);
	if(crtn) {
		printf("***Error fetching OCSP response***\n");
		cssmPerror("ocspdHttpPost", crtn);
		ourRtn = -1;
		goto errOut;
	}
	*outFile = ocspResp.Data;
	*outFileLen = ocspResp.Length;
	if(doParse) {
		parseOcspResp(clHand, ocspResp.Data, ocspResp.Length, verbose);
	}
	/* copy out */
	*outFile = (unsigned char *)malloc(ocspResp.Length);
	*outFileLen = ocspResp.Length;
	memmove(*outFile, ocspResp.Data, ocspResp.Length);

errOut:
	SecAsn1CoderRelease(coder);
	return ourRtn;
}

typedef enum {
	op_genReq,
	op_parseReq,
	op_genReply,
	op_parseResp,
	op_post
} ocspOp;

int main(int argc, char **argv)
{
	if(argc < 2) {
		usage(argv);
	}
	ocspOp op;
	switch(argv[1][0]) {
		case 'g': op = op_genReq; break;
		case 'G': op = op_parseReq; break;
		case 'r': op = op_genReply; break;
		case 'R': op = op_parseResp; break;
		case 'p': op = op_post; break;
		default: usage(argv);
	}
	
	/* user defined vars */
	char *inFile = NULL;
	char *outFile = NULL;
	char *inCertName = NULL;
	char *issuerCertName = NULL;
	SecAsn1OCSPCertStatusTag certStatus = CS_Good;
	CE_CrlReason crlReason = CE_CR_Unspecified;
	char *kcName = NULL;
	bool verbose = false;
	bool doParse = false;
	const char *responderURI = NULL;
	
    extern int optind;
	optind = 2;
    extern char *optarg;
	int arg;
    while ((arg = getopt(argc, argv, "c:C:i:o:s:r:k:phvu:")) != -1) {
		switch (arg) {
			case 'c':
				inCertName = optarg;
				break;
			case 'C':
				issuerCertName = optarg;
				break;
			case 'i':
				inFile = optarg;
				break;
			case 'o':
				outFile = optarg;
				break;
			case 's':
				switch(optarg[0]) {
					case 'g':
						certStatus = CS_Good;
						break;
					case 'r':
						certStatus = CS_Revoked;
						break;
					case 'u':
						certStatus = CS_Unknown;
						break;
					default:
						printf("***Unrecognized certStatus***\n");
						usage(argv);
				}
				break;
			case 'r':
				crlReason = atoi(optarg);
				break;
			case 'k':
				kcName = optarg;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'p':
				doParse = true;
				break;
			case 'u':
				responderURI = optarg;
				break;
			default:
			case '?':
				usage(argv);
		}
	}
	if(optind != argc) {
		/* this happens if you give getopt() an arg which doesn't start with '-' */
		usage(argv);
	}
	
	unsigned char *certData = NULL;
	unsigned certDataLen = 0;
	unsigned char *issuerCertData = NULL;
	unsigned issuerCertDataLen = 0;
	unsigned char *inData = NULL;
	unsigned inDataLen = 0;
	unsigned char *outData = NULL;
	unsigned outDataLen = 0;
	SecKeychainRef kcRef = NULL;
	OSStatus ortn;
	
	if(inCertName) {
		if(readFile(inCertName, &certData, &certDataLen)) {
			printf("***Error reading cert file %s. Aborting.\n", inCertName);
			exit(1);
		}
	}
	if(issuerCertName) {
		if(readFile(issuerCertName, &issuerCertData, &issuerCertDataLen)) {
			printf("***Error reading cert file %s. Aborting.\n", issuerCertName);
			exit(1);
		}
	}
	if(inFile) {
		if(readFile(inFile, &inData, &inDataLen)) {
			printf("***Error reading input file %s. Aborting.\n", inFile);
			exit(1);
		}
	}
	if(kcName) {
		ortn = SecKeychainOpen(kcName, &kcRef);
		if(ortn) {
			cssmPerror("SecKeychainOpen", ortn);
			return ortn;
		}
	}
	CSSM_CL_HANDLE clHand = cuClStartup();
	
	switch(op) {
		case op_genReq:
			ortn = genOcspReq(clHand, certData, certDataLen, 
				issuerCertData, issuerCertDataLen,
				&outData, &outDataLen);
			break;
		case op_parseReq:
			ortn = parseOcspReq(clHand, inData, inDataLen, verbose);
			break;
		case op_genReply:
		{
			SecIdentityRef idRef = NULL;
			ortn = sslSimpleIdentPicker(kcRef, &idRef);
			if(ortn) {
				printf("***Error choosing identity. Aborting.\n");
				exit(1);
			}
			ortn = genOcspResp(clHand, certStatus, crlReason,
				certData, certDataLen, issuerCertData, issuerCertDataLen,
				idRef, &outData, &outDataLen);
			CFRelease(idRef);
			break;
		}
		case op_parseResp:
			ortn = parseOcspResp(clHand, inData, inDataLen, verbose);
			break;
		case op_post:
			ortn = postOcspReq(clHand, certData, certDataLen, 
				issuerCertData, issuerCertDataLen, responderURI,
				doParse, verbose,
				&outData, &outDataLen);
			break;
		default:	
			printf("Op %s is not yet implemented.\n", argv[1]);
			exit(1);
	}
	
	if(ortn == 0) {
		if(outData != NULL) {
			if(outFile== NULL) {
				printf("...generated %u bytes but no place to write it.\n", outDataLen);
			}
			else {
				ortn = writeFile(outFile, outData, outDataLen);
				if(ortn) {
					printf("***Error writing output to %s.\n", outFile);
				}
				else {
					printf("...wrote %u bytes to %s\n", outDataLen, outFile);
				}
			}
		}
	}
	return ortn;
}
