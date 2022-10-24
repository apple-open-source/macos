//
//  NotifyStubs.m
//  IOKitPMUnitTests
//
//  Created by Faramola Isiaka on 8/2/21.
//

#import <Foundation/Foundation.h>
#include "NotifyStubs.h"

@interface FakeNotification : NSObject {
    const char* _name;
    int _token;
    notify_handler_t _handler;
    uint64_t _state;
}
@end

@implementation FakeNotification

/**
 * @brief Initializer of a FakeNotification object
 * @result a new FakeNotification Object
 */
- (instancetype)init
{
  self = [super init];
  if (self) {
      _name = NULL;
      _token = 0;
      _handler = nil;
      _state = 0;
  }
  return self;
}

/**
 * @brief Gets the name of a FakeNotification.
 * @result the notification name
 */
- (const char*) getNotificationName
{
    return _name;
}

/**
 * @brief Gets the registration token a FakeNotification.
 * @result the registration token
 */
- (int) getToken
{
    return _token;
}

/**
 * @brief Gets the state of a FakeNotification.
 * @result the 64 bit unsigned integer representing the state
 */
- (uint64_t) getState
{
    return _state;
}

/**
 * @brief Updates the name of a FakeNotification.
 * @param name the notification name
 */
- (void) callHandler:(int) token
{
    _handler(token);
}

/**
 * @brief Updates the name of a FakeNotification.
 * @param name the notification name
 */
- (void)updateName:(const char *) name
{
    _name = name;
}

/**
 * @brief Updates the registration token of a FakeNotification.
 * @param token the new registration token
 */
- (void)updateToken:(int) token
{
    _token = token;
}

/**
 * @brief Updates the state of a FakeNotification.
 * @param state the new 64 bit state
 */
- (void)updateState:(uint64_t) state
{
    _state = state;
}

/**
 * @brief Updates the handler for a FakeNotification.
 * @param handler the new handler
 */
- (void)updateHandler:(notify_handler_t) handler
{
    _handler = handler;
}

@end

NSMutableDictionary<NSValue*, FakeNotification* > *fakeNotifications;

/**
 * @brief This is a stub for requesting notification delivery to a dispatch queue which creates an associated FakeNotification object.
 * @param name the notification name
 * @param out_token the registration token
 * @param queue the dispatch queue to which the block is submitted, this parameter is unused
 * @param handler handler invoked after receiving a notification
 * @result the notify status
 */
uint32_t notify_register_dispatch(const char *name, int *out_token, __unused dispatch_queue_t queue, notify_handler_t handler)
{
    if(!fakeNotifications)
    {
        fakeNotifications = [[NSMutableDictionary alloc] init];
    }
    
    FakeNotification* fakeNotification = [[FakeNotification alloc] init];
    *out_token = (int) [fakeNotifications count] + 1;
    [fakeNotification updateName:name];
    [fakeNotification updateToken:*out_token];
    [fakeNotification updateHandler:handler];
    NSValue* key = [NSValue value:out_token withObjCType:@encode(int*)];
    [fakeNotifications setObject:fakeNotification forKey:key];
    return NOTIFY_STATUS_OK;
}

/**
 * @brief This is a stub for registration token be used with notify_check(), but no active notifications will be delivered.
 * @param name the notification name
 * @param out_token pointer to the registration token
 * @result Returns true if the value is a valid token, false otherwise.
 */
uint32_t notify_register_check(const char *name, int *out_token)
{
    return notify_register_dispatch(name, out_token, NULL, nil);
}

/**
 * @brief This is a stub for checking if a token is valid via an associated FakeNotification object.
 * @param val integer value
 * @result Returns true if the value is a valid token, false otherwise.
 */
bool notify_is_valid_token(int val)
{
    NSValue* key = [NSValue value:&val withObjCType:@encode(int*)];
    FakeNotification *fakeNotification = [fakeNotifications objectForKey:key];
    return fakeNotification != nil;
}

/**
 * @brief This is a stub for getting the 64-bit integer state value via an associated FakeNotification object..
 * @param token the notification name
 * @param state64 pointer to 64-bit unsigned integer value
 * @result Returns status.
 */
uint32_t notify_get_state(int token, uint64_t *state64)
{
    NSValue* key = [NSValue value:&token withObjCType:@encode(int*)];
    FakeNotification *fakeNotification = [fakeNotifications objectForKey:key];
    if(fakeNotification)
    {
        *state64 = [fakeNotification getState];
        return NOTIFY_STATUS_OK;
    }
    return NOTIFY_STATUS_INVALID_TOKEN;
}

/**
 * @brief This is a stub for setting the 64-bit integer state value from an associated FakeNotification object..
 * @param token the notification name
 * @param state64 64-bit unsigned integer value
 * @result Returns status.
 */
uint32_t notify_set_state(int token, uint64_t state64)
{
    NSValue* key = [NSValue value:&token withObjCType:@encode(int*)];
    FakeNotification *fakeNotification = [fakeNotifications objectForKey:key];
    if(fakeNotification)
    {
        [fakeNotification updateState:state64];
        return NOTIFY_STATUS_OK;
    }
    return NOTIFY_STATUS_INVALID_TOKEN;
}

/**
 * @brief This is a stub for posting a notification for a name via its associated FakeNotification object..
 * @param name the notification name
 * @result Returns status.
 */
uint32_t notify_post(const char *name)
{
    bool posted = false;
    for(id key in fakeNotifications)
    {
        FakeNotification *fakeNotification = [fakeNotifications objectForKey:key];
        if(!strcmp([fakeNotification getNotificationName], name))
        {
            [fakeNotification callHandler:[fakeNotification getToken]];
            posted = true;
        }
    }
    return posted ? NOTIFY_STATUS_OK : NOTIFY_STATUS_INVALID_NAME;
}

/**
 * @brief This is a stub for canceling notifications via its associated FakeNotification object.
 * @param token the notification token
 * @result Returns status.
 */
uint32_t notify_cancel(int token)
{
    NSValue* key = [NSValue value:&token withObjCType:@encode(int*)];
    FakeNotification *fakeNotification = [fakeNotifications objectForKey:key];
    if(fakeNotification)
    {
        [fakeNotifications removeObjectForKey:key];
        return NOTIFY_STATUS_OK;
    }
    return NOTIFY_STATUS_INVALID_TOKEN;
}

/**
 * @brief Teardown Function for NotifyStubs
 */
void NotifyStubsTeardown(void)
{
    fakeNotifications = nil;
}
