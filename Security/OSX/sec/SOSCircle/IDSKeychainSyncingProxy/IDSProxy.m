/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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

//
//  IDSProxy.m
//  ids-xpc
//


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

static const char *kStreamName = "com.apple.notifyd.matching";
NSString *const IDSSendMessageOptionForceEncryptionOffKey = @"IDSSendMessageOptionForceEncryptionOff";

static const int64_t kAttemptFlushBufferInterval = (NSEC_PER_SEC * 15);
static const int64_t kSyncTimerLeeway = (NSEC_PER_MSEC * 250);      // 250ms leeway for handling unhandled messages.
static const int64_t kMaxMessageRetryDelay = (NSEC_PER_SEC * 5);    //   5s  maximun delay for a given request
static const int64_t kMinMessageRetryDelay = (NSEC_PER_MSEC * 500); // 500ms minimum delay before attempting to retry handling messages.

#define SECD_RUN_AS_ROOT_ERROR 550


@implementation IDSKeychainSyncingProxy

+   (IDSKeychainSyncingProxy *) idsProxy
{
    static IDSKeychainSyncingProxy *idsProxy;
    if (!idsProxy) {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            idsProxy = [[self alloc] init];
        });
    }
    return idsProxy;
}

- (void)persistState
{
    if([_unhandledMessageBuffer count] > 0){
        [IDSKeychainSyncingProxyPersistentState setUnhandledMessages:_unhandledMessageBuffer];
    }
}

- (void) importKeyInterests: (NSMutableDictionary*) unhandledMessages
{
    _unhandledMessageBuffer = unhandledMessages;
}

- (id)init
{
    if (self = [super init])
    {
        secnotice("event", "%@ start", self);
        
        _isIDSInitDone = false;
        _service = nil;
        _calloutQueue = dispatch_queue_create("IDSCallout", DISPATCH_QUEUE_SERIAL);
        _unhandledMessageBuffer = [ [NSMutableDictionary alloc] initWithCapacity: 0];
        _isSecDRunningAsRoot = false;
        secdebug(IDSPROXYSCOPE, "%@ done", self);
        
        [self doIDSInitialization];
        if(_isIDSInitDone)
        [self doSetIDSDeviceID:nil];
        
        _syncTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
        dispatch_source_set_timer(_syncTimer, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, kSyncTimerLeeway);
        dispatch_source_set_event_handler(_syncTimer, ^{
            [self timerFired];
        });
        dispatch_resume(_syncTimer);
        
        [self importKeyInterests: [IDSKeychainSyncingProxyPersistentState unhandledMessages]];
        
        xpc_set_event_stream_handler(kStreamName, dispatch_get_main_queue(),
                                     ^(xpc_object_t notification){
                                         [self streamEvent:notification];
                                     });
        
        [self updateUnlockedSinceBoot];
        [self updateIsLocked];
        if (!_isLocked)
        [self keybagDidUnlock];
        
    }
    return self;
}

- (void) keybagDidLock
{
    secnotice("event", "%@", self);
}

- (void) keybagDidUnlock
{
    secnotice("event", "%@", self);
    [self handleAllPendingMessage];
}

- (BOOL) updateUnlockedSinceBoot
{
    CFErrorRef aksError = NULL;
    if (!SecAKSGetHasBeenUnlocked(&_unlockedSinceBoot, &aksError)) {
        secerror("%@ Got error from SecAKSGetHasBeenUnlocked: %@", self, aksError);
        CFReleaseSafe(aksError);
        return NO;
    }
    return YES;
}

- (BOOL) updateIsLocked
{
    CFErrorRef aksError = NULL;
    if (!SecAKSGetIsLocked(&_isLocked, &aksError)) {
        secerror("%@ Got error querying lock state: %@", self, aksError);
        CFReleaseSafe(aksError);
        return NO;
    }
    if (!_isLocked)
    _unlockedSinceBoot = YES;
    return YES;
}

