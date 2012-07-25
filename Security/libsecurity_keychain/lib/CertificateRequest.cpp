/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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

//
// CertificateRequest.cpp
//
#include <security_keychain/CertificateRequest.h>
#include <Security/oidsalg.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/cssmapi.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <string.h>
#include <dotMacTp.h>
#include <Security/oidsattr.h>
#include <security_utilities/simpleprefs.h>

/* one top-level prefs file for all of .mac cert requests */
#define DOT_MAC_REQ_PREFS	"com.apple.security.certreq"

/* 
 * Within that dictionary is a set of per-policy dictionaries; the key in the 
 * top-level prefs for these dictionaries is the raw policy OID data encoded
 * as an ASCII string. 
 *
 * Within one per-policy dictionary exists a number of per-user dictionaries,
 * with the username key as a string. Note that this user name, the one passed to the 
 * .mac server, does NOT have to have any relation to the current Unix user name; one 
 * Unix user can have multiple .mac accounts. 
 *
 *
 * Within the per-policy, per user dictionary are these two values, both stored
 * as raw data (CFData) blobs. 
 */
#define DOT_MAC_REF_ID_KEY	"refId"
#define DOT_MAC_CERT_KEY	"certificate"

/* Domain for .mac cert requests */
#define DOT_MAC_DOMAIN_KEY  "domain"
#define DOT_MAC_DOMAIN      "mac.com"

/* Hosts for .mac cert requests */
#define DOT_MAC_MGMT_HOST   "certmgmt"
#define DOT_MAC_INFO_HOST   "certinfo"

/*
 * Compare two CSSM_DATAs (or two CSSM_OIDs), return true if identical.
 */
bool nssCompareCssmData(
	const CSSM_DATA *data1,
	const CSSM_DATA *data2)
{	
	if((data1 == NULL) || (data1->Data == NULL) || 
	   (data2 == NULL) || (data2->Data == NULL) ||
	   (data1->Length != data2->Length)) {
		return false;
	}
	if(data1->Length != data2->Length) {
		return false;
	}
	if(memcmp(data1->Data, data2->Data, data1->Length) == 0) {
		return true;
	}
	else {
		return false;
	}
}

/* any nonzero value means true */
static bool attrBoolValue(
	const SecCertificateRequestAttribute *attr)
{
	if((attr->value.Data != NULL) &&
	   (attr->value.Length != 0) &&
	   (attr->value.Data[0] != 0)) {
		return true;
	}
	else {
		return false;
	}
}

static void tokenizeName(
		const CSSM_DATA		*inName,    /* required */
		CSSM_DATA			*outName,   /* required */
		CSSM_DATA			*outDomain) /* optional */
{
    if (!inName || !outName) return;
    CSSM_SIZE idx = 0;
    CSSM_SIZE stopIdx = inName->Length;
    uint8 *p = inName->Data;
    *outName = *inName;
    if (outDomain) {
        outDomain->Length = idx;
        outDomain->Data = p;
    }
    if (!p) return;
    while (idx < stopIdx) {
        if (*p++ == '@') {
            outName->Length = idx;
            if (outDomain) {
                outDomain->Length = inName->Length - (idx + 1);
                outDomain->Data = p;
            }
            break;
        }
        idx++;
    }
}

using namespace KeychainCore;

