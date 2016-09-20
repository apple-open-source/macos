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

#import <IDS/IDS.h>
#import <os/activity.h>

#include <utilities/SecAKSWrappers.h>
#include <utilities/SecCFRelease.h>
#include <AssertMacros.h>

#import "IDSPersistentState.h"
#import "IDSKeychainSyncingProxy+IDSProxyReceiveMessage.h"
#import "IDSKeychainSyncingProxy+IDSProxySendMessage.h"
#import "IDSProxy.h"

static NSString *const kIDSNumberOfFragments = @"NumberOfIDSMessageFragments";
static NSString *const kIDSFragmentIndex = @"kFragmentIndex";
static NSString *const kIDSOperationType = @"IDSMessageOperation";
static NSString *const kIDSMessageToSendKey = @"MessageToSendKey";
static NSString *const kIDSMessageUniqueID = @"MessageID";

@implementation IDSKeychainSyncingProxy (IDSProxyReceiveMessage)


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
        NSString *uuidString = [message objectForKey:kIDSMessageUniqueID];
        
        if([IDSKeychainSyncingProxy idsProxy].allFragmentedMessages == nil)
            [IDSKeychainSyncingProxy idsProxy].allFragmentedMessages = [NSMutableDictionary dictionary];
        
        NSMutableDictionary *uniqueMessages = [[IDSKeychainSyncingProxy idsProxy].allFragmentedMessages objectForKey: fromID];
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
        [[IDSKeychainSyncingProxy idsProxy].allFragmentedMessages setObject:uniqueMessages forKey: fromID];
        
        if([self countNumberOfValidObjects:fragmentsForDeviceID] == [idsNumberOfFragments longValue])
            handOffMessage = true;
        else
            handOffMessage = false;
        
    }
    else //no fragmentation in the message, ready to hand off to securityd
        handOffMessage = true;
    
    return handOffMessage;
    
}

-(NSMutableDictionary*) combineMessage:(NSString*)deviceID peerID:(NSString*)peerID uuid:(NSString*)uuid
{
    NSString *dataKey = [ NSString stringWithUTF8String: kMessageKeyIDSDataMessage ];
    NSString *deviceIDKey = [ NSString stringWithUTF8String: kMessageKeyDeviceID ];
    NSString *peerIDKey = [ NSString stringWithUTF8String: kMessageKeyPeerID ];
    
    NSMutableDictionary *uniqueMessage = [[IDSKeychainSyncingProxy idsProxy].allFragmentedMessages objectForKey:deviceID];
    NSMutableArray *messagesForUUID = [uniqueMessage objectForKey:uuid];
    NSMutableData* completeMessage = [NSMutableData data];
    
    [messagesForUUID enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        NSData *messageFragment = (NSData*)obj;
        secnotice("IDS Transport","this is our index: %lu and size: %lu", (unsigned long)idx, (unsigned long)[messageFragment length]);
        
        [completeMessage appendData: messageFragment];
    }];
    [uniqueMessage removeObjectForKey:uuid];
    [[IDSKeychainSyncingProxy idsProxy].allFragmentedMessages removeObjectForKey:deviceID];
    return [NSMutableDictionary dictionaryWithObjectsAndKeys: completeMessage, dataKey, deviceID, deviceIDKey, peerID, peerIDKey, nil];
}

-(void) handleTestMessage:(NSString*)operation id:(NSString*)ID
{
    int operationType = [operation intValue];
    switch(operationType){
        case kIDSPeerAvailabilityDone:
        {
            secnotice("IDS Transport","!received availability response!: %@", ID);
            notify_post(kSOSCCPeerAvailable);
            //cancel timer!
            dispatch_source_t timer = [[IDSKeychainSyncingProxy idsProxy].pingTimers objectForKey:ID];
            if(timer != nil){
                secnotice("IDS Transport", "timer not nil");
                dispatch_cancel(timer);
                [[IDSKeychainSyncingProxy idsProxy].pingTimers removeObjectForKey:ID];
            }
            //call securityd to sync with device over IDS
            __block CFErrorRef cf_error = NULL;
            __block bool success = false;
            
            [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {
                
                success  = SOSCCRequestSyncWithPeerOverIDS(((__bridge CFStringRef)ID), &cf_error);
                
                if(success){
                    secnotice("IDSPing", "sent device ID: %@ to securityd to sync over IDS", ID);
                }
                else{
                    secerror("Could not hand device ID: %@ to securityd, error: %@", ID, cf_error);
                }
                
                return NULL;
            }];
            CFReleaseSafe(cf_error);

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
            NSDictionary* messsageDictionary = @{kIDSOperationType:operationString, kIDSMessageToSendKey:messageString};
            NSString *identifier = [NSString string];
            
            NSError *localError = NULL;
            if (!self.isLocked)
                [self sendIDSMessage:messsageDictionary name:ID peer:@"me" identifier:&identifier error:&localError];
            else
                secnotice("IDS Transport", "device is locked, not responding to availability check");
            
            free(messageCharS);
            
            break;
        }
        default:
            break;
    }
}

