/*
 * grunt-quality p12 parse tool.
 *
 * The PFX ripper in this file uses, and always will use, the 
 * app-space reference PBE and crypto routines in p12Crypto.{h,cpp} 
 * and p12pbe.{h,cpp} in this directory. 
 */
#include "SecNssCoder.h"
#include <Security/asn1Templates.h>
#include <security_asn1/nssUtils.h>
#include <security_cdsa_utils/cuOidParser.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <security_pkcs12/pkcs7Templates.h>
#include <security_pkcs12/pkcs12Templates.h>
#include <security_pkcs12/pkcs12Utils.h>
#include "p12Crypto.h"
#include <security_cdsa_utils/cuFileIo.h>
#include "pkcs12Parsed.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <Security/oidsattr.h>

/*
 * The stuff which gets passed around to all parse modules
 */
class P12ParseInfo
{
public:
	P12ParseInfo(SecNssCoder &coder,
		CSSM_CSP_HANDLE cspHand,
		OidParser &parser,
		/* NULL means don't verify MAC, don't decrypt */
		CFStringRef macPwd,	
		/* if this second pwd is absent, use macPwd for both */
		CFStringRef encrPwd,		
		P12Parsed &parsed)			// destination
			: mCoder(coder),
			mCspHand(cspHand),
			mParser(parser),
			mParsed(parsed)
		{
			pwdToUcode(macPwd, mPwd);
			pwdToUcode(encrPwd, mEncrPwd);
		}
	
	~P12ParseInfo() {}
	
	void pwdToUcode(CFStringRef str, CSSM_DATA &pwd);
	
	SecNssCoder			&mCoder;
	CSSM_CSP_HANDLE 	mCspHand;
	OidParser 			&mParser;
	CSSM_DATA			mPwd;		// unicode, double null terminated
	CSSM_DATA			mEncrPwd;
	P12Parsed 			&mParsed;	// destination

};

void P12ParseInfo::pwdToUcode(
	CFStringRef str,
	CSSM_DATA &pwd)
{
	if(str == NULL) {
		pwd.Data = NULL;
		pwd.Length = 0;
		return;
	}
	CFIndex len = CFStringGetLength(str);
	mCoder.allocItem(pwd, (len * sizeof(UniChar)) + 2);
	uint8 *cp = pwd.Data;
	for(CFIndex dex=0; dex<len; dex++) {
		UniChar uc = CFStringGetCharacterAtIndex(str, dex);
		*cp++ = uc >> 8;
		*cp++ = uc & 0xff;
	}
	*cp++ = 0;
	*cp++ = 0;
}

static void doIndent(unsigned depth)
{
	for(unsigned i=0; i<depth; i++) {
		putchar(' ');
	}
}

/* thread-unsafe oid-to-string converter */
static char oidStrBuf[OID_PARSER_STRING_SIZE];

