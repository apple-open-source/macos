/*
 * Copyright (c) 2001-2013 Apple Inc. All rights reserved.
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
#include "nbo.h"
#include "myCFUtil.h"
#include <SystemConfiguration/SCPrivate.h>

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
RC4KeyDescriptorAppendDescription(EAPOLRC4KeyDescriptorRef descr_p,
				  unsigned int body_length,
				  CFMutableStringRef str)
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
    STRING_APPEND(str,
		  "EAPOL Key Descriptor: type RC4 (%d) length %d %s index %d\n",
		  descr_p->descriptor_type, 
		  key_length, 
		  which,
		  descr_p->key_index & kEAPOLKeyDescriptorIndexMask);
    STRING_APPEND(str, "%-16s", "replay_counter:");
    print_bytes_cfstr(str, descr_p->replay_counter,
		      sizeof(descr_p->replay_counter));
    STRING_APPEND(str, "\n");
    STRING_APPEND(str, "%-16s", "key_IV:");
    print_bytes_cfstr(str, descr_p->key_IV, sizeof(descr_p->key_IV));
    STRING_APPEND(str, "\n");
    STRING_APPEND(str, "%-16s", "key_signature:");
    print_bytes_cfstr(str, descr_p->key_signature,
		      sizeof(descr_p->key_signature));
    STRING_APPEND(str, "\n");
    if (key_data_length > 0) {
	STRING_APPEND(str, "%-16s", "key:");
	print_bytes_cfstr(str, descr_p->key, key_data_length);
	STRING_APPEND(str, "\n");
    }
    return;
}

static void
IEEE80211KeyDescriptorAppendDescription(EAPOLIEEE80211KeyDescriptorRef descr_p,
					unsigned int body_length,
					CFMutableStringRef str)
{
    uint16_t		key_data_length;
    uint16_t		key_information;
    uint16_t		key_length;

    key_length = EAPOLIEEE80211KeyDescriptorGetLength(descr_p);
    key_information =  EAPOLIEEE80211KeyDescriptorGetInformation(descr_p);
    key_data_length =  EAPOLIEEE80211KeyDescriptorGetKeyDataLength(descr_p);
    STRING_APPEND(str, "EAPOL Key Descriptor: type IEEE 802.11 (%d)\n",
		  descr_p->descriptor_type);
    STRING_APPEND(str, "%-18s0x%04x\n", "key_information:", key_information);
    STRING_APPEND(str, "%-18s%d\n", "key_length:", key_length);
    STRING_APPEND(str, "%-18s", "replay_counter:");
    print_bytes_cfstr(str, descr_p->replay_counter,
		      sizeof(descr_p->replay_counter));
    STRING_APPEND(str, "\n");
    STRING_APPEND(str, "%-18s", "key_nonce:");
    print_bytes_cfstr(str, descr_p->key_nonce, sizeof(descr_p->key_nonce));
    STRING_APPEND(str, "\n");
    STRING_APPEND(str, "%-18s", "EAPOL_key_IV:");
    print_bytes_cfstr(str, descr_p->EAPOL_key_IV,
		      sizeof(descr_p->EAPOL_key_IV));
    STRING_APPEND(str, "\n");
    STRING_APPEND(str, "%-18s", "key_RSC:");
    print_bytes_cfstr(str, descr_p->key_RSC, sizeof(descr_p->key_RSC));
    STRING_APPEND(str, "\n");
    STRING_APPEND(str, "%-18s", "key_reserved:");
    print_bytes_cfstr(str, descr_p->key_reserved,
		      sizeof(descr_p->key_reserved));
    STRING_APPEND(str, "\n");
    STRING_APPEND(str, "%-18s", "key_MIC:");
    print_bytes_cfstr(str, descr_p->key_MIC, sizeof(descr_p->key_MIC));
    STRING_APPEND(str, "\n");
    STRING_APPEND(str, "%-18s%d\n", "key_data_length:", key_data_length);
    if (key_data_length > 0) {
	STRING_APPEND(str, "%-18s", "key_data:");
	print_bytes_cfstr(str, descr_p->key_data, key_data_length);
	STRING_APPEND(str, "\n");
    }
    return;
}

static bool
eapol_key_descriptor_valid(void * body, unsigned int body_length,
			   CFMutableStringRef str)
{
    EAPOLIEEE80211KeyDescriptorRef	ieee80211_descr_p = body;
    int					key_data_length;
    EAPOLRC4KeyDescriptorRef		rc4_descr_p = body;

    if (body_length < 1) {
	if (str != NULL) {
	    STRING_APPEND(str, "EAPOLPacket empty body\n");
	}
	return (false);
    }
#define KEY_DESCRIPTOR_LABEL	"EAPOLKeyDescriptor"
    switch (rc4_descr_p->descriptor_type) {
    case kEAPOLKeyDescriptorTypeRC4:
	if (body_length < sizeof(*rc4_descr_p)) {
	    if (str != NULL) {
		STRING_APPEND(str, "%s(RC4) length %d < %d\n",
			      KEY_DESCRIPTOR_LABEL,
			      body_length, (int)sizeof(*rc4_descr_p));
	    }
	    return (false);
	}
	if (str != NULL) {
	    RC4KeyDescriptorAppendDescription(rc4_descr_p, body_length, str);
	}
	break;
    case kEAPOLKeyDescriptorTypeIEEE80211:
    case kEAPOLKeyDescriptorTypeWPA:
	if (body_length < sizeof(*ieee80211_descr_p)) {
	    if (str != NULL) {
		STRING_APPEND(str, "%s(IEEE80211) length %d < %d\n",
			      KEY_DESCRIPTOR_LABEL,
			      body_length, (int)sizeof(*ieee80211_descr_p));
	    }
	    return (false);
	}
	key_data_length 
	    = EAPOLIEEE80211KeyDescriptorGetKeyDataLength(ieee80211_descr_p);
	if ((body_length - sizeof(*ieee80211_descr_p)) < key_data_length) {
	    if (str != NULL) {
		STRING_APPEND(str,
			      "%s(IEEE80211) Key Data truncated %d < %d\n",
			      KEY_DESCRIPTOR_LABEL,
			      body_length - (int)sizeof(*ieee80211_descr_p),
			      key_data_length);
	    }
	    return (false);
	}
	if (str != NULL) {
	    IEEE80211KeyDescriptorAppendDescription(ieee80211_descr_p,
						    body_length, str);
	}
	break;
    default:
	if (str != NULL) {
	    STRING_APPEND(str, "%s Type %d unrecognized\n",
			  KEY_DESCRIPTOR_LABEL,
			  rc4_descr_p->descriptor_type);
	}
	return (false);
    }
    return (true);
}

static bool
eapol_body_valid(EAPOLPacketRef eapol_p, unsigned int length, 
		 CFMutableStringRef str)
{
    unsigned int 	body_length;
    bool 		ret = true;

    body_length = EAPOLPacketGetLength(eapol_p);
    length -= sizeof(*eapol_p);
    if (length < body_length) {
	if (str != NULL) {
	    STRING_APPEND(str,
			  "EAPOLPacket truncated %d < %d\n",
			  length, body_length);
	}
	return (false);
    }
    switch (eapol_p->packet_type) {
    case kEAPOLPacketTypeEAPPacket:
	ret = EAPPacketIsValid((EAPPacketRef)eapol_p->body, body_length, str);
	break;
    case kEAPOLPacketTypeKey:
	ret = eapol_key_descriptor_valid(eapol_p->body, body_length, str);
	break;
    case kEAPOLPacketTypeStart:
    case kEAPOLPacketTypeLogoff:
    case kEAPOLPacketTypeEncapsulatedASFAlert:
	break;
    default:
	if (str != NULL) {
	    STRING_APPEND(str,
			  "EAPOLPacket type %d unrecognized\n",
			  eapol_p->packet_type);
	    print_data_cfstr(str, 
			     ((void *)eapol_p) + sizeof(*eapol_p), body_length);
	}
	break;
    }
    if (str != NULL && body_length < length) {
	STRING_APPEND(str, "EAPOL: %d bytes follow body:\n", 
		      length - body_length);
	print_data_cfstr(str,
			 ((void *)eapol_p) + sizeof(*eapol_p) + body_length, 
			length - body_length);
    }
    return (ret);
}

static bool
eapol_header_valid(EAPOLPacketRef eapol_p, unsigned int length,
		   CFMutableStringRef str)
{
    if (length < sizeof(*eapol_p)) {
	if (str != NULL) {
	    STRING_APPEND(str, "EAPOLPacket truncated header %d < %d\n",
			  length, (int)sizeof(*eapol_p));
	}
	return (false);
    }
    if (str != NULL) {
	STRING_APPEND(str, 
		      "EAPOL: proto version 0x%x type %s (%d) length %d\n",
		      eapol_p->protocol_version, 
		      EAPOLPacketTypeStr(eapol_p->packet_type),
		      eapol_p->packet_type, EAPOLPacketGetLength(eapol_p));
    }
    return (true);
}

bool
EAPOLPacketIsValid(EAPOLPacketRef eapol_p, unsigned int length,
		   CFMutableStringRef str)
{
    if (eapol_header_valid(eapol_p, length, str) == false) {
	return (false);
    }
    return (eapol_body_valid(eapol_p, length, str));
}

bool
EAPOLPacketValid(EAPOLPacketRef eapol_p, unsigned int length, FILE * f)
{
    bool		ret;
    CFMutableStringRef	str = NULL;

    if (f != NULL) {
	str = CFStringCreateMutable(NULL, 0);
    }
    ret = EAPOLPacketIsValid(eapol_p, length, str);
    if (str != NULL) {
	SCPrint(TRUE, f, CFSTR("%@"), str);
	CFRelease(str);
    }
    return (ret);
}

void
EAPOLPacketSetLength(EAPOLPacketRef pkt, uint16_t length)
{
    net_uint16_set(pkt->body_length, length);
    return;
}

uint16_t
EAPOLPacketGetLength(const EAPOLPacketRef pkt)
{
    return (net_uint16_get(pkt->body_length));
}

void
EAPOLRC4KeyDescriptorSetLength(EAPOLRC4KeyDescriptorRef pkt, uint16_t length)
{
    net_uint16_set(pkt->key_length, length);
    return;
}

void
EAPOLKeyDescriptorSetLength(EAPOLKeyDescriptorRef pkt, uint16_t length)
{
    EAPOLRC4KeyDescriptorSetLength(pkt, length);
    return;
}

uint16_t
EAPOLRC4KeyDescriptorGetLength(const EAPOLRC4KeyDescriptorRef pkt)
{
    return (net_uint16_get(pkt->key_length));
}

uint16_t
EAPOLKeyDescriptorGetLength(const EAPOLKeyDescriptorRef pkt)
{
    return (EAPOLRC4KeyDescriptorGetLength(pkt));
}

uint16_t
EAPOLIEEE80211KeyDescriptorGetLength(const EAPOLIEEE80211KeyDescriptorRef pkt)
{
    return (net_uint16_get(pkt->key_length));
}

uint16_t
EAPOLIEEE80211KeyDescriptorGetInformation(const EAPOLIEEE80211KeyDescriptorRef pkt)
{
    return (net_uint16_get(pkt->key_information));
}

uint16_t
EAPOLIEEE80211KeyDescriptorGetKeyDataLength(const EAPOLIEEE80211KeyDescriptorRef pkt)
{
    return (net_uint16_get(pkt->key_data_length));
}
