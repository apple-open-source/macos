//
//  DeviceSimulatorProtocol.h
//  DeviceSimulator
//

#import <Foundation/Foundation.h>

typedef void (^MDNComplete)(NSDictionary * returnedValues, NSError *error);

@protocol MultiDeviceNetworkingCallbackProtocol
- (void)MDNCItemsChanged:(NSDictionary *)values complete:(MDNComplete)complete;
@end

@protocol MultiDeviceNetworkingProtocol

- (void)MDNRegisterCallback:(NSXPCListenerEndpoint *)callback complete:(MDNComplete)complete;
- (void)MDNCloudPut:(NSDictionary *)values complete:(MDNComplete)complete;
- (void)MDNCloudsynchronizeAndWait:(NSDictionary *)values complete:(MDNComplete)complete;
- (void)MDNCloudGet:(NSArray *)keys complete:(MDNComplete)complete;
- (void)MDNCloudGetAll:(MDNComplete)complete;
- (void)MDNCloudRemoveKeys:(NSArray<NSString *> *)keys complete:(MDNComplete)complete;
- (void)MDNCloudFlush:(MDNComplete)complete;

@end


