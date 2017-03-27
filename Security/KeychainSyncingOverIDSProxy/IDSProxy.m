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
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <AssertMacros.h>

#import "IDSProxy.h"
#import "KeychainSyncingOverIDSProxy+ReceiveMessage.h"
#import "KeychainSyncingOverIDSProxy+SendMessage.h"
#import "KeychainSyncingOverIDSProxy+Throttle.h"
#import "IDSPersistentState.h"

#define kSecServerKeychainChangedNotification "com.apple.security.keychainchanged"
#define kSecServerPeerInfoAvailable "com.apple.security.fpiAvailable"

#define IDSServiceNameKeychainSync "com.apple.private.alloy.keychainsync"
static NSString *kMonitorState = @"MonitorState";
static NSString *kExportUnhandledMessages = @"UnhandledMessages";
static NSString *kMessagesInFlight = @"MessagesInFlight";
static const char *kStreamName = "com.apple.notifyd.matching";
static NSString *const kIDSMessageRecipientPeerID = @"RecipientPeerID";
static NSString *const kIDSMessageRecipientDeviceID = @"RecipientDeviceID";
static NSString *const kIDSMessageUseACKModel = @"UsesAckModel";

NSString *const IDSSendMessageOptionForceEncryptionOffKey = @"IDSSendMessageOptionForceEncryptionOff";
static const int64_t kRetryTimerLeeway = (NSEC_PER_MSEC * 250);      // 250ms leeway for handling unhandled messages.
static const int64_t kMinMessageRetryDelay = (NSEC_PER_SEC * 8);


CFIndex kSOSErrorPeerNotFound = 1032;
CFIndex SECD_RUN_AS_ROOT_ERROR = 1041;

#define IDSPROXYSCOPE "IDSProxy"

@implementation KeychainSyncingOverIDSProxy

@synthesize deviceID = deviceID;
@synthesize deviceIDFromAuthToken = deviceIDFromAuthToken;

+   (KeychainSyncingOverIDSProxy *) idsProxy
{
    static KeychainSyncingOverIDSProxy *idsProxy;
    if (!idsProxy) {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            idsProxy = [[self alloc] init];
        });
    }
    return idsProxy;
}

-(NSDictionary*) exportState
{
    return @{ kMonitorState:_monitor,
              kExportUnhandledMessages:_unhandledMessageBuffer,
              kMessagesInFlight:_messagesInFlight
              };

}

-(NSDictionary*) retrievePendingMessages
{
    return [KeychainSyncingOverIDSProxy idsProxy].messagesInFlight;
}

- (void)persistState
{
    [KeychainSyncingOverIDSProxyPersistentState setUnhandledMessages:[self exportState]];
}

- (void) sendPersistedMessagesAgain
{
    NSMutableDictionary *copy = [NSMutableDictionary dictionaryWithDictionary:_messagesInFlight];

    if(copy && [copy count] > 0){
        [copy enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
            NSDictionary* idsMessage = (NSDictionary*)obj;
            NSString *uniqueMessageID = (NSString*)key;

            NSString *peerID = (NSString*)[idsMessage objectForKey:kIDSMessageRecipientPeerID];
            if(!peerID){
                [self->_messagesInFlight removeObjectForKey:key];
                return;
            }
            NSString *ID = (NSString*)[idsMessage objectForKey:kIDSMessageRecipientDeviceID];
            if(!ID){
                [self->_messagesInFlight removeObjectForKey:key];
                return;
            }

            [self->_messagesInFlight removeObjectForKey:key];
            secnotice("IDS Transport", "sending this message: %@", idsMessage);
            if([self sendIDSMessage:idsMessage name:ID peer:peerID]){
                NSString *useAckModel = [idsMessage objectForKey:kIDSMessageUseACKModel];
                if([useAckModel compare:@"YES"] == NSOrderedSame){
                    secnotice("IDS Transport", "setting timer!");
                    [self setMessageTimer:uniqueMessageID deviceID:ID message:idsMessage];
                }
            }
        }];
    }
}

