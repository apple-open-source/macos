//
//  HIDDisplayDevicePreset.h
//  IOHIDFamily
//
//  Created by AB on 1/10/19.
//

#ifndef HIDDisplayDevicePreset_h
#define HIDDisplayDevicePreset_h

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayDevicePreset : NSObject

@property(readonly) BOOL valid;
@property(readonly) BOOL writable;
@property(readonly,nullable) NSString* uniqueID;

-(instancetype) init NS_UNAVAILABLE;
/*!
 GET/SET dictionary of following key value as described in HIDDisplayCAPI.h
 kHIDDisplayPresetFieldWritableKey : BOOL
 kHIDDisplayPresetFieldValidKey : BOOL
 kHIDDisplayPresetFieldNameKey : NSString*
 kHIDDisplayPresetFieldDescriptionKey : NSString*
 kHIDDisplayPresetFieldDataBlockOneLengthKey : NSInteger
 kHIDDisplayPresetFieldDataBlockOneKey : NSData*
 kHIDDisplayPresetFieldDataBlockTwoLengthKey : NSInteger
 kHIDDisplayPresetFieldDataBlockTwoKey : NSData*
 kHIDDisplayPresetUniqueID : NSString*
 */
-(nullable NSDictionary*) get:(NSError**) error;
-(BOOL) set:(NSDictionary*) info error:(NSError**) error;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDDisplayDevicePreset_h */