CertificateRequest::CertificateRequest(const CSSM_OID &policy,	
		CSSM_CERT_TYPE certificateType,
		CSSM_TP_AUTHORITY_REQUEST_TYPE requestType,
		SecKeyRef privateKeyItemRef,
		SecKeyRef publicKeyItemRef,
		const SecCertificateRequestAttributeList *attributeList,
		bool isNew /* = true */)
		:	mAlloc(Allocator::standard()),
			mTP(gGuidAppleDotMacTP),
			mCL(gGuidAppleX509CL),
			mPolicy(mAlloc, policy.Data, policy.Length),
			mCertType(certificateType),
			mReqType(requestType),
			mPrivKey(NULL),
			mPubKey(NULL),
			mEstTime(0),
			mRefId(mAlloc),
			mCertState(isNew ? CRS_New : CRS_Reconstructed),
			mCertData(mAlloc),
			mUserName(mAlloc),
			mPassword(mAlloc),
			mHostName(mAlloc),
			mDomain(mAlloc),
			mDoRenew(false),
			mIsAsync(false),
			mMutex(Mutex::recursive)
{
	StLock<Mutex>_(mMutex);
	certReqDbg("CertificateRequest construct");
	
	/* Validate policy OID. */
	if(!(nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_IDENTITY, &policy) ||
	     nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_EMAIL_SIGN, &policy) || 
	     nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_EMAIL_ENCRYPT, &policy) ||
	     nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_SHARED_SERVICES, &policy))) {
		certReqDbg("CertificateRequest(): unknown policy oid");
		MacOSError::throwMe(paramErr);
	}
	if(privateKeyItemRef) {
		mPrivKey = privateKeyItemRef;
		CFRetain(mPrivKey);
	}
	if(publicKeyItemRef) {
		mPubKey = publicKeyItemRef;
		CFRetain(mPubKey);
	}

	/* parse attr array */
	if(attributeList == NULL) {
		return;
	}
	
	bool doPendingRequest = false;
	for(unsigned dex=0; dex<attributeList->count; dex++) {
		const SecCertificateRequestAttribute *attr = &attributeList->attr[dex];
		
		if((attr->oid.Data == NULL) || (attr->value.Data == NULL)) {
			MacOSError::throwMe(paramErr);
		}
		if(nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_VALUE_USERNAME, &attr->oid)) {
            CSSM_DATA userName = { 0, NULL };
            CSSM_DATA domainName = { 0, NULL };
            tokenizeName(&attr->value, &userName, &domainName);
            if (!domainName.Length || !domainName.Data) {
                domainName.Length = strlen(DOT_MAC_DOMAIN);
                domainName.Data = (uint8*) DOT_MAC_DOMAIN;
            }
			mUserName.copy(userName);
            mDomain.copy(domainName);
		}
		else if(nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_VALUE_PASSWORD, &attr->oid)) {
			mPassword.copy(attr->value);
		}
		else if(nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_VALUE_HOSTNAME, &attr->oid)) {
			mHostName.copy(attr->value);
		}
		else if(nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_VALUE_RENEW, &attr->oid)) {
			/* 
			 * any nonzero value means true 
			 * FIXME: this is deprecated, Treadstone doesn't allow this. Reject this 
			 * request? Ignore?
			 */
			mDoRenew = attrBoolValue(attr);
		}
		else if(nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_VALUE_ASYNC, &attr->oid)) {
			/* any nonzero value means true */
			mIsAsync = attrBoolValue(attr);
		}
		else if(nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_VALUE_IS_PENDING, &attr->oid)) {
			/* any nonzero value means true */
			doPendingRequest = attrBoolValue(attr);
		}
		
		else {
			certReqDbg("CertificateRequest(): unknown name/value oid");
			MacOSError::throwMe(paramErr);
		}
	}
	if(mCertState == CRS_Reconstructed) {
		/* see if we have a refId or maybe even a cert in prefs */
		retrieveResults();
		if(mCertData.data() != NULL) {
			mCertState = CRS_HaveCert;
		}
		else if(mRefId.data() != NULL) {
			mCertState = CRS_HaveRefId;
		}
		else if(doPendingRequest) {
			/* ask the server if there's a request pending */
			postPendingRequest();
			/* NOT REACHED - that always throws */
		}
		else {
			certReqDbg("CertificateRequest(): nothing in prefs");
			/* Nothing found in prefs; nothing to go by */
			MacOSError::throwMe(errSecItemNotFound);
		}
	}
}

CertificateRequest::~CertificateRequest() throw()
{
	StLock<Mutex>_(mMutex);
	certReqDbg("CertificateRequest destruct");
	
	if(mPrivKey) {
		CFRelease(mPrivKey);
	}
	if(mPubKey) {
		CFRelease(mPubKey);
	}
}

