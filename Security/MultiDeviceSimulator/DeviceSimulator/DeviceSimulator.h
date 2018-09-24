//
//  DeviceSimulator.h
//  DeviceSimulator
//
//

#import <Foundation/Foundation.h>
#import "DeviceSimulatorProtocol.h"

extern NSString *deviceInstance;
void boot_securityd(NSXPCListenerEndpoint *network);

// This object implements the protocol which we have defined. It provides the actual behavior for the service. It is 'exported' by the service to make it available to the process hosting the service over an NSXPCConnection.
@interface DeviceSimulator : NSObject <DeviceSimulatorProtocol>
@property NSXPCConnection *conn;
@property NSString *name;
@end
