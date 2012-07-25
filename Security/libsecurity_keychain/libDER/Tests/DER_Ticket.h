/*
 *  DER_Ticket.h
 *  libDER
 *
 *  Created by Michael Brouwer on 10/13/09.
 *  Copyright 2009 Apple Inc. All rights reserved.
 *
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