#pragma mark ----- cert request submit -----

void CertificateRequest::submit(
	sint32 *estimatedTime)
{
	StLock<Mutex>_(mMutex);
	CSSM_DATA &policy = mPolicy.get();
	if(nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_IDENTITY, &policy) ||
	   nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_EMAIL_SIGN, &policy) ||
	   nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_EMAIL_ENCRYPT, &policy) ||
	   nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_SHARED_SERVICES, &policy)) {
		return submitDotMac(estimatedTime);
	}
	else {
		/* shouldn't be here, we already validated policy in constructor */
		assert(0);
		certReqDbg("CertificateRequest::submit(): bad policy");
		MacOSError::throwMe(paramErr);
	}
}

void CertificateRequest::submitDotMac(
	sint32 *estimatedTime)
{
	StLock<Mutex>_(mMutex);
	CSSM_RETURN							crtn;
	CSSM_TP_AUTHORITY_ID				tpAuthority;
	CSSM_TP_AUTHORITY_ID				*tpAuthPtr = NULL;
	CSSM_NET_ADDRESS					tpNetAddrs;
	CSSM_APPLE_DOTMAC_TP_CERT_REQUEST	certReq;
	CSSM_TP_REQUEST_SET					reqSet;
	CSSM_CSP_HANDLE						cspHand = 0;
	CSSM_X509_TYPE_VALUE_PAIR			tvp;
	CSSM_TP_CALLERAUTH_CONTEXT			callerAuth;
	CSSM_FIELD							policyField;
	CSSM_DATA							refId = {0, NULL};
	const CSSM_KEY						*privKey;
	const CSSM_KEY						*pubKey;
	OSStatus							ortn;
	
	if(mCertState != CRS_New) {
		certReqDbg("CertificateRequest: can only submit a new request");
		MacOSError::throwMe(paramErr);
	}
	if((mUserName.data() == NULL) || (mPassword.data() == NULL)) {
		certReqDbg("CertificateRequest: user name and password required");
		MacOSError::throwMe(paramErr);
	}

	/* get keys and CSP handle in CSSM terms */
	if((mPrivKey == NULL) || (mPubKey == NULL)) {
		certReqDbg("CertificateRequest: pub and priv keys required");
		MacOSError::throwMe(paramErr);
	}
	ortn = SecKeyGetCSSMKey(mPrivKey, &privKey);
	if(ortn) {
		MacOSError::throwMe(ortn);
	}
	ortn = SecKeyGetCSSMKey(mPubKey, &pubKey);
	if(ortn) {
		MacOSError::throwMe(ortn);
	}
	ortn = SecKeyGetCSPHandle(mPrivKey, &cspHand);
	if(ortn) {
		MacOSError::throwMe(ortn);
	}
	
	/*
	 * CSSM_X509_TYPE_VALUE_PAIR_PTR - one pair for now.
	 * Caller passes in user name like "johnsmith"; in the CSR,
	 * we write "johnsmith@mac.com".
	 */
	tvp.type = CSSMOID_CommonName;
	tvp.valueType = BER_TAG_PKIX_UTF8_STRING;
	CssmAutoData fullUserName(mAlloc);
	unsigned nameLen = mUserName.length();
	unsigned domainLen = mDomain.length();
	fullUserName.malloc(nameLen + 1 + domainLen);
	tvp.value = fullUserName.get();
	memmove(tvp.value.Data, mUserName.data(), nameLen);
	memmove(tvp.value.Data + nameLen, "@", 1);
	memmove(tvp.value.Data + nameLen + 1, mDomain.data(), domainLen);
	
	/* Fill in the CSSM_APPLE_DOTMAC_TP_CERT_REQUEST */
	memset(&certReq, 0, sizeof(certReq));
	certReq.version = CSSM_DOT_MAC_TP_REQ_VERSION;
	certReq.cspHand = cspHand;
	certReq.clHand = mCL->handle();
	certReq.numTypeValuePairs = 1;
	certReq.typeValuePairs = &tvp;
	certReq.publicKey = const_cast<CSSM_KEY_PTR>(pubKey);
	certReq.privateKey = const_cast<CSSM_KEY_PTR>(privKey);
	certReq.userName = mUserName.get();
	certReq.password = mPassword.get();
	if(mDoRenew) {
		certReq.flags |= CSSM_DOTMAC_TP_SIGN_RENEW;
	}
	/* we don't deal with CSR here, input or output */
	
	/* now the rest of the args for CSSM_TP_SubmitCredRequest() */
	reqSet.Requests = &certReq;	
	reqSet.NumberOfRequests = 1;
	policyField.FieldOid = mPolicy;
	policyField.FieldValue.Data = NULL;
	policyField.FieldValue.Length = 0;
	memset(&callerAuth, 0, sizeof(callerAuth));
	callerAuth.Policy.NumberOfPolicyIds = 1;
	callerAuth.Policy.PolicyIds = &policyField;
	ortn = SecKeyGetCredentials(mPrivKey,
		CSSM_ACL_AUTHORIZATION_SIGN,
		kSecCredentialTypeDefault,
		const_cast<const CSSM_ACCESS_CREDENTIALS **>(&callerAuth.CallerCredentials));
	if(ortn) {
		certReqDbg("CertificateRequest: SecKeyGetCredentials error");
		MacOSError::throwMe(ortn);
	}

	CssmAutoData hostName(mAlloc);
    tpAuthority.AuthorityCert = NULL;
    tpAuthority.AuthorityLocation = &tpNetAddrs;
    tpNetAddrs.AddressType = CSSM_ADDR_NAME;
	if(mHostName.data() != NULL) {
		tpNetAddrs.Address = mHostName.get();
	} else {
        unsigned hostLen = strlen(DOT_MAC_MGMT_HOST);
        hostName.malloc(hostLen + 1 + domainLen);
        tpNetAddrs.Address = hostName.get();
        memmove(tpNetAddrs.Address.Data, DOT_MAC_MGMT_HOST, hostLen);
        memmove(tpNetAddrs.Address.Data + hostLen, ".", 1);
        memmove(tpNetAddrs.Address.Data + hostLen + 1, mDomain.data(), domainLen);
    }
    tpAuthPtr = &tpAuthority;

	/* go */
	crtn = CSSM_TP_SubmitCredRequest(mTP->handle(),
		tpAuthPtr,		
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,	
		&reqSet,	
		&callerAuth,
		&mEstTime,   
		&refId);	// CSSM_DATA_PTR ReferenceIdentifier

	/* handle return, store results */
	switch(crtn) {
		case CSSM_OK:
			/* refID is a cert, we have to store it in prefs for later retrieval. */
			certReqDbg("submitDotMac: full success, storing cert");
			if(!mIsAsync) {
				/* store in prefs if not running in async mode */
				ortn = storeResults(NULL, &refId);
				if(ortn) {
					crtn = ortn;
				}
			}
			/* but keep a local copy too */
			mCertData.copy(refId);
			mCertState = CRS_HaveCert;
			if(estimatedTime) {
				/* it's ready right now */
				*estimatedTime = 0;
			}
			break;

		case CSSMERR_APPLE_DOTMAC_REQ_QUEUED:
			/* refID is the blob we use in CSSM_TP_RetrieveCredResult() */
			certReqDbg("submitDotMac: queued, storing refId");
			mRefId.copy(refId);
			/* return success - this crtn is not visible at API */
			crtn = CSSM_OK;
			if(!mIsAsync) {
				/* store in prefs if not running in async mode */
				ortn = storeResults(&refId, NULL);
				if(ortn) {
					crtn = ortn;
				}
			}
			mCertState = CRS_HaveRefId;
			if(estimatedTime) {
				*estimatedTime = mEstTime;
			}
			break;

		case CSSMERR_APPLE_DOTMAC_REQ_REDIRECT:
			/* refID is a URL, caller obtains via getReturnData() */
			certReqDbg("submitDotMac: redirect");
			mRefId.copy(refId);
			mCertState = CRS_HaveOtherData;
			break;
			
		default:
			/* all others are fatal errors, thrown below */
			break;
	}
	if(refId.Data) {
		/* mallocd on our behalf by TP */
		free(refId.Data);
	}
	if(crtn) {
		CssmError::throwMe(crtn);
	}
}

