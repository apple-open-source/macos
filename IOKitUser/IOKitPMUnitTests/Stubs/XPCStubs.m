//
//  XPCStubs.m
//  IOKitPMUnitTests
//
//  Created by Faramola Isiaka on 8/2/21.
//

#import <Foundation/Foundation.h>
#include "XPCStubs.h"


@interface FakeReply:NSObject {
    xpc_object_t _reply;
    char* _connectionName;
}
@end

@implementation FakeReply

/**
 * @brief Initializer of a FakeReply object
 * @param reply the real reply to be delivered to a connection\
 * @param connectionName name of the connection that this reply is for
 * @result a new FakeReply Object
 */
- (instancetype) initWithParams: (xpc_object_t) reply connectionName: (const char*) connectionName
{
  self = [super init];
  if (self) {
      _reply = reply;
      _connectionName = malloc(sizeof(connectionName));
      strcpy(_connectionName, connectionName);
  }
  return self;
}

/**
 * @brief Gets the reply associated with this FakeReply object
 * @result the reply associated with this FakeReply
 */
- (xpc_object_t) getRealReply
{
    return _reply;
}

/**
 * @brief Gets the name of the connection associated with this FakeReply object
 * @result the name of the connection associated with this FakeReply
 */
- (const char*) getConnectionName
{
    return _connectionName;
}

/**
 * @brief Frees the dynamic memory allocated by the _connectionName attribute of a FakeReply
 */
- (void) freeConnectionName
{
    free(_connectionName);
    _connectionName = NULL;
}
@end

@interface ReceivedMessage:NSObject {
    xpc_object_t _message;
    bool _requiredReply;
    xpc_object_t _deliveredReply;
}
@end

@implementation ReceivedMessage

/**
 * @brief Initializer of a ReceivedMessage object
 */
- (instancetype)init
{
  self = [super init];
  if (self) {
      _message = nil;
      _requiredReply = false;
      _deliveredReply = nil;
  }
  return self;
}

/**
 * @brief Gets the message associated with this ReceivedMessage object
 * @result the message associated with this ReceivedMessage object
 */
- (xpc_object_t) getMessage
{
    return _message;
}

/**
 * @brief Gets the reply delivered in response to the message represented by this ReceivedMessage object
 * @result the delivered reply
 */
- (xpc_object_t) getDeliveredReply
{
    return _deliveredReply;
}

/**
 * @brief Returns whether a ReceivedMessage warrants a reply
 * @result boolean where true means a reply is expected and false means otherwise
 */
- (bool) requiredReply
{
    return _requiredReply;
}

/**
 * @brief Sets the message associated with this ReceivedMessage object
 * @param message the message associated with this ReceivedMessage object
 */
- (void) setMessage: (xpc_object_t) message
{
    _message = message;
}

/**
 * @brief Sets the boolean indicating whether the message associated with this ReceivedMessage object requires a reply
 * @param requiredReply boolean indicating whether the message requries a reply
 */
- (void) setRequiredReply: (bool) requiredReply
{
    _requiredReply = requiredReply;
}

/**
 * @brief Sets the reply delivered in response to the message represented by this ReceivedMessage object
 * @param deliveredReply the delivered reply
 */
- (void) setDeliveredReply: (xpc_object_t) deliveredReply
{
    if(_requiredReply)
    {
        _deliveredReply = deliveredReply;
    }
}

@end

@interface FakeConnection:NSObject {
    xpc_connection_t _connection;
    xpc_handler_t _handler;
    bool _resume;
    NSMutableArray<ReceivedMessage* > *_receivedMessages;
    char* _name;
}
@end

@implementation FakeConnection

/**
 * @brief Initializer of a FakeConnection object
 * @param connection the real connection object that the FakeConnection represents
 * @param name the name of the connection that the Fake Connection represents
 * @result a new FakeReply Object
 */
- (instancetype)initWithParams: (xpc_object_t) connection name:(const char*) name
{
  self = [super init];
  if (self) {
      _connection = connection;
      _handler = nil;
      _resume = false;
      _receivedMessages =  [NSMutableArray new];
      _name = malloc(sizeof(name));
      strcpy(_name, name);
  }
  return self;
}

/**
 * @brief Gets the connection object associated with this FakeConnection object
 * @result the connection object associated with this FakeConnection object
 */
- (xpc_connection_t) getRealConnection
{
    return _connection;
}

/**
 * @brief Gets the name of the connection associated with this FakeConnection object
 * @result the name of the connection associated with this FakeConnection
 */
