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
#include <utilities/SecADWrapper.h>
#include <utilities/SecCFRelease.h>
#include <AssertMacros.h>

#import "IDSProxy.h"
#import "IDSPersistentState.h"
#import "KeychainSyncingOverIDSProxy+SendMessage.h"
#include <Security/SecItemInternal.h>


static NSString *const IDSSendMessageOptionForceEncryptionOffKey = @"IDSSendMessageOptionForceEncryptionOff";

static NSString *const kIDSNumberOfFragments = @"NumberOfIDSMessageFragments";
static NSString *const kIDSFragmentIndex = @"kFragmentIndex";
static NSString *const kIDSMessageUseACKModel = @"UsesAckModel";
static NSString *const kIDSMessageSendersDeviceID = @"SendersDeviceID";

static NSString *const kIDSDeviceID = @"deviceID";
static const int64_t kRetryTimerLeeway = (NSEC_PER_MSEC * 250);      // 250ms leeway for handling unhandled messages.
static const int64_t timeout = 5ull;
static const int64_t KVS_BACKOFF = 5;

static const NSUInteger kMaxIDSMessagePayloadSize = 64000;

@implementation KeychainSyncingOverIDSProxy (SendMessage)

-(bool) chunkAndSendKeychainPayload:(NSData*)keychainData deviceID:(NSString*)deviceName ourPeerID:(NSString*)ourPeerID theirPeerID:(NSString*) theirPeerID operation:(NSString*)operationTypeAsString uuid:(NSString*)uuidString senderDeviceID:(NSString*)senderDeviceID
                  error:(NSError**) error
{
    __block BOOL result = true;

    NSUInteger keychainDataLength = [keychainData length];
    int fragmentIndex = 0;
    int startingPosition = 0;

    NSUInteger totalNumberOfFragments = (keychainDataLength + kMaxIDSMessagePayloadSize - 1)/kMaxIDSMessagePayloadSize;
    secnotice("IDS Transport", "sending %lu number of fragments to: %@", (unsigned long)totalNumberOfFragments, deviceName);
    NSMutableDictionary* fragmentDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                                               deviceName, kIDSDeviceID,
                                               [NSNumber numberWithUnsignedInteger:totalNumberOfFragments], kIDSNumberOfFragments,
                                               [NSNumber numberWithInt:fragmentIndex], kIDSFragmentIndex,
                                               deviceName, kIDSMessageRecipientDeviceID, theirPeerID, kIDSMessageRecipientPeerID,
                                               operationTypeAsString, kIDSOperationType,
                                               uuidString, kIDSMessageUniqueID,
                                               nil];

    NSUInteger remainingLength = keychainDataLength;
    while(remainingLength > 0 && result == true){
        NSUInteger fragmentLength = MIN(remainingLength, kMaxIDSMessagePayloadSize);
        NSData *fragment = [keychainData subdataWithRange:NSMakeRange(startingPosition, fragmentLength)];

        // Insert the current fragment data in dictionary with key peerID and message key.
        [fragmentDictionary setObject:@{theirPeerID:fragment}
                               forKey:(__bridge NSString*)kIDSMessageToSendKey];
        // Insert the fragment number in the dictionary
        [fragmentDictionary setObject:[NSNumber numberWithInt:fragmentIndex]
                               forKey:kIDSFragmentIndex];

        result = [self sendIDSMessage:fragmentDictionary name:deviceName peer:ourPeerID senderDeviceID:senderDeviceID];
        if(!result)
            secerror("Could not send fragmented message");

        startingPosition+=fragmentLength;
        remainingLength-=fragmentLength;
        fragmentIndex++;
    }

    return result;
}

- (void)sendToKVS: (NSString*) theirPeerID message: (NSData*) message
{
    [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {
        CFErrorRef cf_error = NULL;

        bool success  = SOSCCRequestSyncWithPeerOverKVS(((__bridge CFStringRef)theirPeerID), (__bridge CFDataRef)message, &cf_error);

        if(success){
            secnotice("IDS Transport", "rerouting message %@", message);
        }
        else{
            secerror("could not route message to %@, error: %@", theirPeerID, cf_error);
        }

        CFReleaseNull(cf_error);
        return NULL;
    }];
}