- (void) importIDSState: (NSMutableDictionary*) state
{
    _unhandledMessageBuffer = state[kExportUnhandledMessages];
    if(!_unhandledMessageBuffer)
        _unhandledMessageBuffer = [NSMutableDictionary dictionary];
    
    _monitor = state[kMonitorState];
    if(_monitor == nil)
        _monitor = [NSMutableDictionary dictionary];

    _messagesInFlight = state[kMessagesInFlight];
    if(_messagesInFlight == nil)
        _messagesInFlight = [NSMutableDictionary dictionary];
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
        _pingTimers = [ [NSMutableDictionary alloc] initWithCapacity: 0];
        _messagesInFlight = [ [NSMutableDictionary alloc] initWithCapacity: 0];
        deviceIDFromAuthToken = [ [NSMutableDictionary alloc] initWithCapacity: 0];
        _peerNextSendCache = [ [NSMutableDictionary alloc] initWithCapacity: 0];

        _listOfDevices = [[NSMutableArray alloc] initWithCapacity:0];
        _isSecDRunningAsRoot = false;
        _doesSecDHavePeer = true;
        secdebug(IDSPROXYSCOPE, "%@ done", self);
        
        [self doIDSInitialization];
        if(_isIDSInitDone)
            [self doSetIDSDeviceID];
    
        
        // Register for lock state changes
        xpc_set_event_stream_handler(kStreamName, dispatch_get_main_queue(),
                                     ^(xpc_object_t notification){
                                         [self streamEvent:notification];
                                     });

        _retryTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
        dispatch_source_set_timer(_retryTimer, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, kRetryTimerLeeway);
        dispatch_source_set_event_handler(_retryTimer, ^{
            [self timerFired];
        });
        dispatch_resume(_retryTimer);
        [self importIDSState: [KeychainSyncingOverIDSProxyPersistentState idsState]];

        if([_messagesInFlight count ] > 0)
            _sendRestoredMessages = true;
        int notificationToken;
        notify_register_dispatch(kSecServerKeychainChangedNotification, &notificationToken, dispatch_get_main_queue(),
                                 ^ (int token __unused)
                                 {
                                     secinfo("backoff", "keychain changed, wiping backoff monitor state");
                                     self->_monitor = [NSMutableDictionary dictionary];
                                 });
        int peerInfo;
        notify_register_dispatch(kSecServerPeerInfoAvailable, &peerInfo, dispatch_get_main_queue(),
                                 ^ (int token __unused)
                                 {
                                     secinfo("IDS Transport", "secd has a peer info");
                                     if(self->_doesSecDHavePeer == false){
                                         self->_doesSecDHavePeer = true;
                                         [self doSetIDSDeviceID];
                                     }
                                 });

        [self updateUnlockedSinceBoot];
        [self updateIsLocked];
        if (!_isLocked)
            [self keybagDidUnlock];

        
    }
    return self;
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

- (void) keybagDidLock
{
    secnotice("IDS Transport", "%@ locking!", self);
}

- (void) keybagDidUnlock
{
    secnotice("IDS Transport", "%@ unlocking!", self);
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
    secerror("updateIsLocked: %d", _isLocked);
    if (!_isLocked)
        _unlockedSinceBoot = YES;
    return YES;
}

- (void) keybagStateChange
{
    os_activity_initiate("keybagStateChanged", OS_ACTIVITY_FLAG_DEFAULT, ^{
        secerror("keybagStateChange! was locked: %d", self->_isLocked);
        BOOL wasLocked = self->_isLocked;
        if ([self updateIsLocked]) {
            if (wasLocked == self->_isLocked)
                secdebug("IDS Transport", "%@ still %s ignoring", self, self->_isLocked ? "locked" : "unlocked");
            else if (self->_isLocked)
                [self keybagDidLock];
            else
                [self keybagDidUnlock];
        }
    });
}


- (void)timerFired
{
    if(_unhandledMessageBuffer)
        secnotice("IDS Transport", "%@ attempting to hand unhandled messages to securityd, here is our message queue: %@", self, _unhandledMessageBuffer);
   
    if(_isLocked)
        _retryTimerScheduled = NO;
    else if([_unhandledMessageBuffer count] == 0)
        _retryTimerScheduled = NO;
    else if (_retryTimerScheduled && !_isLocked)
        [self handleAllPendingMessage];
    else
        [[KeychainSyncingOverIDSProxy idsProxy] scheduleRetryRequestTimer];

}

- (void)scheduleRetryRequestTimer
{
    secnotice("IDS Transport", "scheduling unhandled messages timer");
    dispatch_source_set_timer(_retryTimer, dispatch_time(DISPATCH_TIME_NOW, kMinMessageRetryDelay), DISPATCH_TIME_FOREVER, kRetryTimerLeeway);
    _retryTimerScheduled = YES;
}

- (void)doIDSInitialization
{

    dispatch_async(self.calloutQueue, ^{
        secnotice("IDS Transport", "doIDSInitialization!");
        
        self->_service = [[IDSService alloc] initWithService: @IDSServiceNameKeychainSync];
        
        if( self->_service == nil ){
            self->_isIDSInitDone = false;
            secerror("Could not create ids service");
        }
        else{
            secnotice("IDS Transport", "IDS Transport Successfully set up IDS!");
            [self->_service addDelegate:self queue: dispatch_get_main_queue()];
            
            self->_isIDSInitDone = true;
            if(self->_isSecDRunningAsRoot == false)
                [self doSetIDSDeviceID];
            
            NSArray *ListOfIDSDevices = [self->_service devices];
            self.listOfDevices = ListOfIDSDevices;
            
            for(NSUInteger i = 0; i < [ self.listOfDevices count ]; i++){
                IDSDevice *device = self.listOfDevices[i];
                NSString *authToken = IDSCopyIDForDevice(device);
                [self.deviceIDFromAuthToken setObject:device.uniqueID forKey:authToken];
            }
        }
    });
}

- (void) doSetIDSDeviceID
{
    NSInteger code = 0;
    NSString *errorMessage = nil;

    if(!_isIDSInitDone){
        [self doIDSInitialization];
    }
    _setIDSDeviceID = YES;

    if(_isSecDRunningAsRoot != false)
    {
        errorMessage = @"cannot set IDS device ID, secd is running as root";
        code = SECD_RUN_AS_ROOT_ERROR;
        secerror("Setting device ID error: %@, code: %ld", errorMessage, (long)code);

    }
    else if(_doesSecDHavePeer != true)
    {
        errorMessage = @"cannot set IDS deviceID, secd does not have a full peer info for account";
        code = kSOSErrorPeerNotFound;
        secerror("Setting device ID error: %@, code: %ld", errorMessage, (long)code);

    }
    else if(!_isIDSInitDone){
        errorMessage = @"KeychainSyncingOverIDSProxy can't set up the IDS service";
        code = kSecIDSErrorNotRegistered;
        secerror("Setting device ID error: %@, code: %ld", errorMessage, (long)code);
    }
    else if(_isLocked){
        errorMessage = @"KeychainSyncingOverIDSProxy can't set device ID, device is locked";
        code = kSecIDSErrorDeviceIsLocked;
        secerror("Setting device ID error: %@, code: %ld", errorMessage, (long)code);
    }

    else{
        [self calloutWith:^(NSMutableDictionary *pending, bool handlePendingMesssages, bool doSetDeviceID, dispatch_queue_t queue, void(^done)(NSMutableDictionary *, bool, bool)) {
            CFErrorRef localError = NULL;
            bool handledSettingID = false;
            NSString *ID = IDSCopyLocalDeviceUniqueID();
            self->deviceID = ID;

            if(ID){
                handledSettingID = SOSCCSetDeviceID((__bridge CFStringRef) ID, &localError);

                if(!handledSettingID && localError != NULL){
                    if(CFErrorGetCode(localError) == SECD_RUN_AS_ROOT_ERROR){
                        secerror("SETTING RUN AS ROOT ERROR: %@", localError);
                        self->_isSecDRunningAsRoot = true;
                    }
                    else if (CFErrorIsMalfunctioningKeybagError(localError)) {
                        secnotice("IDS Transport", "system is unavailable, cannot set device ID, error: %@", localError);
                        self->_isLocked = true;
                    }
                    else if (CFErrorGetCode(localError) == kSOSErrorPeerNotFound && CFStringCompare(CFErrorGetDomain(localError), kSOSErrorDomain, 0) == 0){
                        secnotice("IDS Transport","securityd does not have a peer yet , error: %@", localError);
                        self->_doesSecDHavePeer = false;
                    }
                }
                else
                    self->_setIDSDeviceID = NO;

                CFReleaseNull(localError);
                dispatch_async(queue, ^{
                    done(nil, NO, YES);
                });
            } else {
                dispatch_async(queue, ^{
                    done(nil, NO, NO);
                });
            }
            if(errorMessage != nil){
                secerror("Setting device ID error: KeychainSyncingOverIDSProxy could not retrieve device ID from keychain, code: %ld", (long)kSecIDSErrorNoDeviceID);
            }
        }];
    }
}