- (void) keybagStateChange
{
    os_activity_initiate("keybagStateChanged", OS_ACTIVITY_FLAG_DEFAULT, ^{
        BOOL wasLocked = _isLocked;
        if ([self updateIsLocked]) {
            if (wasLocked == _isLocked)
            secdebug("event", "%@ still %s ignoring", self, _isLocked ? "locked" : "unlocked");
            else if (_isLocked)
            [self keybagDidLock];
            else
            [self keybagDidUnlock];
        }
    });
}

- (void)streamEvent:(xpc_object_t)notification
{
#if (!TARGET_IPHONE_SIMULATOR)
    const char *notificationName = xpc_dictionary_get_string(notification, "Notification");
    if (!notificationName) {
    } else if (strcmp(notificationName, kUserKeybagStateChangeNotification)==0) {
        return [self keybagStateChange];
    }
    const char *eventName = xpc_dictionary_get_string(notification, "XPCEventName");
    char *desc = xpc_copy_description(notification);
    secnotice("event", "%@ event: %s name: %s desc: %s", self, eventName, notificationName, desc);
    if (desc)
    free((void *)desc);
#endif
}

- (void)timerFired
{
    secdebug("IDS Transport", "%@ attempting to hand unhandled messages to securityd, here is our message queue: %@", self, _unhandledMessageBuffer);
    if([_unhandledMessageBuffer count] == 0)
    _syncTimerScheduled = NO;
    else if (_syncTimerScheduled && !_isLocked){
        [self handleAllPendingMessage];
    }
}

- (dispatch_time_t) nextSyncTime
{
    secdebug("IDS Transport", "nextSyncTime");
    
    dispatch_time_t nextSync = dispatch_time(DISPATCH_TIME_NOW, kMinMessageRetryDelay);
    
    // Don't sync again unless we waited at least kAttemptFlushBufferInterval
    if (_lastSyncTime) {
        dispatch_time_t soonest = dispatch_time(_lastSyncTime, kAttemptFlushBufferInterval);
        if (nextSync < soonest || _deadline < soonest) {
            secdebug("timer", "%@ backing off", self);
            return soonest;
        }
    }
    
    // Don't delay more than kMaxMessageRetryDelay after the first request.
    if (nextSync > _deadline) {
        secdebug("timer", "%@ hit deadline", self);
        return _deadline;
    }
    
    // Bump the timer by kMinMessageRetryDelay
    if (_syncTimerScheduled)
    secdebug("timer", "%@ bumped timer", self);
    else
    secdebug("timer", "%@ scheduled timer", self);
    
    return nextSync;
}

- (void)scheduleSyncRequestTimer
{
    secdebug("IDS Transport", "scheduling sync request timer");
    dispatch_source_set_timer(_syncTimer, [self nextSyncTime], DISPATCH_TIME_FOREVER, kSyncTimerLeeway);
    _syncTimerScheduled = YES;
}


- (void)setItemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock
{
    self->itemsChangedCallback = itemsChangedBlock;
}

- (void)doIDSInitialization{
    
    secnotice("IDS Transport", "doIDSInitialization!");
    
    _service = [[IDSService alloc] initWithService: @IDSServiceNameKeychainSync];
    
    if( _service == nil ){
        _isIDSInitDone = false;
        secerror("Could not create ids service");
    }
    else{
        secnotice("IDS Transport", "IDS Transport Successfully set up IDS!");
        [_service addDelegate:self queue: dispatch_get_main_queue()];
        
        _isIDSInitDone = true;
        if(_isSecDRunningAsRoot == false)
        [self doSetIDSDeviceID:nil];
    }
}