- (const char*) getName
{
    return _name;
}

/**
 * @brief Gets the messages that have been received for the connection object represented by this FakeConnection.
 * @result the messages that have been received for the connection object represented by this FakeConnection.
 */
- (NSMutableArray<ReceivedMessage* > *) getReceivedMessages
{
    return _receivedMessages;
}

/**
 * @brief Sets the FakeConnection's _resume attribute to true
 */
- (void) makeActive
{
    _resume = true;
}

/**
 * @brief Sets the FakeConnection's _resume attribute to false
 */
- (void) makeInactive
{
    _resume = false;
}

/**
 * @brief Sets the FakeConnection's _resume attribute to false and discards its associated real connection object
 */
- (void) cancelConnection
{
    [self makeInactive];
    _connection = nil;
}

/**
 * @brief Gets the value of the _resume attribute which indicates if a connection is active or not.
 * @result true means the FakeConnection is active and false means otherwise.
 */
- (bool) isActive
{
    return _resume;
}

/**
 * @brief Appends a new ReceivedMessage object to the list of ReceivedMessages for this FakeConnection object
 * @param message  the real message received for the connection associated with this FakeConnection
 * @param requiresReply boolean where true means requires reply and false means otherwise
 */
- (void) addReceivedMessage: (xpc_object_t) message requiresReply:(bool) requiresReply
{
    ReceivedMessage* receivedMessage = [[ReceivedMessage alloc] init];
    [receivedMessage setMessage:message];
    [receivedMessage setRequiredReply:requiresReply];
    [_receivedMessages addObject:receivedMessage];
}

/**
 * @brief Sets the delivered reply for a ReceivedMessage object associated with a particular message
 * @param message  the real message received for the connection associated with this FakeConnection
 * @param deliveredReply  the delivered reply to the real message received
*/
- (void) setDeliveredReply: (xpc_object_t) message deliveredReply:(xpc_object_t) deliveredReply
{
    for(ReceivedMessage* receivedMessage in _receivedMessages)
    {
        if([receivedMessage getMessage] == message)
        {
            [receivedMessage setDeliveredReply:deliveredReply];
        }
    }
}

/**
 * @brief Sets the connection handler for a FakeConnection.
 * @param handler the new handler
 */
- (void) setConnectionHandler: (xpc_handler_t) handler
{
    _handler = handler;
}

/**
 * @brief Calls the connection handler for a FakeConnection .
 * @param message the message that prompted the connection handler being called
 */
- (void) callConnectionHandler: (xpc_object_t) message
{
    _handler(message);
}

/**
 * @brief Frees the _name atrribute of this FakeConnection
 */
- (void) freeName
{
    free(_name);
    _name = NULL;
}

@end

NSMutableDictionary<NSValue*, FakeConnection* > *fakeConnections;
NSMutableArray<FakeReply* > *replyQueue;

/**
 * @brief This is a stub for creating an XPC connection and an associated FakeNotification object.
 * @param name the name of the service with which to connect.
 * @param targetq The GCD queue to which the event handler block will be submitted.
 * @result A new connection object
 */
xpc_connection_t xpc_connection_create(const char * _Nullable name,
                      __unused dispatch_queue_t _Nullable targetq)
{
    xpc_connection_t newConnection = [[NSObject<OS_xpc_object> alloc] init];
    FakeConnection* fakeConnection = [[FakeConnection alloc] initWithParams:newConnection name:name];
    if(!fakeConnections)
    {
        fakeConnections = [[NSMutableDictionary alloc] init];
    }
    [fakeConnections setObject:fakeConnection forKey:[NSValue valueWithNonretainedObject:newConnection]];
    return newConnection;
}

/**
 * @brief This is a stub for creating an XPC connection and an associated FakeNotification object. It uses xpc_connection_create to achieve this
 * @param name the name of the service with which to connect.
 * @param targetq the GCD queue to which the event handler block will be submitted.
 * @param flags additional attributes with which to create the connection
 * @result A new connection object
 */
xpc_connection_t xpc_connection_create_mach_service(const char *name, dispatch_queue_t targetq, uint64_t __unused flags)
{
    return xpc_connection_create(name, targetq);
}

/**
 * @brief This is a stub for setting  the target queue of a connection object
 * @param connection the connection object
 * @param targetq the GCD queue to which the event handler block will be submitted.
 */
void xpc_connection_set_target_queue(__unused xpc_connection_t connection,
                                __unused dispatch_queue_t _Nullable targetq)
{
    return;
}

