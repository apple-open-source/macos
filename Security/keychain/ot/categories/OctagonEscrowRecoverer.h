
#if OCTAGON

#ifndef OctagonEscrowRecoverer_h
#define OctagonEscrowRecoverer_h

#import <CloudServices/SecureBackup.h>

@protocol OctagonEscrowRecovererPrococol <NSObject>
- (NSError*)recoverWithInfo:(NSDictionary*)info results:(NSDictionary**)results;
- (NSError *)disableWithInfo:(NSDictionary *)info;
@end

@interface SecureBackup (OctagonProtocolConformance) <OctagonEscrowRecovererPrococol>
@end

#endif /* OctagonEscrowRecoverer_h */

#endif // OCTAGON
