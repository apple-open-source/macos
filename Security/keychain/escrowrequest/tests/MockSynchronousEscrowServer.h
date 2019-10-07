
#import <Foundation/Foundation.h>

#import "keychain/escrowrequest/EscrowRequestXPCProtocol.h"
#import "keychain/escrowrequest/EscrowRequestServer.h"

NS_ASSUME_NONNULL_BEGIN

@interface MockSynchronousEscrowServer : NSObject <EscrowRequestXPCProtocol>

- (instancetype)initWithServer:(EscrowRequestServer*)server;

@end

NS_ASSUME_NONNULL_END
