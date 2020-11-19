
#if OCTAGON

#ifndef OctagonTrustEscrowRecoverer_h
#define OctagonTrustEscrowRecoverer_h

@protocol OctagonEscrowRecovererPrococol <NSObject>
- (NSError*)recoverWithInfo:(NSDictionary*)info results:(NSDictionary**)results;
- (NSError *)getAccountInfoWithInfo:(NSDictionary *)info results:(NSDictionary**)results;
- (NSError *)disableWithInfo:(NSDictionary *)info;
- (NSDictionary*)recoverWithCDPContext:(OTICDPRecordContext*)cdpContext
                                    escrowRecord:(OTEscrowRecord*)escrowRecord
                                           error:(NSError**)error;
- (NSDictionary*)recoverSilentWithCDPContext:(OTICDPRecordContext*)cdpContext
                                            allRecords:(NSArray<OTEscrowRecord*>*)allRecords
                                                 error:(NSError**)error;
- (void)restoreKeychainAsyncWithPassword:password
                            keybagDigest:(NSData *)keybagDigest
                         haveBottledPeer:(BOOL)haveBottledPeer
                    viewsNotToBeRestored:(NSMutableSet <NSString*>*)viewsNotToBeRestored
                                   error:(NSError **)error;

@end

#endif /* OctagonTrustEscrowRecoverer_h */

#endif // OCTAGON
