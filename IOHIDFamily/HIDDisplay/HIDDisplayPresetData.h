//
//  HIDDisplayPresetData.h
//  IOHIDFamily
//
//  Created by AB on 1/10/19.
//

#ifndef HIDDisplayPresetData_h
#define HIDDisplayPresetData_h

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDDisplayPresetData : NSObject

@property(readonly) BOOL valid;
@property(readonly) BOOL writable;
@property(readonly,nullable) NSData* uniqueID;

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

#endif /* HIDDisplayPresetData_h */
