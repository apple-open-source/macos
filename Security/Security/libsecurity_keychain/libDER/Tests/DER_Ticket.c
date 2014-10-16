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


#include "DER_Ticket.h"

#include <libDER/asn1Types.h>
#include <libDER/DER_Decode.h>
#include <libDER/DER_Encode.h>
#include <libDER/DER_Keys.h>

/* Application Processor Ticket */
const DERItemSpec DERApTicketItemSpecs[] =
{
	{ DER_OFFSET(DERApTicket, signatureAlgorithm),
			ASN1_CONSTR_SEQUENCE,
			DER_DEC_NO_OPTS | DER_ENC_WRITE_DER },
	{ DER_OFFSET(DERApTicket, body),
			ASN1_CONSTR_SET,
			DER_DEC_NO_OPTS | DER_DEC_SAVE_DER | DER_ENC_WRITE_DER },
	{ DER_OFFSET(DERApTicket, signature),
			ASN1_OCTET_STRING,
			DER_DEC_NO_OPTS },
	{ DER_OFFSET(DERApTicket, certificates),
			ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 1,
			DER_DEC_NO_OPTS | DER_ENC_WRITE_DER }
};
const DERSize DERNumApTicketItemSpecs =
	sizeof(DERApTicketItemSpecs) / sizeof(DERItemSpec);

/* Baseband Ticket */
const DERItemSpec DERBbTicketItemSpecs[] =
{
	{ DER_OFFSET(DERBbTicket, signatureAlgorithm),
			ASN1_CONSTR_SEQUENCE,
			DER_DEC_NO_OPTS | DER_ENC_WRITE_DER },
	{ DER_OFFSET(DERBbTicket, body),
			ASN1_CONSTR_SET,
			DER_DEC_NO_OPTS | DER_DEC_SAVE_DER | DER_ENC_WRITE_DER },
	{ DER_OFFSET(DERBbTicket, signature),
			ASN1_OCTET_STRING,
			DER_DEC_NO_OPTS },
	{ DER_OFFSET(DERBbTicket, gpuk),
			ASN1_CONTEXT_SPECIFIC | 2,
			DER_DEC_NO_OPTS }
};
const DERSize DERNumBbTicketItemSpecs =
	sizeof(DERBbTicketItemSpecs) / sizeof(DERItemSpec);

#if 0
/* We need to verify this value and use it here. */
const DERByte rsaWithSha1Algorithm[] = {
    0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x05
};
#endif

#ifdef FAST_SET_LOOKUP
/* Iterates over all the tags in the set to build an index returned in
   derSet. */
DERReturn DERDecodeSetContentInit(
	const DERItem   *content,			/* data to decode */
	DERSet          *derSet)            /* IN/OUT, to use in DERDecodeSetTag */
{
    DERReturn drtn;
    DERSequence derSeq;
    memset(derSet->byTag, 0, derSet->capacity);
    drtn = DERDecodeSeqContentInit(content, &derSeq);
    if (drtn == DR_Success) {
        DERDecodedInfo element;
        while ((drtn = DERDecodeSeqNext(&derSeq, &element)) == DR_Success) {
            if (element.tag >= derSet->capacity) return DR_UnexpectedTag;
            derSet->byTag[element.tag] = element.content.data;
        }
        if (drtn == DR_EndOfSequence) drtn = DR_Success;
    }
    derSet->end = content->data + content->length;

    return drtn;
}

DERReturn DERDecodeSetTag(
	DERSet          *derSet,		/* data to decode */
	DERTag			tag,			/* tag in sequence/set we are looking for. */
	DERItem         *content)		/* RETURNED */
{
    DERReturn drtn;
    DERTag tagNumber = tag & ASN1_TAGNUM_MASK;
    if (tagNumber > derSet->capacity)
        return DR_UnexpectedTag;
    DERByte *start = derSet->byTag[tagNumber];
    if (!start) return DR_UnexpectedTag;
    DERItem derItem = { .data = start, .length = derSet->end - start };
    DERDecodedInfo element;
    drtn = DERDecodeItem(&derItem, &element);
    if (drtn) return drtn;
    if (tag != element.tag) return DR_UnexpectedTag;
    *content = element.content;

    return drtn;
}
#endif /* FAST_SET_LOOKUP */

/* Returns the item with tag from the sequence or set pointed to by der.
   result DR_EndOfSequence if the tag was not found. */
DERReturn DERSetDecodeItemWithTag(
	const DERItem	*der,			/* data to decode */
	DERTag			tag,			/* tag in sequence/set we are looking for. */
	DERItem         *content)		/* RETURNED */
{
    DERReturn drtn;
    DERSequence derSeq;
    DERTag topTag;
    drtn = DERDecodeSeqInit(der, &topTag, &derSeq);
    if (drtn == DR_Success) {
        DERDecodedInfo info;
        while ((drtn = DERDecodeSeqNext(&derSeq, &info)) == DR_Success) {
            if (info.tag == tag) {
                *content = info.content;
                return DR_Success;
            }
        }
    }

    return drtn;
}

DERReturn DERDecodeApTicket(
	const DERItem	*contents,
	DERApTicket		*ticket,            /* RETURNED */
	DERSize			*numUsedBytes)      /* RETURNED */
{
    DERReturn drtn;
    DERDecodedInfo decodedTicket;
    drtn = DERDecodeItem(contents, &decodedTicket);
    if (drtn != DR_Success) goto badTicket;
    drtn = DERParseSequenceContent(&decodedTicket.content,
        DERNumApTicketItemSpecs, DERApTicketItemSpecs, ticket, 0);
    if (drtn != DR_Success) goto badTicket;

    /* Decode the algorithm sequence. */
    DERAlgorithmId algorithm = {};
    drtn = DERParseSequenceContent(&ticket->signatureAlgorithm,
        DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs, &algorithm, 0);
    if (drtn != DR_Success) goto badTicket;
    /* TODO Check algorithm oid and ensure there are no params.
       Alternatively replace the code above with a simple memcmp with
       an already ASN.1 encoded algorithm parms block. */

badTicket:
    *numUsedBytes = decodedTicket.content.length +
        decodedTicket.content.data - contents->data;

    return drtn;
}

DERReturn DERDecodeBbTicket(
	const DERItem	*contents,
	DERBbTicket		*ticket,            /* RETURNED */
	DERSize			*numUsedBytes)      /* RETURNED */
{
    DERReturn drtn;
    DERDecodedInfo decodedTicket;
    drtn = DERDecodeItem(contents, &decodedTicket);
    if (drtn != DR_Success) goto badTicket;
    drtn = DERParseSequenceContent(&decodedTicket.content,
        DERNumBbTicketItemSpecs, DERBbTicketItemSpecs, ticket, 0);
    if (drtn != DR_Success) goto badTicket;

    /* Decode the algorithm sequence. */
    DERAlgorithmId algorithm = {};
    drtn = DERParseSequenceContent(&ticket->signatureAlgorithm,
        DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs, &algorithm, 0);
    if (drtn != DR_Success) goto badTicket;
    /* TODO Check algorithm oid and ensure there are no params.
       Alternatively replace the code above with a simple memcmp with
       an already ASN.1 encoded algorithm parms block. */

badTicket:
    *numUsedBytes = decodedTicket.content.length +
        decodedTicket.content.data - contents->data;

    return drtn;
}
