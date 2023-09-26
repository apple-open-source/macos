//
//  main.m
//  KeychainDBMover
//

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection.h>
#import <sys/stat.h>
#import <xpc/private.h>
#import "KeychainDBMover.h"
#import "debugging.h"

@interface ServiceDelegate : NSObject <NSXPCListenerDelegate>
@end

@implementation ServiceDelegate

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    // This method is where the NSXPCListener configures, accepts, and resumes a new incoming NSXPCConnection.
    
    // Configure the connection.
    // First, set the interface that the exported object implements.
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(KeychainDBMoverProtocol)];
    
    // Next, set the object that the connection exports. All messages sent on the connection to this service will be sent to the exported object to handle. The connection retains the exported object.
    KeychainDBMover *exportedObject = [[KeychainDBMover alloc] init];
    newConnection.exportedObject = exportedObject;
    
    // Resuming the connection allows the system to deliver more incoming messages.
    [newConnection resume];
    
    // Returning YES from this method tells the system that you have accepted this connection. If you want to reject the connection for some reason, call -invalidate on the connection and return NO.
    return YES;
}

@end

int main(int argc, const char *argv[])
{
    // Set the umask so that dirs & files created by this service can't be read or written by any other user
    umask(077);

    // Create the delegate for the service.
    ServiceDelegate *delegate = [[ServiceDelegate alloc] init];
    
    // Set up the one NSXPCListener for this service. It will handle all incoming connections.
    NSXPCListener *listener = [NSXPCListener serviceListener];
    listener.delegate = delegate;
    
    signal(SIGTERM, SIG_IGN);
    static dispatch_source_t termSource;
    termSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0));
    dispatch_source_set_event_handler(termSource, ^{
        secnotice("sigterm", "Got SIGTERM, exiting when clean 🖖");
        xpc_transaction_exit_clean();
    });
    dispatch_activate(termSource);

    // Resuming the serviceListener starts this service. This method does not return.
    [listener resume];
    return 0;
}
