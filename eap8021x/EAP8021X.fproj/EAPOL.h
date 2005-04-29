
/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPOL_H
#define _EAP8021X_EAPOL_H

/* 
 * Modification History
 *
 * November 1, 2001	Dieter Siegmund (dieter@apple.com)
 * - created
 */

/*
 * EAPOL.h
 * - 802.1X EAPOL protocol definitions
 */
#include <stdint.h>

#define EAPOL_802_1_X_ETHERTYPE		0x888e
#define EAPOL_802_1_X_PROTOCOL_VERSION	1
#define EAPOL_802_1_X_GROUP_ADDRESS	{ 0x01, 0x80, 0xc2, 0x00, 0x00, 0x03 }

typedef struct {
    uint8_t	protocol_version;
    uint8_t	packet_type;
    uint8_t	body_length[2];
    uint8_t	body[0];
} EAPOLPacket, * EAPOLPacketRef;

static __inline__ void
EAPOLPacketSetLength(EAPOLPacketRef pkt, uint16_t length)
{
    *((unsigned short *)pkt->body_length) = htons(length);
    return;
}

static __inline__ uint16_t
EAPOLPacketGetLength(const EAPOLPacketRef pkt)
{
    return (ntohs(*((unsigned short *)pkt->body_length)));
}

enum {
    kEAPOLPacketTypeEAPPacket = 0,
    kEAPOLPacketTypeStart = 1,
    kEAPOLPacketTypeLogoff = 2,
    kEAPOLPacketTypeKey = 3,
    kEAPOLPacketTypeEncapsulatedASFAlert = 4
};
typedef uint32_t EAPOLPacketType;

typedef struct {
    uint8_t		descriptor_type;
    uint8_t		key_length[2];
    uint8_t		replay_counter[8];
    uint8_t		key_IV[16];
    uint8_t		key_index;
    uint8_t		key_signature[16];
    uint8_t		key[0];
} EAPOLKeyDescriptor, * EAPOLKeyDescriptorRef;

static __inline__ void
EAPOLKeyDescriptorSetLength(EAPOLKeyDescriptorRef pkt, uint16_t length)
{
    *((unsigned short *)pkt->key_length) = htons(length);
    return;
}

static __inline__ uint16_t
EAPOLKeyDescriptorGetLength(const EAPOLKeyDescriptorRef pkt)
{
    return (ntohs(*((unsigned short *)pkt->key_length)));
}

enum {
    kEAPOLKeyDescriptorTypeRC4 = 1
};
typedef uint32_t EAPOLKeyDescriptorType;

enum {
    kEAPOLKeyDescriptorIndexUnicastFlag = 0x80,
    kEAPOLKeyDescriptorIndexMask = 0x7f
};
typedef uint32_t EAPOLKeyDescriptorIndex;

#endif _EAP8021X_EAPOL_H