- (void)service:(IDSService *)service account:(IDSAccount *)account incomingMessage:(NSDictionary *)message fromID:(NSString *)fromID context:(IDSMessageContext *)context
{
    secnotice("IDS Transport","message: %@", message);
    NSString *dataKey = [ NSString stringWithUTF8String: kMessageKeyIDSDataMessage ];
    NSString *deviceIDKey = [ NSString stringWithUTF8String: kMessageKeyDeviceID ];
    NSString *peerIDKey = [ NSString stringWithUTF8String: kMessageKeyPeerID ];
    NSString *sendersPeerIDKey = [NSString stringWithUTF8String: kMessageKeySendersPeerID];
    
    NSString *ID = nil;
    uint32_t operationType;
    bool hadError = false;
    CFStringRef errorMessage = NULL;
    __block NSString* myPeerID = @"";
    __block NSData *messageData = nil;
    
    NSString* operationTypeAsString = nil;
    NSMutableDictionary *messageDictionary = nil;
    
    NSArray *devices = [_service devices];
    for(NSUInteger i = 0; i < [ devices count ]; i++){
        IDSDevice *device = devices[i];
        if( [(IDSCopyIDForDevice(device)) containsString: fromID] == YES){
            ID = device.uniqueID;
            break;
        }
    }
    secnotice("IDS Transport", "Received message from: %@", ID);
    NSString *sendersPeerID = [message objectForKey: sendersPeerIDKey];

    if(sendersPeerID == nil)
        sendersPeerID = [NSString string];
        

    require_action_quiet(ID, fail, hadError = true; errorMessage = CFSTR("require the sender's device ID"));
    
    operationTypeAsString = [message objectForKey: kIDSOperationType];
    messageDictionary = [message objectForKey: kIDSMessageToSendKey];
    
    secnotice("IDS Transport","from peer %@, operation type as string: %@, as integer: %d", ID, operationTypeAsString, [operationTypeAsString intValue]);
    operationType = [operationTypeAsString intValue];
    
    if(operationType == kIDSPeerAvailabilityDone || operationType == kIDSEndPingTestMessage || operationType == kIDSSendOneMessage || operationType == kIDSPeerAvailability || operationType == kIDSStartPingTestMessage)
    {
        [self handleTestMessage:operationTypeAsString id:ID];
    }
    else if(operationType == kIDSKeychainSyncIDSFragmentation)
    {
        [messageDictionary enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
            myPeerID = (NSString*)key;
            messageData = (NSData*)obj;
        }];
        
        BOOL readyToHandOffToSecD = [self checkForFragmentation:message id:ID data:messageData];
        
        NSMutableDictionary *messageAndFromID = nil;
        
        if(readyToHandOffToSecD && ([message objectForKey:kIDSFragmentIndex])!= nil){
            secnotice("IDS Transport","fragmentation: messageData: %@, myPeerID: %@", messageData, myPeerID);
            NSString* uuid = [message objectForKey:kIDSMessageUniqueID];
            messageAndFromID = [self combineMessage:ID peerID:myPeerID uuid:uuid];
        }
        else if(readyToHandOffToSecD){
            secnotice("IDS Transport","no fragmentation: messageData: %@, myPeerID: %@", messageData, myPeerID);
            messageAndFromID = [NSMutableDictionary dictionaryWithObjectsAndKeys: messageData, dataKey, ID, deviceIDKey, myPeerID, peerIDKey, nil];
        }
        else
            return;
        
        //set the sender's peer id so we can check it in securityd
        [messageAndFromID setObject:sendersPeerID forKey:sendersPeerIDKey];
        
        if([IDSKeychainSyncingProxy idsProxy].isLocked){
            //hang on to the message and set the retry deadline
            [self.unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
        }
        else
            [self sendMessageToSecurity:messageAndFromID fromID:fromID];
    }
    else
        secerror("dropping IDS message");
    
fail:
    if(hadError)
        secerror("error:%@", errorMessage);
}