#pragma mark ----- cert request get result -----

void CertificateRequest::getResult(
	sint32			*estimatedTime,		// optional
	CssmData		&certData)	
{
	StLock<Mutex>_(mMutex);
	CSSM_DATA &policy = mPolicy.get();
	if(nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_IDENTITY, &policy) ||
	   nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_EMAIL_SIGN, &policy) ||
	   nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_EMAIL_ENCRYPT, &policy) || 
	   nssCompareCssmData(&CSSMOID_DOTMAC_CERT_REQ_SHARED_SERVICES, &policy)) {
		return getResultDotMac(estimatedTime, certData);
	}
	else {
		/* shouldn't be here, we already validated policy in constructor */
		assert(0);
		certReqDbg("CertificateRequest::getResult(): bad policy");
		MacOSError::throwMe(paramErr);
	}
}

void CertificateRequest::getResultDotMac(
	sint32			*estimatedTime,		// optional
	CssmData		&certData)		
{
	StLock<Mutex>_(mMutex);
	switch(mCertState) {
		case CRS_HaveCert:
			/* trivial case, we already have what caller is looking for */
			certReqDbg("getResultDotMac: have the cert right now");
			assert(mCertData.data() != NULL);
			certData = mCertData.get();
			if(estimatedTime) {
				*estimatedTime = 0;
			}
			break;
		case CRS_HaveRefId:
		{
			/* ping the server */
			certReqDbg("getResultDotMac: CRS_HaveRefId; polling server");
			assert(mRefId.data() != NULL);
			CSSM_BOOL ConfirmationRequired;
			CSSM_TP_RESULT_SET_PTR resultSet = NULL;
			CSSM_RETURN crtn;
	
			crtn = CSSM_TP_RetrieveCredResult(mTP->handle(), 
				&mRefId.get(),
				NULL,				// CallerAuthCredentials
				&mEstTime, 
				&ConfirmationRequired, 
				&resultSet);
			switch(crtn) {
				case CSSM_OK:
					break;
				case CSSMERR_TP_CERT_NOT_VALID_YET:
					/* 
					 * By convention, this means "not ready yet".
					 * The dot mac server does not have a way of telling us the 
					 * estimated time on a straight lookup like this (we only get
					 * an estimated completion time on the initial request), so we
					 * fake it. 
					 */
					certReqDbg("getResultDotMac: polled server, not ready yet");
					if(estimatedTime) {
						*estimatedTime = (mEstTime) ? mEstTime : 1;
					}
					MacOSError::throwMe(CSSMERR_APPLE_DOTMAC_REQ_IS_PENDING);
				default:
					certReqDbg("CSSM_TP_RetrieveCredResult error");
					CssmError::throwMe(crtn);
			}
			if(resultSet == NULL) {
				certReqDbg("***CSSM_TP_RetrieveCredResult OK, but no result set");
				MacOSError::throwMe(internalComponentErr);
			}
			if(resultSet->NumberOfResults != 1) {
				certReqDbg("***CSSM_TP_RetrieveCredResult OK, NumberOfResults (%lu)",
					(unsigned long)resultSet->NumberOfResults);
				MacOSError::throwMe(internalComponentErr);
			}
			if(resultSet->Results == NULL) {
				certReqDbg("***CSSM_TP_RetrieveCredResult OK, but empty result set");
				MacOSError::throwMe(internalComponentErr);
			}
			certReqDbg("getResultDotMac: polled server, SUCCESS");
			CSSM_DATA_PTR result = (CSSM_DATA_PTR)resultSet->Results;
			if(result->Data == NULL) {
				certReqDbg("***CSSM_TP_RetrieveCredResult OK, but empty result");
				MacOSError::throwMe(internalComponentErr);
			}
			mCertData.copy(*result);
			certData = mCertData.get();
			mCertState = CRS_HaveCert;
			if(estimatedTime) {
				*estimatedTime = 0;
			}
			
			/* 
			 * Free the stuff allocated on our behalf by TP. 
			 * FIXME - are we sure CssmClient is using alloc, free, etc.?
			 */
			free(result->Data);
			free(result);
			free(resultSet);
			break;
		}
		default:
			/* what do we do with this? */
			certReqDbg("CertificateRequest::getResultDotMac(): bad state");
			MacOSError::throwMe(internalComponentErr);
	}
	
	/*
	 * One more thing: once we pass a cert back to caller, we erase
	 * the record of this transaction from prefs. 
	 */
	assert(mCertData.data() != NULL);
	assert(mCertData.data() == certData.Data);
	removeResults();
}

