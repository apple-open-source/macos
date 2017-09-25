//
//  IOHIDEventSystemTestController.h
//  IOHIDFamily
//
//  Created by YG on 10/24/16.
//
//

#import  <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>


typedef struct _EVENTS_STATS {
  uint32_t  counts[kIOHIDEventTypeCount];
  uint64_t  minLatency;
  uint64_t  maxLatency;
  uint64_t  averageLatency;
  uint32_t  totalCount;
} EVENTS_STATS;

@protocol  IOHIDEventObserver <NSObject>

  -(void) EventCallback: (nonnull IOHIDEventRef) event For: (nullable IOHIDServiceClientRef) service;

@optional

  -(BOOL) FilterCallback: (nonnull IOHIDEventRef) event For: (nullable IOHIDServiceClientRef) service;

@end



@protocol  IOHIDPropertyObserver <NSObject>

-(void) PropertyCallback: (nonnull CFStringRef) property And: (nullable CFTypeRef) value;

@end



@interface IOHIDEventSystemTestController : NSObject

@property (readonly, nonnull)   IOHIDEventSystemClientRef eventSystemClient;
@property (         nullable)   IOHIDServiceClientRef     eventService;
@property (readonly, nullable)  dispatch_queue_t          eventSystemQueue;
@property (          nonnull)   NSMutableArray*           events;
@property (          nonnull)   NSMutableArray*           otherEvents;
@property                       NSInteger                 eventSystemResetCount;
@property (nonatomic,nullable)  id<IOHIDEventObserver>    eventObserver;
@property (nullable)            NSMutableDictionary*      propertyObservers;


/*!
 * @method -initWithDeviceUniqueID:AndQueue:
 *
 * Init Event System Test controller
 *
 * @param uniqueID unique service ID to match service.
 *
 * @param queue Queue to shedule IOHIDEventSystemClientRef with.
 *
 * @result instance of the IOHIDEventSystemTestController or nil.
 *
 */
-(nullable instancetype) initWithDeviceUniqueID: (nonnull id) uniqueID AndQueue:(nonnull dispatch_queue_t) queue;

/*!
 * @method -initWithDeviceUniqueID:::
 *
 * Init Event System Test controller
 *
 * @param uniqueID unique service ID to match service.
 *
 * @param queue Queue to shedule IOHIDEventSystemClientRef with.
 *
 * @param type client type
 *
 * @result instance of the IOHIDEventSystemTestController or nil.
 *
 */
-(nullable instancetype) initWithDeviceUniqueID: (nonnull id) uniqueID :(nonnull dispatch_queue_t) queue :(IOHIDEventSystemClientType) type;

/*!
 * @method -initWithMatching:AndQueue:
 *
 * Init Event System test controller
 *
 * @param matching Test Event Service matching criteria.
 *
 * @param queue Queue to shedule eventSystemClient with.
 *
 * @result instance of the IOHIDEventSystemTestController or nil.
 *
 */
-(nullable instancetype) initWithMatching: (nonnull NSDictionary *) matching AndQueue:(nonnull dispatch_queue_t) queue;

/*!
 * @method -initWithMatching:::
 *
 * Init Event System test controller
 *
 * @param matching Test Event Service matching criteria.
 *
 * @param queue Queue to shedule eventSystemClient with.
 *
 * @result instance of the IOHIDEventSystemTestController or nil.
 *
 */
-(nullable instancetype) initWithMatching: (nonnull NSDictionary *) matching :(nonnull dispatch_queue_t) queue :(IOHIDEventSystemClientType) type;

/*!
 * @method -GetEventsStats:
 *
 * Calculate various event stats
 *
 * @param events Array of the events.
 *
 * @result EVENTS_STATS with event stats.
 *
 */ 
+(EVENTS_STATS) getEventsStats : (nonnull NSArray *) events;

/*!
 * @method invalidate
 *
 * invalidate object.
 *
 */
-(void)invalidate;

/*!
 * @method addPropertyObserver:For:
 *
 * Add observer for porperty change.
 *
 * @param  observer Observer
 *
 * @param  key Property key
 */
-(void) addPropertyObserver : (id  <IOHIDPropertyObserver> _Nonnull) observer   For:(NSString * _Nonnull) key;

/*!
 * @method removePropertyObserver:For:
 *
 * Remove observer for porperty change.
 *
 * @param  observer Observer object
 *
 * @param  key Property key
 */
-(void) removePropertyObserver : (id <IOHIDPropertyObserver> _Nonnull) observer   For:(NSString * _Nonnull) key;

@end
