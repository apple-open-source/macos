//
//  TLE.h
//  security_lwcr_support
//
#ifndef TLE_h
#define TLE_h

#import <Foundation/Foundation.h>
#import <TLE/Core/Core.h>

NS_ASSUME_NONNULL_BEGIN;

extern NSErrorDomain const LWCRErrorDomain;
typedef NS_ENUM(NSInteger, LWCRErrorCode) {
	kLWCROK = 0,
	kLWCRCoreError = 1,
	kLWCRCEError = 2,
};

/*!
 * @interface LWCR
 * @brief LWCR is a high-level interface to working Lightweight Code Requirements
 */
@interface sec_LWCR : NSObject

/*!
 * @property version
 * @brief Represents the version of the LWCR runtime the data was encoded with
 */
@property (readonly) LWCRVersion_t version;
/*!
 * @property constraintCatagory
 * @brief Represents the constraint category associated with this LWCR object, or LWCR_CONSTRAINT_CATEGORY_DEFAULT if no constraint is specified
 */
@property (readonly) int64_t constraintCategory;

/*!
 * @property hasRequirements
 * @brief True if this LWCR specifies an inline requirement
 */
@property (readonly) BOOL hasRequirements;

/*!
 * @property dictionary
 * @brief A dictionary representation of the complete light weight code requirement, including version and constraint category information.
 * @note This parameter is null if the withData constructor was used directly.
 */
@property (readonly, nullable) NSDictionary* dictionary;

/*!
 * @brief Initialize LWCR from an existing external data representation
 * @param data
 * External CoreEntitlements encoded data
 * @param error
 * An optional parameter where an error will be stored if the deserialization failed
 *
 * @return
 * An initialized LWCR object
 */
+(instancetype __nullable) withData:(NSData*)data withError:(NSError* __autoreleasing* __nullable)error;

@end

/*!
 * @interface LWCRFact
 * @brief A LWCR fact with a value
 */
@interface sec_LWCRFact: NSObject
+(instancetype __nullable) boolFact:(BOOL)value;
+(instancetype __nullable) integerFact:(NSNumber*)integer;
+(instancetype __nullable) stringFact:(NSString*)string;
+(instancetype __nullable) entitlementsFact:(NSDictionary*)entitlements;
+(instancetype __nullable) dataFact:(NSData*)data;

-(void) bindName:(const char*)name withLength:(size_t)length;
@end

/*!
 * @interface LWCRExecutor
 * @brief You may use the executor to evaluate LWCR objects
 */
@interface sec_LWCRExecutor : NSObject
-(BOOL) evaluateRequirements:(sec_LWCR*)lwcr withFacts:(NSDictionary<NSString*, sec_LWCRFact*>*)facts;

/*!
 * @brief Returns a fresh executor
 */
+(instancetype) executor;
@end

NS_ASSUME_NONNULL_END;
#endif /* TLE_h */

