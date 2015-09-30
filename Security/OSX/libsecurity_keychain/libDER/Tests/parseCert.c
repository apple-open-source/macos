/*
 * Copyright (c) 2005-2007,2010-2011 Apple Inc. All Rights Reserved.
 *
 * parseCert.c - parse a DER-encoded X509 certificate using libDER. 
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <libDER/libDER.h>
#include <libDER/asn1Types.h>
#include <libDER/DER_CertCrl.h>
#include <libDER/DER_Keys.h>
#include <libDERUtils/fileIo.h>
#include <libDERUtils/libDERUtils.h>
#include <libDERUtils/printFields.h>

static void usage(char **argv)
{
	printf("usage: %s certFile [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -v     -- verbose \n");
	/* etc. */
	exit(1);
}

static void	printValidity(
	DERItem *validity, 
	int verbose)
{
	DERReturn drtn;
	DERValidity derv;
	
	drtn = DERParseSequenceContent(validity,
		DERNumValidityItemSpecs, DERValidityItemSpecs,
		&derv, sizeof(derv));
	if(drtn) {
		DERPerror("DERParseSequenceContent(validity)", drtn);
		return;
	}
	decodePrintItem("notBefore", IT_Leaf, verbose, &derv.notBefore);
	decodePrintItem("notAfter",  IT_Leaf, verbose, &derv.notAfter);
	
}

int main(int argc, char **argv)
{
	unsigned char *certData = NULL;
	unsigned certDataLen = 0;
	DERSignedCertCrl signedCert;
	DERTBSCert tbs;
	DERReturn drtn;
	DERItem item;
	int verbose = 0;
	extern char *optarg;
	int arg;
	extern int optind;
	
	if(argc < 2) {
		usage(argv);
	}
	if(readFile(argv[1], &certData, &certDataLen)) {
		printf("***Error reading cert from %s. Aborting.\n", argv[1]);
		exit(1);
	}

	optind = 2;
	while ((arg = getopt(argc, argv, "vh")) != -1) {
		switch (arg) {
			case 'v':
				verbose = 1;
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}

	/* Top level decode of signed cert into 3 components */
	item.data = certData;
	item.length = certDataLen;
	drtn = DERParseSequence(&item, DERNumSignedCertCrlItemSpecs, DERSignedCertCrlItemSpecs,
		&signedCert, sizeof(signedCert));
	if(drtn) {
		DERPerror("DERParseSequence(SignedCert)", drtn);
		exit(1);
	}
	printItem("TBSCert", IT_Branch, verbose, ASN1_CONSTR_SEQUENCE, &signedCert.tbs);
	
	incrIndent();
	
	/* decode the TBSCert - it was saved in full DER form */
	drtn = DERParseSequence(&signedCert.tbs, 
		DERNumTBSCertItemSpecs, DERTBSCertItemSpecs,
		&tbs, sizeof(tbs));
	if(drtn) {
		DERPerror("DERParseSequenceContent(TBSCert)", drtn);
		exit(1);
	}
	if(tbs.version.data) {
		/* unwrap the explicitly tagged integer.... */
		decodePrintItem("version", IT_Leaf, verbose, &tbs.version);
	}
	printItem("serialNum", IT_Leaf, verbose, ASN1_INTEGER, &tbs.serialNum);
	
	printItem("tbsSigAlg", IT_Branch, verbose, ASN1_CONSTR_SEQUENCE, &tbs.tbsSigAlg);
	incrIndent();
	printAlgId(&tbs.tbsSigAlg, verbose);
	decrIndent();
	
	printItem("issuer", IT_Leaf, verbose, ASN1_CONSTR_SEQUENCE, &tbs.issuer);
	printItem("subject", IT_Leaf, verbose, ASN1_CONSTR_SEQUENCE, &tbs.subject);
	
	printItem("validity", IT_Branch, verbose, ASN1_CONSTR_SEQUENCE, &tbs.validity);
	incrIndent();
	printValidity(&tbs.validity, verbose);
	decrIndent();
	
	printItem("subjectPubKey", IT_Branch, verbose, ASN1_CONSTR_SEQUENCE, 
		&tbs.subjectPubKey);
	incrIndent();
	printSubjPubKeyInfo(&tbs.subjectPubKey, verbose);
	decrIndent();
	
	if(tbs.issuerID.data) {
		/* found tag is implicit context specific: tell printItem what it really is */
		printItem("issuerID", IT_Leaf, verbose, ASN1_BIT_STRING, &tbs.issuerID);
	}
	if(tbs.subjectID.data) {
		printItem("subjectID", IT_Leaf, verbose, ASN1_BIT_STRING, &tbs.subjectID);
	}
	if(tbs.extensions.data) {
		printItem("extensions", IT_Leaf, verbose, ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 3, 
			&tbs.extensions);
	}
	decrIndent();
	
	printItem("sigAlg", IT_Branch, verbose, ASN1_CONSTR_SEQUENCE, &signedCert.sigAlg);
	incrIndent();
	printAlgId(&signedCert.sigAlg, verbose);
	decrIndent();

	printItem("sig", IT_Leaf, verbose, ASN1_BIT_STRING, &signedCert.sig);
	
	return 0;
}