- (void) calloutWith: (void(^)(NSMutableDictionary *pending, bool handlePendingMesssages, bool doSetDeviceID, dispatch_queue_t queue, void(^done)(NSMutableDictionary *handledMessages, bool handledPendingMessage, bool handledSettingDeviceID))) callout
{
    // In IDSKeychainSyncingProxy serial queue
    dispatch_queue_t idsproxy_queue = dispatch_get_main_queue();
    
    _oldInCallout = YES;
    
    // dispatch_get_global_queue - well-known global concurrent queue
    // dispatch_get_main_queue   - default queue that is bound to the main thread
    xpc_transaction_begin();
    dispatch_async(_calloutQueue, ^{
        __block NSMutableDictionary *myPending;
        __block bool myHandlePendingMessage;
        __block bool myDoSetDeviceID;
        __block bool wasLocked;
        dispatch_sync(idsproxy_queue, ^{
            myPending = [_unhandledMessageBuffer copy];
            myHandlePendingMessage = _handleAllPendingMessages;
            myDoSetDeviceID = _setIDSDeviceID;
            wasLocked = _isLocked;
            
            _inCallout = YES;
            if (!_oldInCallout)
            secnotice("deaf", ">>>>>>>>>>> _oldInCallout is NO and we're heading in to the callout!");
            
            _shadowHandleAllPendingMessages = NO;
        });
        
        callout(myPending, myHandlePendingMessage, myDoSetDeviceID, idsproxy_queue, ^(NSMutableDictionary *handledMessages, bool handledPendingMessage, bool handledSetDeviceID) {
            secdebug("event", "%@ %s%s before callout handled: %s%s", self, myHandlePendingMessage ? "P" : "p", myDoSetDeviceID ? "D" : "d", handledPendingMessage ? "H" : "h", handledSetDeviceID ? "I" : "i");
            
            // In IDSKeychainSyncingProxy's serial queue
            _inCallout = NO;
            _oldInCallout = NO;
            
            NSError *error;
            
            // Update setting device id
            _setIDSDeviceID = ((myDoSetDeviceID && !handledSetDeviceID) || _shadowHandleAllPendingMessages);
            
            _shadowDoSetIDSDeviceID = NO;
            
            if(_setIDSDeviceID && !_isLocked && _isSecDRunningAsRoot == false)
            [self doSetIDSDeviceID:&error];
            
            // Update handling pending messages
            _handleAllPendingMessages = ((myHandlePendingMessage && (!handledPendingMessage)) || _shadowHandleAllPendingMessages);
            
            _shadowHandleAllPendingMessages = NO;
            
            if (handledPendingMessage)
            _lastSyncTime = dispatch_time(DISPATCH_TIME_NOW, 0);
            
            // Update pending messages and handle them
            [handledMessages enumerateKeysAndObjectsUsingBlock: ^(id key, id obj, BOOL *stop){
                NSString* fromID = (NSString*)key;
                [_unhandledMessageBuffer removeObjectForKey:fromID];
            }];
            
            // Write state to disk
            [self persistState];
            
            if ([_unhandledMessageBuffer count] > 0 || (!_isLocked && wasLocked))
            [self handleAllPendingMessage];
            
            xpc_transaction_end();
        });
    });
}

- (BOOL) doSetIDSDeviceID: (NSError**)error
{
    BOOL result = false;
    NSDictionary *userInfo;
    NSInteger code = 0;
    NSString *errorMessage;
    __block NSString* deviceID;
    __block CFErrorRef localError = NULL;
    __block bool handledSettingID = false;
    
    if(!_isIDSInitDone){
        [self doIDSInitialization];
    }
    if(_isSecDRunningAsRoot == true)
    {
        secerror("cannot set IDS device ID, secd is running as root");
        return false;
    }
    require_action_quiet(_isIDSInitDone, fail, errorMessage = @"IDSKeychainSyncingProxy can't set up the IDS service"; code = kSecIDSErrorNotRegistered);
    require_action_quiet(!_isLocked, fail, errorMessage = @"IDSKeychainSyncingProxy can't set device ID, device is locked"; code = kSecIDSErrorDeviceIsLocked);
    
    deviceID = IDSCopyLocalDeviceUniqueID();
    secdebug("IDS Transport", "This is our IDS device ID: %@", deviceID);
    
    require_action_quiet(deviceID != nil, fail, errorMessage = @"IDSKeychainSyncingProxy could not retrieve device ID from keychain"; code = kSecIDSErrorNoDeviceID);
    
    if(_inCallout && _isSecDRunningAsRoot == false){
        _shadowDoSetIDSDeviceID = YES;
        result = true;
    }
    else{
        _setIDSDeviceID = YES;
        [self calloutWith:^(NSMutableDictionary *pending, bool handlePendingMesssages, bool doSetDeviceID, dispatch_queue_t queue, void(^done)(NSMutableDictionary *, bool, bool)) {
            handledSettingID = SOSCCSetDeviceID((__bridge CFStringRef) deviceID, &localError);
            
            dispatch_async(queue, ^{
                if(localError){
                    if(CFErrorGetCode(localError) == SECD_RUN_AS_ROOT_ERROR){
                        secerror("SETTING RUN AS ROOT ERROR");
                        _isSecDRunningAsRoot = true;
                    }
                    if(error)
                    *error = (__bridge NSError *)(localError);
                }
                handledSettingID = YES;
                done(nil, NO, handledSettingID);
            });
        }];
        result = handledSettingID;
    }
    return result;
    
fail:
    userInfo = [ NSDictionary dictionaryWithObjectsAndKeys:errorMessage, NSLocalizedDescriptionKey, nil ];
    if(error != nil){
        *error = [NSError errorWithDomain:@"com.apple.security.ids.error" code:code userInfo:userInfo];
        secerror("%@", *error);
    }
    return false;
}

