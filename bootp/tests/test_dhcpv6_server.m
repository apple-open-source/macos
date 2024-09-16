/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#import "test_dhcpv6_icmpv6.h"

#define CMSG_BUF_LEN CMSG_SPACE(sizeof(struct in6_pktinfo))

@implementation DHCPv6Server

static int
openDHCPv6Socket(void)
{
	uint16_t serverPort = DHCPV6_SERVER_PORT;
	struct sockaddr_in6 sockaddr = { 0 };
	int sockfd = -1;
	int options = 1;
	int status = 0;

	sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
	REQUIRE_TRUE(sockfd >= 0, done, "%s: Failed to open server socket", __func__);
	sockaddr.sin6_family = AF_INET6;
	sockaddr.sin6_port = htons(serverPort);
	status = bind(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
	REQUIRE_TRUE(status >= 0, done, "%s: Failed to bind socket to local server interface", __func__);
	status = ioctl(sockfd, FIONBIO, &options);
	REQUIRE_TRUE(status >= 0, done, "%s: Failed to set nonblocking io", __func__);

done:
	if (sockfd < 0) {
		close(sockfd);
	}
	return sockfd;
}

static const struct sockaddr_in6 dhcpv6AllServersAndRelayAgents = {
	sizeof(dhcpv6AllServersAndRelayAgents),
	AF_INET6, 0, 0,
	All_DHCP_Relay_Agents_and_Servers_INIT, 0
};

static int
setDHCPv6MulticastGroupOnInterface(int socket, unsigned int ifIndex, bool join)
{
	struct group_req multicastGroupInfo = { 0 };
	int joinOrLeave = ((join) ? MCAST_JOIN_GROUP : MCAST_LEAVE_GROUP);
	int ret = -1;

	multicastGroupInfo.gr_interface = ifIndex;
	memcpy(&multicastGroupInfo.gr_group, &dhcpv6AllServersAndRelayAgents, sizeof(dhcpv6AllServersAndRelayAgents));
	ret = setsockopt(socket, IPPROTO_IPV6, joinOrLeave, &multicastGroupInfo, sizeof(multicastGroupInfo));
	if (ret != 0) {
		NSLog(@"setsockopt(%s) on ifindex %d failed with error %d (%s)",
		      ((join) ? "MCAST_JOIN_GROUP" : "MCAST_LEAVE_GROUP"), ifIndex, errno, strerror(errno));
	}

	return (ret);
}

static __inline__ NSData *
makeServerDUID(void)
{
	const uint8_t duidBytes[FETH0_LINKADDR_LEN] = FETH0_LINKADDR_BYTES;
	return (__bridge_transfer NSData *)DHCPDUID_LLTDataCreate(duidBytes, sizeof(duidBytes), IFT_OTHER);
}

- (instancetype)initWithFailureMode:(DHCPServerFailureMode)failureMode
		       andInterface:(NSString *)ifname
{
	if (self = [super init]) {
		self.duid = makeServerDUID();

		// vars for simulating DHCP server failures
		if (failureMode != kDHCPServerFailureModeNone) {
			self.failureMode = failureMode;
			self.timeOfRequest = nil;
			self.timeBetweenSubsequentRequests1 = -1;
			self.timeBetweenSubsequentRequests2 = -1;
			self.exponentialBackoffSem = dispatch_semaphore_create(0);
		}

		// opens server socket
		self.socket = openDHCPv6Socket();
		if (self.socket < 0) {
			NSLogDebug(@"FAILED TO MAKE SERVER SOCKET");
			return nil;
		}

		// makes server work queue
		self.queue = dispatch_queue_create(DHCPV6_SERVER_QUEUE_LABEL, NULL);
		if (self.queue == nil) {
			NSLogDebug(@"FAILED TO CREATE SERVER QUEUE");
			return nil;
		}

		// makes server listener
		self.socketListener = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, self.socket, 0, self.queue);
		if (self.socketListener == nil) {
			NSLogDebug(@"FAILED TO CREATE SOCKET LISTENER");
			return nil;
		}
		dispatch_source_set_event_handler(self.socketListener, ^{ [self serverReceive]; });

		// configures the multicast group for DHCPv6 on feth0
		self.interfaceIndex = if_nametoindex([ifname UTF8String]);
		if (self.interfaceIndex <= 0) {
			NSLogDebug(@"FAILED TO GET IF INDEX");
			return nil;
		}
		setDHCPv6MulticastGroupOnInterface(self.socket, self.interfaceIndex, true);

		// starts socket listener
		dispatch_activate(self.socketListener);
		NSLogDebug(@"DHCP SERVER SET UP SUCCESSFULLY");
	}
	return self;
}

