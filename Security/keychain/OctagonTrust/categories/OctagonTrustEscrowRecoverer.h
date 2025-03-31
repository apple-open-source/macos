
#if OCTAGON

#ifndef OctagonTrustEscrowRecoverer_h
#define OctagonTrustEscrowRecoverer_h

NS_ASSUME_NONNULL_BEGIN

@protocol OctagonEscrowRecovererPrococol <NSObject>
- (NSError* _Nullable)recoverWithInfo:(NSDictionary* _Nullable)info results:(NSDictionary* _Nonnull* _Nullable)results;
- (NSError* _Nullable)getAccountInfoWithInfo:(NSDictionary* _Nullable)info results:(NSDictionary* _Nonnull* _Nullable)results;
- (NSError* _Nullable)disableWithInfo:(NSDictionary* _Nullable)info;
- (NSDictionary* _Nullable)recoverWithCDPContext:(OTICDPRecordContext*)cdpContext
                                    escrowRecord:(OTEscrowRecord*)escrowRecord
                                           error:(NSError**)error;
- (NSDictionary* _Nullable)recoverSilentWithCDPContext:(OTICDPRecordContext*)cdpContext
                                            allRecords:(NSArray<OTEscrowRecord*>*)allRecords
                                                 error:(NSError**)error;

- (NSDictionary* _Nullable)recoverWithCDPContext:(OTICDPRecordContext *)cdpContext
                                    escrowRecord:(OTEscrowRecord*)escrowRecord
                                         altDSID:(NSString* _Nullable)altDSID
                                          flowID:(NSString* _Nullable)flowID
                                 deviceSessionID:(NSString* _Nullable)deviceSessionID
                                           error:(NSError *__autoreleasing *)error;

- (NSDictionary* _Nullable)recoverSilentWithCDPContext:(OTICDPRecordContext*)cdpContext
                                            allRecords:(NSArray<OTEscrowRecord*>*)allRecords
                                               altDSID:(NSString* _Nullable)altDSID
                                                flowID:(NSString* _Nullable)flowID
                                       deviceSessionID:(NSString* _Nullable)deviceSessionID
                                                 error:(NSError**)error;

- (void)restoreKeychainAsyncWithPassword:password
                            keybagDigest:(NSData *)keybagDigest
                         haveBottledPeer:(BOOL)haveBottledPeer
                    viewsNotToBeRestored:(NSMutableSet <NSString*>*)viewsNotToBeRestored
                                   error:(NSError **)error;

- (bool)isRecoveryKeySet:(NSError**)error;

- (bool)restoreKeychainWithBackupPassword:(NSData *)password
                                    error:(NSError**)error;
- (NSError* _Nullable)backupWithInfo:(NSDictionary* _Nullable)info;
- (NSError* _Nullable)backupForRecoveryKeyWithInfo:(NSDictionary* _Nullable)info;

- (bool)verifyRecoveryKey:(NSString*)recoveryKey
                    error:(NSError**)error;

- (bool)removeRecoveryKeyFromBackup:(NSError**)error;
@end

NS_ASSUME_NONNULL_END

#endif /* OctagonTrustEscrowRecoverer_h */

#endif // OCTAGON
