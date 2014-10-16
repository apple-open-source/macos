/*
 * extenTest - verify encoding and decoding of extensions.
 */
 
#include <security_cdsa_utils/cuFileIo.h>
#include <clAppUtils/CertBuilderApp.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <clAppUtils/clutils.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <security_cdsa_utils/cuOidParser.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/x509defs.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/certextensions.h>

#define KEY_ALG			CSSM_ALGID_RSA
#define SIG_ALG			CSSM_ALGID_SHA1WithRSA
#define KEY_SIZE_BITS	CSP_RSA_KEY_SIZE_DEFAULT
#define SUBJ_KEY_LABEL	"subjectKey"

#define LOOPS_DEF	10

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("   e=extenSpec (default = all)\n");
	printf("      k  keyUsage\n");
	printf("      b  basicConstraints\n");
	printf("      x  extendedKeyUsage\n");
	printf("      s  subjectKeyId\n");
	printf("      a  authorityKeyId\n");
	printf("      t  SubjectAltName\n");
	printf("      i  IssuerAltName\n");
	printf("      c  certPolicies\n");
	printf("      n  netscapeCertType\n");
	printf("      p  CRLDistributionPoints\n");
	printf("      A  AuthorityInfoAccess\n");
	printf("      S  SubjectInfoAccess\n");
	printf("      q  QualifiedCertStatements\n");
	printf("   w(rite blobs)\n");
	printf("   f=fileName (default is extension-specific file name)\n");
	printf("   d(isplay certs)\n");
	printf("   l=loops (default = %d)\n", LOOPS_DEF);
	printf("   p(ause on each loop)\n");
	printf("   P(ause on each cert)\n");
	exit(1);
}

/* dummy RDN - subject and issuer - we aren't testing this */
CB_NameOid dummyRdn[] = 
{
	{ "Apple Computer",					&CSSMOID_OrganizationName },
	{ "Doug Mitchell",					&CSSMOID_CommonName }
};
#define NUM_DUMMY_NAMES	(sizeof(dummyRdn) / sizeof(CB_NameOid))

/*
 * Static components we reuse for each encode/decode. 
 */
static CSSM_X509_NAME	*dummyName;
static CSSM_X509_TIME	*notBefore;		// UTC-style "not before" time
static CSSM_X509_TIME	*notAfter;		// UTC-style "not after" time
static CSSM_KEY			subjPrivKey;	
static CSSM_KEY			subjPubKey;		

static CSSM_BOOL randBool()
{
	unsigned r = genRand(1, 0x10000000);
	return (r & 0x1) ? CSSM_TRUE : CSSM_FALSE;
}

/* Fill a CSSM_DATA with random data. Its referent is allocd with malloc. */
static void randData(	
	CSSM_DATA_PTR	data,
	uint8			maxLen)
{
	data->Data = (uint8 *)malloc(maxLen);
	simpleGenData(data, 1, maxLen);
}

/*
 * Various compare tests
 */
int compBool(
	CSSM_BOOL pre,
	CSSM_BOOL post,
	const char *desc)
{
	if(pre == post) {
		return 0;
	}
	printf("***Boolean miscompare on %s\n", desc);
	/* in case a CSSM_TRUE isn't exactly right... */
	switch(post) {
		case CSSM_FALSE:
		case CSSM_TRUE:
			break;
		default:
			printf("*** post value is %d expected %d\n",
				(int)post, (int)pre);
			break;
	}
	return 1;
}

static int compCssmData(
	CSSM_DATA &d1,
	CSSM_DATA &d2,
	const char *desc)
{
	if(appCompareCssmData(&d1, &d2)) {
		return 0;
	}
	printf("CSSM_DATA miscompare on %s\n", desc);
	return 1;
}	

#pragma mark ----- individual extension tests -----

#pragma mark --- CE_KeyUsage ---
static void kuCreate(void *arg)
{
	CE_KeyUsage *ku = (CE_KeyUsage *)arg;
	
	/* set two random valid bits */
	*ku = 0;
	*ku |= 1 << genRand(7, 15);
	*ku |= 1 << genRand(7, 15);
}

static unsigned kuCompare(const void *pre, const void *post)
{
	const CE_KeyUsage *kuPre = (CE_KeyUsage *)pre;
	const CE_KeyUsage *kuPost = (CE_KeyUsage *)post;
	if(*kuPre != *kuPost) {
		printf("***Miscompare in CE_KeyUsage\n");
		return 1;
	}
	return 0;
}

#pragma mark --- CE_BasicConstraints ---
static void bcCreate(void *arg)
{
	CE_BasicConstraints *bc = (CE_BasicConstraints *)arg;
	bc->cA = randBool();
	bc->pathLenConstraintPresent = randBool();
	if(bc->pathLenConstraintPresent) {
		bc->pathLenConstraint = genRand(1,10);
	}
}

static unsigned bcCompare(const void *pre, const void *post)
{
	const CE_BasicConstraints *bcpre = (CE_BasicConstraints *)pre;
	const CE_BasicConstraints *bcpost = (CE_BasicConstraints *)post;
	unsigned rtn = 0;
	
	rtn += compBool(bcpre->cA, bcpost->cA, "BasicConstraints.cA");
	rtn += compBool(bcpre->pathLenConstraintPresent, 
		bcpost->pathLenConstraintPresent, 
		"BasicConstraints.pathLenConstraintPresent");
	if(bcpre->pathLenConstraint != bcpost->pathLenConstraint) {
		printf("BasicConstraints.pathLenConstraint mismatch\n");
		rtn++;
	}
	return rtn;
}

#pragma mark --- CE_SubjectKeyID ---
static void skidCreate(void *arg)
{
	CSSM_DATA_PTR skid = (CSSM_DATA_PTR)arg;
	randData(skid, 16);
}

static unsigned skidCompare(const void *pre, const void *post)
{
	CSSM_DATA_PTR spre = (CSSM_DATA_PTR)pre;
	CSSM_DATA_PTR spost = (CSSM_DATA_PTR)post;
	return compCssmData(*spre, *spost, "SubjectKeyID");
}

static void skidFree(void *arg)
{
	CSSM_DATA_PTR skid = (CSSM_DATA_PTR)arg;
	free(skid->Data);
}

#pragma mark --- CE_NetscapeCertType ---
static void nctCreate(void *arg)
{
	CE_NetscapeCertType *nct = (CE_NetscapeCertType *)arg;
	
	/* set two random valid bits */
	*nct = 0;
	*nct |= 1 << genRand(8, 15);
	*nct |= 1 << genRand(8, 15);
}

static unsigned nctCompare(const void *pre, const void *post)
{
	const CE_NetscapeCertType *nPre = (CE_NetscapeCertType *)pre;
	const CE_NetscapeCertType *nPost = (CE_NetscapeCertType *)post;
	if(*nPre != *nPost) {
		printf("***Miscompare in CE_NetscapeCertType\n");
		return 1;
	}
	return 0;
}