-(BOOL) sendIDSMessage:(NSDictionary*)data name:(NSString*) deviceName peer:(NSString*) peerID error:(NSError**) error
{
    BOOL result = true;
    NSDictionary *userInfo;
    NSInteger code = 0;
    
    NSString *errorMessage;
    NSString* identifier = [NSString string];
    NSMutableSet *destinations = [NSMutableSet set];
    NSArray *ListOfIDSDevices = nil;
    IDSMessagePriority priority = IDSMessagePriorityHigh;
    IDSDevice *device = nil;
    BOOL encryptionOff = YES;
    
    NSDictionary *options = [ NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:encryptionOff], IDSSendMessageOptionForceEncryptionOffKey, nil ];
    
    require_action_quiet(_service, fail, errorMessage = @"Could not send message: IDS delegate uninitialized, can't use IDS to send this message"; code = kSecIDSErrorNotRegistered);
    
    secdebug("IDS Transport", "[_service devices]: %@, we have their deviceName: %@", [_service devices], deviceName);
    ListOfIDSDevices = [_service devices];
    
    require_action_quiet([ListOfIDSDevices count]> 0, fail, errorMessage=@"Could not send message: IDS devices are not registered yet"; code = kSecIDSErrorNotRegistered);
    secinfo("IDS Transport", "This is our list of devices: %@", ListOfIDSDevices);
    
    for(NSUInteger i = 0; i < [ ListOfIDSDevices count ]; i++){
        device = ListOfIDSDevices[i];
        if( [ deviceName compare:device.uniqueID ] == 0){
            [destinations addObject: IDSCopyIDForDevice(device)];
        }
    }
    require_action_quiet([destinations count] != 0, fail, errorMessage = @"Could not send message: IDS device ID for peer does not match any devices within an IDS Account"; code = kSecIDSErrorCouldNotFindMatchingAuthToken);
    
    result = [_service sendMessage:data toDestinations:destinations priority:priority options:options identifier:&identifier error:error ] ;
    
    require_action_quiet(*error == nil, fail, errorMessage = @"Had an error sending IDS message"; code = kSecIDSErrorFailedToSend);
    
    secdebug("IDS Transport", "IDSKeychainSyncingProxy sent this message over IDS: %@", data);
    
    return result;
    
fail:
    userInfo = [ NSDictionary dictionaryWithObjectsAndKeys:errorMessage, NSLocalizedDescriptionKey, nil ];
    if(error != nil){
        *error = [NSError errorWithDomain:@"com.apple.security.ids.error" code:code userInfo:userInfo];
        secerror("%@", *error);
    }
    
    return false;
}


- (void) sendKeysCallout: (NSMutableDictionary*(^)(NSMutableDictionary* pending, NSError** error)) handleMessages {
    [self calloutWith: ^(NSMutableDictionary *pending, bool handlePendingMesssages, bool doSetDeviceID, dispatch_queue_t queue, void(^done)(NSMutableDictionary *, bool, bool)) {
        NSError* error = NULL;
        
        NSMutableDictionary* handled = handleMessages(pending, &error);
        
        dispatch_async(queue, ^{
            if (!handled && error) {
                secerror("%@ did not handle message: %@", self, error);
            }
            
            done(handled, NO, NO);
        });
    }];
}

