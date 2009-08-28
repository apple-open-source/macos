
/*
 * Copyright (c) 2001-2009 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAP_H
#define _EAP8021X_EAP_H


/* 
 * Modification History
 *
 * November 1, 2001	Dieter Siegmund (dieter@apple.com)
 * - created
 */

/*
 * EAP.h
 * - EAP protocol definitions
 */

#include <stdint.h>
#include <sys/types.h>

enum {
    kEAPCodeRequest = 1,
    kEAPCodeResponse = 2,
    kEAPCodeSuccess = 3,
    kEAPCodeFailure = 4,
};
typedef uint32_t EAPCode;

enum {
    kEAPTypeInvalid = 0,		/* 0 is invalid */
    kEAPTypeIdentity = 1,
    kEAPTypeNotification = 2,
    kEAPTypeNak = 3,
    kEAPTypeMD5Challenge = 4,
    kEAPTypeOneTimePassword = 5,
    kEAPTypeGenericTokenCard = 6,
    kEAPTypeTLS = 13,
    kEAPTypeCiscoLEAP = 17,
    kEAPTypeEAPSIM = 18,
    kEAPTypeSRPSHA1 = 19,
    kEAPTypeTTLS = 21,
    kEAPTypeEAPAKA = 23,
    kEAPTypePEAP = 25,
    kEAPTypeMSCHAPv2 = 26,
    kEAPTypeExtensions = 33,
    kEAPTypeEAPFAST = 43,
};
typedef uint32_t EAPType;

typedef struct EAPPacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];		/* of entire request/response */
    uint8_t		data[0];
} EAPPacket, *EAPPacketRef;

typedef struct EAPSuccessFailurePacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];
} EAPSuccessPacket, *EAPSuccessPacketRef, 
    EAPFailurePacket, *EAPFailurePacketRef;

typedef struct EAPRequestResponsePacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;		/* EAPType values */
    uint8_t		type_data[0];
} EAPRequestPacket, *EAPRequestPacketRef, 
    EAPResponsePacket, *EAPResponsePacketRef;

typedef struct EAPNotificationPacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* sizeof(EAPNotificationPacket) */
    uint8_t		type;		/* kEAPTypeNotification */
} EAPNotificationPacket, *EAPNotificationPacketRef;

typedef struct EAPNakPacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire response */
    uint8_t		type;
    uint8_t	       	desired_type;
} EAPNakPacket, *EAPNakPacketRef;

typedef struct EAPMD5ChallengePacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		value_size;
    uint8_t		value[0];
    /*
      uint8_t		name[0];
    */
} EAPMD5ChallengePacket, *EAPMD5ChallengePacketRef;

typedef struct EAPMD5ResponsePacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		value_size;	/* will be 16 */
    uint8_t		value[16];
    uint8_t		name[0];
} EAPMD5ResponsePacket, *EAPMD5ResponsePacketRef;

static __inline__ void
EAPPacketSetLength(EAPPacketRef pkt, uint16_t length)
{
    *((u_short *)pkt->length) = htons(length);
    return;
}

static __inline__ uint16_t
EAPPacketGetLength(const EAPPacketRef pkt)
{
    return (ntohs(*((u_short *)pkt->length)));
}

#endif _EAP8021X_EAP_H