#pragma mark --- CE_ExtendedKeyUsage ---

/* a static array of meaningless OIDs, use 1.. NUM_SKU_OIDS */
CSSM_OID ekuOids[] = {
	CSSMOID_CrlNumber,
	CSSMOID_CrlReason,
	CSSMOID_HoldInstructionCode,
	CSSMOID_InvalidityDate
};
#define NUM_SKU_OIDS 4

static void ekuCreate(void *arg)
{
	CE_ExtendedKeyUsage *eku = (CE_ExtendedKeyUsage *)arg;
	eku->numPurposes = genRand(1, NUM_SKU_OIDS);
	eku->purposes = ekuOids;
}

static unsigned ekuCompare(const void *pre, const void *post)
{
	CE_ExtendedKeyUsage *ekupre = (CE_ExtendedKeyUsage *)pre;
	CE_ExtendedKeyUsage *ekupost = (CE_ExtendedKeyUsage *)post;
	
	if(ekupre->numPurposes != ekupost->numPurposes) {
		printf("CE_ExtendedKeyUsage.numPurposes miscompare\n");
		return 1;
	}
	unsigned rtn = 0;
	for(unsigned dex=0; dex<ekupre->numPurposes; dex++) {
		rtn += compCssmData(ekupre->purposes[dex],
			ekupost->purposes[dex], "CE_ExtendedKeyUsage.purposes");
	}
	return rtn;
}


#pragma mark --- general purpose X509 name generator ---

/* Attr/Value pairs, pick one of NUM_ATTR_STRINGS */
static char *attrStrings[] = {
	(char *)"thisName",
	(char *)"anotherName",
	(char *)"someOtherName"
};
#define NUM_ATTR_STRINGS	3

/* A/V type, pick one of NUM_ATTR_TYPES */
static CSSM_OID attrTypes[] = {
	CSSMOID_Surname,
	CSSMOID_CountryName,
	CSSMOID_OrganizationName,
	CSSMOID_Description
};
#define NUM_ATTR_TYPES	4

/* A/V tag, pick one of NUM_ATTR_TAGS */
static char attrTags[] = {
	BER_TAG_PRINTABLE_STRING,
	BER_TAG_IA5_STRING,
	BER_TAG_T61_STRING
};
#define NUM_ATTR_TAGS	3

static void rdnCreate(
	CSSM_X509_RDN_PTR rdn)
{
	unsigned numPairs = genRand(1,4);
	rdn->numberOfPairs = numPairs;
	unsigned len = numPairs * sizeof(CSSM_X509_TYPE_VALUE_PAIR);
	rdn->AttributeTypeAndValue = 
		(CSSM_X509_TYPE_VALUE_PAIR_PTR)malloc(len);
	memset(rdn->AttributeTypeAndValue, 0, len);
	
	for(unsigned atvDex=0; atvDex<numPairs; atvDex++) {
		CSSM_X509_TYPE_VALUE_PAIR &pair = 
			rdn->AttributeTypeAndValue[atvDex];
		unsigned die = genRand(1, NUM_ATTR_TYPES);
		pair.type = attrTypes[die - 1];
		die = genRand(1, NUM_ATTR_STRINGS);
		char *str = attrStrings[die - 1];
		pair.value.Data = (uint8 *)str;
		pair.value.Length = strlen(str);
		die = genRand(1, NUM_ATTR_TAGS);
		pair.valueType = attrTags[die - 1];
	}
}

static unsigned rdnCompare(
	CSSM_X509_RDN_PTR rdn1,
	CSSM_X509_RDN_PTR rdn2)
{
	if(rdn1->numberOfPairs != rdn2->numberOfPairs) {
		printf("***Mismatch in numberOfPairs\n");
		return 1;
	}
	unsigned rtn = 0;
	for(unsigned atvDex=0; atvDex<rdn1->numberOfPairs; atvDex++) {
		CSSM_X509_TYPE_VALUE_PAIR &p1 = 
			rdn1->AttributeTypeAndValue[atvDex];
		CSSM_X509_TYPE_VALUE_PAIR &p2 = 
			rdn2->AttributeTypeAndValue[atvDex];
		if(p1.valueType != p2.valueType) {
			printf("***valueType miscompare\n");
			rtn++;
		}
		if(compCssmData(p1.type, p2.type, "ATV.type")) {
			rtn++;
		}
		if(compCssmData(p1.value, p2.value, "ATV.value")) {
			rtn++;
		}
	}
	return rtn;
}

static void rdnFree(
	CSSM_X509_RDN_PTR rdn)
{
	free(rdn->AttributeTypeAndValue);
}

static void x509NameCreate(
	CSSM_X509_NAME_PTR x509Name)
{
	memset(x509Name, 0, sizeof(*x509Name));
	unsigned numRdns = genRand(1,4);
	x509Name->numberOfRDNs = numRdns;
	unsigned len = numRdns * sizeof(CSSM_X509_RDN);
	x509Name->RelativeDistinguishedName = (CSSM_X509_RDN_PTR)malloc(len);
	memset(x509Name->RelativeDistinguishedName, 0, len);
	
	for(unsigned rdnDex=0; rdnDex<numRdns; rdnDex++) {
		CSSM_X509_RDN &rdn = x509Name->RelativeDistinguishedName[rdnDex];
		rdnCreate(&rdn);
	}
}

static unsigned x509NameCompare(
	const CSSM_X509_NAME_PTR n1,
	const CSSM_X509_NAME_PTR n2)
{
	if(n1->numberOfRDNs != n2->numberOfRDNs) {
		printf("***Mismatch in numberOfRDNs\n");
		return 1;
	}
	unsigned rtn = 0;
	for(unsigned rdnDex=0; rdnDex<n1->numberOfRDNs; rdnDex++) {
		CSSM_X509_RDN &rdn1 = n1->RelativeDistinguishedName[rdnDex];
		CSSM_X509_RDN &rdn2 = n2->RelativeDistinguishedName[rdnDex];
		rtn += rdnCompare(&rdn1, &rdn2);
	}
	return rtn;
}

static void x509NameFree(
	CSSM_X509_NAME_PTR n)
{
	for(unsigned rdnDex=0; rdnDex<n->numberOfRDNs; rdnDex++) {
		CSSM_X509_RDN &rdn = n->RelativeDistinguishedName[rdnDex];
		rdnFree(&rdn);
	}
	free(n->RelativeDistinguishedName);
}

#pragma mark --- general purpose GeneralNames generator ---

#define SOME_URL_1	"http://foo.bar.com"
#define SOME_URL_2	"http://bar.foo.com"
#define SOME_DNS_1	"Some DNS"
#define SOME_DNS_2	"Another DNS"
unsigned char	someIpAdr_1[] = {208, 161, 124, 209 };
unsigned char	someIpAdr_2[] = {10, 0, 61, 5};

