
/*
 * Copyright (c) 2002 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_DIAMETERAVP_H
#define _EAP8021X_DIAMETERAVP_H

/*
 * DiameterAVP.h
 * - definitions for Diameter AVP's
 */

/* 
 * Modification History
 *
 * October 11, 2002	Dieter Siegmund (dieter@apple)
 * - created
 */
typedef struct {
    u_int32_t	AVP_code;
    u_int32_t	AVP_flags_length;
    u_char	AVP_data[0];
} DiameterAVP;

typedef struct {
    u_int32_t	AVPV_code;
    u_int32_t	AVPV_flags_length;
    u_int32_t	AVPV_vendor;
    u_char	AVPV_data[0];
} DiameterVendorAVP;

typedef enum {
    kDiameterFlagsVendorSpecific = 0x80,
    kDiameterFlagsMandatory = 0x40,
} DiameterFlags;

#define DIAMETER_LENGTH_MASK	0xffffff

static __inline__ u_int32_t
DiameterAVPMakeFlagsLength(u_int8_t flags, u_int32_t length)
{
    u_int32_t flags_length;

    flags_length = (length & DIAMETER_LENGTH_MASK) | (flags << 24);
    return (flags_length);
}

static __inline__ u_int32_t
DiameterAVPLengthFromFlagsLength(u_int32_t flags_length)
{
    return (flags_length & DIAMETER_LENGTH_MASK);
}

static __inline__ u_int8_t
DiameterAVPFlagsFromFlagsLength(u_int32_t flags_length)
{
    return (flags_length >> 24);
}

#endif /* _EAP8021X_DIAMETERAVP_H */