static const char *oidStr(
	const CSSM_OID &oid,
	OidParser &parser)
{
	parser.oidParse(oid.Data, oid.Length, oidStrBuf);
	return oidStrBuf;
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

static void printDataAsUnichars(
	const CSSM_DATA &data)
{
	if(data.Length & 1) {
		printf("Unicode can not have odd number of bytes\n");
		return;
	}
	/* don't assume anything endian... */
	unsigned strLen = data.Length / 2;
	UniChar *uc = (UniChar *)malloc(strLen * sizeof(UniChar));
	const uint8 *inp = data.Data;
	UniChar *outp = uc;
	while(inp < (data.Data + data.Length)) {
		*outp = (((unsigned)inp[0]) << 8) | inp[1];
		outp++;
		inp += 2;
	}
	char *outStr = NULL;
	CFStringRef cstr = CFStringCreateWithCharacters(NULL, uc, strLen);
	if(cstr == NULL) {
		printf("***Error creating CFString from unichars\n");
		goto errOut;
	}
	
	outStr = (char *)malloc(strLen + 1);
	if(CFStringGetCString(cstr, outStr, strLen + 1, kCFStringEncodingASCII)) {
		printf("%s\n", outStr);
	}
	else {
		printf("***Error converting from unicode to ASCII\n");
	}
errOut:
	free(uc);
	if(outStr) {
		free(outStr);
	}
	CFRelease(cstr);
	return;
}

uint32 dataToInt(
	const CSSM_DATA &cdata)
{
	if((cdata.Length == 0) || (cdata.Data == NULL)) {
		return 0;
	}
	uint32 len = cdata.Length;
	if(len > sizeof(uint32)) {
		printf("***Bad formatting for DER integer\n");
		len = sizeof(uint32);
	}
	
	uint32 rtn = 0;
	uint8 *cp = cdata.Data;
	for(uint32 i=0; i<len; i++) {
		rtn = (rtn << 8) | *cp++;
	}
	return rtn;
}

#ifdef old_and_in_the_way
static int writeAuthSafeContent(
	const CSSM_DATA &rawBlob, 
	const char *outFile,
	SecNssCoder &coder,
	OidParser &parser)
{
	NSS_P12_RawPFX pfx;
	memset(&pfx, 0, sizeof(pfx));
	if(coder.decodeItem(rawBlob, NSS_P12_RawPFXTemplate, &pfx)) {
		printf("***Error on top-level decode of NSS_P12_RawPFX\n");
		return 1;
	}
	printf("...version = %u\n", (unsigned)dataToInt(pfx.version));
	NSS_P7_RawContentInfo &rci = pfx.authSafe;
	printf("...contentType = %s\n", oidStr(rci.contentType, parser));
	
	/* parse content per OID the only special case is PKCS7_Data,
	 * which we unwrap from an octet string before writing it */
	CSSM_DATA toWrite;
	if(nssCompareCssmData(&rci.contentType, &CSSMOID_PKCS7_Data)) {
		if(coder.decodeItem(rci.content, SEC_OctetStringTemplate,
				&toWrite)) {
			printf("***Error decoding PKCS7_Data Octet string; writing"
				" raw contents\n");
			toWrite = rci.content;
		}
	}
	else if(nssCompareCssmData(&rci.contentType, 
			&CSSMOID_PKCS7_SignedData)) {
		/* the only other legal content type here */
		/* This is encoded SignedData which I am not even close
		 * to worrying about - Panther p12 won't do this */
		toWrite = rci.content;
	}
	else {
		printf("***writeAuthSafeContent: bad contentType\n");
		return 1;
	}
	if(writeFile(outFile, toWrite.Data, toWrite.Length)) {
		printf("***Error writing to %s\n", outFile);
		return 1;
	}
	else {
		printf("...%u bytes written to %s\n", 
			(unsigned)toWrite.Length, outFile);
		return 0;
	}
}
#endif	/* old_and_in_the_way */

/*
 * Decrypt the contents of a NSS_P7_EncryptedData
 */
#define WRITE_DECRYPT_TEXT	0
#if 	WRITE_DECRYPT_TEXT
static int ctr = 0;
#endif

#define IMPORT_EXPORT_COMPLETE  1

static int encryptedDataDecrypt(
	const NSS_P7_EncryptedData &edata,
	P12ParseInfo &pinfo,
	NSS_P12_PBE_Params *pbep,	// preparsed
	CSSM_DATA &ptext)			// result goes here in pinfo.coder space
{
	/* see if we can grok the encr alg */
	CSSM_ALGORITHMS		keyAlg;			// e.g., CSSM_ALGID_DES
	CSSM_ALGORITHMS		encrAlg;		// e.g., CSSM_ALGID_3DES_3KEY_EDE
	CSSM_ALGORITHMS		pbeHashAlg;		// SHA1 or MD5
	uint32				keySizeInBits;
	uint32				blockSizeInBytes;	// for IV, optional
	CSSM_PADDING		padding;		// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode;			// CSSM_ALGMODE_CBCPadIV8, etc.
	#if IMPORT_EXPORT_COMPLETE
	PKCS_Which			pkcs;
	
	bool found = pkcsOidToParams(&edata.contentInfo.encrAlg.algorithm,
		keyAlg, encrAlg, pbeHashAlg, keySizeInBits, blockSizeInBytes,
		padding, mode, pkcs);
	#else
	bool found = pkcsOidToParams(&edata.contentInfo.encrAlg.algorithm,
		keyAlg, encrAlg, pbeHashAlg, keySizeInBits, blockSizeInBytes,
		padding, mode);
	#endif  /* IMPORT_EXPORT_COMPLETE */
	
	if(!found) {
		printf("***EncryptedData encrAlg not understood\n");
		return 1;
	}
		
	unsigned iterCount = dataToInt(pbep->iterations);

	/* go */
	CSSM_RETURN crtn = p12Decrypt_app(pinfo.mCspHand,
		edata.contentInfo.encrContent,
		keyAlg, encrAlg, pbeHashAlg,
		keySizeInBits, blockSizeInBytes,
		padding, mode,
		iterCount, pbep->salt,
		pinfo.mPwd,
		pinfo.mCoder, 
		ptext);
	#if WRITE_DECRYPT_TEXT
	if(crtn == 0) {
		char fname[100];
		sprintf(fname, "decrypt%d.der", ctr++);
		writeFile(fname, ptext.Data, ptext.Length);
		printf("...wrote %u bytes to %s\n", 
			(unsigned)ptext.Length, fname);
	}
	#endif
	return crtn ? 1 : 0;
		
}


/*
 * Parse an CSSM_X509_ALGORITHM_IDENTIFIER specific to P12.
 * Decode the alg params as a NSS_P12_PBE_Params and parse and 
 * return the result if the pbeParams is non-NULL.
 */
static int p12AlgIdParse(
	const CSSM_X509_ALGORITHM_IDENTIFIER &algId,
	NSS_P12_PBE_Params *pbeParams,		// optional
	P12ParseInfo &pinfo,
	unsigned depth)						// print indent depth
{
	doIndent(depth);
	printf("encrAlg = %s\n", oidStr(algId.algorithm, pinfo.mParser));
	const CSSM_DATA &param = algId.parameters;
	if(pbeParams == NULL) {
		/* alg params are uninterpreted */
		doIndent(depth);
		printf("Alg Params : ");
		printDataAsHex(&param);
		return 0;
	}
	
	if(param.Length == 0) {
		printf("===warning: no alg parameters, this is not optional\n");
		return 0;
	}
	
	memset(pbeParams, 0, sizeof(*pbeParams));
	if(pinfo.mCoder.decodeItem(param, 
			NSS_P12_PBE_ParamsTemplate, pbeParams)) {
		printf("***Error decoding NSS_P12_PBE_Params\n");
		return 1;
	}
	doIndent(depth);
	printf("Salt : ");
	printDataAsHex(&pbeParams->salt);
	doIndent(depth);
	if(pbeParams->iterations.Length > 4) {
		printf("warning: iterations greater than max int\n");
		doIndent(depth);
		printf("Iterations : ");
		printDataAsHex(&pbeParams->iterations);
	}
	else {
		printf("Iterations : %u\n", 
			(unsigned)dataToInt(pbeParams->iterations));
	}
	return 0;
}

/*
 * Parse a NSS_P7_EncryptedData - specifically in the context
 * of a P12 in password privacy mode. (The latter assumption is
 * to enable us to infer CSSM_X509_ALGORITHM_IDENTIFIER.parameters
 * format). 
 */
static int encryptedDataParse(
	const NSS_P7_EncryptedData &edata,
	P12ParseInfo &pinfo,
	NSS_P12_PBE_Params *pbep,		// optional, RETURNED
	unsigned depth)					// print indent depth
{
	doIndent(depth);
	printf("version = %u\n", (unsigned)dataToInt(edata.version));
	const NSS_P7_EncrContentInfo &ci = edata.contentInfo;
	doIndent(depth);
	printf("contentType = %s\n", oidStr(ci.contentType, pinfo.mParser));
	
	/*
	 * Parse the alg ID, safe PBE params for when we do the 
	 * key unwrap
	 */
	const CSSM_X509_ALGORITHM_IDENTIFIER &algId = ci.encrAlg;
	if(p12AlgIdParse(algId, pbep, pinfo, depth)) {
		return 1;
	}
	
	doIndent(depth);
	printf("encrContent : ");
	printDataAsHex(&ci.encrContent, 12);
	return 0;
}

static int attrParse(
	const NSS_Attribute *attr,
	P12ParseInfo &pinfo, 
	unsigned depth)
{
	doIndent(depth);
	printf("attrType : %s\n", oidStr(attr->attrType, pinfo.mParser));
	unsigned numVals = nssArraySize((const void **)attr->attrValue);
	doIndent(depth);
	printf("numValues = %u\n", numVals);

	for(unsigned dex=0; dex<numVals; dex++) {
		doIndent(depth);
		printf("val[%u] : ", dex);
		
		/*
		 * Note: these two enumerated types should only have one att value 
		 * per PKCS9. Leave that to real apps, we want to see what's there
		 * in any case.
		 */
		if(nssCompareCssmData(&attr->attrType, &CSSMOID_PKCS9_FriendlyName)) {
			/* BMP string (UniCode) */
			CSSM_DATA ustr;
			if(pinfo.mCoder.decodeItem(*attr->attrValue[dex],
					kSecAsn1BMPStringTemplate, &ustr)) {
				printf("***Error decoding BMP string\n");
				continue;
			}
			printDataAsUnichars(ustr);
		}
		else if(nssCompareCssmData(&attr->attrType, 
					&CSSMOID_PKCS9_LocalKeyId)) {
			/* Octet string */
			CSSM_DATA ostr;
			if(pinfo.mCoder.decodeItem(*attr->attrValue[dex],
					kSecAsn1ObjectIDTemplate, &ostr)) {
				printf("***Error decoding LocalKeyId string\n");
				continue;
			}
			printDataAsHex(&ostr, 16);
		}
		else {
			printDataAsHex(attr->attrValue[dex], 8);
		}
	}
	return 0;
}

/*
 * ShroudedKeyBag parser w/decrypt
 */
static int shroudedKeyBagParse(
	const NSS_P12_ShroudedKeyBag *keyBag,
	P12ParseInfo &pinfo,
	unsigned depth)
{
	const CSSM_X509_ALGORITHM_IDENTIFIER &algId = keyBag->algorithm;
	NSS_P12_PBE_Params pbep;
	if(p12AlgIdParse(algId, &pbep, pinfo, depth)) {
		return 1;
	}
	if(pinfo.mPwd.Data == NULL) {
		doIndent(depth);
		printf("=== Key not decrypted (no passphrase)===\n");
		return 0;
	}

	/*
	 * Prepare for decryption
	 */
	CSSM_ALGORITHMS		keyAlg;			// e.g., CSSM_ALGID_DES
	CSSM_ALGORITHMS		encrAlg;		// e.g., CSSM_ALGID_3DES_3KEY_EDE
	CSSM_ALGORITHMS		pbeHashAlg;		// SHA1 or MD5
	uint32				keySizeInBits;
	uint32				blockSizeInBytes;	// for IV, optional
	CSSM_PADDING		padding;		// CSSM_PADDING_PKCS7, etc.
	CSSM_ENCRYPT_MODE	mode;			// CSSM_ALGMODE_CBCPadIV8, etc.
	#if IMPORT_EXPORT_COMPLETE
	PKCS_Which			pkcs;
	
	bool found = pkcsOidToParams(&algId.algorithm,
		keyAlg, encrAlg, pbeHashAlg, keySizeInBits, blockSizeInBytes,
		padding, mode, pkcs);
	#else
	bool found = pkcsOidToParams(&algId.algorithm,
		keyAlg, encrAlg, pbeHashAlg, keySizeInBits, blockSizeInBytes,
		padding, mode);
	#endif
	
	if(!found) {
		printf("***ShroudedKeyBag encrAlg not understood\n");
		return 1;
	}

	unsigned iterCount = dataToInt(pbep.iterations);
	CSSM_DATA berPrivKey;
	
	/* decrypt, result is BER encoded private key */
	CSSM_RETURN crtn = p12Decrypt_app(pinfo.mCspHand,
		keyBag->encryptedData,
		keyAlg, encrAlg, pbeHashAlg,
		keySizeInBits, blockSizeInBytes,
		padding, mode,
		iterCount, pbep.salt,
		pinfo.mPwd,
		pinfo.mCoder, 
		berPrivKey);
	if(crtn) {
		doIndent(depth);
		printf("***Error decrypting private key\n");
		return 1;
	}

	/* decode */
	NSS_PrivateKeyInfo privKey;
	memset(&privKey, 0, sizeof(privKey));
	if(pinfo.mCoder.decodeItem(berPrivKey,
			kSecAsn1PrivateKeyInfoTemplate, &privKey)) {
		doIndent(depth);
		printf("***Error decoding decrypted private key\n");
		return 1;
	}
	
	/* 
	 * in P12 library, we'd convert the result into a CSSM_KEY
	 * or a SecItem...
	 */
	CSSM_X509_ALGORITHM_IDENTIFIER &privAlg = privKey.algorithm;
	doIndent(depth);
	printf("Priv Key Alg  : %s\n", oidStr(privAlg.algorithm, pinfo.mParser));
	doIndent(depth);
	printf("Priv Key Blob : ");
	printDataAsHex(&privKey.privateKey, 16);
	
	unsigned numAttrs = nssArraySize((const void**)privKey.attributes);
	if(numAttrs) {
		doIndent(depth+3);
		printf("numAttrs = %u\n", numAttrs);
		for(unsigned i=0; i<numAttrs; i++) {
			doIndent(depth+3);
			printf("attr[%u]:\n", i);
			attrParse(privKey.attributes[i], pinfo, depth+6);
		}
	}
	return 0;
}

/*
 * CertBag parser
 */
static int certBagParse(
	const NSS_P12_CertBag *certBag,
	P12ParseInfo &pinfo,
	unsigned depth)
{
	/* fixe - we really need to store the attrs along with the cert here! */
	switch(certBag->type) {
		case CT_X509:
			doIndent(depth);
			printf("X509 cert found, size %u\n", 
				(unsigned)certBag->certValue.Length);
			pinfo.mParsed.mCerts.addBlob(certBag->certValue);
			break;
		default:
			doIndent(depth);
			printf("Unknown cert type found\n");
			P12UnknownBlob *uk = new P12UnknownBlob(certBag->certValue,
				certBag->bagType);
			pinfo.mParsed.mUnknown.addBlob(uk);
	}
	return 0;
}

/*
 * CrlBag parser
 */
static int crlBagParse(
	const NSS_P12_CrlBag *crlBag,
	P12ParseInfo &pinfo,
	unsigned depth)
{
	/* fixe - we really need to store the attrs along with the crl here! */
	switch(crlBag->type) {
		case CRT_X509:
			doIndent(depth);
			printf("X509 CRL found, size %u\n", 
				(unsigned)crlBag->crlValue.Length);
			pinfo.mParsed.mCrls.addBlob(crlBag->crlValue);
			break;
		default:
			doIndent(depth);
			printf("Unknown CRL type found\n");
			P12UnknownBlob *uk = new P12UnknownBlob(crlBag->crlValue,
				crlBag->bagType);
			pinfo.mParsed.mUnknown.addBlob(uk);
	}
	return 0;
}


/*
 * Parse an encoded NSS_P12_SafeContents. This could be either 
 * present as plaintext in an AuthSafe or decrypted. 
 */
static int safeContentsParse(
	const CSSM_DATA &contentsBlob,
	P12ParseInfo &pinfo,
	unsigned depth)		// print indent depth
{
	NSS_P12_SafeContents sc;
	memset(&sc, 0, sizeof(sc));
	if(pinfo.mCoder.decodeItem(contentsBlob, NSS_P12_SafeContentsTemplate,
			&sc)) {
		printf("***Error decoding SafeContents\n");
		return 1;
	}
	unsigned numBags = nssArraySize((const void **)sc.bags);
	doIndent(depth);
	printf("SafeContents num bags %u\n", numBags);
	int rtn = 0;
	
	for(unsigned dex=0; dex<numBags; dex++) {
		NSS_P12_SafeBag *bag = sc.bags[dex];
		doIndent(depth);
		printf("Bag %u:\n", dex);
		
		/* common stuff here */
		doIndent(depth+3);
		printf("bagId = %s\n", oidStr(bag->bagId, pinfo.mParser));
		doIndent(depth+3);
		printf("type = %s\n", p12BagTypeStr(bag->type));
		unsigned numAttrs = nssArraySize((const void**)bag->bagAttrs);
		if(numAttrs) {
			doIndent(depth+3);
			printf("numAttrs = %u\n", numAttrs);
			for(unsigned i=0; i<numAttrs; i++) {
				doIndent(depth+3);
				printf("attr[%u]:\n", i);
				attrParse(bag->bagAttrs[i], pinfo, depth+6);
			}
		}
		
		/*
		 * Now break out to individual bag type
		 * 
		 * This hacked line breaks when we have a real key bag defined
		 */
		unsigned defaultLen = (unsigned)bag->bagValue.keyBag->Length;
		switch(bag->type) {
			case BT_KeyBag:
				doIndent(depth+3);
				printf("KeyBag: size %u\n", defaultLen);
				break;
			case BT_ShroudedKeyBag:
				doIndent(depth+3);
				printf("ShroudedKeyBag:\n");
				rtn = shroudedKeyBagParse(bag->bagValue.shroudedKeyBag,
					pinfo,
					depth+6);
				break;
			case BT_CertBag:
				doIndent(depth+3);
				printf("CertBag:\n");
				rtn = certBagParse(bag->bagValue.certBag,
					pinfo,
					depth+6);
				break;
			case BT_CrlBag:
				doIndent(depth+3);
				printf("CrlBag:\n");
				rtn = crlBagParse(bag->bagValue.crlBag,
					pinfo,
					depth+6);
				break;
			case BT_SecretBag:
				doIndent(depth+3);
				printf("SecretBag: size %u\n", defaultLen);
				break;
			case BT_SafeContentsBag:
				doIndent(depth+3);
				printf("SafeContentsBag: size %u\n", defaultLen);
				break;
			default:
				doIndent(depth+3);
				printf("===Warning: unknownBagType (%u)\n",
					(unsigned)bag->type);
				break;
		}
		if(rtn) {
			break;
		}
	}
	return rtn;
}

/*
 * Parse a ContentInfo in the context of (i.e., as an element of)
 * an element in a AuthenticatedSafe
 */
static int authSafeElementParse(
	const NSS_P7_DecodedContentInfo *info,
	P12ParseInfo &pinfo,
	unsigned depth)		// print indent depth
{
	char oidStr[OID_PARSER_STRING_SIZE];
	pinfo.mParser.oidParse(info->contentType.Data, 
		info->contentType.Length, oidStr);

	doIndent(depth);
	printf("contentType = %s\n", oidStr);
	doIndent(depth);
	printf("type = %s\n", p7ContentInfoTypeStr(info->type));
	int rtn = 0;
	switch(info->type) {
		case CT_Data:
			/* unencrypted SafeContents */
			doIndent(depth);
			printf("raw size: %u\n", 
				(unsigned)info->content.data->Length);
			doIndent(depth);
			printf("Plaintext SafeContents:\n");
			rtn = safeContentsParse(*info->content.data,
				pinfo, depth+3);
			break;
			
		case CT_EncryptedData:
		{
			doIndent(depth);
			printf("EncryptedData:\n");
			NSS_P12_PBE_Params pbep;
			rtn = encryptedDataParse(*info->content.encryptData,
				pinfo, &pbep, depth+3);
			if(rtn) {
				break;
			}
			if(pinfo.mPwd.Data == NULL) {
				doIndent(depth+3);
				printf("=== Contents not decrypted (no passphrase)===\n");
			}
			else {
				/* 
				* Decrypt contents to get a SafeContents and
				* then parse that.
				*/
				CSSM_DATA ptext = {0, NULL};
				rtn = encryptedDataDecrypt(*info->content.encryptData,
					pinfo, &pbep, ptext);
				doIndent(depth);
				if(rtn) {
					printf("***Error decrypting CT_EncryptedData\n");
					break;
				}
				printf("Decrypted SafeContents {\n");
				rtn = safeContentsParse(ptext, pinfo, depth+3);
				doIndent(depth);
				printf("}\n");
			}
			break;
		}	
		default:
			/* the rest map to an ASN_ANY/CSSM_DATA for now */
			doIndent(depth+3);
			printf("size of %u is all we know today\n",
				(unsigned)info->content.data->Length);
			rtn = 0;
			break;
	}
	return rtn;
}

/*
 * Parse an encoded NSS_P12_AuthenticatedSafe
 */
static int authSafeParse(
	const CSSM_DATA authSafeBlob,
	P12ParseInfo &pinfo,
	unsigned depth)		// print indent depth
{
	NSS_P12_AuthenticatedSafe authSafe;
	
	memset(&authSafe, 0, sizeof(authSafe));
	if(pinfo.mCoder.decodeItem(authSafeBlob,
			NSS_P12_AuthenticatedSafeTemplate,
			&authSafe)) {
		printf("***Error decoding authSafe\n");
		return 1;
	}
	unsigned numInfos = nssArraySize((const void **)authSafe.info);
	doIndent(depth);
	printf("authSafe numInfos %u\n", numInfos);
	
	int rtn = 0;
	for(unsigned dex=0; dex<numInfos; dex++) {
		NSS_P7_DecodedContentInfo *info = authSafe.info[dex];
		doIndent(depth);
		printf("AuthSafe.info[%u] {\n", dex);
		rtn = authSafeElementParse(info, pinfo, depth+3);
		if(rtn) {
			break;
		}
		doIndent(depth);
		printf("}\n");
	}
	return rtn;
}

static int p12MacParse(
	const NSS_P12_MacData &macData, 
	P12ParseInfo &pinfo,
	unsigned depth)		// print indent depth
{
	if(p12AlgIdParse(macData.mac.digestAlgorithm, NULL, pinfo, depth)) {
		return 1;
	}
	doIndent(depth);
	printf("Digest : ");
	printDataAsHex(&macData.mac.digest, 20);
	doIndent(depth);
	printf("Salt : ");
	printDataAsHex(&macData.macSalt, 16);
	const CSSM_DATA &iter = macData.iterations;
	
	if(iter.Length > 4) {
		doIndent(depth);
		printf("***Warning: malformed iteraton length (%u)\n",
			(unsigned)iter.Length);
	}
	unsigned i = dataToInt(iter);
	doIndent(depth);
	printf("Iterations = %u\n", i);
	return 0;
}

static int p12Parse(
	const CSSM_DATA &rawBlob, 
	P12ParseInfo &pinfo,
	unsigned depth)		// print indent depth
{
	NSS_P12_DecodedPFX pfx;
	memset(&pfx, 0, sizeof(pfx));
	if(pinfo.mCoder.decodeItem(rawBlob, NSS_P12_DecodedPFXTemplate, &pfx)) {
		printf("***Error on top-level decode of NSS_P12_DecodedPFX\n");
		return 1;
	}
	doIndent(depth);
	printf("version = %u\n", (unsigned)dataToInt(pfx.version));
	NSS_P7_DecodedContentInfo &dci = pfx.authSafe;

	doIndent(depth);
	printf("contentType = %s\n", oidStr(dci.contentType, pinfo.mParser));
	doIndent(depth);
	printf("type = %s\n", p7ContentInfoTypeStr(dci.type));
	int rtn = 0;
	if(nssCompareCssmData(&dci.contentType, &CSSMOID_PKCS7_Data)) {
		doIndent(depth);
		printf("AuthenticatedSafe Length %u {\n", 
			(unsigned)dci.content.data->Length);
		rtn = authSafeParse(*dci.content.data, pinfo, depth+3);
		doIndent(depth);
		printf("}\n");
	}
	else {
		printf("Not parsing any other content type today.\n");
	}
	if(pfx.macData) {
		doIndent(depth);
		printf("Mac Data {\n");
		p12MacParse(*pfx.macData, pinfo, depth+3);
		doIndent(depth);
		printf("}\n");
		if(pinfo.mPwd.Data == NULL) {
			doIndent(depth);
			printf("=== MAC not verified (no passphrase)===\n");
		}
		else {
			CSSM_RETURN crtn = p12VerifyMac_app(pfx, pinfo.mCspHand, 
				pinfo.mPwd, pinfo.mCoder);
			doIndent(depth);
			if(crtn) {
				cssmPerror("p12VerifyMac", crtn);
				doIndent(depth);
				printf("***MAC verify failure.\n");
			}
			else {
				printf("MAC verifies OK.\n");
			}
		}
	}
	return 0;
}

int p12ParseTop(
	CSSM_DATA		&rawBlob,
	CSSM_CSP_HANDLE cspHand,
	CFStringRef 	pwd,
	bool 			verbose)
{
	SecNssCoder coder;
	OidParser parser;
	P12Parsed parsed(coder);
	P12ParseInfo pinfo(coder,
		cspHand, 
		parser,
		pwd,
		NULL,			// no separate pwd
		parsed);

	printf("PKCS12 PFX:\n");
	int rtn = p12Parse(rawBlob, pinfo, 3);
	
	/* find anything? */
	if(verbose) {
		P12KnownBlobs &certs = pinfo.mParsed.mCerts;
		if(certs.mNumBlobs) {
			printf("\n\n");
			for(unsigned dex=0; dex<certs.mNumBlobs; dex++) {
				printf("Cert %u:\n", dex);
				printCert(certs.mBlobs[dex].Data, 
					certs.mBlobs[dex].Length, CSSM_FALSE);
				printf("\n");
			}
		}
		P12KnownBlobs &crls = pinfo.mParsed.mCrls;
		if(crls.mNumBlobs) {
			printf("\n\n");
			for(unsigned dex=0; dex<crls.mNumBlobs; dex++) {
				printf("CRL %u:\n", dex);
				printCrl(crls.mBlobs[dex].Data, 
					crls.mBlobs[dex].Length, CSSM_FALSE);
			}
		}
	}
	return rtn;
}