static void genNameCreate(CE_GeneralName *name)
{
	unsigned type = genRand(1, 5);
	const char *src;
	unsigned char *usrc;
	switch(type) {
		case 1:
			name->nameType = GNT_URI;
			name->berEncoded = CSSM_FALSE;
			src = randBool() ? SOME_URL_1 : SOME_URL_2;
			appCopyData(src, strlen(src), &name->name);
			break;

		case 2:
			name->nameType = GNT_RegisteredID;
			name->berEncoded = CSSM_FALSE;
			appCopyData(CSSMOID_SubjectDirectoryAttributes.Data,
				CSSMOID_SubjectDirectoryAttributes.Length,
				&name->name);
			break;
			
		case 3:
			name->nameType = GNT_DNSName;
			name->berEncoded = CSSM_FALSE;
			src = randBool() ? SOME_DNS_1 : SOME_DNS_2;
			appCopyData(src, strlen(src), &name->name);
			break;
			
		case 4:
			name->nameType = GNT_IPAddress;
			name->berEncoded = CSSM_FALSE;
			usrc = randBool() ? someIpAdr_1 : someIpAdr_2;
			appCopyData(usrc, 4, &name->name);
			break;
			
		case 5:
		{
			/* X509_NAME, the hard one */
			name->nameType = GNT_DirectoryName;
			name->berEncoded = CSSM_FALSE;
			appSetupCssmData(&name->name, sizeof(CSSM_X509_NAME));
			x509NameCreate((CSSM_X509_NAME_PTR)name->name.Data);
		}
	}
}

static void genNamesCreate(void *arg)
{
	CE_GeneralNames *names = (CE_GeneralNames *)arg;
	names->numNames = genRand(1, 3);
	// one at a time
	//names->numNames = 1;
	names->generalName = (CE_GeneralName *)malloc(names->numNames * 
		sizeof(CE_GeneralName));
	memset(names->generalName, 0, names->numNames * sizeof(CE_GeneralName));

	for(unsigned i=0; i<names->numNames; i++) {
		CE_GeneralName *name = &names->generalName[i];
		genNameCreate(name);
	}
}

static unsigned genNameCompare(
	CE_GeneralName *npre,
	CE_GeneralName *npost)
{
	unsigned rtn = 0;
	if(npre->nameType != npost->nameType) {
		printf("***CE_GeneralName.nameType miscompare\n");
		rtn++;
	}
	if(compBool(npre->berEncoded, npost->berEncoded, 
			"CE_GeneralName.berEncoded")) {
		rtn++;
	}
	
	/* nameType-specific compare */
	switch(npre->nameType) {
		case GNT_RFC822Name:
			rtn += compCssmData(npre->name, npost->name,
				"CE_GeneralName.RFC822Name");
			break;
		case GNT_DNSName:
			rtn += compCssmData(npre->name, npost->name,
				"CE_GeneralName.DNSName");
			break;
		case GNT_URI:
			rtn += compCssmData(npre->name, npost->name,
				"CE_GeneralName.URI");
			break;
		case GNT_IPAddress:
			rtn += compCssmData(npre->name, npost->name,
				"CE_GeneralName.RFIPAddressC822Name");
			break;
		case GNT_RegisteredID:
			rtn += compCssmData(npre->name, npost->name,
				"CE_GeneralName.RegisteredID");
			break;
		case GNT_DirectoryName:
			rtn += x509NameCompare((CSSM_X509_NAME_PTR)npre->name.Data,
				(CSSM_X509_NAME_PTR)npost->name.Data);
			break;
		default:
			printf("****BRRZAP! genNamesCompare needs work\n");
			rtn++;
	}
	return rtn;
}

static unsigned genNamesCompare(const void *pre, const void *post)
{
	const CE_GeneralNames *gnPre = (CE_GeneralNames *)pre;
	const CE_GeneralNames *gnPost = (CE_GeneralNames *)post;
	unsigned rtn = 0;
	
	if((gnPre == NULL) || (gnPost == NULL)) {
		printf("***Bad GenNames pointer\n");
		return 1;
	}
	if(gnPre->numNames != gnPost->numNames) {
		printf("***CE_GeneralNames.numNames miscompare\n");
		return 1;
	}
	for(unsigned dex=0; dex<gnPre->numNames; dex++) {
		CE_GeneralName *npre  = &gnPre->generalName[dex];
		CE_GeneralName *npost = &gnPost->generalName[dex];
		rtn += genNameCompare(npre, npost);
	}
	return rtn;
}


static void genNameFree(CE_GeneralName *n)
{
	switch(n->nameType) {
		case GNT_DirectoryName:
			x509NameFree((CSSM_X509_NAME_PTR)n->name.Data);
			CSSM_FREE(n->name.Data);
			break;
		default:
			CSSM_FREE(n->name.Data);
			break;
	}
}


static void genNamesFree(void *arg)
{
	const CE_GeneralNames *gn = (CE_GeneralNames *)arg;
	for(unsigned dex=0; dex<gn->numNames; dex++) {
		CE_GeneralName *n = (CE_GeneralName *)&gn->generalName[dex];
		genNameFree(n);
	}
	free(gn->generalName);
}

#pragma mark --- CE_CRLDistPointsSyntax ---
static void cdpCreate(void *arg)
{
	CE_CRLDistPointsSyntax *cdp = (CE_CRLDistPointsSyntax *)arg;
	//cdp->numDistPoints = genRand(1,3);
	// one at a time
	cdp->numDistPoints = 1;
	unsigned len = sizeof(CE_CRLDistributionPoint) * cdp->numDistPoints;
	cdp->distPoints = (CE_CRLDistributionPoint *)malloc(len);
	memset(cdp->distPoints, 0, len);
	
	for(unsigned dex=0; dex<cdp->numDistPoints; dex++) {
		CE_CRLDistributionPoint *pt = &cdp->distPoints[dex];
		
		/* all fields optional */
		if(randBool()) {
			CE_DistributionPointName *dpn = pt->distPointName =
				(CE_DistributionPointName *)malloc(
					sizeof(CE_DistributionPointName));
			memset(dpn, 0, sizeof(CE_DistributionPointName));
			
			/* CE_DistributionPointName has two flavors */
			if(randBool()) {
				dpn->nameType = CE_CDNT_FullName;
				dpn->dpn.fullName = (CE_GeneralNames *)malloc(
					sizeof(CE_GeneralNames));
				memset(dpn->dpn.fullName, 0, sizeof(CE_GeneralNames));
				genNamesCreate(dpn->dpn.fullName);
			}
			else {
				dpn->nameType = CE_CDNT_NameRelativeToCrlIssuer;
				dpn->dpn.rdn = (CSSM_X509_RDN_PTR)malloc(
					sizeof(CSSM_X509_RDN));
				memset(dpn->dpn.rdn, 0, sizeof(CSSM_X509_RDN));
				rdnCreate(dpn->dpn.rdn);
			}
		}	/* creating CE_DistributionPointName */
		
		pt->reasonsPresent = randBool();
		if(pt->reasonsPresent) {
			CE_CrlDistReasonFlags *cdr = &pt->reasons;
			/* set two random valid bits */
			*cdr = 0;
			*cdr |= 1 << genRand(0,7);
			*cdr |= 1 << genRand(0,7);
		}
		
		/* make sure at least one present */
		if((!pt->distPointName && !pt->reasonsPresent) || randBool()) {
			pt->crlIssuer = (CE_GeneralNames *)malloc(sizeof(CE_GeneralNames));
			memset(pt->crlIssuer, 0, sizeof(CE_GeneralNames));
			genNamesCreate(pt->crlIssuer);
		}
	}
}