- (void)disconnect
{
	NSLogDebug(@"%s", __func__);
	self.queue = NULL;
	self.exponentialBackoffSem = NULL;
	if (self.socketListener != NULL) {
		dispatch_source_cancel(self.socketListener);
		self.socketListener = NULL;
	}
	if (self.socket != -1) {
		setDHCPv6MulticastGroupOnInterface(self.socket, self.interfaceIndex, false);
		close(self.socket);
		self.socket = -1;
	}
}

- (void)dealloc
{
	NSLogDebug(@"%s", __func__);
	[self disconnect];
}

static int
extractInterfaceIndex(struct msghdr * messageHeader)
{
	struct in6_pktinfo *packetInfo = NULL;
	struct cmsghdr *controlMessageHeader = NULL;
	int ret = -1;

	for (controlMessageHeader = (struct cmsghdr *)CMSG_FIRSTHDR(messageHeader);
	     controlMessageHeader != NULL;
	     controlMessageHeader = (struct cmsghdr *)CMSG_NXTHDR(messageHeader, controlMessageHeader)) {
		if (controlMessageHeader->cmsg_level != IPPROTO_IPV6) {
			continue;
		}
		switch (controlMessageHeader->cmsg_type) {
			case IPV6_PKTINFO:
				if (controlMessageHeader->cmsg_len < CMSG_LEN(sizeof(struct in6_pktinfo))) {
					continue;
				}
				packetInfo = (struct in6_pktinfo *)(void *)(CMSG_DATA(controlMessageHeader));
				break;
			default:
				NSLogDebug(@"%s: unexpected cmsg_type %d", __func__, controlMessageHeader->cmsg_type);
				break;
		}
	}
	if (packetInfo == NULL) {
		NSLogDebug(@"%s: recvd no IPv6 packet info", __func__);
		goto done;
	}
	ret = packetInfo->ipi6_ifindex;

done:
	return ret;
}

- (void)serverReceive
{
	struct sockaddr_in6 sourceAddr = { 0 };
	struct iovec iov = { 0 };
	ssize_t messageLen = 0;
	struct msghdr messageHeader = { 0 };
	char controlMessageBuf[CMSG_BUF_LEN] = { 0 };
	int ifIndex = -1;
	char ifNameBuf[IFNAMSIZ] = { 0 };
	char packetBuf[FETH_IF_MTU] = { 0 };
	char ntopbuf[INET6_ADDRSTRLEN] = { 0 };

	iov.iov_base = (void *)packetBuf;
	iov.iov_len = sizeof(packetBuf);
	messageHeader.msg_name = (void *)&sourceAddr;
	messageHeader.msg_namelen = sizeof(sourceAddr);
	messageHeader.msg_iov = (void *)&iov;
	messageHeader.msg_iovlen = 1;
	messageHeader.msg_control = (void *)controlMessageBuf;
	messageHeader.msg_controllen = sizeof(controlMessageBuf);

	messageLen = recvmsg(self.socket, &messageHeader, 0);
	if (messageLen < 0) {
		NSLogDebug(@"%s: recvmsg failed", __func__);
		goto done;
	} else if (messageLen < DHCPV6_PACKET_HEADER_LENGTH) {
		NSLogDebug(@"%s: recvmsg too short", __func__);
		goto done;
	}
	NSLogDebug(@"%s: received message from %s", __func__,
		   inet_ntop(AF_INET6, &sourceAddr.sin6_addr, ntopbuf, sizeof(ntopbuf)));
	ifIndex = extractInterfaceIndex(&messageHeader);
	if (ifIndex == -1) {
		/* this gets the ifindex statically if the cmsg way fails */
		ifIndex = self.interfaceIndex;
	}
	if_indextoname(ifIndex, ifNameBuf);
	NSLogDebug(@"%s: got outbound ifindex %d (%s)", __func__, ifIndex, ifNameBuf);
	[self serverReplyToMessage:(DHCPv6PacketRef)packetBuf
			withLength:(int)messageLen
	    outboundInterfaceIndex:ifIndex
		  andSourceAddress:&sourceAddr];

done:
	return;
}

