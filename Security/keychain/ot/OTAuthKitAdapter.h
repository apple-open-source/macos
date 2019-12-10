
#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ot/OTDefines.h"
#import <AuthKit/AuthKit.h>
#import <AuthKit/AuthKit_Private.h>

NS_ASSUME_NONNULL_BEGIN

@protocol OTAuthKitAdapterNotifier
- (void)machinesAdded:(NSArray<NSString*>*)machineIDs altDSID:(NSString*)altDSID;
- (void)machinesRemoved:(NSArray<NSString*>*)machineIDs altDSID:(NSString*)altDSID;
- (void)incompleteNotificationOfMachineIDListChange;
@end

@protocol OTAuthKitAdapter

// Returns nil if there is no such primary account
- (NSString* _Nullable)primaryiCloudAccountAltDSID:(NSError **)error;

- (BOOL)accountIsHSA2ByAltDSID:(NSString*)altDSID;

- (NSString* _Nullable)machineID:(NSError**)error;
- (void)fetchCurrentDeviceList:(void (^)(NSSet<NSString*>* _Nullable machineIDs, NSError* _Nullable error))complete;
- (void)registerNotification:(id<OTAuthKitAdapterNotifier>)notifier;
@end

@interface OTAuthKitActualAdapter : NSObject <OTAuthKitAdapter>
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON

