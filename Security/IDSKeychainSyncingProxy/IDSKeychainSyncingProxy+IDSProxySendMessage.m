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
#import "IDSKeychainSyncingProxy+IDSProxySendMessage.h"
#import "IDSKeychainSyncingProxy+IDSProxyThrottle.h"

#define kSecServerKeychainChangedNotification "com.apple.security.keychainchanged"


static NSString *const IDSSendMessageOptionForceEncryptionOffKey = @"IDSSendMessageOptionForceEncryptionOff";

static NSString *const kIDSNumberOfFragments = @"NumberOfIDSMessageFragments";
static NSString *const kIDSFragmentIndex = @"kFragmentIndex";
static NSString *const kIDSOperationType = @"IDSMessageOperation";
static NSString *const kIDSMessageToSendKey = @"MessageToSendKey";
static NSString *const kIDSMessageUniqueID = @"MessageID";
static const int64_t kRetryTimerLeeway = (NSEC_PER_MSEC * 250);      // 250ms leeway for handling unhandled messages.
static const int64_t timeout = 7;

static const int64_t kMaxIDSMessagePayloadSize = 64000;


@implementation IDSKeychainSyncingProxy (IDSProxySendMessage)

-(bool) chunkAndSendKeychainPayload:(NSMutableData*)keychainData deviceID:(NSString*)deviceName ourPeerID:(NSString*)ourPeerID theirPeerID:(NSString*) theirPeerID operation:(NSString*)operationTypeAsString error:(NSError**) error
{
    __block BOOL result = false;

    CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
    CFStringRef uuidString = CFUUIDCreateString(kCFAllocatorDefault, uuid);

    uint64_t keychainDataLength = (uint64_t)[keychainData length];
    NSUInteger tempLength = [keychainData length];
    int fragmentIndex = 0;
    int startingPosition = 0;

    int totalNumberOfFragments = ceil((double)((double)keychainDataLength/(double)kMaxIDSMessagePayloadSize));
    secnotice("IDS Transport","Total number of Fragments: %d", totalNumberOfFragments);

    while(tempLength != 0){
        secnotice("IDS Transport","length: %lu", (unsigned long)tempLength);
        NSUInteger endlength;
        if(tempLength < kMaxIDSMessagePayloadSize)
            endlength = tempLength;
        else
            endlength = kMaxIDSMessagePayloadSize;

        NSData *fragment = [keychainData subdataWithRange:NSMakeRange(startingPosition, endlength)];
        NSMutableDictionary *newFragmentDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:fragment, theirPeerID, nil];

        NSMutableDictionary* newMessageFragment = [NSMutableDictionary dictionaryWithObjectsAndKeys:deviceName, @"deviceID",
                                                   [[NSNumber alloc]initWithInt: totalNumberOfFragments], kIDSNumberOfFragments,
                                                   [[NSNumber alloc] initWithInt: fragmentIndex], kIDSFragmentIndex,
                                                   newFragmentDictionary,kIDSMessageToSendKey,
                                                   operationTypeAsString, kIDSOperationType,
                                                   (__bridge NSString*)uuidString, kIDSMessageUniqueID, nil];
        NSString *identifier = [NSString string];
        
        secnotice("IDS Transport","sending fragment: %@", newMessageFragment);
        result = [self sendIDSMessage:newMessageFragment name:deviceName peer:ourPeerID identifier:&identifier error:error];
        startingPosition+=endlength;
        tempLength -= endlength;
        fragmentIndex++;
    }
    CFReleaseNull(uuidString);
    CFReleaseNull(uuid);
    return result;
}

-(BOOL) sendFragmentedIDSMessages:(NSDictionary*)data name:(NSString*) deviceName peer:(NSString*) ourPeerID error:(NSError**) error
{

    __block BOOL result = false;

    __block NSMutableData *keychainData = nil;
    __block NSString *theirPeerID = nil;
    secnotice("IDS Transport","fragmenting message! %@", data);
    NSString *identifier = [NSString string];

    NSString* operationTypeAsString = [data objectForKey: kIDSOperationType];
    NSMutableDictionary *messageDictionary = [data objectForKey: kIDSMessageToSendKey];

    if([operationTypeAsString intValue] == kIDSKeychainSyncIDSFragmentation){

        [messageDictionary enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
            keychainData = (NSMutableData*)obj;
            theirPeerID = (NSString*)key;
            return;
        }];
        secnotice("IDS Transport","keychainData length: %lu", (unsigned long)[keychainData length]);
        if((uint64_t)[keychainData length] >= kMaxIDSMessagePayloadSize){
            [self chunkAndSendKeychainPayload:keychainData deviceID:deviceName ourPeerID:ourPeerID theirPeerID:theirPeerID operation:operationTypeAsString error:error];
        }
        else{ //message is less than the max encryption size, pass it along
            secnotice("IDS Transport","sending message, no fragmentation: %@", data);
            result = [self sendIDSMessage:data name:deviceName peer:ourPeerID identifier:&identifier error:error];
        }
    }
    else
        result = [self sendIDSMessage:data name:deviceName peer:ourPeerID identifier:&identifier error:error];
        


    secnotice("IDS Transport","returning result: %d, error: %@", result, *error);
    return result;
}

- (void)pingTimerFired:(NSString*)deviceID peerID:(NSString*)peerID identifier:(NSString*)identifier
{
    secnotice("IDS Transport", "device ID: %@ !!!!!!!!!!!!!!!!Ping timeout is up!!!!!!!!!!!!", deviceID);
    //call securityd to sync with device over KVS
    __block CFErrorRef cf_error = NULL;
    __block bool success = kHandleIDSMessageSuccess;

    dispatch_source_t timer = [[IDSKeychainSyncingProxy idsProxy].pingTimers objectForKey:deviceID]; //remove timer
    dispatch_cancel(timer); //cancel timer

    [[IDSKeychainSyncingProxy idsProxy].pingTimers removeObjectForKey:deviceID];

    [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {

        success  = SOSCCRequestSyncWithPeerOverKVS(((__bridge CFStringRef)deviceID), &cf_error);

        if(success){
            secnotice("IDSPing", "sent peerID: %@ to securityd to sync over KVS", deviceID);
        }
        else{
            secerror("Could not hand peerID: %@ to securityd, error: %@", deviceID, cf_error);
        }

        return NULL;
    }];
    CFReleaseSafe(cf_error);
}


