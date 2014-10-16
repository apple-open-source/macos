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

#ifndef	_PRINT_FIELDS_H_
#define _PRINT_FIELDS_H_

#include <libDER/libDER.h>

#ifdef __cplusplus
extern "C" {
#endif

void doIndent(void);
void incrIndent(void);
void decrIndent(void);
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