- (void)serverTransmitPacket:(DHCPv6PacketRef)replyPacket
		  withLength:(int)packetLen
      outboundInterfaceIndex:(int)ifIndex
       andDestinationAddress:(struct sockaddr_in6 *)destAddr
{
	char controlMessageBuf[CMSG_BUF_LEN] = { 0 };
	struct cmsghdr *controlMessageHeader = NULL;
	struct iovec iov = { 0 };
	struct msghdr messageHeader = { 0 };
	ssize_t sentSize = 0;
	struct in6_pktinfo *packetInfo = NULL;
	struct sockaddr_in6 clientSockaddr = { 0 };
	char ntopBuf[INET6_ADDRSTRLEN] = { 0 };
	CFMutableStringRef packetStr = NULL;
	DHCPv6OptionListRef optionsList = NULL;
	DHCPv6OptionErrorString errStr = { 0 };

	iov.iov_base = (void *)replyPacket;
	iov.iov_len = packetLen;
	messageHeader.msg_name = destAddr;
	messageHeader.msg_namelen = sizeof(*destAddr);
	messageHeader.msg_iov = &iov;
	messageHeader.msg_iovlen = 1;
	messageHeader.msg_control = (void *)controlMessageBuf;
	messageHeader.msg_controllen = CMSG_BUF_LEN;
	controlMessageHeader = CMSG_FIRSTHDR(&messageHeader);
	if (controlMessageHeader == NULL) {
		goto done;
	}
	controlMessageHeader->cmsg_level = IPPROTO_IPV6;
	controlMessageHeader->cmsg_type = IPV6_PKTINFO;
	controlMessageHeader->cmsg_len = CMSG_BUF_LEN;
	packetInfo = (struct in6_pktinfo *)(void *)CMSG_DATA(controlMessageHeader);
	packetInfo->ipi6_ifindex = ifIndex;

	clientSockaddr.sin6_family = AF_INET6;
	clientSockaddr.sin6_addr = destAddr->sin6_addr;
	clientSockaddr.sin6_port = htons(DHCPV6_CLIENT_PORT);
	sentSize = sendmsg(self.socket, &messageHeader, 0);
	if (sentSize == -1) {
		NSLog(@"%s: sendto failed with error %d (%s)", __func__, errno, strerror(errno));
	} else if (sentSize != packetLen) {
		NSLog(@"%s: failed to send full reply packet to client", __func__);
	} else {
		packetStr = CFStringCreateMutable(kCFAllocatorDefault, 0);
		DHCPv6PacketPrintToString(packetStr, replyPacket, packetLen);
		optionsList = DHCPv6OptionListCreateWithPacket(replyPacket, packetLen, &errStr);
		DHCPv6OptionListPrintToString(packetStr, optionsList);
		NSLogDebug(@"%s: Sent %zu byte %s message to '%s':\n%@",
			   __func__,
			   sentSize,
			   DHCPv6MessageTypeName(replyPacket->msg_type),
			   inet_ntop(AF_INET6, &clientSockaddr.sin6_addr, ntopBuf, sizeof(ntopBuf)),
			   packetStr);
	}

done:
	DHCPv6OptionListRelease(&optionsList);
	RELEASE_NULLSAFE(packetStr);
}

#define PREFERRED_LIFETIME 1800
#define VALID_LIFETIME 0xFFFFFFFF

