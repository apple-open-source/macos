/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
/*
 * asn1Templates.h - Common ASN1 templates for use with libNSSDer.
 */

#ifndef	_ASN1_TEMPLATES_H_
#define _ASN1_TEMPLATES_H_

#include <SecurityNssAsn1/secasn1.h>

SEC_BEGIN_PROTOS

/************************************************************************/

/*
 * Generic Templates
 * One for each of the simple types, plus a special one for ANY, plus:
 *	- a pointer to each one of those
 *	- a set of each one of those
 *	- a sequence of each one of those
 *
 * Note that these are alphabetical (case insensitive); please add new
 * ones in the appropriate place.
 */

extern const SEC_ASN1Template SEC_AnyTemplate[];
extern const SEC_ASN1Template SEC_BitStringTemplate[];
extern const SEC_ASN1Template SEC_BMPStringTemplate[];
extern const SEC_ASN1Template SEC_BooleanTemplate[];
extern const SEC_ASN1Template SEC_EnumeratedTemplate[];
extern const SEC_ASN1Template SEC_GeneralizedTimeTemplate[];
extern const SEC_ASN1Template SEC_IA5StringTemplate[];
extern const SEC_ASN1Template SEC_IntegerTemplate[];
extern const SEC_ASN1Template SEC_UnsignedIntegerTemplate[];
extern const SEC_ASN1Template SEC_NullTemplate[];
extern const SEC_ASN1Template SEC_ObjectIDTemplate[];
extern const SEC_ASN1Template SEC_OctetStringTemplate[];
extern const SEC_ASN1Template SEC_PrintableStringTemplate[];
extern const SEC_ASN1Template SEC_T61StringTemplate[];
extern const SEC_ASN1Template SEC_UniversalStringTemplate[];
extern const SEC_ASN1Template SEC_UTCTimeTemplate[];
extern const SEC_ASN1Template SEC_UTF8StringTemplate[];
extern const SEC_ASN1Template SEC_VisibleStringTemplate[];
extern const SEC_ASN1Template SEC_TeletexStringTemplate[];

extern const SEC_ASN1Template SEC_PointerToAnyTemplate[];
extern const SEC_ASN1Template SEC_PointerToBitStringTemplate[];
extern const SEC_ASN1Template SEC_PointerToBMPStringTemplate[];
extern const SEC_ASN1Template SEC_PointerToBooleanTemplate[];
extern const SEC_ASN1Template SEC_PointerToEnumeratedTemplate[];
extern const SEC_ASN1Template SEC_PointerToGeneralizedTimeTemplate[];
extern const SEC_ASN1Template SEC_PointerToIA5StringTemplate[];
extern const SEC_ASN1Template SEC_PointerToIntegerTemplate[];
extern const SEC_ASN1Template SEC_PointerToNullTemplate[];
extern const SEC_ASN1Template SEC_PointerToObjectIDTemplate[];
extern const SEC_ASN1Template SEC_PointerToOctetStringTemplate[];
extern const SEC_ASN1Template SEC_PointerToPrintableStringTemplate[];
extern const SEC_ASN1Template SEC_PointerToT61StringTemplate[];
extern const SEC_ASN1Template SEC_PointerToUniversalStringTemplate[];
extern const SEC_ASN1Template SEC_PointerToUTCTimeTemplate[];
extern const SEC_ASN1Template SEC_PointerToUTF8StringTemplate[];
extern const SEC_ASN1Template SEC_PointerToVisibleStringTemplate[];
extern const SEC_ASN1Template SEC_PointerToTeletexStringTemplate[];

extern const SEC_ASN1Template SEC_SequenceOfAnyTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfBitStringTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfBMPStringTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfBooleanTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfEnumeratedTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfGeneralizedTimeTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfIA5StringTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfIntegerTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfNullTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfObjectIDTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfOctetStringTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfPrintableStringTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfT61StringTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfUniversalStringTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfUTCTimeTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfUTF8StringTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfVisibleStringTemplate[];
extern const SEC_ASN1Template SEC_SequenceOfTeletexStringTemplate[];

extern const SEC_ASN1Template SEC_SetOfAnyTemplate[];
extern const SEC_ASN1Template SEC_SetOfBitStringTemplate[];
extern const SEC_ASN1Template SEC_SetOfBMPStringTemplate[];
extern const SEC_ASN1Template SEC_SetOfBooleanTemplate[];
extern const SEC_ASN1Template SEC_SetOfEnumeratedTemplate[];
extern const SEC_ASN1Template SEC_SetOfGeneralizedTimeTemplate[];
extern const SEC_ASN1Template SEC_SetOfIA5StringTemplate[];
extern const SEC_ASN1Template SEC_SetOfIntegerTemplate[];
extern const SEC_ASN1Template SEC_SetOfNullTemplate[];
extern const SEC_ASN1Template SEC_SetOfObjectIDTemplate[];
extern const SEC_ASN1Template SEC_SetOfOctetStringTemplate[];
extern const SEC_ASN1Template SEC_SetOfPrintableStringTemplate[];
extern const SEC_ASN1Template SEC_SetOfT61StringTemplate[];
extern const SEC_ASN1Template SEC_SetOfUniversalStringTemplate[];
extern const SEC_ASN1Template SEC_SetOfUTCTimeTemplate[];
extern const SEC_ASN1Template SEC_SetOfUTF8StringTemplate[];
extern const SEC_ASN1Template SEC_SetOfVisibleStringTemplate[];
extern const SEC_ASN1Template SEC_SetOfTeletexStringTemplate[];

/*
 * Template for skipping a subitem; this only makes sense when decoding.
 */
extern const SEC_ASN1Template SEC_SkipTemplate[];

/* These functions simply return the address of the above-declared templates.
** This is necessary for Windows DLLs.  Sigh.
*/
SEC_ASN1_CHOOSER_DECLARE(SEC_AnyTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_BMPStringTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_BooleanTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_BitStringTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_GeneralizedTimeTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_IA5StringTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_IntegerTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_NullTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_ObjectIDTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_OctetStringTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_UTCTimeTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_UTF8StringTemplate)

SEC_ASN1_CHOOSER_DECLARE(SEC_PointerToAnyTemplate)
SEC_ASN1_CHOOSER_DECLARE(SEC_PointerToOctetStringTemplate)

SEC_ASN1_CHOOSER_DECLARE(SEC_SetOfAnyTemplate)

SEC_END_PROTOS

#endif	/* _ASN1_TEMPLATES_H_ */
