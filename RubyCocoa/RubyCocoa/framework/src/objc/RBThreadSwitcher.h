/* 
 * Copyright (c) 2006-2007, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSTimer.h>
#import <sys/time.h>

@interface RBThreadSwitcher : NSObject
{
  id timer;
  struct timeval wait;
}
+ (void) start: (double)interval wait: (double) a_wait;
+ (void) start: (double)interval;
+ (void) start;
+ (void) stop;
@end