/* 
 * Obtain policy/error specific return data blob. We own the data, it's
 * not copied. 
 */
void CertificateRequest::getReturnData(
	CssmData	&rtnData)
{
	StLock<Mutex>_(mMutex);
	rtnData = mRefId.get();
}

#pragma mark ----- preferences support -----

/* Current user as CFString, for use as key in per-policy dictionary */
CFStringRef CertificateRequest::createUserKey()
{
	StLock<Mutex>_(mMutex);
	return CFStringCreateWithBytes(NULL, (UInt8 *)mUserName.data(), mUserName.length(), 
		kCFStringEncodingUTF8, false);
}

#define MAX_OID_LEN	2048		// way big... */

/* current policy as CFString, for use as key in prefs dictionary */
CFStringRef CertificateRequest::createPolicyKey()
{
	StLock<Mutex>_(mMutex);
	char oidstr[MAX_OID_LEN];
	unsigned char *inp = (unsigned char *)mPolicy.data();
	char *outp = oidstr;
	unsigned len = mPolicy.length();
	for(unsigned dex=0; dex<len; dex++) {
		sprintf(outp, "%02X ", *inp++);
		outp += 3;
	}
	return CFStringCreateWithBytes(NULL, (UInt8 *)oidstr, len * 3,
		kCFStringEncodingUTF8, false);
}

