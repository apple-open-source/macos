/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 @class msdosProgressHelper
 @abstract Class to help manage phases of work, Especially helpful in a C/CF bridged environment
 @discussion This class serves to help work with nested NSProgress objects in a C/CF environment. It acts as function calls and a context.
 */
@interface msdosProgressHelper : NSObject

- (instancetype)init NS_UNAVAILABLE;

- (nullable instancetype)initWithProgress:(NSProgress *)progress;

/*
 * startPhase: and endPhase:
 *
 *      Start or end a phase of our overall progress object. This phase will
 * consume `parentUnitCount` of the parent's work. For instance, if one phase
 * of work represents 20 of the parent's 100 units, this parameter would be 20.
 * This phase of work will, itself, have `phaseTotalCount` items of work.
 *
 *      This API was designed to support being used to wrap an existing, unmodified
 * for loop as in:
 *
 *  unsigned int loop_counter = 0;
 *  [helper startPhase:... completedCounter:&loop_counter];
 *  for(cluster_num = 0; cluster_num < number_of_clusters_to_process; cluster_num++) {
 *      loop_counter = cluster_num;
 *      .. do work
 *  }
 *  [helper endPhase:...];
 *
 *      Currently we don't use it as such, so we don't use completedCounter.
 */

/**
 @method startPhase:parentUnitCount:phaseTotalCount:completedCounter: - start a phase of work on the parent
 @param description string describing the work being done
 @param parentUnitCount count of units in the parent work space being done in this phase
 @param phaseTotalCount count of ujits of work in this sub-phase
 @param completedCounter pointer to an unsigned int storing the current step of work
 */
- (NSError* _Nullable)startPhase:(NSString *)description
                 parentUnitCount:(int64_t)parentUnitCount
                 phaseTotalCount:(int64_t)phaseTotalCount
                completedCounter:(const unsigned int *)completedCounter;

- (void)endPhase:(NSString *)description;

@property (retain)              NSProgress  *parentProgress;
@property (nullable, retain)    NSProgress  *childProgress;

@end

NS_ASSUME_NONNULL_END
