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

#import "IDSProxy.h"
#import "IDSPersistentState.h"
#import "KeychainSyncingOverIDSProxy+SendMessage.h"
#import "KeychainSyncingOverIDSProxy+Throttle.h"

#include <Security/SecItemInternal.h>


static NSString *const IDSSendMessageOptionForceEncryptionOffKey = @"IDSSendMessageOptionForceEncryptionOff";

static NSString *const kIDSNumberOfFragments = @"NumberOfIDSMessageFragments";
static NSString *const kIDSFragmentIndex = @"kFragmentIndex";
static NSString *const kIDSOperationType = @"IDSMessageOperation";
static NSString *const kIDSMessageToSendKey = @"MessageToSendKey";
static NSString *const kIDSMessageUniqueID = @"MessageID";
static NSString *const kIDSMessageRecipientPeerID = @"RecipientPeerID";
static NSString *const kIDSMessageRecipientDeviceID = @"RecipientDeviceID";
static NSString *const kIDSMessageUseACKModel = @"UsesAckModel";
static NSString *const kIDSDeviceID = @"deviceID";

static const int64_t kRetryTimerLeeway = (NSEC_PER_MSEC * 250);      // 250ms leeway for handling unhandled messages.
static const int64_t timeout = 3ull;
static const int64_t KVS_BACKOFF = 5;

static const NSUInteger kMaxIDSMessagePayloadSize = 64000;


@implementation KeychainSyncingOverIDSProxy (SendMessage)


-(bool) chunkAndSendKeychainPayload:(NSData*)keychainData deviceID:(NSString*)deviceName ourPeerID:(NSString*)ourPeerID theirPeerID:(NSString*) theirPeerID operation:(NSString*)operationTypeAsString uuid:(NSString*)uuidString error:(NSError**) error
{
    __block BOOL result = true;

    NSUInteger keychainDataLength = [keychainData length];
    int fragmentIndex = 0;
    int startingPosition = 0;

    NSUInteger totalNumberOfFragments = (keychainDataLength + kMaxIDSMessagePayloadSize - 1)/kMaxIDSMessagePayloadSize;

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
                               forKey:kIDSMessageToSendKey];
        // Insert the fragment number in the dictionary
        [fragmentDictionary setObject:[NSNumber numberWithInt:fragmentIndex]
                               forKey:kIDSFragmentIndex];

        result = [self sendIDSMessage:fragmentDictionary name:deviceName peer:ourPeerID];
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
            secnotice("IDSPing", "sent peerID: %@ to securityd to sync over KVS", theirPeerID);
        }
        else{
            secerror("Could not hand peerID: %@ to securityd, error: %@", theirPeerID, cf_error);
        }

        CFReleaseNull(cf_error);
        return NULL;
    }];
}

