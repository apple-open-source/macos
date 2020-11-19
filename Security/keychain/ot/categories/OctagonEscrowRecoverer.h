
#if OCTAGON

#ifndef OctagonEscrowRecoverer_h
#define OctagonEscrowRecoverer_h

@protocol OctagonEscrowRecovererPrococol <NSObject>
- (NSError*)recoverWithInfo:(NSDictionary*)info results:(NSDictionary**)results;
- (NSError *)getAccountInfoWithInfo:(NSDictionary *)info results:(NSDictionary**)results;
- (NSError *)disableWithInfo:(NSDictionary *)info;
@end

#endif /* OctagonEscrowRecoverer_h */

#endif // OCTAGON
