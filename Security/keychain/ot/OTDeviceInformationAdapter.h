
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol OTDeviceInformationNameUpdateListener
- (void)deviceNameUpdated;
@end

@protocol OTDeviceInformationAdapter

/* Returns a string like "iPhone3,5" */
- (NSString*)modelID;

/* Returns the user-entered name for this device */
- (NSString* _Nullable)deviceName;

/* Returns a string describing the current os version */
- (NSString*)osVersion;

/* Returns the serial number as a string. */
- (NSString*)serialNumber;

/* register for deviceName updates */
- (void)registerForDeviceNameUpdates:(id<OTDeviceInformationNameUpdateListener>)listener;

@end

@interface OTDeviceInformationActualAdapter : NSObject <OTDeviceInformationAdapter>

@end

NS_ASSUME_NONNULL_END
