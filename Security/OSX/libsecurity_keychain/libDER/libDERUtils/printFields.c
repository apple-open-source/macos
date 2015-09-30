/*
 * Copyright (c) 2005-2007,2011-2012,2014 Apple Inc. All Rights Reserved.
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


/*
 * printFeilds.h - print various DER objects
 *
 */

#include <libDERUtils/printFields.h>
#include <libDER/DER_Decode.h>
#include <libDER/asn1Types.h>
#include <libDER/DER_Keys.h>
#include <libDERUtils/libDERUtils.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>

static int indentLevel = 0;

void doIndent(void)
{
	int i;
	for (i = 0; i<indentLevel; i++) {
		putchar(' ');
	}
} /* indent */

void incrIndent(void)
{
	indentLevel += 3;
}

void decrIndent(void)
{
	indentLevel -= 3;
}

#define TO_PRINT_MAX	12

void printHex(
	DERItem *item)
{
	unsigned long dex;
	unsigned long toPrint = item->length;
	
	printf("<%lu> ", item->length);
	if(toPrint > TO_PRINT_MAX) {
		toPrint = TO_PRINT_MAX;
	}
	for(dex=0; dex<toPrint; dex++) {
		printf("%02x ", item->data[dex]);
	}
	if(item->length > TO_PRINT_MAX) {
		printf("...");
	}
	printf("\n");
}

void printBitString(
	DERItem *item)
{
	DERSize dex;
	DERSize toPrint = item->length;
	DERItem bitStringBytes;
	DERByte numUnused;
	DERReturn drtn;
			
	drtn = DERParseBitString(item, &bitStringBytes, &numUnused);
	if(drtn) {
		DERPerror("DERParseBitString", drtn);
		return;
	}

	printf("<%lu, %lu> ", (unsigned long)bitStringBytes.length, (unsigned long)numUnused);
	toPrint = bitStringBytes.length;
	if(toPrint > TO_PRINT_MAX) {
		toPrint = TO_PRINT_MAX;
	}
	for(dex=0; dex<toPrint; dex++) {
		printf("%02x ", bitStringBytes.data[dex]);
	}
	if(item->length > TO_PRINT_MAX) {
		printf("...");
	}
	printf("\n");
}

void printString(
	DERItem *item)
{
	unsigned dex;
	char *cp = (char *)item->data;
	printf("'");
	for(dex=0; dex<item->length; dex++) {
		putchar(*cp++);
	}
	printf("'\n");

}

#define COLON_COLUMN	20

/*
 * Print line header, with current indent, followed by specified label, followed
 * by a ':' in column COLON_COLUMN, followed by one space. 
 */
void printHeader(
	const char *label)
{
	size_t numPrinted;
	
	doIndent();
	printf("%s", label);
	numPrinted = indentLevel + strlen(label);
	if(numPrinted < COLON_COLUMN) {
		size_t numSpaces = COLON_COLUMN - numPrinted;
		size_t dex;
		for(dex=0; dex<numSpaces; dex++) {
			putchar(' ');
		}
	}
	printf(": ");
}

void printItem(
	const char *label,
	ItemType itemType,
	int verbose,
	DERTag tag,         // maybe from decoding, maybe the real tag underlying
						// an implicitly tagged item
	DERItem *item)		// content 
{
	DERTag tagClass = tag & ASN1_CLASS_MASK;
	DERTag tagNum = tag & ASN1_TAGNUM_MASK;
	char printable = 0;
	char *asnType = NULL;

	printHeader(label);
	
	if((itemType == IT_Branch) && !verbose) {
		printf("\n");
		return;
	}
	switch(tagClass) {
		case ASN1_UNIVERSAL:
			break;		// proceed with normal tags */
		case ASN1_APPLICATION:
			printf("APPLICATION (tag %u) ", tagNum);
			printHex(item);
			return;
		case ASN1_CONTEXT_SPECIFIC:
			printf("CONTEXT SPECIFIC (tag %u) ", tagNum);
			printHex(item);
			return;
		case ASN1_PRIVATE:
			printf("PRIVATE (tag %u) ", tagNum);
			printHex(item);
			return;
	}
	switch(tagNum) {
		case ASN1_BOOLEAN:
			asnType = "BOOLEAN";
			break;
		case ASN1_INTEGER:
			asnType = "INTEGER";
			break;
		case ASN1_BIT_STRING:
			/* special case here... */
			printf("BIT STRING ");
			printBitString(item);
			return;
		case ASN1_OCTET_STRING:
			asnType = "OCTET STRING";
			break;
		case ASN1_NULL:
			asnType = "NULL";
			break;
		case ASN1_OBJECT_ID:
			asnType = "OID";
			break;
		case ASN1_OBJECT_DESCRIPTOR:
			asnType = "OBJECT_DESCRIPTOR";
			break;
		case ASN1_REAL:
			asnType = "REAL";
			break;
		case ASN1_ENUMERATED:
			asnType = "ENUM";
			break;
		case ASN1_EMBEDDED_PDV:
			asnType = "EMBEDDED_PDV";
			break;
		case ASN1_UTF8_STRING:
			asnType = "UTF8 STRING";
			/* FIXME print these too */
			break;
		case ASN1_SEQUENCE:
			asnType = "SEQ";
			break;
		case ASN1_SET:
			asnType = "SET";
			break;
		case ASN1_NUMERIC_STRING:
			asnType = "NUMERIC_STRING";
			break;
		case ASN1_PRINTABLE_STRING:
			asnType = "PRINTABLE_STRING";
			printable = 1;
			break;
		case ASN1_T61_STRING:
			asnType = "T61_STRING";
			printable = 1;
			break;
		case ASN1_VIDEOTEX_STRING:
			asnType = "VIDEOTEX_STRING";
			printable = 1;
			break;
		case ASN1_IA5_STRING:
			asnType = "IA5_STRING";
			printable = 1;
			break;
		case ASN1_UTC_TIME:
			asnType = "UTC_TIME";
			printable = 1;
			break;
		case ASN1_GENERALIZED_TIME:
			asnType = "GENERALIZED_TIME";
			printable = 1;
			break;
		case ASN1_GRAPHIC_STRING:
			asnType = "GRAPHIC_STRING";
			break;
		case ASN1_VISIBLE_STRING:
			asnType = "VISIBLE_STRING";
			break;
		case ASN1_GENERAL_STRING:
			asnType = "GENERAL_STRING";
			break;
		case ASN1_UNIVERSAL_STRING:
			asnType = "UNIVERSAL_STRING";
			break;
		case ASN1_BMP_STRING:
			asnType = "BMP_STRING";
			break;
		default:
			asnType = "[unknown]";
			break;
	}
	printf("%s ", asnType);
	if(printable) {
		printString(item);
	}
	else {
		printHex(item);
	}
}