/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |         OPTION_IA_PD          |           option-len          |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                         IAID (4 octets)                       |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                              T1                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                              T2                               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 .                                                               .
 .                          IA_PD-options                        .
 .                                                               .
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/*
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |        OPTION_IAPREFIX        |         option-length         |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                      preferred-lifetime                       |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                        valid-lifetime                         |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 | prefix-length |                                               |
 +-+-+-+-+-+-+-+-+          IPv6 prefix                          |
 |                           (16 octets)                         |
 |                                                               |
 |                                                               |
 |                                                               |
 |               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |               |                                               .
 +-+-+-+-+-+-+-+-+                                               .
 .                       IAprefix-options                        .
 .                                                               .
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

- (void)handleExponentialBackoff
{
	if ([self timeOfRequest] == nil) {
		[self setTimeOfRequest:[NSDate date]];
	} else if ([self timeBetweenSubsequentRequests1] == -1) {
		[self setTimeBetweenSubsequentRequests1:fabs([[self timeOfRequest] timeIntervalSinceNow])];
		[self setTimeOfRequest:[NSDate date]];
#define TIME_BETWEEN_FIRST_REQUESTS_LEEWAY 1.5
	} else if ([self timeBetweenSubsequentRequests1] < TIME_BETWEEN_FIRST_REQUESTS_LEEWAY) {
		// case to fend off first 3 UDP transmits that get sent by the client in SOLICIT
		[self setTimeBetweenSubsequentRequests1:fabs([[self timeOfRequest] timeIntervalSinceNow])];
		[self setTimeOfRequest:[NSDate date]];
	} else if ([self timeBetweenSubsequentRequests2] == -1) {
		[self setTimeBetweenSubsequentRequests2:fabs([[self timeOfRequest] timeIntervalSinceNow])];
		[self setTimeOfRequest:[NSDate date]];
#define TIME_BETWEEN_REQUESTS_LEEWAY 1.0
#define EXPONENTIAL_BASE 2
	} else if ([self timeBetweenSubsequentRequests2] > (EXPONENTIAL_BASE * [self timeBetweenSubsequentRequests1] - TIME_BETWEEN_REQUESTS_LEEWAY)
		   && [self timeBetweenSubsequentRequests2] < (EXPONENTIAL_BASE * [self timeBetweenSubsequentRequests1] + TIME_BETWEEN_REQUESTS_LEEWAY)) {
		// given 3 subsequent request times t1 t2 t3
		// this sounds the signal once when (t3-t2) is within +- 0.5 of twice (t2-t1)
		if ([self exponentialBackoffSem] != 0) {
			(void)dispatch_semaphore_signal([self exponentialBackoffSem]);
		}
	} else {
		NSLogDebug(@"%s: other case");
	}
}


/*
 * This method makes reply packets according to RFC 8415.
 */