/**
 * @brief This is a stub for setting the event handler of a connection object and its associated FakeConnection
 * @param connection the connection object
 * @param handler the new event handler
 */
void xpc_connection_set_event_handler(xpc_connection_t connection, xpc_handler_t handler)
{
    NSValue* key = [NSValue valueWithNonretainedObject:connection];
    FakeConnection *fakeConnection = [fakeConnections objectForKey:key];
    if(fakeConnection)
    {
        [fakeConnection setConnectionHandler:handler];
    }
}

/**
 * @brief This is a stub for sending a message over a connection and receiving a reply.
 * @param connection the connection object to send the message to
 * @param message the message to send.
 * @result the received reply
 */
xpc_object_t xpc_connection_send_message_with_reply_sync(xpc_connection_t connection, xpc_object_t message)
{
    NSValue* key = [NSValue valueWithNonretainedObject:connection];
    FakeConnection *fakeConnection = [fakeConnections objectForKey:key];
    if(fakeConnection && [fakeConnection isActive])
    {
        [fakeConnection addReceivedMessage:message requiresReply:true];
        FakeReply* finalFakeReply = nil;
        for(FakeReply* fakeReply in replyQueue)
        {
            if(!strcmp([fakeReply getConnectionName], [fakeConnection getName]))
            {
                finalFakeReply = fakeReply;
            }
        }
        
        if(finalFakeReply)
        {
            xpc_object_t reply = [finalFakeReply getRealReply];
            [fakeConnection setDeliveredReply:message deliveredReply:reply];
            [fakeConnection callConnectionHandler:message];
            [replyQueue removeObject:finalFakeReply];
            return reply;
        }
    }
    return XPC_ERROR_CONNECTION_INVALID;
}

/**
 * @brief This is a stub for sending a message over a connection.
 * @param connection the connection object to send the message to
 * @param message the message to send.
 */
void xpc_connection_send_message(xpc_connection_t connection, xpc_object_t message)
{
    NSValue* key = [NSValue valueWithNonretainedObject:connection];
    FakeConnection *fakeConnection = [fakeConnections objectForKey:key];
    if(fakeConnection && [fakeConnection isActive])
    {
        [fakeConnection addReceivedMessage:message requiresReply:false];
        [fakeConnection callConnectionHandler:message];
    }
}

/**
 * @brief This is a stub for making a connection and its associated FakeConnection active.
 * @param connection the connection object
 */
void xpc_connection_resume(xpc_connection_t connection)

{
    NSValue* key = [NSValue valueWithNonretainedObject:connection];
    FakeConnection *fakeConnection = [fakeConnections objectForKey:key];
    if(fakeConnection && ![fakeConnection isActive])
    {
        [fakeConnection makeActive];
    }
}

/**
 * @brief This is a stub for making a connection and its associated FakeConnection inactive.
 * @param connection the connection object
 */
void xpc_connection_suspend(xpc_connection_t connection)
{
    NSValue* key = [NSValue valueWithNonretainedObject:connection];
    FakeConnection *fakeConnection = [fakeConnections objectForKey:key];
    if(fakeConnection && [fakeConnection isActive])
    {
        [fakeConnection makeInactive];
    }
}

/**
 * @brief This is a stub for canceling a connection and its associated FakeConnection.
 * @param connection the connection object
 */
void xpc_connection_cancel(xpc_connection_t connection)
{
    NSValue* key = [NSValue valueWithNonretainedObject:connection];
    FakeConnection *fakeConnection = [fakeConnections objectForKey:key];
    if(fakeConnection && ![fakeConnection isActive])
    {
        [fakeConnection cancelConnection];
    }
}

/**
 * @brief Adds a new FakeReply to the replyQueue
 * @param connectionName the name of the connection that this reply is for
 * @param reply the reply we want delivered in response to a message
 */
void addReplyForConnection(const char* connectionName, xpc_object_t reply)
{
    FakeReply* fakeReply = [[FakeReply alloc]  initWithParams:reply connectionName:connectionName];
    if(!replyQueue)
    {
        replyQueue = [[NSMutableArray alloc] init];
    }
    [replyQueue addObject:fakeReply];
}

/**
 * @brief Teardown Function for XPCStubs
 */
void XPCStubsTeardown(void)
{
    for(id key in fakeConnections)
    {
        [[fakeConnections objectForKey:key] freeName];
    }
    fakeConnections = nil;
    
    for (FakeReply *reply in replyQueue)
    {
        [reply freeConnectionName];
    }
    replyQueue = nil;
}