void printAlgId(
	const DERItem *content,
	int verbose)
{
	DERReturn drtn;
	DERAlgorithmId algId;
	
	drtn = DERParseSequenceContent(content,
		DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
		&algId, sizeof(algId));
	if(drtn) {
		DERPerror("DERParseSequenceContent(algId)", drtn);
		return;
	}
	printItem("alg", IT_Leaf, verbose, ASN1_OBJECT_ID, &algId.oid);
	if(algId.params.data) {
		printItem("params", IT_Leaf, verbose, algId.params.data[0], &algId.params);
	}
}

void printSubjPubKeyInfo(
	const DERItem *content,
	int verbose)
{
	DERReturn drtn;
	DERSubjPubKeyInfo pubKeyInfo;
	DERRSAPubKeyPKCS1 pkcs1Key;
	DERItem bitStringContents;
	DERByte numUnused;
	
	drtn = DERParseSequence(content,
		DERNumSubjPubKeyInfoItemSpecs, DERSubjPubKeyInfoItemSpecs,
		&pubKeyInfo, sizeof(pubKeyInfo));
	if(drtn) {
		DERPerror("DERParseSequenceContent(pubKeyInfo)", drtn);
		return;
	}
	printItem("algId", IT_Branch, verbose, ASN1_CONSTR_SEQUENCE, &pubKeyInfo.algId);
	incrIndent();
	printAlgId(&pubKeyInfo.algId, verbose);
	decrIndent();

	printItem("pubKey", IT_Branch, verbose, ASN1_BIT_STRING, &pubKeyInfo.pubKey);
	
	/* 
	 * The contents of that bit string are a PKCS1 format RSA key. 
	 */
	drtn = DERParseBitString(&pubKeyInfo.pubKey, &bitStringContents, &numUnused);
	if(drtn) {
		DERPerror("DERParseBitString(pubKeyInfo.pubKey)", drtn);
		decrIndent();
		return;
	}
	drtn = DERParseSequence(&bitStringContents,
		DERNumRSAPubKeyPKCS1ItemSpecs, DERRSAPubKeyPKCS1ItemSpecs,
		&pkcs1Key, sizeof(pkcs1Key));
	if(drtn) {
		DERPerror("DERParseSequenceContent(pubKeyBits)", drtn);
		decrIndent();
		return;
	}
	incrIndent();
	printItem("modulus", IT_Leaf, verbose, ASN1_INTEGER, &pkcs1Key.modulus);
	printItem("pubExponent", IT_Leaf, verbose, ASN1_INTEGER, &pkcs1Key.pubExponent);
	
	decrIndent();
}

/* decode one item and print it */
void decodePrintItem(
	const char *label,
	ItemType itemType,
	int verbose,
	DERItem *derItem)
{
	DERDecodedInfo decoded;
	DERReturn drtn;
	
	drtn = DERDecodeItem(derItem, &decoded);
	if(drtn) {
		DERPerror("DERDecodeItem()", drtn);
		return;
	}
	printItem(label, IT_Leaf, 0, decoded.tag, &decoded.content);
}

