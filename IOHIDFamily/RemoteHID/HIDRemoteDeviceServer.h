//
//  HIDRemoteSimpleServer.h
//  IOHIDFamily
//
//  Created by yg on 1/14/18.
//

#ifndef HIDRemoteSimpleServer_h
#define HIDRemoteSimpleServer_h

#import "HIDUserDevice.h"

//
// Timeout on get report in sec
//
#define kRemoteHIDDeviceTimeout    2

@interface HIDRemoteDevice : HIDUserDevice

@property   (retain)    id  __nonnull          endpoint;
@property               uint64_t               deviceID;

-(nullable instancetype)initWithProperties:(nonnull NSDictionary *)properties;
-(IOReturn) getReportHandler:(IOHIDReportType) type
                    reportID:(uint8_t) reportID
                      report:( uint8_t * _Nonnull ) report
                reportLength:(NSUInteger) reportLength;
@end


@interface HIDRemoteDeviceServer : NSObject

@property (readonly) NSMutableDictionary * __nonnull devices;
@property (readonly) dispatch_queue_t __nonnull queue;

-(nullable instancetype) initWithQueue:(__nonnull dispatch_queue_t) queue;
-(void) activate;
-(void) cancel;
-(BOOL) connectEndpoint:(__nonnull id) endpoint;
-(void) disconnectEndpoint:(__nonnull id) endpoint;
-(void) disconnectAll;
-(void) endpointMessageHandler:(__nonnull id) endpoint data:(uint8_t * __nonnull) data size:(size_t) dataSize;
-(IOReturn) remoteDeviceSetReport:(HIDRemoteDevice * __nonnull) device
                             type:(IOHIDReportType) type
                         reportID:(uint8_t) reportID
                           report:( uint8_t * _Nonnull ) report
                     reportLength:(NSUInteger) reportLength;

-(IOReturn) remoteDeviceGetReport:(HIDRemoteDevice * __nonnull) device
                             type:(IOHIDReportType) type
                         reportID:(uint8_t) reportID
                           report:(uint8_t * __nonnull) report
                     reportLength:(NSUInteger * __nonnull) reportLength;

-(BOOL) createRemoteDevice:(__nonnull id) endpoint  deviceID:(uint64_t) deviceID property:(NSMutableDictionary * __nonnull) property;

@end


#endif /* HIDRemoteSimpleServer_h */