- (void) sendMessageToKVS: (NSDictionary<NSString*, NSDictionary*>*) encapsulatedKeychainMessage
{
    SecADAddValueForScalarKey(CFSTR("com.apple.security.sos.kvsreroute"), 1);
    [encapsulatedKeychainMessage enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        if ([key isKindOfClass: [NSString class]] && [obj isKindOfClass:[NSData class]]) {
            [self sendToKVS:key message:obj];
        } else {
            secerror("Couldn't send to KVS key: %@ obj: %@", key, obj);
        }
    }];
}


- (void)pingTimerFired:(NSString*)IDSid peerID:(NSString*)peerID identifier:(NSString*)identifier
{
    //setting next time to send
    [self updateNextTimeToSendFor5Minutes:IDSid];

    secnotice("IDS Transport", "device ID: %@ !!!!!!!!!!!!!!!!Ping timeout is up!!!!!!!!!!!!", IDSid);
    //call securityd to sync with device over KVS
    __block CFErrorRef cf_error = NULL;
    __block bool success = kHandleIDSMessageSuccess;

    //cleanup timers
    dispatch_async(self.pingQueue, ^{
        dispatch_source_t timer = [[KeychainSyncingOverIDSProxy idsProxy].pingTimers objectForKey:IDSid]; //remove timer
        dispatch_cancel(timer); //cancel timer
        [[KeychainSyncingOverIDSProxy idsProxy].pingTimers removeObjectForKey:IDSid];
    });

    [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {
        
        success  = SOSCCRequestSyncWithPeerOverKVSUsingIDOnly(((__bridge CFStringRef)IDSid), &cf_error);
        
        if(success){
            secnotice("IDS Transport", "rerouting message for %@", peerID);
        }
        else{
            secerror("Could not hand peerID: %@ to securityd, error: %@", IDSid, cf_error);
        }
        
        return NULL;
    }];
    CFReleaseSafe(cf_error);
}

-(void) pingDevices:(NSArray*)list peerID:(NSString*)peerID
{
    NSDictionary *messageDictionary = @{(__bridge NSString*)kIDSOperationType : [NSString stringWithFormat:@"%d", kIDSPeerAvailability], (__bridge NSString*)kIDSMessageToSendKey : @"checking peers"};
    
    [list enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL * top) {
        NSString* IDSid = (NSString*)obj;
        NSString* identifier = [NSString string];
        bool result = false;
        secnotice("IDS Transport", "sending to id: %@", IDSid);
        
        result = [self sendIDSMessage:messageDictionary name:IDSid peer:peerID senderDeviceID:[NSString string]];
        
        if(!result){
            secerror("Could not send message over IDS");
            [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {
                CFErrorRef kvsError = nil;
                bool success  = SOSCCRequestSyncWithPeerOverKVSUsingIDOnly(((__bridge CFStringRef)IDSid), &kvsError);
                
                if(success){
                    secnotice("IDS Transport", "sent peerID: %@ to securityd to sync over KVS", IDSid);
                }
                else{
                    secerror("Could not hand peerID: %@ to securityd, error: %@", IDSid, kvsError);
                }
                CFReleaseNull(kvsError);
                return NULL;
            }];
        }
        else{
            dispatch_async(self.pingQueue, ^{
                //create a timer!
                if( [self.pingTimers objectForKey:IDSid] == nil){
                    dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
                    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, kRetryTimerLeeway);
                    dispatch_source_set_event_handler(timer, ^{
                        [self pingTimerFired:IDSid peerID:peerID identifier:identifier];
                    });
                    dispatch_resume(timer);
                    
                    [self.pingTimers setObject:timer forKey:IDSid];
                }
            });
        }
    }];
}

-(BOOL) shouldProxySendMessage:(NSString*)deviceName
{
    BOOL result = false;
    
    //checking peer cache to see if the message should be sent over IDS or back to KVS
    if(self.peerNextSendCache == nil)
    {
        self.peerNextSendCache = [[NSMutableDictionary alloc]initWithCapacity:0];
    }
    NSDate *nextTimeToSend = [self.peerNextSendCache objectForKey:deviceName];
    if(nextTimeToSend != nil)
    {
        //check if the timestamp is stale or set sometime in the future
        NSDate *currentTime = [[NSDate alloc] init];
        //if the current time is greater than the next time to send -> time to send!
        if([[nextTimeToSend laterDate:currentTime] isEqual:currentTime]){
            result = true;
        }
    }
    else{ //next time to send is not set yet
        result = true;
    }
    return result;
}

