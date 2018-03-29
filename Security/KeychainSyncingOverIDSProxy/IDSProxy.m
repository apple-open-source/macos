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
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <AssertMacros.h>

#import "IDSProxy.h"
#import "KeychainSyncingOverIDSProxy+ReceiveMessage.h"
#import "KeychainSyncingOverIDSProxy+SendMessage.h"
#import "IDSPersistentState.h"

#define kSecServerKeychainChangedNotification "com.apple.security.keychainchanged"
#define kSecServerPeerInfoAvailable "com.apple.security.fpiAvailable"

#define IDSServiceNameKeychainSync "com.apple.private.alloy.keychainsync"
static NSString *kMonitorState = @"MonitorState";
static NSString *kExportUnhandledMessages = @"UnhandledMessages";
static NSString *kMessagesInFlight = @"MessagesInFlight";
static const char *kStreamName = "com.apple.notifyd.matching";
static NSString *const kIDSMessageUseACKModel = @"UsesAckModel";
static NSString *const kIDSNumberOfFragments = @"NumberOfIDSMessageFragments";
static NSString *const kIDSFragmentIndex = @"kFragmentIndex";

static NSString *const kOutgoingMessages = @"IDS Outgoing Messages";
static NSString *const kIncomingMessages = @"IDS Incoming Messages";

NSString *const IDSSendMessageOptionForceEncryptionOffKey = @"IDSSendMessageOptionForceEncryptionOff";
static const int64_t kRetryTimerLeeway = (NSEC_PER_MSEC * 250);      // 250ms leeway for handling unhandled messages.
static const int64_t kMinMessageRetryDelay = (NSEC_PER_SEC * 8);

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
    return @{ kMonitorState:self.monitor,
              kExportUnhandledMessages:self.unhandledMessageBuffer,
              kMessagesInFlight:self.messagesInFlight
              };

}

-(NSDictionary*) retrievePendingMessages
{
    NSDictionary * __block messages;
    dispatch_sync(self.dataQueue, ^{
        messages = [[KeychainSyncingOverIDSProxy idsProxy].messagesInFlight copy];
    });
    return messages;
}

@synthesize unhandledMessageBuffer = _unhandledMessageBuffer;

- (NSMutableDictionary *) unhandledMessageBuffer {
    dispatch_assert_queue(self.dataQueue);
    return _unhandledMessageBuffer;
}

- (void) setUnhandledMessageBuffer:(NSMutableDictionary *)unhandledMessageBuffer {
    dispatch_assert_queue(self.dataQueue);
    _unhandledMessageBuffer = unhandledMessageBuffer;
}

@ synthesize messagesInFlight = _messagesInFlight;

- (NSMutableDictionary *) messagesInFlight {
    dispatch_assert_queue(self.dataQueue);
    return _messagesInFlight;
}

- (void) setMessagesInFlight:(NSMutableDictionary *)messagesInFlight {
    dispatch_assert_queue(self.dataQueue);
    _messagesInFlight = messagesInFlight;
}

@synthesize monitor = _monitor;

- (NSMutableDictionary *) monitor {
    dispatch_assert_queue(self.dataQueue);
    return _monitor;
}

- (void) setMonitor:(NSMutableDictionary *)monitor {
    dispatch_assert_queue(self.dataQueue);
    _monitor = monitor;
}

- (void) persistState
{
    dispatch_sync(self.dataQueue, ^{
        [KeychainSyncingOverIDSProxyPersistentState setUnhandledMessages:[self exportState]];
    });
}

- (BOOL) haveMessagesInFlight {
    BOOL __block inFlight = NO;
    dispatch_sync(self.dataQueue, ^{
        inFlight = [self.messagesInFlight count] > 0;
    });
    return inFlight;
}

- (void) sendPersistedMessagesAgain
{
    NSMutableDictionary * __block copy;
    
    dispatch_sync(self.dataQueue, ^{
        copy = [NSMutableDictionary dictionaryWithDictionary:self.messagesInFlight];
    });

    if(copy && [copy count] > 0){
        [copy enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
            NSDictionary* idsMessage = (NSDictionary*)obj;
            NSString *uniqueMessageID = (NSString*)key;

            NSString *peerID = (NSString*)[idsMessage objectForKey:(__bridge NSString*)kIDSMessageRecipientPeerID];
            NSString *ID = (NSString*)[idsMessage objectForKey:(__bridge NSString*)kIDSMessageRecipientDeviceID];
            NSString *senderDeviceID = (NSString*)[idsMessage objectForKey:(__bridge NSString*)kIDSMessageSenderDeviceID];
            dispatch_sync(self.dataQueue, ^{
                [self.messagesInFlight removeObjectForKey:key];
            });
            
            if (!peerID || !ID) {
                return;
            }
            [self printMessage:idsMessage state:@"sending persisted message"];

            if([self sendIDSMessage:idsMessage name:ID peer:peerID senderDeviceID:senderDeviceID]){
                NSString *useAckModel = [idsMessage objectForKey:kIDSMessageUseACKModel];
                if([useAckModel compare:@"YES"] == NSOrderedSame && [KeychainSyncingOverIDSProxy idsProxy].allowKVSFallBack){
                    secnotice("IDS Transport", "setting timer!");
                    [self setMessageTimer:uniqueMessageID deviceID:ID message:idsMessage];
                }
            }
        }];
    }
}

