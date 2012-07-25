/* Copyright (c) 2005-2007 Apple Inc. All Rights Reserved. */

/*
 * printFeilds.h - print various DER objects
 *
 * Created Nov. 9 2005 by dmitch
 */

#ifndef	_PRINT_FIELDS_H_
#define _PRINT_FIELDS_H_

#include <libDER/libDER.h>

#ifdef __cplusplus
extern "C" {
#endif

void doIndent();
void incrIndent();
void decrIndent();
void printHex(DERItem *item);
void printBitString(DERItem *item);
void printString(DERItem *item);
void printHeader(const char *label);

typedef enum {
	IT_Leaf,		// leaf; always print contents
	IT_Branch		// branch; print contents iff verbose
} ItemType;

void printItem(
	const char *label,
	ItemType itemType,
	int verbose,
	DERTag tag,         // maybe from decoding, maybe the real tag underlying
						// an implicitly tagged item
	DERItem *item);		// content 

void printAlgId(
	const DERItem *content,
	int verbose);
void printSubjPubKeyInfo(
	const DERItem *content,
	int verbose);
	
/* decode one item and print it */
void decodePrintItem(
	const char *label,
	ItemType itemType,
	int verbose,
	DERItem *derItem);

#ifdef __cplusplus
}
#endif

#endif	/* _PRINT_FIELDS_H_ */
