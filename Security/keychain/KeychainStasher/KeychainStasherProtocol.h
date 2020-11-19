#ifndef KeychainStasherProtocol_h
#define KeychainStasherProtocol_h

#import <Foundation/Foundation.h>

@protocol KeychainStasherProtocol

- (void)stashKey:(NSData*)key withReply:(void (^)(NSError*))reply;
- (void)loadKeyWithReply:( void (^)(NSData*, NSError*))reply;
    
@end

#endif /* KeychainStasherProtocol_h */
