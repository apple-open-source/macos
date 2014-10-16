#include <Security/Security.h>
#include "printCertName.h"
#include <clAppUtils/clutils.h>
#include <utilLib/common.h>

static CSSM_CL_HANDLE gClHand = 0;

static CSSM_CL_HANDLE getClHand()
{
    if(gClHand) {
		return gClHand;
    }
    gClHand = clStartup();
    return gClHand;
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

static void printData(
    const CSSM_DATA *cd)
{
    for(unsigned dex=0; dex<cd->Length; dex++) {
		printf("%02X", cd->Data[dex]);
		if((dex % 4) == 3) {
			printf(" ");
		}
	}
    printf("\n");
}

/*
 * Print an CSSM_X509_TYPE_VALUE_PAIR
 */
static void printAtv(
    const CSSM_X509_TYPE_VALUE_PAIR_PTR atv)
{
    const CSSM_OID *oid = &atv->type;
    const char *fieldName = "Other";
    if(appCompareCssmData(oid, &CSSMOID_CountryName)) {
		fieldName = "Country       ";      
    }
    else if(appCompareCssmData(oid, &CSSMOID_OrganizationName)) {
		fieldName = "Org           ";      
    }
    else if(appCompareCssmData(oid, &CSSMOID_LocalityName)) {
		fieldName = "Locality      ";      
    }
    else if(appCompareCssmData(oid, &CSSMOID_OrganizationalUnitName)) {
		fieldName = "OrgUnit       ";      
    }
    else if(appCompareCssmData(oid, &CSSMOID_CommonName)) {
		fieldName = "Common Name   ";      
    }
    else if(appCompareCssmData(oid, &CSSMOID_Surname)) {
		fieldName = "Surname       ";      
    }
    else if(appCompareCssmData(oid, &CSSMOID_Title)) {
		fieldName = "Title         ";      
    }
    else if(appCompareCssmData(oid, &CSSMOID_Surname)) {
		fieldName = "Surname       ";      
    }
    else if(appCompareCssmData(oid, &CSSMOID_StateProvinceName)) {
		fieldName = "State         ";      
    }
    else if(appCompareCssmData(oid, &CSSMOID_CollectiveStateProvinceName)) {
		fieldName = "Coll. State   ";      
    }
    else if(appCompareCssmData(oid, &CSSMOID_EmailAddress)) {
		/* deprecated, used by Thawte */
		fieldName = "Email addrs   ";      
    }
    else {
		fieldName = "Other name    ";      
    }
    printf("      %s : ", fieldName);
    switch(atv->valueType) {
		case BER_TAG_PRINTABLE_STRING:
		case BER_TAG_IA5_STRING:	
		case BER_TAG_T61_STRING:		// mostly printable....	
		case BER_TAG_PKIX_UTF8_STRING:	// ditto
			printString(&atv->value);
			break;
		default:
			printData(&atv->value);
			break;
    }
}

/*
 * Print contents of a CSSM_X509_NAME.
 */
static void printName(
    const char *title,
	const CSSM_X509_NAME *name)
{
    printf("   %s:\n", title);
    unsigned numRdns = name->numberOfRDNs;
    for(unsigned rdnDex=0; rdnDex<numRdns; rdnDex++) {
		const CSSM_X509_RDN *rdn = &name->RelativeDistinguishedName[rdnDex];
		unsigned numAtvs = rdn->numberOfPairs;
		for(unsigned atvDex=0; atvDex<numAtvs; atvDex++) {
			printAtv(&rdn->AttributeTypeAndValue[atvDex]);
		}
    }
}

static void printOneCertName(
    CSSM_CL_HANDLE clHand,
    CSSM_HANDLE cacheHand,
    const char *title,
    const CSSM_OID *oid)
{
    CSSM_HANDLE resultHand = 0;
    CSSM_DATA_PTR field = NULL;
    uint32 numFields;
    CSSM_RETURN crtn;
    
    crtn = CSSM_CL_CertGetFirstCachedFieldValue(clHand, cacheHand,
		oid, &resultHand, &numFields, &field);
    if(crtn) {
		printf("***Error parsing cert\n");
		cssmPerror("CSSM_CL_CertGetFirstCachedFieldValue", crtn);
		return;
    }
    printName(title, (CSSM_X509_NAME_PTR)field->Data);
    CSSM_CL_FreeFieldValue(clHand, oid, field);
	CSSM_CL_CertAbortQuery(clHand, resultHand);
}

/*
 * Print subject and/or issuer of a cert.
 */
void printCertName(
    const unsigned char *cert,
    unsigned certLen,
    WhichName whichName)
{
    CSSM_CL_HANDLE clHand = getClHand();
    CSSM_HANDLE cacheHand;
    CSSM_DATA certData = {certLen, (uint8 *)cert};
    CSSM_RETURN crtn;
    bool printSubj = false;
    bool printIssuer = false;
    
    switch(whichName) {
		case NameBoth:
			printSubj = true;
			printIssuer = true;
			break;
		case NameSubject:
			printSubj = true;
			break;
		case NameIssuer:
			printIssuer = true;
			break;
		default:
			printf("***BRRZAP! Illegal whichName argument\n");
			return;
    }
    
    crtn = CSSM_CL_CertCache(clHand, &certData, &cacheHand);
    if(crtn) {
		printf("***Error parsing cert\n");
		cssmPerror("CSSM_CL_CertCache", crtn);
		return;
    }
    
    if(printSubj) {
		printOneCertName(clHand, cacheHand, "Subject", &CSSMOID_X509V1SubjectNameCStruct);
    }
    if(printIssuer) {
		printOneCertName(clHand, cacheHand, "Issuer", &CSSMOID_X509V1IssuerNameCStruct);
    }
    CSSM_CL_CertAbortCache(clHand, cacheHand);
    return;
}