- (void) handleAllPendingMessage
{
    secnotice("IDS Transport", "Attempting to handle pending messsages");
    if([self.unhandledMessageBuffer count] > 0){
        secnotice("IDS Transport", "handling Message: %@", self.unhandledMessageBuffer);
        NSMutableDictionary *copyOfUnhanlded = [NSMutableDictionary dictionaryWithDictionary:self.unhandledMessageBuffer];
        [copyOfUnhanlded enumerateKeysAndObjectsUsingBlock: ^(id key, id obj, BOOL *stop)
         {
             NSMutableDictionary *messageAndFromID = (NSMutableDictionary*)obj;
             NSString *fromID = (NSString*)key;
             //remove the message from the official message buffer (if it fails to get handled it'll be reset again in sendMessageToSecurity)
             [self.unhandledMessageBuffer removeObjectForKey: fromID];
             [self sendMessageToSecurity:messageAndFromID fromID:fromID];
         }];
    }
}

- (bool) shouldPersistMessage:(NSDictionary*) newMessageAndFromID id:(NSString*)fromID
{
    __block bool persistMessage = true;
    
    //get the dictionary of messages for a particular device id
    NSDictionary* messagesForID = [self.unhandledMessageBuffer valueForKey:fromID];
 
    //Grab the data blob
    CFStringRef dataKey = CFStringCreateWithCString(kCFAllocatorDefault, kMessageKeyIDSDataMessage, kCFStringEncodingASCII);
    CFDataRef messageData = asData(CFDictionaryGetValue((__bridge CFDictionaryRef)newMessageAndFromID, dataKey), NULL);

    [messagesForID enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL * stop) {
        NSData* queuedMessage = (NSData*)obj;
        
        if([queuedMessage isEqual:(__bridge NSData*)messageData])
            persistMessage = false;
    }];
    
    CFReleaseNull(dataKey);
    return persistMessage;
}

-(void)sendMessageToSecurity:(NSMutableDictionary*)messageAndFromID fromID:(NSString*)fromID
{
    __block CFErrorRef cf_error = NULL;
    __block HandleIDSMessageReason success = kHandleIDSMessageSuccess;
    
    [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {
        
        success  = SOSCCHandleIDSMessage(((__bridge CFDictionaryRef)messageAndFromID), &cf_error);
        //turns out the error needs to be evaluated as sync_and_do returns bools
        if(cf_error != NULL)
        {
            CFStringRef errorDescription = CFErrorCopyDescription(cf_error);
            if (CFStringCompare(errorDescription, CFSTR("The operation couldn’t be completed. (Mach error -536870174 - Kern return error)"), 0) == 0 ) {
                success = kHandleIDSMessageLocked;
            }
            CFReleaseNull(errorDescription);
        }
        
        if(success == kHandleIDSMessageLocked){
            secnotice("IDS Transport","cannot handle messages from: %@ when locked, error:%@", fromID, cf_error);
            if(!self.unhandledMessageBuffer)
                self.unhandledMessageBuffer = [NSMutableDictionary dictionary];
            
            //write message to disk if message is new to the unhandled queue
            if([self shouldPersistMessage:messageAndFromID id:fromID])
                [self persistState];
            
            [self.unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
            secnotice("IDS Transport", "unhandledMessageBuffer: %@", self.unhandledMessageBuffer);
            
            return NULL;
        }
        else if(success == kHandleIDSMessageNotReady){
            secnotice("IDS Transport","not ready to handle message from: %@, error:%@", fromID, cf_error);
            if(!self.unhandledMessageBuffer)
                self.unhandledMessageBuffer = [NSMutableDictionary dictionary];
            [self.unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
            secnotice("IDS Transport","unhandledMessageBuffer: %@", self.unhandledMessageBuffer);
            //set timer
            [[IDSKeychainSyncingProxy idsProxy] scheduleRetryRequestTimer];
           
            //write message to disk if message is new to the unhandled queue
            if([self shouldPersistMessage:messageAndFromID id:fromID])
                [self persistState];
            
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
            return (NSMutableDictionary*)messageAndFromID;
        }

        CFReleaseNull(cf_error);
    }];
}

@end