- (void) calloutWith: (void(^)(NSMutableDictionary *pending, bool handlePendingMesssages, bool doSetDeviceID, dispatch_queue_t queue, void(^done)(NSMutableDictionary *handledMessages, bool handledPendingMessage, bool handledSettingDeviceID))) callout
{
    // In KeychainSyncingOverIDSProxy serial queue
    dispatch_queue_t idsproxy_queue = dispatch_get_main_queue();

    // dispatch_get_global_queue - well-known global concurrent queue
    // dispatch_get_main_queue   - default queue that is bound to the main thread
    xpc_transaction_begin();
    dispatch_async(_calloutQueue, ^{
        __block NSMutableDictionary *myPending;
        __block bool myHandlePendingMessage;
        __block bool myDoSetDeviceID;
        __block bool wasLocked;
        dispatch_sync(idsproxy_queue, ^{
            myPending = [self->_unhandledMessageBuffer copy];
            myHandlePendingMessage = self->_handleAllPendingMessages;
            myDoSetDeviceID = self->_setIDSDeviceID;
            wasLocked = self->_isLocked;
            
            self->_inCallout = YES;

            self->_shadowHandleAllPendingMessages = NO;
        });
        
        callout(myPending, myHandlePendingMessage, myDoSetDeviceID, idsproxy_queue, ^(NSMutableDictionary *handledMessages, bool handledPendingMessage, bool handledSetDeviceID) {
            secdebug("event", "%@ %s%s before callout handled: %s%s", self, myHandlePendingMessage ? "P" : "p", myDoSetDeviceID ? "D" : "d", handledPendingMessage ? "H" : "h", handledSetDeviceID ? "I" : "i");
            
            // In IDSKeychainSyncingProxy's serial queue
            self->_inCallout = NO;
            
            // Update setting device id
            self->_setIDSDeviceID = ((myDoSetDeviceID && !handledSetDeviceID));

            self->_shadowDoSetIDSDeviceID = NO;
            
            xpc_transaction_end();
        });
    });
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

NSString* createErrorString(NSString* format, ...)
{
    va_list va;
    va_start(va, format);
    NSString* errorString = ([[NSString alloc] initWithFormat:format arguments:va]);
    va_end(va);
    return errorString;
    
}

@end
