/*
 * Copyright (c) 2009,2012,2014 Apple Inc. All Rights Reserved.
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


#include <libDER/libDER.h>


#define FAST_SET_LOOKUP     1

#ifdef FAST_SET_LOOKUP
/* state representing a fast by tag set accessor, the caller needs to provide
   a set large enough to hold all */
typedef struct {
	DERTag	capacity;   /* should be large enough to hold all encountered tags.
                           otherwise DR_UnexpectedTag will be returned, note
                           that only one tag per tag number can exist. */
	DERByte	*end;
	DERByte	*byTag[];   /* maxTag element array of pointers to tag + length
                           of items in set indexed by tagNumber. */
} DERSet;

/* Iterates over all the tags in the set to build an index returned in
   derSet. */
DERReturn DERDecodeSetContentInit(
	const DERItem   *der,			/* data to decode */
	DERSet          *derSet);		/* IN/OUT, to use in DERDecodeSetTag */

/* Returns DR_UnexpectedTag if the requested tag is not in derSet, returns
   the content of the decoded item in content otherwise. */
DERReturn DERDecodeSetTag(
	DERSet          *derSeq,		/* data to decode */
	DERTag			tag,			/* tag in sequence/set we are looking for. */
	DERItem         *content);		/* RETURNED */
#endif /* FAST_SET_LOOKUP */


DERReturn DERSetDecodeItemWithTag(
	const DERItem	*der,			/* data to decode */
	DERTag			tag,			/* tag in sequence/set we are looking for. */
	DERItem         *content);		/* RETURNED */


/* Application Processor Ticket */
typedef struct {
	DERItem		signatureAlgorithm;     /* AlgorithmId */
	DERItem		body;                   /* SET OF OCTECT STRING, DER_DEC_SAVE_DER */
	DERItem		signature;              /* OCTET STRING */
	DERItem		certificates;            /* SEQUENCE of CERTIFICATE */
} DERApTicket;

/* DERItemSpecs to decode into a DERApTicket */
extern const DERItemSpec DERApTicketItemSpecs[];
extern const DERSize DERNumApTicketItemSpecs;

DERReturn DERDecodeApTicket(
	const DERItem	*contents,
	DERApTicket		*ticket,            /* RETURNED */
	DERSize			*numUsedBytes);     /* RETURNED */


/* Baseband Ticket */
typedef struct {
	DERItem		signatureAlgorithm;     /* AlgorithmId */
	DERItem		body;                   /* SET OF OCTECT STRING, DER_DEC_SAVE_DER */
	DERItem		signature;              /* OCTET STRING */
	DERItem		gpuk;                   /* OCTET STRING */
} DERBbTicket;

/* DERItemSpecs to decode into a DERBbTicket */
extern const DERItemSpec DERBbTicketItemSpecs[];
extern const DERSize DERNumBbTicketItemSpecs;

DERReturn DERDecodeBbTicket(
	const DERItem	*contents,
	DERBbTicket		*ticket,            /* RETURNED */
	DERSize			*numUsedBytes);     /* RETURNED */
