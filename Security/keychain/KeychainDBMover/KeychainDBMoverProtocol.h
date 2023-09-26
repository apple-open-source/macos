//
//  KeychainDBMoverProtocol.h
//  KeychainDBMover
//

#import <Foundation/Foundation.h>

// The protocol that this service will vend as its API. This header file will also need to be visible to the process hosting the service.
@protocol KeychainDBMoverProtocol

// Replace the API of this protocol with an API appropriate to the service you are vending.
- (void)moveUserDbWithReply:(void (^)(NSError * nullable))reply;
    
@end