/* 
 * Store cert data or refId in prefs. If both are NULL, delete the 
 * user dictionary entry from the policy dictionary if there, and then 
 * delete the policy dictionary if it's empty. 
 */
OSStatus CertificateRequest::storeResults(
	const CSSM_DATA		*refId,			// optional, for queued requests
	const CSSM_DATA		*certData)		// optional, for immediate completion
{
	StLock<Mutex>_(mMutex);
	assert(mPolicy.data() != NULL);
	assert(mUserName.data() != NULL);
	assert(mDomain.data() != NULL);
	
	bool deleteEntry = ((refId == NULL) && (certData == NULL));
	
	/* get a mutable copy of the existing prefs, or a fresh empty one */
	MutableDictionary *prefsDict = MutableDictionary::CreateMutableDictionary(DOT_MAC_REQ_PREFS, Dictionary::US_User);
	if (prefsDict == NULL)
	{
		prefsDict = new MutableDictionary();
	}
	
	/* get a mutable copy of the dictionary for this policy, or a fresh empty one */
	CFStringRef policyKey = createPolicyKey();
	MutableDictionary *policyDict = prefsDict->copyMutableDictValue(policyKey);
	
	CFStringRef userKey = createUserKey();
	if(deleteEntry) {
		/* remove user dictionary from this policy dictionary */
		policyDict->removeValue(userKey);
	}
	else {
		/* get a mutable copy of the dictionary for this user, or a fresh empty one */
		MutableDictionary *userDict = policyDict->copyMutableDictValue(userKey);
        
        CFStringRef domainKey = CFStringCreateWithBytes(NULL, (UInt8 *)mDomain.data(), mDomain.length(), kCFStringEncodingUTF8, false);
        userDict->setValue(CFSTR(DOT_MAC_DOMAIN_KEY), domainKey);
        CFRelease(domainKey);
	
		/* write refId and/or cert --> user dictionary */
		if(refId) {
			userDict->setDataValue(CFSTR(DOT_MAC_REF_ID_KEY), refId->Data, refId->Length);
		}
		if(certData) {
			userDict->setDataValue(CFSTR(DOT_MAC_CERT_KEY), certData->Data, certData->Length);
		}

		/* new user dictionary --> policy dictionary */
		policyDict->setValue(userKey, userDict->dict());
		delete userDict;
	}
	CFRelease(userKey);
	
	/* new policy dictionary to prefs dictionary, or nuke it */
	if(policyDict->count() == 0) {
		prefsDict->removeValue(policyKey);
	}
	else {
		prefsDict->setValue(policyKey, policyDict->dict());
	}
	CFRelease(policyKey);
	delete policyDict;
	
	/* prefs --> disk */
	OSStatus ortn = noErr;
	if(!prefsDict->writePlistToPrefs(DOT_MAC_REQ_PREFS, Dictionary::US_User)) {
		certReqDbg("storeResults: error writing prefs to disk");
		ortn = ioErr;
	}
	delete prefsDict;
	return ortn;
}

