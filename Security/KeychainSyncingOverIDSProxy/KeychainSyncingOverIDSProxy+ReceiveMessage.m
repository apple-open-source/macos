/*
 * Copyright (c) 2012-2017 Apple Inc. All Rights Reserved.
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


#import <Foundation/Foundation.h>
#import <Foundation/NSArray.h>
#import <Foundation/Foundation.h>

#import <Security/SecBasePriv.h>
#import <Security/SecItemPriv.h>
#import <utilities/debugging.h>
#import <notify.h>

#include <Security/CKBridge/SOSCloudKeychainConstants.h>
#include <Security/SecureObjectSync/SOSARCDefines.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <utilities/SecCFWrappers.h>

#import <IDS/IDS.h>
#import <os/activity.h>

#include <utilities/SecAKSWrappers.h>
#include <utilities/SecCFRelease.h>
#include <AssertMacros.h>

#import "IDSPersistentState.h"
#import "KeychainSyncingOverIDSProxy+ReceiveMessage.h"
#import "KeychainSyncingOverIDSProxy+SendMessage.h"
#import "IDSProxy.h"


static NSString *const kIDSNumberOfFragments = @"NumberOfIDSMessageFragments";
static NSString *const kIDSFragmentIndex = @"kFragmentIndex";
static NSString *const kIDSMessageRecipientID = @"RecipientPeerID";
static NSString *const kIDSMessageUseACKModel = @"UsesAckModel";
static NSString *const kIDSMessageSendersDeviceID = @"SendersDeviceID";

@implementation KeychainSyncingOverIDSProxy (ReceiveMessage)

-(int) countNumberOfValidObjects:(NSMutableArray*)fragmentsForDeviceID
{
    __block int count = 0;
    [fragmentsForDeviceID enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL * _Nonnull stop) {
        if(obj != [NSNull null]){
            count++;
        }
    }];
    return count;
}

-(BOOL) checkForFragmentation:(NSDictionary*)message id:(NSString*)fromID data:(NSData*)messageData
{
    BOOL handOffMessage = false;

    if([message valueForKey:kIDSNumberOfFragments] != nil){
        NSNumber *idsNumberOfFragments = [message objectForKey:kIDSNumberOfFragments];
        NSNumber *index = [message objectForKey:kIDSFragmentIndex];
        NSString *uuidString = [message objectForKey:(__bridge NSString*)kIDSMessageUniqueID];

        if([KeychainSyncingOverIDSProxy idsProxy].allFragmentedMessages == nil)
            [KeychainSyncingOverIDSProxy idsProxy].allFragmentedMessages = [NSMutableDictionary dictionary];

        NSMutableDictionary *uniqueMessages = [[KeychainSyncingOverIDSProxy idsProxy].allFragmentedMessages objectForKey: fromID];
        if(uniqueMessages == nil)
            uniqueMessages = [NSMutableDictionary dictionary];

        NSMutableArray *fragmentsForDeviceID = [uniqueMessages objectForKey: uuidString];
        if(fragmentsForDeviceID == nil){
            fragmentsForDeviceID = [ [NSMutableArray alloc] initWithCapacity: [idsNumberOfFragments longValue]];
            for (int i = 0; i <[idsNumberOfFragments longValue] ; i++) {
                [fragmentsForDeviceID addObject:[NSNull null]];
            }
        }

        [fragmentsForDeviceID replaceObjectAtIndex: [index intValue] withObject:messageData ];
        [uniqueMessages setObject: fragmentsForDeviceID forKey:uuidString];
        [[KeychainSyncingOverIDSProxy idsProxy].allFragmentedMessages setObject:uniqueMessages forKey: fromID];

        if([self countNumberOfValidObjects:fragmentsForDeviceID] == [idsNumberOfFragments longValue])
            handOffMessage = true;
        else
            handOffMessage = false;

    }
    else //no fragmentation in the message, ready to hand off to securityd
        handOffMessage = true;

    return handOffMessage;

}

-(NSMutableDictionary*) combineMessage:(NSString*)ID peerID:(NSString*)peerID uuid:(NSString*)uuid
{
    NSString *dataKey = [ NSString stringWithUTF8String: kMessageKeyIDSDataMessage ];
    NSString *deviceIDKey = [ NSString stringWithUTF8String: kMessageKeyDeviceID ];
    NSString *peerIDKey = [ NSString stringWithUTF8String: kMessageKeyPeerID ];

    NSMutableDictionary *arrayOfFragmentedMessagesByUUID = [[KeychainSyncingOverIDSProxy idsProxy].allFragmentedMessages objectForKey:ID];
    NSMutableArray *messagesForUUID = [arrayOfFragmentedMessagesByUUID objectForKey:uuid];
    NSMutableData* completeMessage = [NSMutableData data];
    
    [messagesForUUID enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        NSData *messageFragment = (NSData*)obj;
        
        [completeMessage appendData: messageFragment];
    }];
    //we've combined the message, now remove it from the fragmented messages dictionary
    [arrayOfFragmentedMessagesByUUID removeObjectForKey:uuid];

    return [NSMutableDictionary dictionaryWithObjectsAndKeys: completeMessage, dataKey, ID, deviceIDKey, peerID, peerIDKey, nil];
}

-(void) handleTestMessage:(NSString*)operation id:(NSString*)ID messageID:(NSString*)uniqueID senderPeerID:(NSString*)senderPeerID
{
    int operationType = [operation intValue];
    switch(operationType){
        case kIDSPeerAvailabilityDone:
        {
            //set current timestamp to indicate success!
            [self.peerNextSendCache setObject:[NSDate date] forKey:ID];
            
            secnotice("IDS Transport","!received availability response!: %@", ID);
            notify_post(kSOSCCPeerAvailable);
            break;
        }
        case kIDSEndPingTestMessage:
            secnotice("IDS Transport","received pong message from other device: %@, ping test PASSED", ID);
            break;
        case kIDSSendOneMessage:
            secnotice("IDS Transport","received ping test message, dropping on the floor now");
            break;
            
        case kIDSPeerAvailability:
        case kIDSStartPingTestMessage:
        {
            char* messageCharS;
            if(operationType == kIDSPeerAvailability){
                secnotice("IDS Transport","Received Availability Message from:%@!", ID);
                asprintf(&messageCharS, "%d",kIDSPeerAvailabilityDone);
            }
            else{
                secnotice("IDS Transport","Received PingTest Message from: %@!", ID);
                asprintf(&messageCharS, "%d", kIDSEndPingTestMessage);
            }
            
            NSString *operationString = [[NSString alloc] initWithUTF8String:messageCharS];
            NSString* messageString = @"peer availability check finished";
            NSDictionary* messsageDictionary = @{(__bridge NSString*)kIDSOperationType:operationString, (__bridge NSString*)kIDSMessageToSendKey:messageString};
            
            // We can always hold on to a message and our remote peers would bother everyone
            [self sendIDSMessage:messsageDictionary name:ID peer:@"me" senderDeviceID:NULL];
            
            free(messageCharS);
            
            break;
        }
        case kIDSPeerReceivedACK:
        {
            //set current timestamp to indicate success!
            [self.peerNextSendCache setObject:[[NSDate alloc] init] forKey:ID];
            
            //cancel timer!
            secnotice("IDS Transport", "received ack for: %@", uniqueID);
            dispatch_async(self.pingQueue, ^{
                //remove timer for message id
                dispatch_source_t timer = [[KeychainSyncingOverIDSProxy idsProxy].pingTimers objectForKey:uniqueID];
                if(timer != nil){
                    dispatch_cancel(timer);
                    [[KeychainSyncingOverIDSProxy idsProxy].pingTimers removeObjectForKey:uniqueID];
                    dispatch_sync(self.dataQueue, ^{
                        [[KeychainSyncingOverIDSProxy idsProxy].messagesInFlight removeObjectForKey:uniqueID];
                    });
                    [[KeychainSyncingOverIDSProxy idsProxy] persistState];
                }
            });
            //call out to securityd to set a NULL
            [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {

                CFErrorRef localError = NULL;
                SOSCCClearPeerMessageKeyInKVS((__bridge CFStringRef)senderPeerID, &localError);
                return NULL;
            }];
            break;
        }
        default:
            break;
    }
}

- (void)sendACK:(NSString*)ID peerID:(NSString*)sendersPeerID uniqueID:(NSString*)uniqueID senderDeviceID:(NSString*)senderDeviceID
{
    char* messageCharS;
    NSString* messageString = @"ACK";

    asprintf(&messageCharS, "%d",kIDSPeerReceivedACK);
    NSString *operationString = [[NSString alloc] initWithUTF8String:messageCharS];

    NSDictionary* messageDictionary = @{(__bridge NSString*)kIDSOperationType:operationString, (__bridge NSString*)kIDSMessageToSendKey:messageString, (__bridge NSString*)kIDSMessageUniqueID:uniqueID};

    [self sendIDSMessage:messageDictionary name:ID peer:sendersPeerID senderDeviceID:senderDeviceID];

    free(messageCharS);

}

- (void)updateDeviceList
{
    self.deviceIDFromAuthToken = nil;
    self.deviceIDFromAuthToken = [NSMutableDictionary dictionary];
    [self calloutWith:^(NSMutableDictionary *pending, bool handlePendingMesssages, bool doSetDeviceID, dispatch_queue_t queue, void(^done)(NSMutableDictionary *, bool, bool)) {
           }];
}
- (void)service:(IDSService *)service account:(IDSAccount *)account incomingMessage:(NSDictionary *)message fromID:(NSString *)fromID context:(IDSMessageContext *)context
{
    NSString *dataKey = [ NSString stringWithUTF8String: kMessageKeyIDSDataMessage ];
    NSString *deviceIDKey = [ NSString stringWithUTF8String: kMessageKeyDeviceID ];
    NSString *peerIDKey = [ NSString stringWithUTF8String: kMessageKeyPeerID ];
    NSString *sendersPeerIDKey = [NSString stringWithUTF8String: kMessageKeySendersPeerID];
    
    [KeychainSyncingOverIDSProxy idsProxy].incomingMessages++;

    dispatch_async(self.calloutQueue, ^{
        NSString* messageID = nil;
        uint32_t operationType;
        bool hadError = false;
        CFStringRef errorMessage = NULL;
        __block NSString* myPeerID = @"";
        __block NSData *messageData = nil;
        NSString* operationTypeAsString = nil;
        NSMutableDictionary *messageDictionary = nil;
        NSString *useAck = nil;
        NSString *senderDeviceID = nil;
        NSArray *devices = [self->_service devices];
        for(NSUInteger i = 0; i < [ devices count ]; i++){
            IDSDevice *device = devices[i];
            if( [(IDSCopyIDForDevice(device)) containsString: fromID] == YES){
                senderDeviceID = device.uniqueID;
                break;
            }
        }
        [[KeychainSyncingOverIDSProxy idsProxy] printMessage:message state:[NSString stringWithFormat:@"received message from: %@", senderDeviceID]];
        NSString *sendersPeerID = [message objectForKey: sendersPeerIDKey];
        
        if(sendersPeerID == nil)
            sendersPeerID = [NSString string];

        if(!senderDeviceID){
            senderDeviceID = message[kIDSMessageSendersDeviceID];
            secnotice("IDS Transport", "Their device ID!: %@", senderDeviceID);
        }
        require_action_quiet(senderDeviceID, fail, hadError = true; errorMessage = CFSTR("require the sender's device ID"));
        
        operationTypeAsString = [message objectForKey: (__bridge NSString*)kIDSOperationType];
        messageDictionary = [message objectForKey: (__bridge NSString*)kIDSMessageToSendKey];
        
        messageID = [message objectForKey:(__bridge NSString*)kIDSMessageUniqueID];
        useAck = [message objectForKey:kIDSMessageUseACKModel];
        
        if(useAck != nil && [useAck compare:@"YES"] == NSOrderedSame)
            require_quiet(messageID != nil, fail);
        
        secnotice("IDS Transport","from peer %@, operation: %@", senderDeviceID, operationTypeAsString);
        operationType = [operationTypeAsString intValue];
        
        if(operationType != kIDSKeychainSyncIDSFragmentation)
        {
            [self handleTestMessage:operationTypeAsString id:senderDeviceID messageID:messageID senderPeerID:sendersPeerID];
        }
        else{
            
            [messageDictionary enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
                myPeerID = (NSString*)key;
                messageData = (NSData*)obj;
            }];

            BOOL readyToHandOffToSecD = [self checkForFragmentation:message id:senderDeviceID data:messageData];
            
            NSMutableDictionary *messageAndFromID = nil;
            
            if(readyToHandOffToSecD && ([message objectForKey:kIDSFragmentIndex])!= nil){
                secnotice("IDS Transport", "combing message");
                NSString* uuid = [message objectForKey:(__bridge NSString*)kIDSMessageUniqueID];
                messageAndFromID = [self combineMessage:senderDeviceID peerID:myPeerID uuid:uuid];
                //update next sequence number
            }
            else if(readyToHandOffToSecD){
                messageAndFromID = [NSMutableDictionary dictionaryWithObjectsAndKeys: messageData, dataKey, senderDeviceID, deviceIDKey, myPeerID, peerIDKey, nil];
            }
            else
                return;
            
            //set the sender's peer id so we can check it in securityd
            [messageAndFromID setObject:sendersPeerID forKey:sendersPeerIDKey];
            
            if([KeychainSyncingOverIDSProxy idsProxy].isLocked){
                //hang on to the message and set the retry deadline
                dispatch_sync(self.dataQueue, ^{
                    [self.unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
                });
            }
            else{
                [self sendMessageToSecurity:messageAndFromID fromID:fromID shouldSendAck:useAck peerID:myPeerID messageID:messageID deviceID:senderDeviceID];

            }
        }
        
    fail:
        if(hadError)
            secerror("error:%@", errorMessage);
    });
}


- (void) handleAllPendingMessage
{
    secnotice("IDS Transport", "Attempting to handle pending messsages");
    
    NSMutableDictionary * __block copyOfUnhandled = nil;
    dispatch_sync(self.dataQueue, ^{
        if ([self.unhandledMessageBuffer count] > 0) {
            secnotice("IDS Transport", "handling messages: %@", self.unhandledMessageBuffer);
            copyOfUnhandled = [NSMutableDictionary dictionaryWithDictionary:self.unhandledMessageBuffer];
        }
    });
    
    [copyOfUnhandled enumerateKeysAndObjectsUsingBlock: ^(id key, id obj, BOOL *stop)
     {
         NSMutableDictionary *messageAndFromID = (NSMutableDictionary*)obj;
         NSString *fromID = (NSString*)key;
         //remove the message from the official message buffer (if it fails to get handled it'll be reset again in sendMessageToSecurity)
         dispatch_sync(self.dataQueue, ^{
             [self.unhandledMessageBuffer removeObjectForKey: fromID];
         });
         [self sendMessageToSecurity:messageAndFromID fromID:fromID shouldSendAck:nil peerID:nil messageID:nil deviceID:nil];
     }];
}

- (bool) shouldPersistMessage:(NSDictionary*) newMessageAndFromID id:(NSString*)fromID
{
    //get the dictionary of messages for a particular device id
    NSDictionary* __block messagesFromBuffer;
    dispatch_sync(self.dataQueue, ^{
        messagesFromBuffer = [self.unhandledMessageBuffer valueForKey:fromID];
    });

    if([messagesFromBuffer isEqual:newMessageAndFromID])
        return false;

    return true;
}

-(void)sendMessageToSecurity:(NSMutableDictionary*)messageAndFromID fromID:(NSString*)fromID shouldSendAck:(NSString *)useAck peerID:(NSString*)peerID messageID:(NSString*)messageID deviceID:(NSString*)senderDeviceID
{
    __block CFErrorRef cf_error = NULL;
    __block HandleIDSMessageReason success = kHandleIDSMessageSuccess;
    
    [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {
        
        success  = SOSCCHandleIDSMessage(((__bridge CFDictionaryRef)messageAndFromID), &cf_error);
        //turns out the error needs to be evaluated as sync_and_do returns bools
        if(cf_error != NULL)
        {
            if(CFErrorIsMalfunctioningKeybagError(cf_error)){
                success = kHandleIDSMessageLocked;
            }
        }
        
        if(success == kHandleIDSMessageLocked){
            secnotice("IDS Transport","cannot handle messages from: %@ when locked, error:%@", fromID, cf_error);
            // I don't think this is ever nil but it was like this when I got here
            dispatch_sync(self.dataQueue, ^{
                if(!self.unhandledMessageBuffer) {
                    self.unhandledMessageBuffer = [NSMutableDictionary dictionary];
                }
            });
            
            //write message to disk if message is new to the unhandled queue
            if([self shouldPersistMessage:messageAndFromID id:fromID]) {
                [self persistState];
            }
            
            dispatch_sync(self.dataQueue, ^{
                [self.unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
                secnotice("IDS Transport", "unhandledMessageBuffer: %@", self.unhandledMessageBuffer);
            });
            
            return NULL;
        }
        else if(success == kHandleIDSMessageNotReady){
            secnotice("IDS Transport","not ready to handle message from: %@, error:%@", fromID, cf_error);
            dispatch_sync(self.dataQueue, ^{
                if(!self.unhandledMessageBuffer) {
                    self.unhandledMessageBuffer = [NSMutableDictionary dictionary];
                }
                [self.unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
                secnotice("IDS Transport","unhandledMessageBuffer: %@", self.unhandledMessageBuffer);
            });
            //write message to disk if message is new to the unhandled queue
            if([self shouldPersistMessage:messageAndFromID id:fromID])
                [self persistState];
                
            [[KeychainSyncingOverIDSProxy idsProxy] scheduleRetryRequestTimer];
            return NULL;
        }
        else if(success == kHandleIDSmessageDeviceIDMismatch){
            secnotice("IDS Transport","message for a ghost! dropping message. error:%@", cf_error);
            return NULL;
        }
        else if(success == kHandleIDSMessageDontHandle){
            secnotice("IDS Transport","error in message, dropping message. error:%@", cf_error);
            return NULL;
        }
        else{
            secnotice("IDS Transport","IDSProxy handled this message %@, from: %@", messageAndFromID, fromID);

            if(useAck != nil && [useAck compare:@"YES"] == NSOrderedSame)
                [self sendACK:senderDeviceID peerID:peerID uniqueID:messageID senderDeviceID:senderDeviceID];
            return (NSMutableDictionary*)messageAndFromID;
        }

        CFReleaseNull(cf_error);
    }];
}

@end
