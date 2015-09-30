/*
     File: QServer.h
 Abstract: A generic TCP server object.
  Version: 2.0
 
 Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple
 Inc. ("Apple") in consideration of your agreement to the following
 terms, and your use, installation, modification or redistribution of
 this Apple software constitutes acceptance of these terms.  If you do
 not agree with these terms, please do not use, install, modify or
 redistribute this Apple software.
 
 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Apple grants you a personal, non-exclusive
 license, under Apple's copyrights in this original Apple software (the
 "Apple Software"), to use, reproduce, modify and redistribute the Apple
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Apple Software in its entirety and
 without modifications, you must retain this notice and the following
 text and disclaimers in all such redistributions of the Apple Software.
 Neither the name, trademarks, service marks or logos of Apple Inc. may
 be used to endorse or promote products derived from the Apple Software
 without specific prior written permission from Apple.  Except as
 expressly stated in this notice, no other rights or licenses, express or
 implied, are granted by Apple herein, including but not limited to any
 patent rights that may be infringed by your derivative works or by other
 works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
 MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
 THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
 OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
 AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
 STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 
 Copyright (C) 2013 Apple Inc. All Rights Reserved.
 
 */

#import <Foundation/Foundation.h>

// QServer is a general purpose class that starts a TCP server and listens for 
// incoming connections.  It is intended to be used in one of two ways:
//
// o With a fixed port number, as is traditional for TCP servers.
// o With a dynamic port number which is registered with Bonjour.
//
// QServer does the right thing by default in a number of cases:
// 
// o It supports IPv6 if it's available (can be disabled for testing).
// o It supports automatic Bonjour service renaming.
// o If you have a fixed port and register with Bonjour, it automatically 
//   backs off to a dynamic port if the fixed port is not available.
// 
// The class is run loop based and must be called from a single thread. 
// Specifically, the -start and -stop methods add and remove run loop sources 
// to the current thread's run loop, and it's that thread that calls the 
// delegate callbacks.
//
// You typically use this class by:
// 
// o allocating and hold on to a QServer object
// o setting the delegate
// o calling -start
// o implementing the -server:connectionForInputStream:outputStream: 
//   to create a new connection object to handle a connection over the input and 
//   output streams
// o implementation -server:closeConnection: to handle the case where the server 
//   force closes that connection (because the server was stopped)

@protocol QServerDelegate;

@interface QServer : NSObject

- (id)initWithDomain:(NSString *)domain type:(NSString *)type name:(NSString *)name preferredPort:(NSUInteger)preferredPort;
    // Initialise the server object.  This does not actually start the server; call 
    // -start to do that.
    //
    // If type is nil, the server is not registered with Bonjour.
    // If type is not nil, the server is registered in the specified 
    // domain with the specified name.  A domain of nil is equivalent 
    // to @"", that is, all standard domains.  A name of nil is equivalent 
    // to @"", that is, the service is given the default name.
    // 
    // If preferredPort is 0, a dynamic port is used, otherwise preferredPort is 
    // used if it's not busy.  If it busy, and no type is set, the server fails 
    // to start.  OTOH, if type is set, the server backs off to using a dynamic port 
    // (the logic being that, if it's advertised with Bonjour, clients will stil 
    // be able to find it).

// properties set by the init method

@property (nonatomic, copy,   readonly ) NSString *             domain;         // immutable, what you passed in to -initXxx
@property (nonatomic, copy,   readonly ) NSString *             type;           // immutable, what you passed in to -initXxx
@property (nonatomic, copy,   readonly ) NSString *             name;           // immutable, what you passed in to -initXxx
@property (nonatomic, assign, readonly ) NSUInteger             preferredPort;  // mutable, but only effective on the next -start

// properties you can configure, but not between a -start and -stop

@property (nonatomic, assign, readwrite) BOOL                   disableIPv6;    // primarily for testing purposes, default is NO, only effective on next -start

// properties you can configure at any time

@property (nonatomic, weak,   readwrite) id<QServerDelegate>    delegate;

// properties that change as the result of other actions

@property (nonatomic, assign, readonly ) NSUInteger             connectionSequenceNumber;       // observable
    // This increments each time a connection is made.  It's primarily for debugging purposes.

#pragma mark * Start and Stop

// It is reasonable to start and stop the same server object multiple times.

- (void)start;
    // Starts the server.  It's not legal to call this if the server is started.  
    // If startup attempted, will eventually call -serverDidStart: or 
    // -server:didStopWithError:.

- (void)stop;
    // Does nothing if the server is already stopped.
    // This does not call -server:didStopWithError:.
    // This /does/ call -server:didStopConnection: for each running connection.

@property (nonatomic, assign, readonly, getter=isStarted) BOOL  started;        // observable
@property (nonatomic, assign, readonly ) NSUInteger             registeredPort; // observable, only meaningful if isStarted is YES
@property (nonatomic, copy,   readonly ) NSString *             registeredName; // observable, only meaningful if isStarted is YES, 
                                                                                // may change due to Bonjour auto renaming, 
                                                                                // will be nil if Bonjour registration not requested, 
                                                                                // may be nil if Bonjour registration in progress

// These two methods let you temporarily deregister the server with Bonjour and then 
// reregister it again.  This is needed for WiTap, which wants to leave the server 
// listening at the TCP level (to avoid the port number creeping up) but wants it 
// to disappear from Bonjour.

- (void)deregister;
    // Deregistering can't fail, so there's no error result here.  Furthermore, 
    // it's safe to deregister a server that was not configured to use Bonjour 
    // and a server that's stopped.
    
