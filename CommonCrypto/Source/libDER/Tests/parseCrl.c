/*
 * Copyright (c) 2005-2007,2010 Apple Inc. All Rights Reserved.
 *
 * parseCrl.c - parse a DER-encoded X509 CRL using libDER. 
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
	printf("usage: %s crlFile [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -v     -- verbose \n");
	/* etc. */
	exit(1);
}

/* 
 * This is a SEQUENCE OF so we use the low-level DERDecodeSeq* routines to snag one entry 
 * at a time.
 */
static void	printRevokedCerts(
	DERItem *revokedCerts, 
	int verbose)
{
	DERReturn drtn;
	DERDecodedInfo currItem;
	DERSequence seq;
	unsigned certNum;
	DERRevokedCert revoked;
	
	drtn = DERDecodeSeqContentInit(revokedCerts, &seq);
	if(drtn) {
		DERPerror("DERDecodeSeqContentInit(revokedCerts)", drtn);
		return;
	}
	
	for(certNum=0; ; certNum++) {
		drtn = DERDecodeSeqNext(&seq, &currItem);
		switch(drtn) {
			case DR_EndOfSequence:
				/* normal termination */
				return;
			default:
				DERPerror("DERDecodeSeqNext", drtn);
				return;
			case DR_Success:
				doIndent();
				printf("revoked cert %u\n", certNum);
				incrIndent();
				drtn = DERParseSequenceContent(&currItem.content, 
					DERNumRevokedCertItemSpecs, DERRevokedCertItemSpecs,
					&revoked, sizeof(revoked));
				if(drtn) {
					DERPerror("DERParseSequenceContent(RevokedCert)", drtn);
					decrIndent();
					return;
				}
				printItem("serialNum", IT_Leaf, verbose, ASN1_INTEGER, &revoked.serialNum);
				decodePrintItem("revocationDate",  IT_Leaf, verbose, &revoked.revocationDate);
				printItem("extensions", IT_Branch, verbose, ASN1_CONSTR_SEQUENCE, &revoked.extensions);
				decrIndent();
		}
	}
}

int main(int argc, char **argv)
{
	unsigned char *crlData = NULL;
	unsigned crlDataLen = 0;
	DERSignedCertCrl signedCrl;
	DERTBSCrl tbs;
	DERReturn drtn;
	DERItem item;
	int verbose = 0;
	extern char *optarg;
	int arg;
	extern int optind;
	
	if(argc < 2) {
		usage(argv);
	}
	if(readFile(argv[1], &crlData, &crlDataLen)) {
		printf("***Error reading CRL from %s. Aborting.\n", argv[1]);
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

	/* Top level decode of signed CRL into 3 components */
	item.data = crlData;
	item.length = crlDataLen;
	drtn = DERParseSequence(&item, DERNumSignedCertCrlItemSpecs, DERSignedCertCrlItemSpecs,
		&signedCrl, sizeof(signedCrl));
	if(drtn) {
		DERPerror("DERParseSequence(SignedCrl)", drtn);
		exit(1);
	}
	printItem("TBSCrl", IT_Branch, verbose, ASN1_CONSTR_SEQUENCE, &signedCrl.tbs);
	
	incrIndent();
	
	/* decode the TBSCrl - it was saved in full DER form */
	drtn = DERParseSequence(&signedCrl.tbs, 
		DERNumTBSCrlItemSpecs, DERTBSCrlItemSpecs,
		&tbs, sizeof(tbs));
	if(drtn) {
		DERPerror("DERParseSequenceContent(TBSCrl)", drtn);
		exit(1);
	}
	if(tbs.version.data) {
		printItem("version", IT_Leaf, verbose, ASN1_INTEGER, &tbs.version);
	}
	
	printItem("tbsSigAlg", IT_Branch, verbose, ASN1_CONSTR_SEQUENCE, &tbs.tbsSigAlg);
	incrIndent();
	printAlgId(&tbs.tbsSigAlg, verbose);
	decrIndent();
	
	printItem("issuer", IT_Leaf, verbose, ASN1_CONSTR_SEQUENCE, &tbs.issuer);
	
	decodePrintItem("thisUpdate",  IT_Leaf, verbose, &tbs.thisUpdate);
	decodePrintItem("nextUpdate",  IT_Leaf, verbose, &tbs.nextUpdate);
	
	if(tbs.revokedCerts.data) {
		printItem("version", IT_Leaf, verbose, ASN1_CONSTR_SEQUENCE, &tbs.revokedCerts);
		incrIndent();
		printRevokedCerts(&tbs.revokedCerts, verbose);
		decrIndent();
	}
	
	if(tbs.extensions.data) {
		printItem("extensions", IT_Leaf, verbose, ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 3, 
			&tbs.extensions);
	}
	
	printItem("sigAlg", IT_Branch, verbose, ASN1_CONSTR_SEQUENCE, &signedCrl.sigAlg);
	incrIndent();
	printAlgId(&signedCrl.sigAlg, verbose);
	decrIndent();

	printItem("sig", IT_Leaf, verbose, ASN1_BIT_STRING, &signedCrl.sig);
	
	return 0;
}
