/*
 * Copyright (c) 2012-2016 Apple Inc. All Rights Reserved.
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


#import "IDSProxy.h"

@interface KeychainSyncingOverIDSProxy (ReceiveMessage)
//receive message routines
-(BOOL) checkForFragmentation:(NSDictionary*)message id:(NSString*)fromID data:(NSData*)messageData;
-(NSMutableDictionary*) combineMessage:(NSString*)deviceID peerID:(NSString*)peerID uuid:(NSString*)uuid;
- (void)service:(IDSService *)service account:(IDSAccount *)account incomingMessage:(NSDictionary *)message fromID:(NSString *)fromID context:(IDSMessageContext *)context;
- (void)sendMessageToSecurity:(NSMutableDictionary*)messageAndFromID fromID:(NSString*)fromID;
- (void) handleAllPendingMessage;

@end
