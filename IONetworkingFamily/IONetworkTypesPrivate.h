/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IONETWORKTYPESPRIVATE_H
#define _IONETWORKTYPESPRIVATE_H

#ifdef KERNEL

/*! @defined kIONetworkNotificationBPFTapStateChange
    @discussion Notification indicates the BPF tap on the network
    interface was enabled or disabled.
*/
#define kIONetworkNotificationBPFTapStateChange \
        iokit_family_msg(sub_iokit_networking, 0x800)

/*! @defined kIONetworkNotificationLoggingLevelChange
    @discussion Notification indicates the logging level for the network
    interface was changed. Call IONetworkInterface::getLoggingLevel() to
    get the current logging level.
*/
#define kIONetworkNotificationLoggingLevelChange \
        iokit_family_msg(sub_iokit_networking, 0x801)

/*! @defined kIONetworkNotificationLoggingParametersChange
    @discussion Notification indicates the interface has received a request
    to change the logging parameters. An IONetworkInterfaceLoggingParameters
    passed to the notification handler describes the new parameters.
*/
#define kIONetworkNotificationLoggingParametersChange \
        iokit_family_msg(sub_iokit_networking, 0x802)

/*! @defined kIONetworkNotificationInterfaceAddressChange
    @discussion Notification indicates an interface address change.
    The data payload points to an IONetworkInterfaceAddressChangeParameters.
*/
#define kIONetworkNotificationInterfaceAddressChange \
        iokit_family_msg(sub_iokit_networking, 0x803)

struct IONetworkInterfaceLoggingParameters
{
    int32_t     level;
    uint32_t    flags;
    int32_t     category;
    int32_t     subCategory;
    uint32_t    reserved[4];
};

// See IONetworkInterface::setInterfaceSubType()
// Must mirror the IFNET_SUBFAMILY_* enums.
enum {
    kIONetworkInterfaceSubTypeNone          = 0,
    kIONetworkInterfaceSubTypeUSB           = 1,
    kIONetworkInterfaceSubTypeBluetooth     = 2,
    kIONetworkInterfaceSubTypeWiFi          = 3,
    kIONetworkInterfaceSubTypeThunderbolt   = 4,
    kIONetworkInterfaceSubTypeReserved      = 5
};

struct IONetworkInterfaceAddressChangeParameters
{
    sa_family_t addressFamily;
    uint32_t    reserved[3];
};

#endif /* KERNEL */

/*! @defined kIONetworkTxStatusFailure
    @discussion General transmission error. 
*/
#define kIONetworkTxStatusFailure \
        iokit_family_msg(sub_iokit_networking, 0x200)

/*! @defined kIONetworkTxStatusRemotePeerAsleep
    @discussion Peer is asleep. 
*/
#define kIONetworkTxStatusRemotePeerAsleep \
        iokit_family_msg(sub_iokit_networking, 0x201)

/*! @defined kIONetworkTxStatusRemotePeerUnavailable
    @discussion Peer is missing. 
*/
#define kIONetworkTxStatusRemotePeerUnavailable \
        iokit_family_msg(sub_iokit_networking, 0x202)

/*! @defined kIONetworkTxStatusNoRemotePeer
    @discussion Never found a peer. 
*/
#define kIONetworkTxStatusNoRemotePeer \
        iokit_family_msg(sub_iokit_networking, 0x203)

/*! @defined kIONetworkTxStatusMACChannelUnavailable
    @discussion Channel is non-configurable or denied.
*/
#define kIONetworkTxStatusMACChannelUnavailable \
        iokit_family_msg(sub_iokit_networking, 0x204)

/*! @defined kIONetworkTxStatusCRCError
    @discussion CRC error on packet.
*/
#define kIONetworkTxStatusCRCError \
        iokit_family_msg(sub_iokit_networking, 0x205)

/*! @defined kIONetworkTxStatusLifetimeExpired
    @discussion Packet waited too long and has expired.
*/
#define kIONetworkTxStatusLifetimeExpired \
        iokit_family_msg(sub_iokit_networking, 0x206)

/*! @defined kIONetworkTxStatusQueueStall
    @discussion Packet was refused because queue stall was detected.
*/
#define kIONetworkTxStatusQueueStall \
        iokit_family_msg(sub_iokit_networking, 0x207)

/*! @defined kIONetworkTxStatusBusIOError
    @discussion Packet never reached chip.
*/
#define kIONetworkTxStatusBusIOError \
        iokit_family_msg(sub_iokit_networking, 0x208)

/*! @defined kIONetworkTxStatusChipBufferUnavailable
    @discussion Packet didn’t get space on chip, credit or flush issue.
*/
#define kIONetworkTxStatusChipBufferUnavailable \
        iokit_family_msg(sub_iokit_networking, 0x209)

/*! @defined kIONetworkTxStatusChipModeError
    @discussion Packet wasn’t sent due to mode or state issue.
*/
#define kIONetworkTxStatusChipModeError \
        iokit_family_msg(sub_iokit_networking, 0x20a)

/*! @defined kIONetworkTxStatusUnderflow
    @discussion Insufficient information was sent.
*/
#define kIONetworkTxStatusUnderflow \
        iokit_family_msg(sub_iokit_networking, 0x20b)

/*! @defined kIONetworkTxStatusOverflow
    @discussion Too much information was sent.
*/
#define kIONetworkTxStatusOverflow \
        iokit_family_msg(sub_iokit_networking, 0x20c)

/*! @defined kIONetworkTxStatusNoResources
    @discussion Insufficient resources to transmit.
*/
#define kIONetworkTxStatusNoResources \
        iokit_family_msg(sub_iokit_networking, 0x20d)

#endif /* !_IONETWORKTYPESPRIVATE_H */
