/*
 * Copyright (c) 2001-2010 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * September 3, 2010	Dieter Siegmund (dieter@apple.com)
 * - moved here from EAPOLSocket.c
 */

/*
 * EAPOLUtil.c
 * - EAPOL utility functions
 */

#include "EAPUtil.h"
#include "EAPOLUtil.h"
#include "printdata.h"

static bool
EAPOLPacketTypeValid(EAPOLPacketType type)
{
    if (type >= kEAPOLPacketTypeEAPPacket
	&& type <= kEAPOLPacketTypeEncapsulatedASFAlert) {
	return (true);
    }
    return (false);
}

static const char *
EAPOLPacketTypeStr(EAPOLPacketType type)
{
    static const char * str[] = { 
	"EAP Packet",
	"Start",
	"Logoff",
	"Key",
	"Encapsulated ASF Alert" 
    };

    if (EAPOLPacketTypeValid(type)) {
	return (str[type]);
    }
    return ("<unknown>");
}

static void
fprint_eapol_rc4_key_descriptor(FILE * f, 
				EAPOLRC4KeyDescriptorRef descr_p,
				unsigned int body_length)
{
    int				key_data_length;
    u_int16_t			key_length;
    const char *		which;
    
    if (descr_p->key_index & kEAPOLKeyDescriptorIndexUnicastFlag) {
	which = "Unicast";
    }
    else {
	which = "Broadcast";
    }
    key_length = EAPOLKeyDescriptorGetLength(descr_p);
    key_data_length = body_length - sizeof(*descr_p);
    fprintf(f, "EAPOL Key Descriptor: type RC4 (%d) length %d %s index %d\n",
	   descr_p->descriptor_type, 
	   key_length, 
	   which,
	   descr_p->key_index & kEAPOLKeyDescriptorIndexMask);
    fprintf(f, "%-16s", "replay_counter:");
    fprint_bytes(f, descr_p->replay_counter, sizeof(descr_p->replay_counter));
    fprintf(f, "\n");
    fprintf(f, "%-16s", "key_IV:");
    fprint_bytes(f, descr_p->key_IV, sizeof(descr_p->key_IV));
    fprintf(f, "\n");
    fprintf(f, "%-16s", "key_signature:");
    fprint_bytes(f, descr_p->key_signature, sizeof(descr_p->key_signature));
    fprintf(f, "\n");
    if (key_data_length > 0) {
	fprintf(f, "%-16s", "key:");
	fprint_bytes(f, descr_p->key, key_data_length);
	fprintf(f, "\n");
    }
    return;
}

static void
fprint_eapol_ieee80211_key_descriptor(FILE * f,
				      EAPOLIEEE80211KeyDescriptorRef descr_p,
				      unsigned int body_length)
{
    uint16_t		key_data_length;
    uint16_t		key_information;
    uint16_t		key_length;
    
    key_length = EAPOLIEEE80211KeyDescriptorGetLength(descr_p);
    key_information =  EAPOLIEEE80211KeyDescriptorGetInformation(descr_p);
    key_data_length =  EAPOLIEEE80211KeyDescriptorGetKeyDataLength(descr_p);
    fprintf(f, "EAPOL Key Descriptor: type IEEE 802.11 (%d)\n",
	   descr_p->descriptor_type);
    fprintf(f, "%-18s0x%04x\n", "key_information:", key_information);
    fprintf(f, "%-18s%d\n", "key_length:", key_length);
    fprintf(f, "%-18s", "replay_counter:");
    fprint_bytes(f, descr_p->replay_counter, sizeof(descr_p->replay_counter));
    fprintf(f, "\n");
    fprintf(f, "%-18s", "key_nonce:");
    fprint_bytes(f, descr_p->key_nonce, sizeof(descr_p->key_nonce));
    fprintf(f, "\n");
    fprintf(f, "%-18s", "EAPOL_key_IV:");
    fprint_bytes(f, descr_p->EAPOL_key_IV, sizeof(descr_p->EAPOL_key_IV));
    fprintf(f, "\n");
    fprintf(f, "%-18s", "key_RSC:");
    fprint_bytes(f, descr_p->key_RSC, sizeof(descr_p->key_RSC));
    fprintf(f, "\n");
    fprintf(f, "%-18s", "key_reserved:");
    fprint_bytes(f, descr_p->key_reserved, sizeof(descr_p->key_reserved));
    fprintf(f, "\n");
    fprintf(f, "%-18s", "key_MIC:");
    fprint_bytes(f, descr_p->key_MIC, sizeof(descr_p->key_MIC));
    fprintf(f, "\n");
    fprintf(f, "%-18s%d\n", "key_data_length:", key_data_length);
    if (key_data_length > 0) {
	fprintf(f, "%-18s", "key_data:");
	fprint_bytes(f, descr_p->key_data, key_data_length);
	fprintf(f, "\n");
    }
    return;
}