static unsigned cdpCompare(const void *pre, const void *post)
{
	CE_CRLDistPointsSyntax *cpre = (CE_CRLDistPointsSyntax *)pre;
	CE_CRLDistPointsSyntax *cpost = (CE_CRLDistPointsSyntax *)post;
	
	if(cpre->numDistPoints != cpost->numDistPoints) {
		printf("***CE_CRLDistPointsSyntax.numDistPoints miscompare\n");
		return 1;
	}
	unsigned rtn = 0;
	for(unsigned dex=0; dex<cpre->numDistPoints; dex++) {
		CE_CRLDistributionPoint *ptpre  = &cpre->distPoints[dex];
		CE_CRLDistributionPoint *ptpost = &cpost->distPoints[dex];
		
		if(ptpre->distPointName) {
			if(ptpost->distPointName == NULL) {
				printf("***NULL distPointName post decode\n");
				rtn++;
				goto checkReason;
			}
			CE_DistributionPointName *dpnpre = ptpre->distPointName;
			CE_DistributionPointName *dpnpost = ptpost->distPointName;
			if(dpnpre->nameType != dpnpost->nameType) {
				printf("***CE_DistributionPointName.nameType miscompare\n");
				rtn++;
				goto checkReason;
			}
			if(dpnpre->nameType == CE_CDNT_FullName) {
				rtn += genNamesCompare(dpnpre->dpn.fullName, dpnpost->dpn.fullName);
			}
			else {
				rtn += rdnCompare(dpnpre->dpn.rdn, dpnpost->dpn.rdn);
			}
				
		}
		else if(ptpost->distPointName != NULL) {
			printf("***NON NULL distPointName post decode\n");
			rtn++;
		}
		
	checkReason:
		if(ptpre->reasons != ptpost->reasons) {
			printf("***CE_CRLDistributionPoint.reasons miscompare\n");
			rtn++;
		}
		
		if(ptpre->crlIssuer) {
			if(ptpost->crlIssuer == NULL) {
				printf("***NULL crlIssuer post decode\n");
				rtn++;
				continue;
			}
			CE_GeneralNames *gnpre = ptpre->crlIssuer;
			CE_GeneralNames *gnpost = ptpost->crlIssuer;
			rtn += genNamesCompare(gnpre, gnpost);
		}
		else if(ptpost->crlIssuer != NULL) {
			printf("***NON NULL crlIssuer post decode\n");
			rtn++;
		}
	}
	return rtn;
}

static void cdpFree(void *arg)
{
	CE_CRLDistPointsSyntax *cdp = (CE_CRLDistPointsSyntax *)arg;
	for(unsigned dex=0; dex<cdp->numDistPoints; dex++) {
		CE_CRLDistributionPoint *pt = &cdp->distPoints[dex];
		if(pt->distPointName) {
			CE_DistributionPointName *dpn = pt->distPointName;
			if(dpn->nameType == CE_CDNT_FullName) {
				genNamesFree(dpn->dpn.fullName);
				free(dpn->dpn.fullName);
			}
			else {
				rdnFree(dpn->dpn.rdn);
				free(dpn->dpn.rdn);
			}
			free(dpn);
		}
		
		if(pt->crlIssuer) {
			genNamesFree(pt->crlIssuer);
			free(pt->crlIssuer);
		}
	}	
	free(cdp->distPoints);
}

#pragma mark --- CE_AuthorityKeyID ---
static void authKeyIdCreate(void *arg)
{
	CE_AuthorityKeyID *akid = (CE_AuthorityKeyID *)arg;
	
	/* all three fields optional */
	
	akid->keyIdentifierPresent = randBool();
	if(akid->keyIdentifierPresent) {
		randData(&akid->keyIdentifier, 16);
	}
	
	akid->generalNamesPresent = randBool();
	if(akid->generalNamesPresent) {
		akid->generalNames = 
			(CE_GeneralNames *)malloc(sizeof(CE_GeneralNames));
		memset(akid->generalNames, 0, sizeof(CE_GeneralNames));
		genNamesCreate(akid->generalNames);
	}
	
	if(!akid->keyIdentifierPresent & !akid->generalNamesPresent) {
		/* force at least one to be present */
		akid->serialNumberPresent = CSSM_TRUE;
	}
	else  {
		akid->serialNumberPresent = randBool();
	}
	if(akid->serialNumberPresent) {
		randData(&akid->serialNumber, 16);
	}

}

static unsigned authKeyIdCompare(const void *pre, const void *post)
{
	CE_AuthorityKeyID *akpre = (CE_AuthorityKeyID *)pre;
	CE_AuthorityKeyID *akpost = (CE_AuthorityKeyID *)post;
	unsigned rtn = 0;
	
	if(compBool(akpre->keyIdentifierPresent, akpost->keyIdentifierPresent,
			"CE_AuthorityKeyID.keyIdentifierPresent")) {
		rtn++;
	}
	else if(akpre->keyIdentifierPresent) {
		rtn += compCssmData(akpre->keyIdentifier,
			akpost->keyIdentifier, "CE_AuthorityKeyID.keyIdentifier");
	}
	
	if(compBool(akpre->generalNamesPresent, akpost->generalNamesPresent,
			"CE_AuthorityKeyID.generalNamesPresent")) {
		rtn++;
	}
	else if(akpre->generalNamesPresent) {
		rtn += genNamesCompare(akpre->generalNames,
			akpost->generalNames);
	}

	if(compBool(akpre->serialNumberPresent, akpost->serialNumberPresent,
			"CE_AuthorityKeyID.serialNumberPresent")) {
		rtn++;
	}
	else if(akpre->serialNumberPresent) {
		rtn += compCssmData(akpre->serialNumber,
			akpost->serialNumber, "CE_AuthorityKeyID.serialNumber");
	}
	return rtn;
}

static void authKeyIdFree(void *arg)
{
	CE_AuthorityKeyID *akid = (CE_AuthorityKeyID *)arg;

	if(akid->keyIdentifier.Data) {
		free(akid->keyIdentifier.Data);
	}
	if(akid->generalNames) {
		genNamesFree(akid->generalNames);		// genNamesCreate mallocd
		free(akid->generalNames);				// we mallocd
	}
	if(akid->serialNumber.Data) {
		free(akid->serialNumber.Data);
	}
}

#pragma mark --- CE_CertPolicies ---