- (void)serverReplyToMessage:(DHCPv6PacketRef)receivedPacket
		  withLength:(int)packetLen
      outboundInterfaceIndex:(unsigned int)ifIndex
	    andSourceAddress:(struct sockaddr_in6 *)sourceAddr
{
	CFMutableStringRef packetStr = NULL;
	DHCPv6MessageType receivedMessageType = -1;
	DHCPv6OptionListRef receivedOptionList = NULL;
	int optionLen = -1;
	DHCPv6OptionErrorString errStr = {0};
	DHCPv6OptionArea optionsArea;
	char replyBuf[FETH_IF_MTU] = {0};
	DHCPv6PacketRef replyPacket = NULL;
	int replyPacketLen = -1;
	DHCPDUIDRef clientID = NULL;
	
	// informs about the new message
	receivedMessageType = receivedPacket->msg_type;
	receivedOptionList = DHCPv6OptionListCreateWithPacket(receivedPacket, packetLen, &errStr);
	REQUIRE_NONNULL(receivedOptionList, done, "%s: Failed to get options list from rcvd packet", __func__);
	packetStr = CFStringCreateMutable(kCFAllocatorDefault, 0);
	DHCPv6PacketPrintToString(packetStr, receivedPacket, (int)packetLen);
	DHCPv6OptionListPrintToString(packetStr, receivedOptionList);
	NSLogDebug(@"%s: Received %zu byte %s message:\n%@",
		   __func__,
		   packetLen,
		   DHCPv6MessageTypeName(receivedMessageType),
		   packetStr);
	
	// validates server id
	
	replyPacket = (DHCPv6PacketRef)replyBuf;
	// replies with same transaction id
	memcpy(replyPacket->transaction_id, receivedPacket->transaction_id, sizeof(replyPacket->transaction_id));
	DHCPv6OptionAreaInit(&optionsArea, replyPacket->options, sizeof(replyBuf) - DHCPV6_PACKET_HEADER_LENGTH);
	// replies with same client id
	clientID = (DHCPDUIDRef)DHCPv6OptionListGetOptionDataAndLength(receivedOptionList, kDHCPv6OPTION_CLIENTID, &optionLen, NULL);
	REQUIRE_TRUE(DHCPDUIDIsValid(clientID, optionLen), done, "%s: Failed to validate clientID", __func__);
	REQUIRE_TRUE(DHCPv6OptionAreaAddOption(&optionsArea, kDHCPv6OPTION_CLIENTID, optionLen, clientID, &errStr),
		     done, "%s: Failed to add option clientID", __func__);
	// puts server duid into reply
	REQUIRE_TRUE(DHCPv6OptionAreaAddOption(&optionsArea,
					       kDHCPv6OPTION_SERVERID,
					       CFDataGetLength((__bridge CFDataRef)[self duid]),
					       CFDataGetBytePtr((__bridge CFDataRef)[self duid]),
					       &errStr),
		     done, "%s: Failed to add option serverID", __func__);
	
	// replies according to message type received
	switch (receivedMessageType) {
		case kDHCPv6MessageSOLICIT: {
			if ([self failureMode] == kDHCPServerFailureModeNotOnLink) {
				DHCPv6OptionIA_PDRef receivedPacketOption = NULL;
				DHCPv6OptionIA_PD iaOption = {0};
				DHCPv6OptionSTATUS_CODE statusCodeOption = {0};
				
				NSLogDebug(@"%s: Got SOLICIT message, sending NotOnLink ADVERTISE", __func__);
				DHCPv6PacketSetMessageType(replyPacket, kDHCPv6MessageADVERTISE);
				receivedPacketOption = (DHCPv6OptionIA_PDRef)DHCPv6OptionListGetOptionDataAndLength(receivedOptionList,
														    kDHCPv6OPTION_IA_PD,
														    &optionLen,
														    NULL);
				// keeps everything in IA_PD the same except it removes IAPREFIX option and adds STATUS_CODE NotOnLink
				DHCPv6OptionIA_PDSetIAID(&iaOption, DHCPv6OptionIA_PDGetIAID(receivedPacketOption));
				DHCPv6OptionIA_PDSetT1(&iaOption, DHCPv6OptionIA_PDGetT1(receivedPacketOption));
				DHCPv6OptionIA_PDSetT2(&iaOption, DHCPv6OptionIA_PDGetT2(receivedPacketOption));
				optionLen = DHCPv6OptionIA_PD_MIN_LENGTH;
				REQUIRE_TRUE(DHCPv6OptionAreaAddOption(&optionsArea, kDHCPv6OPTION_IA_PD, optionLen, &iaOption, &errStr),
					     done, "%s: Failed to add option IA_PD", __func__);
				net_uint16_set((uint8_t *)&statusCodeOption.code, kDHCPv6StatusCodeNotOnLink);
				optionLen = DHCPv6OptionSTATUS_CODE_MIN_LENGTH;
				REQUIRE_TRUE(DHCPv6OptionAreaAddOption(&optionsArea, kDHCPv6OPTION_STATUS_CODE, optionLen, &statusCodeOption, &errStr),
					     done, "%s: Failed to add option STATUS_CODE", __func__);
				
				// expects the DHCP client to decrease frequency of requests (exponentially)
				[self handleExponentialBackoff];
			} else {
				DHCPv6OptionRef iaOption = NULL;
				DHCPv6OptionIA_PDRef optionData = NULL;
				DHCPv6OptionIAPREFIXRef optionOptionData = NULL;
				uint32_t prefLifetime = PREFERRED_LIFETIME;
				uint32_t validLifetime = VALID_LIFETIME;
				
				// Initial client message in DHCPV6-PD service tests is SOLICIT.
				// Server must advertise that it can assign the requested prefix.
				NSLogDebug(@"%s: Got SOLICIT message, sending ADVERTISE", __func__);
				DHCPv6PacketSetMessageType(replyPacket, kDHCPv6MessageADVERTISE);
				iaOption = (DHCPv6OptionRef)DHCPv6OptionListGetOptionDataAndLength(receivedOptionList, kDHCPv6OPTION_IA_PD, &optionLen, NULL);
				// updates preferred and valid lifetimes
				optionData = (DHCPv6OptionIA_PDRef)DHCPv6OptionGetData(iaOption);
				optionOptionData = (DHCPv6OptionIAPREFIXRef)optionData->options;
				DHCPv6OptionIAPREFIXSetPreferredLifetime(optionOptionData, prefLifetime);
				DHCPv6OptionIAPREFIXSetValidLifetime(optionOptionData, validLifetime);
				REQUIRE_TRUE(DHCPv6OptionAreaAddOption(&optionsArea, kDHCPv6OPTION_IA_PD, optionLen, iaOption, &errStr),
					     done, "%s: Failed to add option IA_PD", __func__);
			}
			
			break;
		}
		case kDHCPv6MessageREQUEST: {
			if ([self failureMode] == kDHCPServerFailureModeNoPrefixAvail) {
				DHCPv6OptionIA_PDRef receivedPacketOption = NULL;
				DHCPv6OptionIA_PD iaOption = {0};
				DHCPv6OptionSTATUS_CODE statusCodeOption = {0};
				
				NSLogDebug(@"%s: Got REQUEST message, sending NoPrefixAvail RESPONSE", __func__);
				DHCPv6PacketSetMessageType(replyPacket, kDHCPv6MessageREPLY);
				receivedPacketOption = (DHCPv6OptionIA_PDRef)DHCPv6OptionListGetOptionDataAndLength(receivedOptionList,
														    kDHCPv6OPTION_IA_PD,
														    &optionLen,
														    NULL);
				// keeps everything in IA_PD the same except it removes IAPREFIX option and adds STATUS_CODE NoPrefixAvail
				DHCPv6OptionIA_PDSetIAID(&iaOption, DHCPv6OptionIA_PDGetIAID(receivedPacketOption));
				DHCPv6OptionIA_PDSetT1(&iaOption, DHCPv6OptionIA_PDGetT1(receivedPacketOption));
				DHCPv6OptionIA_PDSetT2(&iaOption, DHCPv6OptionIA_PDGetT2(receivedPacketOption));
				optionLen = DHCPv6OptionIA_PD_MIN_LENGTH;
				REQUIRE_TRUE(DHCPv6OptionAreaAddOption(&optionsArea, kDHCPv6OPTION_IA_PD, optionLen, &iaOption, &errStr),
					     done, "%s: Failed to add option IA_PD", __func__);
				net_uint16_set((uint8_t *)&statusCodeOption.code, kDHCPv6StatusCodeNoPrefixAvail);
				optionLen = DHCPv6OptionSTATUS_CODE_MIN_LENGTH;
				REQUIRE_TRUE(DHCPv6OptionAreaAddOption(&optionsArea, kDHCPv6OPTION_STATUS_CODE, optionLen, &statusCodeOption, &errStr),
					     done, "%s: Failed to add option STATUS_CODE", __func__);
				
				// expects the DHCP client to decrease frequency of requests (exponentially)
				[self handleExponentialBackoff];
			} else {
				DHCPv6OptionRef iaOption = NULL;
				DHCPv6OptionIA_PDRef optionData = NULL;
				DHCPv6OptionIAPREFIXRef optionOptionData = NULL;
				uint32_t prefLifetime = PREFERRED_LIFETIME;
				uint32_t validLifetime = VALID_LIFETIME;
				
				NSLogDebug(@"%s: Got REQUEST message, sending REPLY", __func__);
				DHCPv6PacketSetMessageType(replyPacket, kDHCPv6MessageREPLY);
				iaOption = (DHCPv6OptionRef)DHCPv6OptionListGetOptionDataAndLength(receivedOptionList, kDHCPv6OPTION_IA_PD, &optionLen, NULL);
				optionData = (DHCPv6OptionIA_PDRef)DHCPv6OptionGetData(iaOption);
				optionOptionData = (DHCPv6OptionIAPREFIXRef)optionData->options;
				DHCPv6OptionIAPREFIXSetPreferredLifetime(optionOptionData, prefLifetime);
				DHCPv6OptionIAPREFIXSetValidLifetime(optionOptionData, validLifetime);
				REQUIRE_TRUE(DHCPv6OptionAreaAddOption(&optionsArea, kDHCPv6OPTION_IA_PD, optionLen, iaOption, &errStr),
					     done, "%s: Failed to add option IA_PD", __func__);
			}
			
			break;
		}
		case kDHCPv6MessageRELEASE: {
			if ([self failureMode] == kDHCPServerFailureModeNone) {
				DHCPv6OptionSTATUS_CODE statusCodeOption = {0};
				
				NSLogDebug(@"%s: Got RELEASE message, sending REPLY", __func__);
				DHCPv6PacketSetMessageType(replyPacket, kDHCPv6MessageREPLY);
				net_uint16_set((uint8_t *)&statusCodeOption.code, kDHCPv6StatusCodeSuccess);
				REQUIRE_TRUE(DHCPv6OptionAreaAddOption(&optionsArea, kDHCPv6OPTION_STATUS_CODE, sizeof(statusCodeOption), &statusCodeOption, &errStr),
					     done, "%s: Failed to add option STATUS_CODE", __func__);
			}
			
			break;
		}
		default: {
			NSLog(@"%s: GOT OTHER MESSAGE", __func__);
			break;
		}
	}
	
	// sends reply message back to client
	replyPacketLen = DHCPV6_PACKET_HEADER_LENGTH + DHCPv6OptionAreaGetUsedLength(&optionsArea);
	[self serverTransmitPacket:replyPacket 
			withLength:replyPacketLen
	    outboundInterfaceIndex:ifIndex
	     andDestinationAddress:sourceAddr];

done:
	DHCPv6OptionListRelease(&receivedOptionList);
	RELEASE_NULLSAFE(packetStr);
}

@end