-(BOOL) isMessageAPing:(NSDictionary*)data
{
    NSDictionary *messageDictionary = [data objectForKey: (__bridge NSString*)kIDSMessageToSendKey];
    BOOL isPingMessage = false;

    if(messageDictionary && ![messageDictionary isKindOfClass:[NSDictionary class]])
    {
        NSString* messageString = [data objectForKey: (__bridge NSString*)kIDSMessageToSendKey];
        if(messageString && [messageString isKindOfClass:[NSString class]])
            isPingMessage = true;
    }
    else if(!messageDictionary){
        secerror("IDS Transport: message is null?");
    }

    return isPingMessage;
}

-(BOOL) sendFragmentedIDSMessages:(NSDictionary*)data name:(NSString*) deviceName peer:(NSString*) ourPeerID senderDeviceID:(NSString*)senderDeviceID error:(NSError**) error
{
    BOOL result = false;
    BOOL isPingMessage = false;
    
    NSError* localError = nil;

    NSString* operationTypeAsString = [data objectForKey: (__bridge NSString*)kIDSOperationType];
    NSMutableDictionary *messageDictionary = [data objectForKey: (__bridge NSString*)kIDSMessageToSendKey];

    isPingMessage = [self isMessageAPing:data];

    //check the peer cache for the next time to send timestamp
    //if the timestamp is set in the future, reroute the message to KVS
    //otherwise send the message over IDS
    if(![self shouldProxySendMessage:deviceName] && [KeychainSyncingOverIDSProxy idsProxy].allowKVSFallBack)
     {
        if(isPingMessage){
            secnotice("IDS Transport", "peer negative cache check: peer cannot send yet. not sending ping message");
            return true;
        }
        else{
            [self sendMessageToKVS:messageDictionary];
            return true;
        }
    }

    if(isPingMessage){ //foward the ping message, no processing
       result = [self sendIDSMessage:data
                                 name:deviceName
                                 peer:ourPeerID
                                 senderDeviceID:senderDeviceID];
        if(!result){
            secerror("Could not send ping message");
        }
        return result;
    }
    
    NSString *localMessageIdentifier = [[NSUUID UUID] UUIDString];

    bool fragment = [operationTypeAsString intValue] == kIDSKeychainSyncIDSFragmentation;
    bool useAckModel = fragment && [[data objectForKey:kIDSMessageUseACKModel] compare: @"YES"] == NSOrderedSame;

    __block NSData *keychainData = nil;
    __block NSString *theirPeerID = nil;
    
    [messageDictionary enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        if ([key isKindOfClass:[NSString class]] && [obj isKindOfClass:[NSData class]]) {
            theirPeerID = (NSString*)key;
            keychainData = (NSData*)obj;
        }
        *stop = YES;
    }];

    if(fragment && keychainData && [keychainData length] >= kMaxIDSMessagePayloadSize){
        secnotice("IDS Transport","sending chunked keychain messages");
            result = [self chunkAndSendKeychainPayload:keychainData
                                              deviceID:deviceName
                                             ourPeerID:ourPeerID
                                           theirPeerID:theirPeerID
                                             operation:operationTypeAsString
                                                  uuid:localMessageIdentifier
                                        senderDeviceID:senderDeviceID
                                                 error:&localError];
    }
    else{
        NSMutableDictionary* dataCopy = [NSMutableDictionary dictionaryWithDictionary:data];
        [dataCopy setObject:localMessageIdentifier forKey:(__bridge NSString*)kIDSMessageUniqueID];

        result = [self sendIDSMessage:dataCopy
                                 name:deviceName
                                 peer:ourPeerID
                                  senderDeviceID:senderDeviceID];
    }

    if(result && useAckModel && [KeychainSyncingOverIDSProxy idsProxy].allowKVSFallBack){
        secnotice("IDS Transport", "setting ack timer");
        [self setMessageTimer:localMessageIdentifier deviceID:deviceName message:data];
    }

    secnotice("IDS Transport","returning result: %d, error: %@", result, error ? *error : nil);
    return result;
}

-(void) updateNextTimeToSendFor5Minutes:(NSString*)ID
{
    secnotice("IDS Transport", "Setting next time to send in 5 minutes for device: %@", ID);
    
    NSTimeInterval backOffInterval = (KVS_BACKOFF * 60);
    NSDate *nextTimeToTransmit = [NSDate dateWithTimeInterval:backOffInterval sinceDate:[NSDate date]];
    
    [self.peerNextSendCache setObject:nextTimeToTransmit forKey:ID];
}

- (void)ackTimerFired:(NSString*)identifier deviceID:(NSString*)ID
{
    secnotice("IDS Transport", "IDS device id: %@, Ping timeout is up for message identifier: %@", ID, identifier);

    //call securityd to sync with device over KVS
    NSMutableDictionary * __block message;
    dispatch_sync(self.dataQueue, ^{
        message = [[KeychainSyncingOverIDSProxy idsProxy].messagesInFlight objectForKey:identifier];
        [[KeychainSyncingOverIDSProxy idsProxy].messagesInFlight removeObjectForKey:identifier];
    });
    if(!message){
        return;
    }
    NSDictionary *mesageInFlight = [message objectForKey:(__bridge NSString*)kIDSMessageToSendKey];

    [[KeychainSyncingOverIDSProxy idsProxy] printMessage:mesageInFlight state:@"timeout occured, rerouting to KVS"];

    //cleanup timers
    dispatch_async(self.pingQueue, ^{
        dispatch_source_t timer = [[KeychainSyncingOverIDSProxy idsProxy].pingTimers objectForKey:identifier]; //remove timer
        if(timer != nil)
            dispatch_cancel(timer); //cancel timer
        [[KeychainSyncingOverIDSProxy idsProxy].pingTimers removeObjectForKey:identifier];
    });

    [self sendMessageToKVS:mesageInFlight];

    //setting next time to send
    [self updateNextTimeToSendFor5Minutes:ID];
    
    [[KeychainSyncingOverIDSProxy idsProxy] persistState];
}

-(void) setMessageTimer:(NSString*)identifier deviceID:(NSString*)ID message:(NSDictionary*)message
{
    dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, kRetryTimerLeeway);

    dispatch_source_set_event_handler(timer, ^{
        [self ackTimerFired:identifier deviceID:ID];
    });
    dispatch_resume(timer);
    //restructure message in flight


    //set the timer for message id
    dispatch_async(self.pingQueue, ^{
        [self.pingTimers setObject:timer forKey:identifier];
    });
    
    dispatch_sync(self.dataQueue, ^{
        [[KeychainSyncingOverIDSProxy idsProxy].messagesInFlight setObject:message forKey:identifier];
    });
    [[KeychainSyncingOverIDSProxy idsProxy] persistState];
}

//had an immediate error, remove it from messages in flight, and immediately send it over KVS
-(void) cleanupAfterHardIDSError:(NSDictionary*)data
{
    NSString *messageIdentifier = [data objectForKey:(__bridge NSString*)kIDSMessageUniqueID];
    NSMutableDictionary * __block messageToSendToKVS = nil;
    
    if(messageIdentifier != nil){
        secerror("removing message id: %@ from message timers", messageIdentifier);
        dispatch_sync(self.dataQueue, ^{
            messageToSendToKVS = [[KeychainSyncingOverIDSProxy idsProxy].messagesInFlight objectForKey:messageIdentifier];
            [[KeychainSyncingOverIDSProxy idsProxy].messagesInFlight removeObjectForKey:messageIdentifier];
        });
        if(!messageToSendToKVS){
            secnotice("IDS Transport", "no message for identifier: %@", messageIdentifier);
            return;
        }
        [[KeychainSyncingOverIDSProxy idsProxy] printMessage:messageToSendToKVS state:@"IDS rejected send, message rerouted to KVS"];

        //cleanup timer for message
        dispatch_async(self.pingQueue, ^{
            dispatch_source_t timer = [[KeychainSyncingOverIDSProxy idsProxy].pingTimers objectForKey:messageIdentifier]; //remove timer
            if(timer)
                dispatch_cancel(timer); //cancel timer
            [[KeychainSyncingOverIDSProxy idsProxy].pingTimers removeObjectForKey:messageIdentifier];
        });
    }

    NSDictionary *messageInFlight = [messageToSendToKVS objectForKey:(__bridge NSString*)kIDSMessageToSendKey];

    if([messageInFlight isKindOfClass:[NSDictionary class]]){
        [[KeychainSyncingOverIDSProxy idsProxy] printMessage:messageInFlight state:@"IDS rejected send, message rerouted to KVS"];
        [self sendMessageToKVS:messageInFlight];
    }
}