static bool
eapol_key_descriptor_valid(void * body, unsigned int body_length, 
			   FILE * f)
{
    EAPOLIEEE80211KeyDescriptorRef	ieee80211_descr_p = body;
    EAPOLRC4KeyDescriptorRef		rc4_descr_p = body;

    if (body_length < 1) {
	if (f != NULL) {
	    fprintf(f, "eapol_key_descriptor_valid: body_length is %d < 1\n",
		    body_length);
	}
	return (false);
    }
    switch (rc4_descr_p->descriptor_type) {
    case kEAPOLKeyDescriptorTypeRC4:
	if (body_length < sizeof(*rc4_descr_p)) {
	    if (f != NULL) {
		fprintf(f, "eapol_key_descriptor_valid: body_length %d"
			" < sizeof(*rc4_descr_p) %ld\n",
			body_length, sizeof(*rc4_descr_p));
	    }
	    return (false);
	}
	if (f != NULL) {
	    fprint_eapol_rc4_key_descriptor(f, rc4_descr_p, body_length);
	}
	break;
    case kEAPOLKeyDescriptorTypeIEEE80211:
	if (body_length < sizeof(*ieee80211_descr_p)) {
	    if (f != NULL) {
		fprintf(f, "eapol_key_descriptor_valid: body_length %d"
			" < sizeof(*ieee80211_descr_p) %ld\n",
			body_length, sizeof(*ieee80211_descr_p));
	    }
	    return (false);
	}
	if (EAPOLIEEE80211KeyDescriptorGetKeyDataLength(ieee80211_descr_p)
	    > (body_length - sizeof(*ieee80211_descr_p))) {
	    if (f != NULL) {
		fprintf(f, "eapol_key_descriptor_valid: key_data_length %d"
			" > body_length - sizeof(*ieee80211_descr_p) %ld\n",
			EAPOLIEEE80211KeyDescriptorGetKeyDataLength(ieee80211_descr_p),
			body_length - sizeof(*ieee80211_descr_p));
	    }
	    return (false);
	}
	if (f != NULL) {
	    fprint_eapol_ieee80211_key_descriptor(f, ieee80211_descr_p,
						  body_length);
	}
	break;
    default:
	if (f != NULL) {
	    fprintf(f, "eapol_key_descriptor_valid: descriptor_type unknown %d",
		    rc4_descr_p->descriptor_type);
	}
	return (false);
    }
    return (true);
}

static bool
eapol_body_valid(EAPOLPacketRef eapol_p, unsigned int length, FILE * f)
{
    unsigned int 	body_length;
    bool 		ret = true;

    body_length = EAPOLPacketGetLength(eapol_p);
    length -= sizeof(*eapol_p);
    if (length < body_length) {
	if (f != NULL) {
	    fprintf(f, "packet length %d < body_length %d\n",
		    length, body_length);
	}
	return (false);
    }
    switch (eapol_p->packet_type) {
    case kEAPOLPacketTypeEAPPacket:
	ret = EAPPacketValid((EAPPacketRef)eapol_p->body, body_length, f);
	break;
    case kEAPOLPacketTypeKey:
	ret = eapol_key_descriptor_valid(eapol_p->body, body_length, f);
	break;
    case kEAPOLPacketTypeStart:
    case kEAPOLPacketTypeLogoff:
    case kEAPOLPacketTypeEncapsulatedASFAlert:
	break;
    default:
	if (f != NULL) {
	    fprintf(f, "unrecognized EAPOL packet type %d\n",
		   eapol_p->packet_type);
	    fprint_data(f, ((void *)eapol_p) + sizeof(*eapol_p), body_length);
	}
	break;
    }

    if (f != NULL) {
	if (body_length < length) {
	    fprintf(f, "EAPOL: %d bytes follow body:\n", length - body_length);
	    fprint_data(f, ((void *)eapol_p) + sizeof(*eapol_p) + body_length, 
			length - body_length);
	}
    }
    return (ret);
}

static bool
eapol_header_valid(EAPOLPacketRef eapol_p, unsigned int length,
		   FILE * f)
{
    if (length < sizeof(*eapol_p)) {
	if (f != NULL) {
	    fprintf(f, "Data length %d < sizeof(*eapol_p) %ld\n",
		    length, sizeof(*eapol_p));
	}
	return (false);
    }
    if (f != NULL) {
	fprintf(f, "EAPOL: proto version 0x%x type %s (%d) length %d\n",
		eapol_p->protocol_version, 
		EAPOLPacketTypeStr(eapol_p->packet_type),
		eapol_p->packet_type, EAPOLPacketGetLength(eapol_p));
    }
    return (true);
}

bool
EAPOLPacketValid(EAPOLPacketRef eapol_p, unsigned int length, FILE * f)
{
    if (eapol_header_valid(eapol_p, length, f) == false) {
	return (false);
    }
    return (eapol_body_valid(eapol_p, length, f));
}

