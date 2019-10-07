//
//  HIDAnalyticsHistogramEventField.m
//  HIDAnalytics
//
//  Created by AB on 11/26/18.
//

#import "HIDAnalyticsHistogramEventField.h"
#import <os/log.h>

typedef struct _HIDAnalyticsHistogramBucket
{
    uint16_t minVal;
    uint16_t maxVal;
    uint8_t count;
} HIDAnalyticsHistogramBucket;

typedef struct _HIDAnalyticsHistogramSegment
{
    uint8_t bucketCount;
    HIDAnalyticsHistogramBucket *buckets;
    
} HIDAnalyticsHistogramSegment;

@implementation HIDAnalyticsHistogramEventField
{
    uint8_t _segmentCount;
    HIDAnalyticsHistogramSegment *_segments;
}

-(nullable instancetype) initWithAttributes:(NSString*) name
                                    segments:(HIDAnalyticsHistogramSegmentConfig*) segments
                                      count:(NSInteger) count
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _fieldName = name;
    
    // create buckets from user segments
    [self createBuckets:segments count:count];
    
    return self;
}

-(void) dealloc
{
    for (NSInteger i = 0; _segments && i < (NSInteger)_segmentCount; i++) {
        
        if (_segments[i].buckets) {
            free(_segments[i].buckets);
        }
    }
    
    if (_segments) {
        free(_segments);
    }
}

-(void) setValue:(id) value
{
    NSInteger  val = ((NSNumber*)value).unsignedIntegerValue;
    
    for (NSInteger i=0; i < (NSInteger)_segmentCount; i++) {
        for (NSInteger j=0; j < (NSInteger)_segments[i].bucketCount; j++) {
            _segments[i].buckets[j].count = val;
        }
    }
}
-(void) createBuckets:(HIDAnalyticsHistogramSegmentConfig*) segments count:(NSInteger) count
{
    
    _segmentCount = (uint8_t)count;
    
    _segments = (HIDAnalyticsHistogramSegment*)malloc(_segmentCount*sizeof(HIDAnalyticsHistogramSegment));
    memset(_segments, 0, _segmentCount*sizeof(HIDAnalyticsHistogramSegment));
    
    for (NSInteger i = 0; i < count; i++) {
        
        NSUInteger normalizer = segments[i].value_normalizer ? segments[i].value_normalizer : 1;
        if (normalizer > 1) {
            //Log it for now, for any furture changes
            os_log(OS_LOG_DEFAULT, "HIDAnalytics higher value normalizer %lu for field %@ , reduce to 1 ", (unsigned long)normalizer, _fieldName);
        }
        
        uint16_t normalizedBase = segments[i].bucket_base;
        _segments[i].bucketCount = segments[i].bucket_count;
        _segments[i].buckets = (HIDAnalyticsHistogramBucket*)malloc(segments[i].bucket_count*sizeof(HIDAnalyticsHistogramBucket));
        memset(_segments[i].buckets, 0, segments[i].bucket_count*sizeof(HIDAnalyticsHistogramBucket));
        
        for (NSInteger j = 0; j < segments[i].bucket_count; j++) {
            
            _segments[i].buckets[j].minVal = normalizedBase;
            _segments[i].buckets[j].maxVal = normalizedBase + segments[i].bucket_width;
            
            normalizedBase = normalizedBase + segments[i].bucket_width;
            
        }
    }
}
-(void) setIntegerValue:(uint64_t) value
{
    // look through all the segments and increment count if it fits
    // any bucket in any segment.
    
    uint8_t bucketIndex = 0;
    uint8_t segmentIndex = 0;
    uint8_t segmentCount = _segmentCount;

    for (NSInteger i = 0; i < segmentCount; i++) {
        
        segmentIndex = i;
        
        uint16_t normalizedValue = value;//expect normalizer to be 1
        for (NSInteger j = 0; j < (NSInteger)_segments[segmentIndex].bucketCount; j++) {
            
            bucketIndex = j;
            
            //check for first bucket in segment
            if (bucketIndex == 0 && normalizedValue <= _segments[segmentIndex].buckets[bucketIndex].minVal) {
                _segments[segmentIndex].buckets[bucketIndex].count++;
                continue;
            }
            
            //check for last bucket in segment
            if (bucketIndex == _segments[segmentIndex].bucketCount - 1 && normalizedValue > _segments[segmentIndex].buckets[bucketIndex].maxVal) {
                _segments[segmentIndex].buckets[bucketIndex].count++;
                continue;
            }
            
            if (normalizedValue > _segments[segmentIndex].buckets[bucketIndex].minVal && normalizedValue <= _segments[segmentIndex].buckets[bucketIndex].maxVal)  {
                _segments[segmentIndex].buckets[bucketIndex].count++;
            }
        }
    }
    
    // we don't break on single bucket match
    // since this is not on critical path , lets check for all buckets
    // if user gives different normalize scale
    
}

-(nullable id) value
{
    
    // not on critical path
    // do for all the buckets
    @autoreleasepool {
        
        NSMutableArray *ret = [[NSMutableArray alloc] init];
        
        uint8_t bucketIndex = 0;
        uint8_t segmentIndex = 0;
        uint8_t segmentCount = _segmentCount;
        
        for (NSInteger i = 0 ; i < (NSInteger)segmentCount; i++) {
            segmentIndex = i;
            
            for (NSInteger j = 0; j < (NSInteger)_segments[segmentIndex].bucketCount; j++) {
                bucketIndex = j;
                [ret addObject:@(_segments[segmentIndex].buckets[bucketIndex].count)];
            }
        }
        
        return ret;
    }
}
@end