-(BOOL) sendIDSMessage:(NSDictionary*)data name:(NSString*) deviceName peer:(NSString*) peerID senderDeviceID:(NSString*)senderDeviceID
{

    if(!self->_service){
        secerror("Could not send message to peer: %@: IDS delegate uninitialized, can't use IDS to send this message", deviceName);
        return NO;
    }

    NSMutableDictionary *dataCopy = [NSMutableDictionary dictionaryWithDictionary: data];
 
    __block NSString* senderDeviceIDCopy = nil;
    if(senderDeviceID){
       senderDeviceIDCopy = [[NSString alloc]initWithString:senderDeviceID];
    }
    else{
        secnotice("IDS Transport", "device id doesn't exist for peer:%@", peerID);
        senderDeviceIDCopy = [NSString string];
    }
    
    dispatch_async(self.calloutQueue, ^{

        IDSMessagePriority priority = IDSMessagePriorityHigh;
        BOOL encryptionOff = YES;
        NSString *sendersPeerIDKey = [ NSString stringWithUTF8String: kMessageKeySendersPeerID];

        NSDictionary *options = @{IDSSendMessageOptionForceEncryptionOffKey : [NSNumber numberWithBool:encryptionOff] };

        //set our peer id and a unique id for this message
        [dataCopy setObject:peerID forKey:sendersPeerIDKey];
        [dataCopy setObject:senderDeviceIDCopy forKey:kIDSMessageSendersDeviceID];
        secnotice("IDS Transport","Our device Name: %@", senderDeviceID);
        [[KeychainSyncingOverIDSProxy idsProxy] printMessage:dataCopy state:@"sending"];

        NSDictionary *info;
        NSInteger errorCode = 0;
        NSUInteger numberOfDevices = 0;
        NSString *errMessage = nil;
        NSMutableSet *destinations = nil;
        NSError *localError = nil;
        NSString *identifier = nil;
        IDSDevice *device = nil;
        NSArray* listOfDevices = [self->_service devices];
        numberOfDevices = [listOfDevices count];

        require_action_quiet(numberOfDevices > 0, fail, errorCode = kSecIDSErrorNotRegistered; errMessage=createErrorString(@"Could not send message to peer: %@: IDS devices are not registered yet", deviceName));
        secnotice("IDS Transport","List of devices: %@", [self->_service devices]);

        destinations = [NSMutableSet set];
        for(NSUInteger i = 0; i < numberOfDevices; i++){
            device = listOfDevices[i];
            if( [ deviceName compare:device.uniqueID ] == 0){
                [destinations addObject: IDSCopyIDForDevice(device)];
            }
        }
        require_action_quiet([destinations count] != 0, fail, errorCode = kSecIDSErrorCouldNotFindMatchingAuthToken; errMessage = createErrorString(@"Could not send message to peer: %@: IDS device ID for peer does not match any devices within an IDS Account", deviceName));

        bool result = [self->_service sendMessage:dataCopy toDestinations:destinations priority:priority options:options identifier:&identifier error:&localError ] ;

        [KeychainSyncingOverIDSProxy idsProxy].outgoingMessages++;
        require_action_quiet(localError == nil && result, fail, errorCode = kSecIDSErrorFailedToSend; errMessage = createErrorString(@"Had an error sending IDS message to peer: %@", deviceName));

        [[KeychainSyncingOverIDSProxy idsProxy] printMessage:dataCopy state:@"sent!"];
    fail:
        
        if(errMessage != nil){
            info = [ NSDictionary dictionaryWithObjectsAndKeys:errMessage, NSLocalizedDescriptionKey, nil ];
            localError = [[NSError alloc] initWithDomain:@"com.apple.security.ids.error" code:errorCode userInfo:info ];
            secerror("%@", localError);
            [self cleanupAfterHardIDSError: data];
        }
    });

    return YES;
}

@end