/* random OIDs, pick 1..NUM_CP_OIDS */
static CSSM_OID cpOids[] = 
{
	CSSMOID_EmailAddress,
	CSSMOID_UnstructuredName,
	CSSMOID_ContentType,
	CSSMOID_MessageDigest
};
#define NUM_CP_OIDS 4

/* CPS strings, pick one of NUM_CPS_STR */
static char *someCPSs[] = 
{
	(char *)"http://www.apple.com",
	(char *)"https://cdnow.com",
	(char *)"ftp:backwards.com"
};
#define NUM_CPS_STR		3

/* make these looks like real sequences */
static uint8 someUnotice[] = {0x30, 0x03, BER_TAG_BOOLEAN, 1, 0xff};
static uint8 someOtherData[] = {0x30, 0x02, BER_TAG_NULL, 0};

static void cpCreate(void *arg)
{
	CE_CertPolicies *cp = (CE_CertPolicies *)arg;
	cp->numPolicies = genRand(1,3);
	//cp->numPolicies = 1;
	unsigned len = sizeof(CE_PolicyInformation) * cp->numPolicies;
	cp->policies = (CE_PolicyInformation *)malloc(len);
	memset(cp->policies, 0, len);
	
	for(unsigned polDex=0; polDex<cp->numPolicies; polDex++) {
		CE_PolicyInformation *pi = &cp->policies[polDex];
		unsigned die = genRand(1, NUM_CP_OIDS);
		pi->certPolicyId = cpOids[die - 1];
		unsigned numQual = genRand(1,3);
		pi->numPolicyQualifiers = numQual;
		len = sizeof(CE_PolicyQualifierInfo) * numQual;
		pi->policyQualifiers = (CE_PolicyQualifierInfo *)
			malloc(len);
		memset(pi->policyQualifiers, 0, len);
		for(unsigned cpiDex=0; cpiDex<numQual; cpiDex++) {
			CE_PolicyQualifierInfo *qi = 
				&pi->policyQualifiers[cpiDex];
			if(randBool()) {
				qi->policyQualifierId = CSSMOID_QT_CPS;
				die = genRand(1, NUM_CPS_STR);
				qi->qualifier.Data = (uint8 *)someCPSs[die - 1];
				qi->qualifier.Length = strlen((char *)qi->qualifier.Data);
			}
			else {
				qi->policyQualifierId = CSSMOID_QT_UNOTICE;
				if(randBool()) {
					qi->qualifier.Data = someUnotice;
					qi->qualifier.Length = 5;
				}
				else {
					qi->qualifier.Data = someOtherData;
					qi->qualifier.Length = 4;
				}
			}
		}
	}
}

static unsigned cpCompare(const void *pre, const void *post)
{
	CE_CertPolicies *cppre  = (CE_CertPolicies *)pre;
	CE_CertPolicies *cppost = (CE_CertPolicies *)post;
	
	if(cppre->numPolicies != cppost->numPolicies) {
		printf("CE_CertPolicies.numPolicies mismatch\n");
		return 1;
	}
	unsigned rtn = 0;
	for(unsigned polDex=0; polDex<cppre->numPolicies; polDex++) {
		CE_PolicyInformation *pipre  = &cppre->policies[polDex];
		CE_PolicyInformation *pipost = &cppost->policies[polDex];
		rtn += compCssmData(pipre->certPolicyId, pipost->certPolicyId,
			"CE_PolicyInformation.certPolicyId");
		if(pipre->numPolicyQualifiers != pipost->numPolicyQualifiers) {
			printf("CE_PolicyInformation.CE_PolicyInformation mismatch\n");
			rtn++;
			continue;
		}
		
		for(unsigned qiDex=0; qiDex<pipre->numPolicyQualifiers; qiDex++) {
			CE_PolicyQualifierInfo *qipre  = &pipre->policyQualifiers[qiDex];
			CE_PolicyQualifierInfo *qipost = &pipost->policyQualifiers[qiDex];
			rtn += compCssmData(qipre->policyQualifierId,
				qipost->policyQualifierId,
				"CE_PolicyQualifierInfo.policyQualifierId");
			rtn += compCssmData(qipre->qualifier,
				qipost->qualifier,
				"CE_PolicyQualifierInfo.qualifier");
		}
	}
	return rtn;
}

static void cpFree(void *arg)
{
	CE_CertPolicies *cp = (CE_CertPolicies *)arg;
	for(unsigned polDex=0; polDex<cp->numPolicies; polDex++) {
		CE_PolicyInformation *pi = &cp->policies[polDex];
		free(pi->policyQualifiers);
	}
	free(cp->policies);
}

#pragma mark --- CE_AuthorityInfoAccess ---

/* random OIDs, pick 1..NUM_AI_OIDS */
static CSSM_OID aiOids[] = 
{
	CSSMOID_AD_OCSP,
	CSSMOID_AD_CA_ISSUERS,
	CSSMOID_AD_TIME_STAMPING,
	CSSMOID_AD_CA_REPOSITORY
};
#define NUM_AI_OIDS 4

static void aiaCreate(void *arg)
{
	CE_AuthorityInfoAccess *aia = (CE_AuthorityInfoAccess *)arg;
	aia->numAccessDescriptions = genRand(1,3);
	unsigned len = aia->numAccessDescriptions * sizeof(CE_AccessDescription);
	aia->accessDescriptions = (CE_AccessDescription *)malloc(len);
	memset(aia->accessDescriptions, 0, len);
	
	for(unsigned dex=0; dex<aia->numAccessDescriptions; dex++) {
		CE_AccessDescription *ad = &aia->accessDescriptions[dex];
		int die = genRand(1, NUM_AI_OIDS);
		ad->accessMethod = aiOids[die - 1];
		genNameCreate(&ad->accessLocation);
	}
}

static unsigned aiaCompare(const void *pre, const void *post)
{
	CE_AuthorityInfoAccess *apre = (CE_AuthorityInfoAccess *)pre;
	CE_AuthorityInfoAccess *apost = (CE_AuthorityInfoAccess *)post;
	unsigned rtn = 0;
	
	if(apre->numAccessDescriptions != apost->numAccessDescriptions) {
		printf("***CE_AuthorityInfoAccess.numAccessDescriptions miscompare\n");
		return 1;
	}
	for(unsigned dex=0; dex<apre->numAccessDescriptions; dex++) {
		CE_AccessDescription *adPre  = &apre->accessDescriptions[dex];
		CE_AccessDescription *adPost = &apost->accessDescriptions[dex];
		if(compCssmData(adPre->accessMethod, adPost->accessMethod, 
				"CE_AccessDescription.accessMethod")) {
			rtn++;
		}
		rtn += genNameCompare(&adPre->accessLocation, &adPost->accessLocation);
	}
	return rtn;
}

static void aiaFree(void *arg)
{
	CE_AuthorityInfoAccess *aia = (CE_AuthorityInfoAccess *)arg;
	for(unsigned dex=0; dex<aia->numAccessDescriptions; dex++) {
		CE_AccessDescription *ad = &aia->accessDescriptions[dex];
		genNameFree(&ad->accessLocation);
	}
	free(aia->accessDescriptions);
}

#pragma mark --- CE_QC_Statements ---

/* a static array of CE_QC_Statement.statementId */
static const CSSM_OID qcsOids[] = {
	CSSMOID_OID_QCS_SYNTAX_V1,
	CSSMOID_OID_QCS_SYNTAX_V2,
	CSSMOID_ETSI_QCS_QC_COMPLIANCE,
};
#define NUM_QCS_OIDS 3
#define WHICH_QCS_V2 1

static void qcsCreate(void *arg)
{
	CE_QC_Statements *qcss = (CE_QC_Statements *)arg;
	//unsigned numQcs = genRand(1,3);
	unsigned numQcs = 1;
	qcss->numQCStatements = numQcs;
	qcss->qcStatements = (CE_QC_Statement *)malloc(numQcs * sizeof(CE_QC_Statement));
	memset(qcss->qcStatements, 0, numQcs * sizeof(CE_QC_Statement));

	for(unsigned dex=0; dex<numQcs; dex++) {
		CE_QC_Statement *qcs = &qcss->qcStatements[dex];
		unsigned whichOid = genRand(0, NUM_QCS_OIDS-1);
		qcs->statementId = qcsOids[whichOid];

		/* three legal combos of (semanticsInfo, otherInfo), constrained by whichOid */
		unsigned coin = genRand(1, 2);
		switch(coin) {
			case 1:
				/* nothing */
				break;
			case 2:
			{
				/* 
				 * CSSMOID_OID_QCS_SYNTAX_V2 --> semanticsInfo 
				 * other --> otherInfo
				 */
				if(whichOid == WHICH_QCS_V2) {
					CE_SemanticsInformation *si = (CE_SemanticsInformation *)malloc(
						sizeof(CE_SemanticsInformation));
					qcs->semanticsInfo = si;
					memset(si, 0, sizeof(CE_SemanticsInformation));

					/* flip a coin; heads --> semanticsIdentifier */
					coin = genRand(1, 2);
					if(coin == 2) {
						si->semanticsIdentifier = (CSSM_OID *)malloc(sizeof(CSSM_OID));
						*si->semanticsIdentifier = qcsOids[0];
					}

					/* flip a coin; heads --> nameRegistrationAuthorities */
					/* also gen this one if semanticsInfo is empty */
					coin = genRand(1, 2);
					if((coin == 2) || (si->semanticsIdentifier == NULL)) {	
						si->nameRegistrationAuthorities = (CE_NameRegistrationAuthorities *)
							malloc(sizeof(CE_NameRegistrationAuthorities));
						genNamesCreate(si->nameRegistrationAuthorities);
					}
				}
				else {
					/* ASN_ANY - just take an encoded NULL */
					CSSM_DATA *otherInfo = (CSSM_DATA *)malloc(sizeof(CSSM_DATA));
					otherInfo->Data = (uint8 *)malloc(2);
					otherInfo->Data[0] = 5;
					otherInfo->Data[1] = 0;
					otherInfo->Length = 2;
					qcs->otherInfo = otherInfo;
				}
				break;
			}
		}
	}
}

static unsigned qcsCompare(const void *pre, const void *post)
{
	CE_QC_Statements *qpre = (CE_QC_Statements *)pre;
	CE_QC_Statements *qpost = (CE_QC_Statements *)post;
	uint32 numQcs = qpre->numQCStatements;
	if(numQcs != qpost->numQCStatements) {
		printf("***numQCStatements miscompare\n");
		return 1;
	}

	unsigned rtn = 0;
	for(unsigned dex=0; dex<numQcs; dex++) {
		CE_QC_Statement *qcsPre  = &qpre->qcStatements[dex];
		CE_QC_Statement *qcsPost = &qpost->qcStatements[dex];
		if(compCssmData(qcsPre->statementId, qcsPost->statementId, 
				"CE_QC_Statement.statementId")) {
			rtn++;
		}
		if(qcsPre->semanticsInfo) {
			if(qcsPost->semanticsInfo == NULL) {
				printf("***semanticsInfo in pre but not in post\n");
				rtn++;
			}
			else {
				CE_SemanticsInformation *siPre  = qcsPre->semanticsInfo;
				CE_SemanticsInformation *siPost = qcsPost->semanticsInfo;
				if((siPre->semanticsIdentifier == NULL) != (siPost->semanticsIdentifier == NULL)) {
					printf("***mismatch in presence of semanticsIdentifier\n");
					rtn++;
				}
				else if(siPre->semanticsIdentifier) {
					if(compCssmData(*siPre->semanticsIdentifier, *siPost->semanticsIdentifier, 
							"CE_SemanticsInformation.semanticsIdentifier")) {
						rtn++;
					}
				}
				if((siPre->nameRegistrationAuthorities == NULL) != 
				   (siPost->nameRegistrationAuthorities == NULL)) {
					printf("***mismatch in presence of nameRegistrationAuthorities\n");
					rtn++;
				}
				else if(siPre->nameRegistrationAuthorities) {
					rtn += genNamesCompare(siPre->nameRegistrationAuthorities,
						siPost->nameRegistrationAuthorities);
				}
			}
		}
		else if(qcsPost->semanticsInfo != NULL) {
			printf("***semanticsInfo in post but not in pre\n");
			rtn++;
		}
		if(qcsPre->otherInfo) {
			if(qcsPost->otherInfo == NULL) {
				printf("***otherInfo in pre but not in post\n");
				rtn++;
			}
			else {
				if(compCssmData(*qcsPre->otherInfo, *qcsPre->otherInfo, 
						"CE_QC_Statement.otherInfo")) {
					rtn++;
				}
			}
		}
		else if(qcsPost->otherInfo != NULL) {
			printf("***otherInfo in post but not in pre\n");
			rtn++;
		}
	}
	return rtn;
}

static void qcsFree(void *arg)
{
	CE_QC_Statements *qcss = (CE_QC_Statements *)arg;
	uint32 numQcs = qcss->numQCStatements;
	for(unsigned dex=0; dex<numQcs; dex++) {
		CE_QC_Statement *qcs = &qcss->qcStatements[dex];
		if(qcs->semanticsInfo) {
			CE_SemanticsInformation *si = qcs->semanticsInfo;
			if(si->semanticsIdentifier) {
				free(si->semanticsIdentifier);
			}
			if(si->nameRegistrationAuthorities) {
				genNamesFree(si->nameRegistrationAuthorities);
				free(si->nameRegistrationAuthorities);
			}
			free(qcs->semanticsInfo);
		}
		if(qcs->otherInfo) {
			free(qcs->otherInfo->Data);
			free(qcs->otherInfo);
		}
	}
	free(qcss->qcStatements);
}

#pragma mark --- test definitions ---

/*
 * Define one extension test.
 */

/* 
 * Cook up this extension with random, reasonable values. 
 * Incoming pointer refers to extension-specific C struct, mallocd
 * and zeroed by main test routine.
 */
typedef void (*extenCreateFcn)(void *arg);

/*
 * Compare two instances of this extension. Return number of 
 * compare errors.
 */
typedef unsigned (*extenCompareFcn)(
	const void *preEncode, 
	const void *postEncode);
	
/*
 * Free struct components mallocd in extenCreateFcn. Do not free
 * the outer struct.
 */
typedef void (*extenFreeFcn)(void *arg);

typedef struct {
	/* three extension-specific functions */
	extenCreateFcn		createFcn;
	extenCompareFcn		compareFcn;
	extenFreeFcn		freeFcn;
	
	/* size of C struct passed to all three functions */
	unsigned			extenSize;
	
	/* the OID for this extension */
	CSSM_OID			extenOid;
	
	/* description for error logging and blob writing */
	const char			*extenDescr;
	
	/* command-line letter for this one */
	char				extenLetter;
	
} ExtenTest;

/* empty freeFcn means no extension-specific resources to free */
#define NO_FREE		NULL

static ExtenTest extenTests[] = {
	{ kuCreate, kuCompare, NO_FREE, 
	  sizeof(CE_KeyUsage), CSSMOID_KeyUsage, 
	  "KeyUsage", 'k' },
	{ bcCreate, bcCompare, NO_FREE,
	  sizeof(CE_BasicConstraints), CSSMOID_BasicConstraints,
	  "BasicConstraints", 'b' },
	{ ekuCreate, ekuCompare, NO_FREE,
	  sizeof(CE_ExtendedKeyUsage), CSSMOID_ExtendedKeyUsage,
	  "ExtendedKeyUsage", 'x' },
	{ skidCreate, skidCompare, skidFree,
	  sizeof(CSSM_DATA), CSSMOID_SubjectKeyIdentifier,
	  "SubjectKeyID", 's' },
	{ authKeyIdCreate, authKeyIdCompare, authKeyIdFree,
	  sizeof(CE_AuthorityKeyID), CSSMOID_AuthorityKeyIdentifier, 
	  "AuthorityKeyID", 'a' },
	{ genNamesCreate, genNamesCompare, genNamesFree, 
	  sizeof(CE_GeneralNames), CSSMOID_SubjectAltName,
	  "SubjectAltName", 't' },
	{ genNamesCreate, genNamesCompare, genNamesFree, 
	  sizeof(CE_GeneralNames), CSSMOID_IssuerAltName,
	  "IssuerAltName", 'i' },
	{ nctCreate, nctCompare, NO_FREE, 
	  sizeof(CE_NetscapeCertType), CSSMOID_NetscapeCertType, 
	  "NetscapeCertType", 'n' },
	{ cdpCreate, cdpCompare, cdpFree,
	  sizeof(CE_CRLDistPointsSyntax), CSSMOID_CrlDistributionPoints,
	  "CRLDistPoints", 'p' },
	{ cpCreate, cpCompare, cpFree,
	  sizeof(CE_CertPolicies), CSSMOID_CertificatePolicies,
	  "CertPolicies", 'c' },
	{ aiaCreate, aiaCompare, aiaFree,
	  sizeof(CE_AuthorityInfoAccess), CSSMOID_AuthorityInfoAccess,
	  "AuthorityInfoAccess", 'A' },
	{ aiaCreate, aiaCompare, aiaFree,
	  sizeof(CE_AuthorityInfoAccess), CSSMOID_SubjectInfoAccess,
	  "SubjectInfoAccess", 'S' },
	{ qcsCreate, qcsCompare, qcsFree,
	  sizeof(CE_QC_Statements), CSSMOID_QC_Statements,
	  "QualifiedCertStatements", 'q' },
};

#define NUM_EXTEN_TESTS		(sizeof(extenTests) / sizeof(ExtenTest))

static void printExten(
	CSSM_X509_EXTENSION	&extn,
	bool				preEncode,
	OidParser			&parser)
{
	CSSM_FIELD field;
	field.FieldOid = extn.extnId;
	field.FieldValue.Data = (uint8 *)&extn;
	field.FieldValue.Length = sizeof(CSSM_X509_EXTENSION);
	printf("=== %s:\n", preEncode ? "PRE-ENCODE" : "POST-DECODE" );
	printCertField(field, parser, CSSM_TRUE);
}