- (id)init
{
    if (self = [super init])
    {
        secnotice("event", "%@ start", self);
        
        _isIDSInitDone = false;
        _service = nil;
        _calloutQueue = dispatch_queue_create("IDSCallout", DISPATCH_QUEUE_SERIAL);
        _pingQueue = dispatch_queue_create("PingQueue", DISPATCH_QUEUE_SERIAL);
        _dataQueue = dispatch_queue_create("DataQueue", DISPATCH_QUEUE_SERIAL);
        _pingTimers = [[NSMutableDictionary alloc] init];
        deviceIDFromAuthToken = [[NSMutableDictionary alloc] init];
        _peerNextSendCache = [[NSMutableDictionary alloc] init];
        _counterValues = [[NSMutableDictionary alloc] init];
        _outgoingMessages = 0;
        _incomingMessages = 0;
        _isSecDRunningAsRoot = false;
        _doesSecDHavePeer = true;
        _allowKVSFallBack = true;
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
        NSMutableDictionary *state = [KeychainSyncingOverIDSProxyPersistentState idsState];

        _unhandledMessageBuffer = state[kExportUnhandledMessages];
        if (!_unhandledMessageBuffer) {
            _unhandledMessageBuffer = [NSMutableDictionary dictionary];
        }
        _messagesInFlight = state[kMessagesInFlight];
        if(_messagesInFlight == nil) {
            _messagesInFlight = [NSMutableDictionary dictionary];
        }
        _monitor = state[kMonitorState];
        if(_monitor == nil) {
            _monitor = [NSMutableDictionary dictionary];
        }
        
        if([_messagesInFlight count ] > 0)
            _sendRestoredMessages = true;
        int notificationToken;
        notify_register_dispatch(kSecServerKeychainChangedNotification, &notificationToken, self.dataQueue,
                                 ^ (int token __unused)
                                 {
                                     secinfo("backoff", "keychain changed, wiping backoff monitor state");
                                     self.monitor = [NSMutableDictionary dictionary];
                                 });
        int peerInfo;
        notify_register_dispatch(kSecServerPeerInfoAvailable, &peerInfo, dispatch_get_main_queue(),
                                 ^ (int token __unused)
                                 {
                                     secinfo("IDS Transport", "secd has a peer info");
                                     if(self.doesSecDHavePeer == false){
                                         self.doesSecDHavePeer = true;
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
    NSUInteger __block messagecount = 0;
    dispatch_sync(self.dataQueue, ^{
        if(self.unhandledMessageBuffer) {
            secnotice("IDS Transport", "%@ attempting to hand unhandled messages to securityd, here is our message queue: %@", self, self.unhandledMessageBuffer);
            messagecount = [self.unhandledMessageBuffer count];
        }
    });
        
    if(self.isLocked) {
        self.retryTimerScheduled = NO;
    } else if(messagecount == 0) {
        self.retryTimerScheduled = NO;
    } else if (self.retryTimerScheduled && !self.isLocked) {
        [self handleAllPendingMessage];
    } else {
        [[KeychainSyncingOverIDSProxy idsProxy] scheduleRetryRequestTimer];
    }
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
            dispatch_sync(self.dataQueue, ^{
                myPending = [self.unhandledMessageBuffer copy];
            });
            myHandlePendingMessage = self.handleAllPendingMessages;
            myDoSetDeviceID = self.setIDSDeviceID;
            wasLocked = self.isLocked;
            
            self.inCallout = YES;

            self.shadowHandleAllPendingMessages = NO;
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

- (NSDictionary*) collectStats{
    [_counterValues setObject:[NSNumber numberWithInteger:[KeychainSyncingOverIDSProxy idsProxy].outgoingMessages] forKey:kOutgoingMessages];
    [_counterValues setObject:[NSNumber numberWithInteger:[KeychainSyncingOverIDSProxy idsProxy].incomingMessages] forKey:kIncomingMessages];
    
    return _counterValues;
}

-(void) printMessage:(NSDictionary*) message state:(NSString*)state
{
    secnotice("IDS Transport", "message state: %@", state);
    secnotice("IDS Transport", "msg id: %@", message[(__bridge NSString*)kIDSMessageUniqueID]);
    secnotice("IDS Transport", "receiver ids device id: %@", message[(__bridge NSString*)kIDSMessageRecipientDeviceID]);
    secnotice("IDS Transport", "sender device id: %@", message[(__bridge NSString*)kIDSMessageSenderDeviceID]);
    secnotice("IDS Transport", "receiver peer id: %@", message[(__bridge NSString*)kIDSMessageRecipientPeerID]);
    secnotice("IDS Transport", "fragment index: %@", (NSNumber*)message[kIDSFragmentIndex]);
    secnotice("IDS Transport", "total number of fragments: %@", (NSNumber*)message[kIDSNumberOfFragments]);
    secnotice("IDS Transport", "%@ data: %@", state, message[(__bridge NSString*)kIDSMessageToSendKey]);
}

@end
