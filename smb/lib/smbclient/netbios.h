/*
 * Copyright (c) 2009-2010 Apple Inc. All rights reserved.
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

#ifndef NETBIOS_H_8C21E785_0577_44E0_8CA1_8577A1010DF0
#define NETBIOS_H_8C21E785_0577_44E0_8CA1_8577A1010DF0

#include <stdint.h>
#include <sys/socket.h> /* sockaddr_storage */
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(SMBCLIENT_EXPORT)
#if defined(__GNUC__)
#define SMBCLIENT_EXPORT __attribute__((visibility("default")))
#else
#define SMBCLIENT_EXPORT
#endif
#endif /* SMBCLIENT_EXPORT */

/*
 * All NetBIOS names are 16 characters in length. A NetBIOS suffix is the 16th
 * character of the 16-character NetBIOS name. The NetBIOS suffix is used by
 * Microsoft Networking software to identify functionality installed on the
 * registered device.
 * The following is a partial lists of NetBIOS suffixes that are used by Microsoft
 * Windows NT. The suffixes are listed in hexadecimal format because many of
 * them are unprintable otherwise.
 * For a complete list please see http://support.microsoft.com/kb/163409
 */
#define kNetBIOSWorkstationService          0x00
#define kNetBIOSMSBrowseService             0x01
#define kNetBIOSMessengerService            0x03
#define kNetBIOSRASServerService            0x06
#define kNetBIOSDomainMasterBrowser         0x1B
#define kNetBIOSDomainControllers           0x1C
#define kNetBIOSMasterBrowser               0x1D
#define kNetBIOSBrowserServiceElections     0x1E
#define kNetBIOSNetDDEService               0x1F
#define kNetBIOSFileServerService           0x20
#define kNetBIOSRASClientService            0x21
#define kNetBIOSMSExchangeInterchange       0x22
#define kNetBIOSMicrosoftExchangeStore      0x23
#define kNetBIOSMicrosoftExchangeDirectory  0x24
#define kNetBIOSModemSharingServerService   0x30
#define kNetBIOSModemSharingClientService   0x31
#define kNetBIOSSMSClientsRemoteControl     0x43
#define kNetBIOSSMSAdminRemoteControlTool   0x44
#define kNetBIOSSMSClientsRemoteChat        0x45
#define kNetBIOSDECPathworks                0x4C
#define kNetBIOSMicrosoftExchangeIMC        0x6A
#define kNetBIOSMicrosoftExchangeMTA        0x87
#define kNetBIOSNetworkMonitorAgent         0xBE
#define kNetBIOSNetworkMonitorApplication   0xBF

/*!
 * @function SMBResolveNetBIOSNameEx
 * @abstract Resolve the NetBIOS name to a set of sockaddr structures.
 * @param hostname The NetBIOS name that needs to be resolved
 * @param node_type suffix NetBIOS Name suffix
 * @param timeout number of seconds to wait, 0 means use default value.
 * @param winserver Optional wins server dns name to use when resolving, if
 * null then use the system defined WINS server.
 * @param respAddr Sockaddr of the server that responded to the request.
 * @param count Number of sockaddr returned.
 * @result Returns an array of sockaddrs that must be freed with free(3).
 */
SMBCLIENT_EXPORT
struct sockaddr_storage *
SMBResolveNetBIOSNameEx(
    const char *hostName,
	uint8_t nodeType,
    const char *winServer,
	uint32_t timeout,
    struct sockaddr_storage *respAddr,
    int32_t *outCount)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;
/*!
 * @function SMBResolveNetBIOSName
 * @abstract Resolve the NetBIOS name to a set of sockaddr structures.
 * @param hostname The NetBIOS name that needs to be resolved
 * @param node_type suffix NetBIOS Name suffix
 * @param timeout number of seconds to wait, 0 means use default value.
 * @param results an array of sockaddrs that must be freed with free(3).
 * @result  Number of sockaddr returned or -1 if any error besides timed out.
 */	
ssize_t
SMBResolveNetBIOSName(
	const char * hostname,
	uint8_t node_type,
	uint32_t timeout,
	struct sockaddr_storage ** results)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;
	
#define NetBIOS_NAME_LEN    16

/*
* NB_FLAGS field of the RESOURCE RECORD RDATA field for RR_TYPE of "NB":
*
*   1   1   1   1   1   1
*   0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5
*   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
*   | G |  ONT  |                RESERVED                           |
*   +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
*
*   Symbol     Bit(s)   Description:
*
*   RESERVED     3-15   Reserved for future use.  Must be zero (0).
*       ONT           1,2   Owner Node Type:
*       00 = B node
*       01 = P node
*       10 = M node
*       11 = Reserved for future use
*       For registration requests this is the claimant's type. For responses this
*       is the actual owner's type.
*       G               0   Group Name Flag.
*       If one (1) then the RR_NAME is a GROUP NetBIOS name.
*       If zero (0) then the RR_NAME is a UNIQUE NetBIOS name.
*/
#define NBNS_UNIQUE_NAME    0x0000
#define NBNS_GROUP_NAME     0x8000

typedef struct NBResourceRecord {
    char        rrName[NetBIOS_NAME_LEN];
    uint16_t    nbFlags;
} NBResourceRecord;

typedef struct NodeStatusInfo {
    struct sockaddr_storage node_storage;
    char                    node_servername[16];
    char                    node_workgroupname[16];
    int32_t                 node_errno;
    uint32_t                node_nbrrArrayCnt;
    struct NBResourceRecord *node_nbrrArray;
} SMBNodeStatusInfo;

/*!
 * @function SMBGetNodeStatus
 * @abstract Find all names associated using supplied host name.
 * @param hostname The host name of the server
 * @param count Number of node status info returned.
 * @result Returns an array of node status info structures that must be free.
 */
SMBCLIENT_EXPORT
struct NodeStatusInfo *
SMBGetNodeStatus(
    const char  *hostName,
    uint32_t    *outCount)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

/*!
 * @function SMBCreateNetBIOSName
 * @abstract Transform the proposed NetBIOS name to be a valid NetBIOS name.
 * @param proposedName The proposed NetBIOS name.
 * @result Returns a new CFStringRef or NULL if the name cannot be transformed.
 */
SMBCLIENT_EXPORT
CFStringRef
SMBCreateNetBIOSName(
        CFStringRef proposedName)
__OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA)
;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* NETBIOS_H_8C21E785_0577_44E0_8CA1_8577A1010DF0 */
/* vim: set sw=4 ts=4 tw=79 et: */