static int doTest(
	CSSM_CL_HANDLE	clHand,
	CSSM_CSP_HANDLE	cspHand,
	ExtenTest		&extenTest,
	bool			writeBlobs,
	const char		*constFileName,	// all blobs to this file if non-NULL
	bool			displayExtens,
	OidParser 		&parser)
{
	/*
	 * 1. Cook up a random and reasonable instance of the C struct 
	 *    associated with this extension.
	 */
	void *preEncode = CSSM_MALLOC(extenTest.extenSize);
	memset(preEncode, 0, extenTest.extenSize);
	extenTest.createFcn(preEncode);

	/*
	 * Cook up the associated CSSM_X509_EXTENSION.
	 */
	CSSM_X509_EXTENSION	extnPre;
	
	extnPre.extnId   			= extenTest.extenOid;
	extnPre.critical 			= randBool();
	extnPre.format   			= CSSM_X509_DATAFORMAT_PARSED;
	extnPre.value.parsedValue 	= preEncode;
	extnPre.BERvalue.Data = NULL;
	extnPre.BERvalue.Length = 0;
	
	/* encode the extension in a TBSCert */
	CSSM_DATA_PTR rawCert = CB_MakeCertTemplate(clHand,
		0x12345678,			// serial number
		dummyName,
		dummyName,
		notBefore,
		notAfter,
		&subjPubKey,
		SIG_ALG,
		NULL,				// subjUniqueId
		NULL,				// issuerUniqueId
		&extnPre,			// extensions
		1);					// numExtensions
	if(rawCert == NULL) {
		printf("Error generating template; aborting.\n");
		/* show what we tried to encode */
		printExten(extnPre, true, parser);
		return 1;
	}
	
	/* sign the cert */
	CSSM_DATA signedCert = {0, NULL};
	CSSM_CC_HANDLE sigHand;
	CSSM_RETURN crtn = CSSM_CSP_CreateSignatureContext(cspHand,
			SIG_ALG,
			NULL,			// no passphrase for now
			&subjPrivKey,
			&sigHand);
	if(crtn) {
		printError("CreateSignatureContext", crtn);
		return 1;
	}
	
	crtn = CSSM_CL_CertSign(clHand,
		sigHand,
		rawCert,			// CertToBeSigned
		NULL,				// SignScope per spec
		0,					// ScopeSize per spec
		&signedCert);
	if(crtn) {
		printError("CSSM_CL_CertSign", crtn);
		/* show what we tried to encode */
		printExten(extnPre, true, parser);
		return 1;
	}
	CSSM_DeleteContext(sigHand);
	
	if(writeBlobs) {
		char fileName[200];
		if(constFileName) {
			strcpy(fileName, constFileName);
		}
		else {
			sprintf(fileName, "%scert.der", extenTest.extenDescr);
		}
		writeFile(fileName, signedCert.Data, signedCert.Length);
		printf("...wrote %lu bytes to %s\n", signedCert.Length, fileName);
	}
	
	/* snag the same extension from the encoded cert */
	CSSM_DATA_PTR postField;
	CSSM_HANDLE resultHand;
	uint32 numFields;
	
	crtn = CSSM_CL_CertGetFirstFieldValue(clHand,
		&signedCert,
		&extenTest.extenOid,
		&resultHand,
		&numFields,
		&postField);
	if(crtn) {
		printf("****Extension field not found on decode for %s\n",
			extenTest.extenDescr);
		printError("CSSM_CL_CertGetFirstFieldValue", crtn);
		
		/* show what we tried to encode and decode */
		printExten(extnPre, true, parser);
		return 1;
	}
	
	if(numFields != 1) {
		printf("****GetFirstFieldValue: expect 1 value, got %u\n",
			(unsigned)numFields);
		return 1;
	}
	CSSM_CL_CertAbortQuery(clHand, resultHand);
	
	/* verify the fields we generated */
	CSSM_X509_EXTENSION *extnPost = (CSSM_X509_EXTENSION *)postField->Data;
	if((extnPost == NULL) || 
	   (postField->Length != sizeof(CSSM_X509_EXTENSION))) {
		printf("***Malformed CSSM_X509_EXTENSION (1) after decode\n");
		return 1;
	}
	int rtn = 0;
	rtn += compBool(extnPre.critical, extnPost->critical, 
		"CSSM_X509_EXTENSION.critical");
	rtn += compCssmData(extnPre.extnId, extnPost->extnId, 
		"CSSM_X509_EXTENSION.extnId");
		
	if(extnPost->format != CSSM_X509_DATAFORMAT_PARSED) {
		printf("***Expected CSSM_X509_DATAFORMAT_PARSED (%x(x), got %x(x)\n",
			CSSM_X509_DATAFORMAT_PARSED, extnPost->format);
	}
	if(extnPost->value.parsedValue == NULL) {
		printf("***no parsedValue pointer!\n");
		return 1;
	}
	
	/* down to extension-specific compare */
	rtn += extenTest.compareFcn(preEncode, extnPost->value.parsedValue);
	
	if(rtn) {
		/* print preencode only on error */
		printExten(extnPre, true, parser);
	}
	if(displayExtens || rtn) {
		printExten(*extnPost, false, parser);
	}
	
	/* free the allocated data */
	if(extenTest.freeFcn) {
		extenTest.freeFcn(preEncode);
	}
	CSSM_CL_FreeFieldValue(clHand, &extenTest.extenOid, postField);
	CSSM_FREE(rawCert->Data);
	CSSM_FREE(rawCert);
	CSSM_FREE(signedCert.Data);
	CSSM_FREE(preEncode);
	return rtn;
}

static void doPause(bool pause)
{
	if(!pause) {
		return;
	}
	fpurge(stdin);
	printf("CR to continue ");
	getchar();
}

int main(int argc, char **argv)
{
	CSSM_CL_HANDLE		clHand;
	CSSM_CSP_HANDLE		cspHand;
	CSSM_RETURN			crtn;
	int					arg;
	int					rtn;
	OidParser			parser;
	char				*argp;
	unsigned			i;
	
	/* user-specificied params */
	unsigned			minExtenNum = 0;
	unsigned 			maxExtenNum = NUM_EXTEN_TESTS-1;
	bool				writeBlobs = false;
	bool				displayExtens = false;
	bool				quiet = false;
	unsigned			loops = LOOPS_DEF;
	bool				pauseLoop = false;
	bool				pauseCert = false;
	const char			*constFileName = NULL;

	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'w':
				writeBlobs = true;
				break;
			case 'd':
				displayExtens = true;
				break;
			case 'q':
				quiet = true;
				break;
			case 'p':
				pauseLoop = true;
				break;
			case 'P':
				pauseCert = true;
				break;
			case 'l':
				loops = atoi(&argp[2]);
				break;
			case 'f':
				constFileName = &argp[2];
				break;
			case 'e':
				if(argp[1] != '=') {
					usage(argv);
				}
				/* scan thru test array looking for epecified extension */
				for(i=0; i<NUM_EXTEN_TESTS; i++) {
					if(extenTests[i].extenLetter == argp[2]) {
						minExtenNum = maxExtenNum = i;
						break;
					}
				}
				if(i == NUM_EXTEN_TESTS) {
					usage(argv);
				}
				break;
			default:
				usage(argv);
		}
	}
	
	
	/* common setup */
	clHand = clStartup();
	if(clHand == 0) {
		return 0;
	}
	cspHand = cspStartup();
	if(cspHand == 0) {
		return 0;
	}

	printf("Starting extenTest; args: ");
	for(i=1; i<(unsigned)argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");

	/* one common key pair - we're definitely not testing this */
	crtn = cspGenKeyPair(cspHand,
		KEY_ALG,
		SUBJ_KEY_LABEL,
		strlen(SUBJ_KEY_LABEL),
		KEY_SIZE_BITS,
		&subjPubKey,
		CSSM_FALSE,			// pubIsRef - should work both ways, but not yet
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&subjPrivKey,
		CSSM_TRUE,			// privIsRef - doesn't matter
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		exit(1);
	}
	
	/* common issuer/subject - not testing this */
	dummyName = CB_BuildX509Name(dummyRdn, NUM_DUMMY_NAMES);
	if(dummyName == NULL) {
		printf("CB_BuildX509Name failure");
		exit(1);
	}
	
	/* not before/after in generalized time format */
	notBefore = CB_BuildX509Time(0);
	notAfter  = CB_BuildX509Time(10000);

	for(unsigned loop=0; loop<loops; loop++) {
		if(!quiet) {
			printf("...loop %u\n", loop);
		}
		for(unsigned extenDex=minExtenNum; extenDex<=maxExtenNum; extenDex++) {
			rtn = doTest(clHand, cspHand, extenTests[extenDex],
				writeBlobs, constFileName, displayExtens, parser);
			if(rtn) {
				break;
			}
			doPause(pauseCert);
		}
		if(rtn) {
			break;
		}
		doPause(pauseLoop);
	}
	
	if(rtn) {
		printf("***%s FAILED\n", argv[0]);
	}
	else if(!quiet) {
		printf("...%s passed\n", argv[0]);
	}
	
	
	/* cleanup */
	CB_FreeX509Name(dummyName);
	CB_FreeX509Time(notBefore);
	CB_FreeX509Time(notAfter);
	CSSM_ModuleDetach(cspHand);
	CSSM_ModuleDetach(clHand);
	return 0;
}

