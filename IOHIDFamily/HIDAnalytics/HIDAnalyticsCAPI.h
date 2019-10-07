//
//  HIDAnalyticsCAPI.h
//  IOHIDFamily
//
//  Created by AB on 11/27/18.
//

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

#if defined(__cplusplus)
extern "C" {
#endif
    
CF_ASSUME_NONNULL_BEGIN

    
typedef CFTypeRef HIDAnalyticsHistogramFieldEventRef;
typedef CFTypeRef HIDAnalyticsEventRef;
typedef CFTypeRef HIDAnalyticsHistogramEventRef;
    
    
/*!
 @typedef HIDAnalyticsHistogramSegmentConfig
 @abstract Struct spefifying bucketization of value. Each segement has uniform bucket width. Use multiple segments for variable width.
 @var bucket_count Number of buckets in segement.
 @var bucket_width Normalized value range for bucket.
 @var bucket_base Normalized start value of bucket.
 @var value_normalizer scale used to normalize value for given segment.
 */

typedef struct _HIDAnalyticsHistogramSegmentConfig {
    
    uint8_t  bucket_count;
    uint8_t  bucket_width;  // normalized on scale of value_normalizer
    uint8_t  bucket_base;   // normalized
    uint64_t value_normalizer; // used for normalizing value to be bucketized
    
} HIDAnalyticsHistogramSegmentConfig;

/*!
 * HIDAnalyticsEventCreate
 *
 * @abstract
 * Creates a HIDAnalyticsEvent object and registers it with HIDAnalyticsReporter.
 *
 * @discussion
 * HIDAnalyticsEvent object is used to log data to analytics server. It is composed of
 * mutable and immutable fields. Immutable fields can be added as part of description
 * during creation. All field added using HIDAnalyticsEventAdd* call are considered to be
 * mutable. Data collection for given event begins after it's creation. User need to
 * invalidate event to stop data collection for it.
 *
 * @param eventName
 * Event name to uniquely identify event in CA.
 *
 * @param description
 * Immutable fields associated with event. For example event to log copy property for service
 * API latency can have service description as immutable field.
 *
 * @result
 * Returns an instance of a HIDAnalyticsEvent object on success.
 */
HIDAnalyticsEventRef __nullable HIDAnalyticsEventCreate(CFStringRef eventName, CFDictionaryRef _Nullable description);
    
/*!
 * HIDAnalyticsEventAddHistogramField
 *
 * @abstract
 * Add HIDAnalyticsEventField of type Histogram.
 *
 * @discussion
 * Add histogram field to HIDAnalytics event.
 *
 * @param event
 * HIDAnayticsEventRef returned from  HIDAnalyticsEventCreate.
 *
 * @param fieldName
 * Field name to uniquely identify field in event.
 *
 * @param segments
 * Array of segment each containing bucketization information
 *
 * Example
 * Data : 1000, 2000, 2500, 4000, 8000
 *
 * .bucket_count = 5
 * .bucket_width = 2
 * .bucket_base = 1
 * .bucket_normalizer = 1000
 *
 * [ <=1     <=3     <=5   <=7   <=9 ]
 * [  1      2        1     0     1  ]
 *
 * @param count
 * count of segment array.
 */
void HIDAnalyticsEventAddHistogramField(HIDAnalyticsEventRef  event, CFStringRef  fieldName, HIDAnalyticsHistogramSegmentConfig*  segments, CFIndex count);

/*!
 * HIDAnalyticsEventAddField
 *
 * @abstract
 * Add HIDAnalyticsEventField of type Integer.
 * Need to extend this with type option to support
 * normal fields for future
 *
 * @discussion
 * Field is added to event. Value set by user is what it gets.
 * No value processing is done for this field.
 *
 * @param event
 * HIDAnayticsEventRef returned from  HIDAnalyticsEventCreate.
 *
 * @param fieldName
 * Field name to uniquely identify field in event.
 */
void HIDAnalyticsEventAddField(HIDAnalyticsEventRef  event, CFStringRef  fieldName);
    
/*!
 * HIDAnalyticsEventActivate
 *
 * @abstract
 * Register event with reporter and start collection of data for it.
 *
 * @discussion
 * Once event is activated, no addition of field to event is allowed.
 * Any attempt to add field to activates will cause os fault.
 *
 * @param event
 * HIDAnayticsEventRef returned from  HIDAnalyticsEventCreate.
 *
 */
void HIDAnalyticsEventActivate(HIDAnalyticsEventRef  event);

/*!
 * HIDAnalyticsEventCancel
 *
 * @abstract
 * Stop data collection for given event.
 *
 * @discussion
 * Stop data collection for event and invalidate any resources attached with event.
 *
 * @param event
 * HIDAnayticsEventRef returned from  HIDAnalyticsEventCreate.
 *
 */
void HIDAnalyticsEventCancel(HIDAnalyticsEventRef  event);

/*!
 * HIDAnalyticsEventSetIntegerValueForField
 *
 * @abstract
 * Set value for given event field
 *
 * @discussion
 * Set value for given event field. Update of field value is based on type of field. For example
 * if field type is histogram then call for given API will bucketize value based on bucket config
 * specified during  HIDAnalyticsEventAdd*Field.
 *
 * @param event
 * HIDAnayticsEventRef returned from  HIDAnalyticsEventCreate.
 *
 * @param fieldName
 * Event field name.
 *
 * @param value
 * Event field value to set.
 *
 */
void HIDAnalyticsEventSetIntegerValueForField(HIDAnalyticsEventRef  event, CFStringRef  fieldName, uint64_t value);
    
    
/*!
 * HIDAnalyticsHistogramEventCreate
 *
 * @abstract
 * Creates a HIDAnalyticsHistogramEventCreate object and registers it with HIDAnalyticsReporter.
 *
 * @discussion
 * HIDAnalyticsHistogramEventCreate object is used to log data to analytics server. It is composed of
 * single histogram field. HIDAnalyticsEventAdd* is not supported.
 *
 * @param eventName
 * Event name to uniquely identify event in CA.
 *
 * @param description
 * Immutable fields associated with event. For example event to log copy property for service
 * API latency can have service description as immutable field.
 *
 * @param fieldName
 * Field name to uniquely identify field in event.
 *
 * @param segments
 * Array of segment each containing bucketization information
 *
 * Example
 * Data : 1000, 2000, 2500, 4000, 8000
 *
 * .bucket_count = 5
 * .bucket_width = 2
 * .bucket_base = 1
 * .bucket_normalizer = 1000
 *
 * [ <=1     <=3     <=5   <=7   <=9 ]
 * [  1      2        1     0     1  ]
 *
 * @param count
 * count of segment array.
 *
 * @result
 * Returns an instance of a HIDAnalyticsHistogramEventobject on success.
 */
HIDAnalyticsHistogramEventRef __nullable HIDAnalyticsHistogramEventCreate(CFStringRef eventName, CFDictionaryRef _Nullable description, CFStringRef fieldName,HIDAnalyticsHistogramSegmentConfig*  segments, CFIndex count);


/*!
 * HIDAnalyticsHistogramEventSetIntegerValue
 *
 * @abstract
 * Set value  HIDAnalyticsHistogramEvent
 *
 * @discussion
 * Set value for given histogram event . Not supported for  event with multiple fields.
 *
 * @param event
 * HIDAnalyticsHistogramEventRef returned from  HIDAnalyticsHistogramEventCreate.
 *
 * @param value
 * Event field value to set.
 *
 */
void HIDAnalyticsHistogramEventSetIntegerValue(HIDAnalyticsHistogramEventRef event, uint64_t value);
    

CF_ASSUME_NONNULL_END

#ifdef __cplusplus
}
#endif

__END_DECLS