- (void) sendMessageToKVS: (NSDictionary<NSString*, NSDictionary*>*) encapsulatedKeychainMessage
{
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
    
    dispatch_source_t timer = [[KeychainSyncingOverIDSProxy idsProxy].pingTimers objectForKey:IDSid]; //remove timer
    dispatch_cancel(timer); //cancel timer
    
    [[KeychainSyncingOverIDSProxy idsProxy].pingTimers removeObjectForKey:IDSid];
    
    [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {
        
        success  = SOSCCRequestSyncWithPeerOverKVSUsingIDOnly(((__bridge CFStringRef)IDSid), &cf_error);
        
        if(success){
            secnotice("IDSPing", "sent peerID: %@ to securityd to sync over KVS", IDSid);
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
    NSDictionary *messageDictionary = @{kIDSOperationType : [NSString stringWithFormat:@"%d", kIDSPeerAvailability], kIDSMessageToSendKey : @"checking peers"};
    
    [list enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL * top) {
        NSString* IDSid = (NSString*)obj;
        NSString* identifier = [NSString string];
        bool result = false;
        secnotice("IDS Transport", "sending to id: %@", IDSid);

        [self recordTimestampOfWriteToIDS: messageDictionary deviceName:IDSid peerID:peerID]; //add pings to throttling
        NSDictionary *safeValues = [self filterForWritableValues:messageDictionary];
        
        if(safeValues != nil && [safeValues count] > 0){
            result = [self sendIDSMessage:safeValues name:IDSid peer:peerID];
            
            if(!result){
                secerror("Could not send message over IDS");
                [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {
                    CFErrorRef kvsError = nil;
                    bool success  = SOSCCRequestSyncWithPeerOverKVSUsingIDOnly(((__bridge CFStringRef)IDSid), &kvsError);
                    
                    if(success){
                        secnotice("IDSPing", "sent peerID: %@ to securityd to sync over KVS", IDSid);
                    }
                    else{
                        secerror("Could not hand peerID: %@ to securityd, error: %@", IDSid, kvsError);
                    }
                    CFReleaseNull(kvsError);
                    return NULL;
                }];
            }
            else{
                dispatch_source_t timer = nil;
                if( [self.pingTimers objectForKey:IDSid] == nil){
                    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
                    
                    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, kRetryTimerLeeway);
                    dispatch_source_set_event_handler(timer, ^{
                        [self pingTimerFired:IDSid peerID:peerID identifier:identifier];
                    });
                    dispatch_resume(timer);
                    
                    [self.pingTimers setObject:timer forKey:IDSid];
                }
            }
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
    NSDictionary *messageDictionary = [data objectForKey: kIDSMessageToSendKey];
    BOOL isPingMessage = false;

    if(messageDictionary && ![messageDictionary isKindOfClass:[NSDictionary class]])
    {
        NSString* messageString = [data objectForKey: kIDSMessageToSendKey];
        if(messageString && [messageString isKindOfClass:[NSString class]])
            isPingMessage = true;
    }
    else if(!messageDictionary){
        secerror("IDS Transport: message is null?");
    }

    return isPingMessage;
}

-(BOOL) sendFragmentedIDSMessages:(NSDictionary*)data name:(NSString*) deviceName peer:(NSString*) ourPeerID error:(NSError**) error
{
    BOOL result = false;
    BOOL isPingMessage = false;
    
    NSError* localError = nil;

    NSString* operationTypeAsString = [data objectForKey: kIDSOperationType];
    NSMutableDictionary *messageDictionary = [data objectForKey: kIDSMessageToSendKey];

    isPingMessage = [self isMessageAPing:data];
    
    //check the peer cache for the next time to send timestamp
    //if the timestamp is set in the future, reroute the message to KVS
    //otherwise send the message over IDS
    if(![self shouldProxySendMessage:deviceName])
    {
        if(isPingMessage){
            secnotice("IDS Transport", "peer negative cache check: peer cannot send yet. not sending ping message");
            return true;
        }
        else{
            secnotice("IDS Transport", "peer negative cache check: peer cannot send yet. rerouting message to be sent over KVS: %@", messageDictionary);
            [self sendMessageToKVS:messageDictionary];
            return true;
        }
    }
    
    if(isPingMessage){ //foward the ping message, no processing
       result = [self sendIDSMessage:data
                                 name:deviceName
                                 peer:ourPeerID];
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
            result = [self chunkAndSendKeychainPayload:keychainData
                                              deviceID:deviceName
                                             ourPeerID:ourPeerID
                                           theirPeerID:theirPeerID
                                             operation:operationTypeAsString
                                                  uuid:localMessageIdentifier
                                                 error:&localError];
    }
    else{
        NSMutableDictionary* dataCopy = [NSMutableDictionary dictionaryWithDictionary:data];
        [dataCopy setObject:localMessageIdentifier forKey:kIDSMessageUniqueID];
        result = [self sendIDSMessage:dataCopy
                                 name:deviceName
                                 peer:ourPeerID];
    }

    if(result && useAckModel){
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
    NSMutableDictionary *message = [[KeychainSyncingOverIDSProxy idsProxy].messagesInFlight objectForKey:identifier];
    if(!message){
        return;
    }
    NSDictionary *encapsulatedKeychainMessage = [message objectForKey:kIDSMessageToSendKey];

    [[KeychainSyncingOverIDSProxy idsProxy].messagesInFlight removeObjectForKey:identifier];

    secnotice("IDS Transport", "Encapsulated message: %@", encapsulatedKeychainMessage);
    dispatch_source_t timer = [[KeychainSyncingOverIDSProxy idsProxy].pingTimers objectForKey:identifier]; //remove timer
    dispatch_cancel(timer); //cancel timer

    [[KeychainSyncingOverIDSProxy idsProxy].pingTimers removeObjectForKey:identifier];

    [self sendMessageToKVS:encapsulatedKeychainMessage];

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

    [[KeychainSyncingOverIDSProxy idsProxy].messagesInFlight setObject:message forKey:identifier];
    [self.pingTimers setObject:timer forKey:identifier];
    [[KeychainSyncingOverIDSProxy idsProxy] persistState];
}


-(BOOL) sendIDSMessage:(NSDictionary*)data name:(NSString*) deviceName peer:(NSString*) peerID
{

    if(!self->_service){
        secerror("Could not send message to peer: %@: IDS delegate uninitialized, can't use IDS to send this message", deviceName);
        return NO;
    }

    dispatch_async(self.calloutQueue, ^{

        IDSMessagePriority priority = IDSMessagePriorityHigh;
        BOOL encryptionOff = YES;
        NSString *sendersPeerIDKey = [ NSString stringWithUTF8String: kMessageKeySendersPeerID];

        secnotice("backoff","!!writing these keys to IDS!!: %@", data);

        NSDictionary *options = @{IDSSendMessageOptionForceEncryptionOffKey : [NSNumber numberWithBool:encryptionOff] };

        NSMutableDictionary *dataCopy = [NSMutableDictionary dictionaryWithDictionary: data];

        //set our peer id and a unique id for this message
        [dataCopy setObject:peerID forKey:sendersPeerIDKey];
        secnotice("IDS Transport", "%@ sending message %@ to: %@", peerID, data, deviceName);

        NSDictionary *info;
        NSInteger errorCode = 0;
        NSInteger numberOfDevices = 0;
        NSString *errMessage = nil;
        NSMutableSet *destinations = nil;
        NSError *localError = nil;
        NSString *identifier = nil;
        IDSDevice *device = nil;
        numberOfDevices = [self.listOfDevices count];

        require_action_quiet(numberOfDevices > 0, fail, errorCode = kSecIDSErrorNotRegistered; errMessage=createErrorString(@"Could not send message to peer: %@: IDS devices are not registered yet", deviceName));
        secnotice("IDS Transport","List of devices: %@", [self->_service devices]);

        destinations = [NSMutableSet set];
        for(NSUInteger i = 0; i < [ self.listOfDevices count ]; i++){
            device = self.listOfDevices[i];
            if( [ deviceName compare:device.uniqueID ] == 0){
                [destinations addObject: IDSCopyIDForDevice(device)];
            }
        }
        require_action_quiet([destinations count] != 0, fail, errorCode = kSecIDSErrorCouldNotFindMatchingAuthToken; errMessage = createErrorString(@"Could not send message to peer: %@: IDS device ID for peer does not match any devices within an IDS Account", deviceName));

        bool result = [self->_service sendMessage:dataCopy toDestinations:destinations priority:priority options:options identifier:&identifier error:&localError ] ;

        require_action_quiet(localError == nil && result, fail, errorCode = kSecIDSErrorFailedToSend; errMessage = createErrorString(@"Had an error sending IDS message to peer: %@", deviceName));

        secnotice("IDS Transport","successfully sent to peer:%@, message: %@", deviceName, dataCopy);
    fail:

        info = [ NSDictionary dictionaryWithObjectsAndKeys:errMessage, NSLocalizedDescriptionKey, nil ];
        if(localError != nil){
            localError = [[NSError alloc] initWithDomain:@"com.apple.security.ids.error" code:errorCode userInfo:info ];
            secerror("%@", localError);
        }
        if(localError != nil)
            secerror("%@", localError);

    });

    return YES;
}

@end