/* 
 * Attempt to fetch mCertData or mRefId from preferences. 
 */
void CertificateRequest::retrieveResults()
{
	StLock<Mutex>_(mMutex);
	assert(mPolicy.data() != NULL);
	assert(mUserName.data() != NULL);
	
	/* get the .mac cert prefs as a dictionary */
	Dictionary *pd = Dictionary::CreateDictionary(DOT_MAC_REQ_PREFS, Dictionary::US_User);
	if (pd == NULL)
	{
		certReqDbg("retrieveResults: no prefs found");
		return;
	}
	
	auto_ptr<Dictionary> prefsDict(pd);

	/* get dictionary for current policy */
	CFStringRef policyKey = createPolicyKey();
	Dictionary *policyDict = prefsDict->copyDictValue(policyKey);
	CFRelease(policyKey);
	if(policyDict != NULL) {
		/* dictionary for user */
		CFStringRef userKey = createUserKey();
		Dictionary *userDict = policyDict->copyDictValue(userKey);
		if(userDict != NULL) {
			/* is there a cert in there? */
			CFDataRef val = userDict->getDataValue(CFSTR(DOT_MAC_CERT_KEY));
			if(val) {
				mCertData.copy(CFDataGetBytePtr(val), CFDataGetLength(val));
			}
			
			/* how about refId? */
			val = userDict->getDataValue(CFSTR(DOT_MAC_REF_ID_KEY));
			if(val) {
				mRefId.copy(CFDataGetBytePtr(val), CFDataGetLength(val));
			}
			delete userDict;
		}
		CFRelease(userKey);
		delete policyDict; 
	}
}
	
/*
 * Remove all trace of current policy/user. Called when we successfully transferred
 * the cert back to caller. 
 */
void CertificateRequest::removeResults()
{
	StLock<Mutex>_(mMutex);
	assert(mPolicy.data() != NULL);
	assert(mUserName.data() != NULL);
	storeResults(NULL, NULL);
}

/* 
 * Have the TP ping the server to see of there's a request pending for the current
 * user. Always throws: either 
 * CSSMERR_APPLE_DOTMAC_REQ_IS_PENDING  -- request pending
 * CSSMERR_APPLE_DOTMAC_NO_REQ_PENDING  -- no request pending
 * paramErr -- no user, no password
 * other gross errors, e.g. ioErr for server connection failure
 *
 * The distinguishing features about this TP request are:
 *
 * policy OID = CSSMOID_DOTMAC_CERT_REQ_{IDENTITY,EMAIL_SIGN,EMAIL_ENCRYPT,SHARED_SERVICES}
 * CSSM_TP_AUTHORITY_REQUEST_TYPE = CSSM_TP_AUTHORITY_REQUEST_CERTLOOKUP
 * CSSM_APPLE_DOTMAC_TP_CERT_REQUEST.flags = CSSM_DOTMAC_TP_IS_REQ_PENDING
 * must have userName and password
 * hostname optional as usual 
 */
