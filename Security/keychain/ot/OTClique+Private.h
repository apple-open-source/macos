
#ifndef OTClique_Private_h
#define OTClique_Private_h

#import <Security/OTClique.h>

NS_ASSUME_NONNULL_BEGIN

@interface OTClique(Private)

+ (NSArray<NSData*>* _Nullable)fetchEscrowRecordsInternal:(OTConfigurationContext*)configurationContext
                                                    error:(NSError* __autoreleasing *)error;

+ (BOOL)isCloudServicesAvailable;

- (BOOL)resetAndEstablish:(CuttlefishResetReason)resetReason error:(NSError**)error;

- (BOOL)establish:(NSError**)error;

@end

NS_ASSUME_NONNULL_END

#endif /* OTClique_Private_h */