-(void) pingDevices:(NSArray*)list peerID:(NSString*)peerID
{
    NSDictionary *messageDictionary = @{kIDSOperationType : [NSString stringWithFormat:@"%d", kIDSPeerAvailability], kIDSMessageToSendKey : @"checking peers"};
    
    [list enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL * top) {
        NSString* deviceID = (NSString*)obj;
        NSString* identifier = [NSString string];
        
        secnotice("IDS Transport", "sending to id: %@", deviceID);
        NSError *localErr = nil;
        
        [self recordTimestampOfWriteToIDS: messageDictionary deviceName:deviceID peerID:peerID]; //add pings to throttling
        NSDictionary *safeValues = [self filterForWritableValues:messageDictionary];
        
        if(safeValues != nil && [safeValues count] > 0){
            [self sendIDSMessage:safeValues name:deviceID peer:peerID identifier:&identifier error:&localErr];
        
            if(localErr != nil){
                secerror("sending ping to peer %@ had an error: %@", deviceID, localErr);
                [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {
                    CFErrorRef kvsError = nil;
                    bool success  = SOSCCRequestSyncWithPeerOverKVS(((__bridge CFStringRef)deviceID), &kvsError);
                    
                    if(success){
                        secnotice("IDSPing", "sent peerID: %@ to securityd to sync over KVS", deviceID);
                    }
                    else{
                        secerror("Could not hand peerID: %@ to securityd, error: %@", deviceID, kvsError);
                    }
                    CFReleaseNull(kvsError);
                    return NULL;
                }];
            }
            else{
                dispatch_source_t timer = nil;
                if( [self.pingTimers objectForKey:deviceID] == nil){
                    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
                    
                    dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_SEC), DISPATCH_TIME_FOREVER, kRetryTimerLeeway);
                    dispatch_source_set_event_handler(timer, ^{
                        [self pingTimerFired:deviceID peerID:peerID identifier:identifier];
                    });
                    dispatch_resume(timer);
                    
                    [self.pingTimers setObject:timer forKey:deviceID];
                }
            }
        }
    }];
    
}
-(BOOL) sendIDSMessage:(NSDictionary*)data name:(NSString*) deviceName peer:(NSString*) peerID identifier:(NSString **)identifier error:(NSError**) error
{
    BOOL result = true;
    NSDictionary *userInfo;
    NSInteger code = 0;

    NSString *errorMessage;
    NSMutableSet *destinations = [NSMutableSet set];
    NSArray *ListOfIDSDevices = nil;
    IDSMessagePriority priority = IDSMessagePriorityHigh;
    IDSDevice *device = nil;
    BOOL encryptionOff = YES;
    NSError *localError = nil;
    NSString *sendersPeerIDKey = [ NSString stringWithUTF8String: kMessageKeySendersPeerID];

    secnotice("backoff","!!writing these keys to IDS!!: %@", data);

    NSDictionary *options = @{IDSSendMessageOptionForceEncryptionOffKey : [NSNumber numberWithBool:encryptionOff] };

    NSMutableDictionary *dataCopy = [NSMutableDictionary dictionaryWithDictionary: data];

    [dataCopy setObject:peerID forKey:sendersPeerIDKey];

    secnotice("IDS Transport", "Sending message from: %@ to: %@", peerID, deviceName);

    require_action_quiet(_service, fail, code = kSecIDSErrorNotRegistered; errorMessage = createErrorString(@"Could not send message to peer: %@: IDS delegate uninitialized, can't use IDS to send this message", deviceName));

    secnotice("IDS Transport","devices: %@", [_service devices]);
    secnotice("IDS Transport", " we have their deviceName: %@", deviceName);

    ListOfIDSDevices = [_service devices];

    require_action_quiet([ListOfIDSDevices count]> 0, fail, code = kSecIDSErrorNotRegistered; errorMessage=createErrorString(@"Could not send message to peer: %@: IDS devices are not registered yet", deviceName));
    secnotice("IDS Transport","This is our list of devices: %@", ListOfIDSDevices);

    for(NSUInteger i = 0; i < [ ListOfIDSDevices count ]; i++){
        device = ListOfIDSDevices[i];
        if( [ deviceName compare:device.uniqueID ] == 0){
            [destinations addObject: IDSCopyIDForDevice(device)];
        }
    }

    require_action_quiet([destinations count] != 0, fail, code = kSecIDSErrorCouldNotFindMatchingAuthToken; errorMessage = createErrorString(@"Could not send message to peer: %@: IDS device ID for peer does not match any devices within an IDS Account", deviceName));

    result = [_service sendMessage:dataCopy toDestinations:destinations priority:priority options:options identifier:identifier error:&localError ] ;

    require_action_quiet(localError == nil, fail, code = kSecIDSErrorFailedToSend; errorMessage = createErrorString(@"Had an error sending IDS message to peer: %@", deviceName));

    secnotice("IDS Transport", "identifier: %@", *identifier);
    
    secnotice("IDS Transport","sent to peer:%@, message: %@", deviceName, dataCopy);

    return result;

fail:
    userInfo = [ NSDictionary dictionaryWithObjectsAndKeys:errorMessage, NSLocalizedDescriptionKey, nil ];
    if(error != nil){
        *error = [NSError errorWithDomain:@"com.apple.security.ids.error" code:code userInfo:userInfo];
        secerror("%@", *error);
    }
    if(localError != nil)
        secerror("%@", localError);
    
    return false;
}

- (void)service:(IDSService *)service account:(IDSAccount *)account identifier:(NSString *)identifier didSendWithSuccess:(BOOL)success error:(NSError *)error
{
    if (error) {
        NSLog(@"IDSKeychainSyncingProxy didSendWithSuccess identifier=%@ error=%@", identifier, error);
    } else {
        NSLog(@"IDSKeychainSyncingProxy didSendWithSuccess identifier=%@ Success!", identifier);
    }
}

@end