- (void) handleAllPendingMessage
{
    if([_unhandledMessageBuffer count] > 0){
        secinfo("IDS Transport", "handling Message: %@", _unhandledMessageBuffer);
        [_unhandledMessageBuffer enumerateKeysAndObjectsUsingBlock: ^(id key, id obj, BOOL *stop)
         {
             NSDictionary *messageAndFromID = (NSDictionary*)obj;
             NSString *fromID = (NSString*)key;
             
             if(_inCallout){
                 _shadowHandleAllPendingMessages = YES;
             }
             else{
                 __block CFErrorRef cf_error = NULL;
                 __block HandleIDSMessageReason success = kHandleIDSMessageSuccess;
                 _handleAllPendingMessages = YES;
                 
                 [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {
                     success  = SOSCCHandleIDSMessage(((__bridge CFDictionaryRef)messageAndFromID), &cf_error);
                     
                     if(success == kHandleIDSMessageLocked){
                         secdebug("IDS Transport", "cannot handle messages when locked, error:%@", cf_error);
                         [_unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
                         
                         _lastSyncTime = dispatch_time(DISPATCH_TIME_NOW, 0);
                         _deadline = dispatch_time(DISPATCH_TIME_NOW, kAttemptFlushBufferInterval);
                         //set timer
                         [self scheduleSyncRequestTimer];
                         return NULL;
                     }
                     else if(success == kHandleIDSMessageNotReady){
                         secdebug("IDS Transport", "not ready to handle message, error:%@", cf_error);
                         [_unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
                         _lastSyncTime = dispatch_time(DISPATCH_TIME_NOW, 0);
                         _deadline = dispatch_time(DISPATCH_TIME_NOW, kAttemptFlushBufferInterval);
                         //set timer
                         [self scheduleSyncRequestTimer];
                         return NULL;
                     }
                     else if(success == kHandleIDSMessageOtherFail){
                         secdebug("IDS Transport", "not ready to handle message, error:%@", cf_error);
                         [_unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
                         _lastSyncTime = dispatch_time(DISPATCH_TIME_NOW, 0);
                         _deadline = dispatch_time(DISPATCH_TIME_NOW, kAttemptFlushBufferInterval);
                         //set timer
                         [self scheduleSyncRequestTimer];
                         return NULL;
                     }
                     else{
                         secdebug("IDS Transport", "IDSProxy handled this message! %@", messageAndFromID);
                         _syncTimerScheduled = NO;
                         return (NSMutableDictionary*)messageAndFromID;
                     }
                 }];
             }
         }];
    }
}

- (void)service:(IDSService *)service account:(IDSAccount *)account incomingMessage:(NSDictionary *)message fromID:(NSString *)fromID context:(IDSMessageContext *)context;
{
    secdebug("IDS Transport", "IDSKeychainSyncingProxy handling this message sent over IDS%@", message);
    NSString *dataKey = [ NSString stringWithUTF8String: kMessageKeyIDSDataMessage ];
    NSString *deviceIDKey = [ NSString stringWithUTF8String: kMessageKeyDeviceID ];
    NSString *ID = nil;
    uint32_t operationType;
    bool hadError = false;
    CFStringRef errorMessage = NULL;
    __block NSString* operation = nil;
    NSString *messageString = nil;
    __block NSData *messageData = nil;
    
    NSArray *devices = [_service devices];
    for(NSUInteger i = 0; i < [ devices count ]; i++){
        IDSDevice *device = devices[i];
        if( [(IDSCopyIDForDevice(device)) containsString: fromID] == YES){
            ID = device.uniqueID;
            break;
        }
    }
    require_action_quiet(ID, fail, hadError = true; errorMessage = CFSTR("require the sender's device ID"));
    require_action_quiet([message count] == 1, fail, hadError = true; errorMessage = CFSTR("message contained too many objects"););
    
    [message enumerateKeysAndObjectsUsingBlock: ^(id key, id obj, BOOL *stop){
        operation = (NSString*)key;
        messageData = (NSData*)obj;
    }];
    
    operationType = [operation intValue];
    
    switch(operationType){
        case kIDSPeerAvailabilityDone:
        {
            secdebug("ids transport", "received availability done!");
            notify_post(kSOSCCPeerAvailable);
            break;
        }
        case kIDSEndPingTestMessage:
            secdebug("ids transport", "received pong message from other device: %@, ping test PASSED", ID);
            break;
        case kIDSSendOneMessage:
            secdebug("ids transport","received ping test message, dropping on the floor now");
            break;
            
        case kIDSPeerAvailability:
        case kIDSStartPingTestMessage:
        {
            char* messageCharS;
            if(operationType == kIDSPeerAvailability){
                secdebug("ids transport", "Received Availability Message!");
                asprintf(&messageCharS, "%d",kIDSPeerAvailabilityDone);
            }
            else{
                secdebug("ids transport", "Received PingTest Message!");
                asprintf(&messageCharS, "%d", kIDSEndPingTestMessage);
            }
     
            NSString *operationString = [[NSString alloc] initWithUTF8String:messageCharS];
            messageString = @"peer availability check finished";
            NSDictionary* messsageDictionary = @{operationString : messageString};
            
            NSError *localError = NULL;
            [self sendIDSMessage:messsageDictionary name:ID peer:@"me" error:&localError];
            free(messageCharS);
            
            break;
            
        }
        default:
        {
            NSDictionary *messageAndFromID = @{dataKey : messageData, deviceIDKey: ID};
            if(_isLocked){
                //hang on to the message and set the retry deadline
                [_unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
                _deadline = dispatch_time(DISPATCH_TIME_NOW, kMaxMessageRetryDelay);
            }
            else{
                __block CFErrorRef cf_error = NULL;
                __block HandleIDSMessageReason success = kHandleIDSMessageSuccess;
                _handleAllPendingMessages = YES;
                
                [self sendKeysCallout:^NSMutableDictionary *(NSMutableDictionary *pending, NSError** error) {
                    
                    success  = SOSCCHandleIDSMessage(((__bridge CFDictionaryRef)messageAndFromID), &cf_error);
                    
                    if(success == kHandleIDSMessageLocked){
                        secdebug("IDS Transport", "cannot handle messages when locked, error:%@", cf_error);
                        [_unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
                        
                        _lastSyncTime = dispatch_time(DISPATCH_TIME_NOW, 0);
                        _deadline = dispatch_time(DISPATCH_TIME_NOW, kAttemptFlushBufferInterval);
                        //set timer
                        [self scheduleSyncRequestTimer];
                        return NULL;
                    }
                    else if(success == kHandleIDSMessageNotReady){
                        secdebug("IDS Transport", "not ready to handle message, error:%@", cf_error);
                        [_unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
                        _lastSyncTime = dispatch_time(DISPATCH_TIME_NOW, 0);
                        _deadline = dispatch_time(DISPATCH_TIME_NOW, kAttemptFlushBufferInterval);
                        //set timer
                        [self scheduleSyncRequestTimer];
                        return NULL;
                    }
                    else if(success == kHandleIDSMessageOtherFail){
                        secdebug("IDS Transport", "not ready to handle message, error:%@", cf_error);
                        [_unhandledMessageBuffer setObject: messageAndFromID forKey: fromID];
                        _lastSyncTime = dispatch_time(DISPATCH_TIME_NOW, 0);
                        _deadline = dispatch_time(DISPATCH_TIME_NOW, kAttemptFlushBufferInterval);
                        //set timer
                        [self scheduleSyncRequestTimer];
                        return NULL;
                    }
                    else{
                        secdebug("IDS Transport", "IDSProxy handled this message! %@", messageAndFromID);
                        return (NSMutableDictionary*)messageAndFromID;
                    }
                }];
                CFReleaseSafe(cf_error);
            }
            break;
        }
    }
fail:
    if(hadError)
        secerror("error:%@", errorMessage);
}

@end

