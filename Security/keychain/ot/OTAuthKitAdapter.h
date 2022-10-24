
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

- (BOOL)accountIsHSA2ByAltDSID:(NSString*)altDSID;
- (BOOL)accountIsDemoAccountByAltDSID:(NSString*)altDSID error:(NSError**)error NS_SWIFT_NOTHROW;

- (NSString* _Nullable)machineID:(NSError**)error;
- (void)fetchCurrentDeviceListByAltDSID:(NSString*)altDSID reply:(void (^)(NSSet<NSString*>* _Nullable machineIDs, NSError* _Nullable error))complete;
- (void)registerNotification:(id<OTAuthKitAdapterNotifier>)notifier;

- (void)deliverAKDeviceListDeltaMessagePayload:(NSDictionary* _Nullable)notificationDictionary;
@end

@interface OTAuthKitActualAdapter : NSObject <OTAuthKitAdapter>
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON

