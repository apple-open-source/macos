
#if OCTAGON

#ifndef OctagonEscrowRecoverer_h
#define OctagonEscrowRecoverer_h

@protocol OctagonEscrowRecovererPrococol <NSObject>
- (NSError*)recoverWithInfo:(NSDictionary*)info results:(NSDictionary**)results;
- (NSError *)getAccountInfoWithInfo:(NSDictionary *)info results:(NSDictionary**)results;
- (NSError *)disableWithInfo:(NSDictionary *)info;
- (bool)isRecoveryKeySet:(NSError * __autoreleasing *)error;
- (bool)restoreKeychainWithBackupPassword:(NSData *)password
                                    error:(NSError * __autoreleasing *)error;
- (bool)verifyRecoveryKey:(NSString*)recoveryKey
                    error:(NSError * __autoreleasing *)error;
- (bool)removeRecoveryKeyFromBackup:(NSError * __autoreleasing *)error;
@end

#endif /* OctagonEscrowRecoverer_h */

#endif // OCTAGON