void CertificateRequest::postPendingRequest()
{
	StLock<Mutex>_(mMutex);
	CSSM_RETURN							crtn;
	CSSM_TP_AUTHORITY_ID				tpAuthority;
	CSSM_TP_AUTHORITY_ID				*tpAuthPtr = NULL;
	CSSM_NET_ADDRESS					tpNetAddrs;
	CSSM_APPLE_DOTMAC_TP_CERT_REQUEST	certReq;
	CSSM_TP_REQUEST_SET					reqSet;
	CSSM_TP_CALLERAUTH_CONTEXT			callerAuth;
	CSSM_FIELD							policyField;
	CSSM_DATA							refId = {0, NULL};
	
	assert(mCertState == CRS_Reconstructed);
	if((mUserName.data() == NULL) || (mPassword.data() == NULL)) {
		certReqDbg("postPendingRequest: user name and password required");
		MacOSError::throwMe(paramErr);
	}
	
	/* Fill in the CSSM_APPLE_DOTMAC_TP_CERT_REQUEST */
	memset(&certReq, 0, sizeof(certReq));
	certReq.version = CSSM_DOT_MAC_TP_REQ_VERSION;
	certReq.userName = mUserName.get();
	certReq.password = mPassword.get();
	certReq.flags = CSSM_DOTMAC_TP_IS_REQ_PENDING;
	
	/* now the rest of the args for CSSM_TP_SubmitCredRequest() */
	reqSet.Requests = &certReq;	
	reqSet.NumberOfRequests = 1;
	/* 
	 * This OID actually doesn't matter - right? This RPC doesn't know about 
	 * which request we seek... 
	 */
	policyField.FieldOid = mPolicy;
	policyField.FieldValue.Data = NULL;
	policyField.FieldValue.Length = 0;
	memset(&callerAuth, 0, sizeof(callerAuth));
	callerAuth.Policy.NumberOfPolicyIds = 1;
	callerAuth.Policy.PolicyIds = &policyField;
	/* no other creds here */

	if(mHostName.data() != NULL) {
		tpAuthority.AuthorityCert = NULL;
		tpAuthority.AuthorityLocation = &tpNetAddrs;
		tpNetAddrs.AddressType = CSSM_ADDR_NAME;
		tpNetAddrs.Address = mHostName.get();
		tpAuthPtr = &tpAuthority;
	}

	/* go */
	crtn = CSSM_TP_SubmitCredRequest(mTP->handle(),
		tpAuthPtr,		
		CSSM_TP_AUTHORITY_REQUEST_CERTLOOKUP,	
		&reqSet,	
		&callerAuth,
		&mEstTime,   
		&refId);	// CSSM_DATA_PTR ReferenceIdentifier

	if(refId.Data) {
		/* shouldn't be any but just in case.... */
		free(refId.Data);
	}
	switch(crtn) {
		case CSSMERR_APPLE_DOTMAC_REQ_IS_PENDING:
			certReqDbg("postPendingRequest: REQ_IS_PENDING");
			break;
		case CSSMERR_APPLE_DOTMAC_NO_REQ_PENDING:
			certReqDbg("postPendingRequest: NO_REQ_PENDING");
			break;
		case CSSM_OK:
			/* should never happen */
			certReqDbg("postPendingRequest: unexpected success!");
			crtn = internalComponentErr;
			break;
		default:
			certReqDbg("postPendingRequest: unexpected rtn %lu", (unsigned long)crtn);
			break;
	}
	CssmError::throwMe(crtn);
}