- (void)reregister;
    // Reregistering /can/ fail and will shut down the server if it does so.
    // On failure, you'll get the -server:didStopWithError: delegate callback. 
    // On success, you'll get the -serverDidStart: delegate callback, which is a 
    // little weird but makes some sense (for example, you can use that to snarf 
    // the latest .registeredName).
    // 
    // It is an error to attempt to reregister a server that was not configured 
    // to use Bonjour or a server that's stopped.

@property (nonatomic, assign,   readonly ) BOOL                 isDeregistered;
    // Returns YES if the server has been deregistered (that is, if the server 
    // was configured to user Bonjour but there's no registration in place or 
    // in progress).

#pragma mark * Connections

- (void)closeOneConnection:(id)connection;
    // Remove a connection from the connections set.  This does /not/ call the 
    // -server:closeConnection: delegate method for the connection.  A connection can 
    // can call this on itself.  Does nothing if the connection not in the connections set.

- (void)closeAllConnections;
    // Closes all connections known to the server.  This /does/ call (synchronously) 
    // -server:closeConnection: on each connection.

@property (nonatomic, copy,   readonly ) NSSet *                connections;

#pragma mark * Run Loop Modes

// You can't add or remove run loop modes while the server is running.

- (void)addRunLoopMode:(NSString *)modeToAdd;
- (void)removeRunLoopMode:(NSString *)modeToRemove;

@property (nonatomic, copy,   readonly ) NSSet *                runLoopModes;   // contains NSDefaultRunLoopMode by default

// The following are utility methods that allow you to easily schedule streams in 
// the same run loop modes as the server.

- (void)scheduleInRunLoopModesInputStream:(NSInputStream *)inputStream outputStream:(NSOutputStream *)outputStream;
- (void)removeFromRunLoopModesInputStream:(NSInputStream *)inputStream outputStream:(NSOutputStream *)outputStream;
    // One of inputStream or outputStream may be nil.

@end


@protocol QServerDelegate <NSObject>

@optional

- (void)serverDidStart:(QServer *)server;
    // Called after the server has fully started, that is, once the Bonjour name 
    // registration (if requested) is complete.  You can use registeredName to get 
    // the actual service name that was registered.
    
- (void)server:(QServer *)server didStopWithError:(NSError *)error;
    // Called when the server stops of its own accord, typically in response to some 
    // horrible network problem.

// You should implement one and only one of the following callbacks.  If you implement 
// both, -server:connectionForSocket: is called.

- (id)server:(QServer *)server connectionForSocket:(int)fd;
    // Called to get a connection object for a new, incoming connection.  If you don't implement 
    // this, or you return nil, the socket for the connection is just closed.  If you do return 
    // a connection object, you are responsible for holding on to socket and ensuring that it's 
    // closed on the -server:closeConnection: delegate callback.

- (id)server:(QServer *)server connectionForInputStream:(NSInputStream *)inputStream outputStream:(NSOutputStream *)outputStream;
    // Called to get a connection object for a new, incoming connection.  If you don't implement 
    // this, or you return nil, the incoming connection is just closed.  If you do return a 
    // connection object, you are responsible for opening the two streams (or just one of the 
    // streams, if you only need one), holding on to them, and ensuring that they are closed 
    // and released on the -server:closeConnection delegate callback.

- (void)server:(QServer *)server closeConnection:(id)connection;
    // Called when the server shuts down or if someone calls -closeAllConnections. 
    // Typically the delegate would just forward this call to the connection object itself.

- (void)server:(QServer *)server logWithFormat:(NSString *)format arguments:(va_list)argList;
    // Called to log server activity.

@end


@interface QServer (ForSubclassers)

// The methods in this category are for subclassers only.  Client code would not 
// be expected to use any of this.

// QServer uses a reasonable default algorithm for binding its listening sockets. 
// Specifically:
//
// A. IPv4 is always enabled.  IPv6 is enabled if it's present on the system and is 
//    not explicitly disabled using disableIPv6.
// B. It always binds IPv4 and IPv6 (if enabled) to the same port.
// C. If preferredPort is zero, binding is very likely to succeed with registeredPort set to 
//    some dynamic port.
// D. If preferredPort is non-zero and type is nil, it either binds to the preferred 
//    port or fails to start up.
// E. If preferredPort is non-zero and type is set, it first tries to bind to the preferred 
//    port but, if that fails, uses a dynamic port.
//
// If this algorithm is not appropriate for your application you can override it by 
// subclassing QServer and overriding the -listenOnPortError: method.  Any listening 
// sockets that you create should be registered with run loop by calling -addListeningSocket:.

- (NSUInteger)listenOnPortError:(NSError **)errorPtr;
    // Override this method to change the binding algorithm used by QServer.  The method 
    // must return the port number to which you bound (all listening sockets must be bound 
    // to the same port lest you confuse Bonjour).  If it returns 0 and errorPtr is not NULL, 
    // then *errorPtr must be an NSError indicating the reason for the failure.

- (void)addListeningSocket:(int)fd;
    // Adds the specified listening socket to the run loop (well, to the set of sockets 
    // that get added to the run loop when you start the server).  You should only call 
    // this from a -listenOnPortError: override.

// The following methods allow subclasses to see the delegate methods without actually 
// being the delegate.  The default implementation of these routines just calls the 
// delegate callback, if any (except for -connectionForSocket:, which does the 
// -server:connectionForSocket: / -server:connectionForInputStream:outputStream: dance; 
// see the code for the details).

- (void)didStart;
- (void)didStopWithError:(NSError *)error;
- (id)connectionForSocket:(int)fd;
// There is no -connectionForInputStream:outputStream:.  A subclasser must override 
// -connectionForSocket:.
//
// -(id)connectionForInputStream:(NSInputStream *)inputStream outputStream:(NSOutputStream *)outputStream;
- (void)closeConnection:(id)connection;
- (void)logWithFormat:(NSString *)format arguments:(va_list)argList;

@end
