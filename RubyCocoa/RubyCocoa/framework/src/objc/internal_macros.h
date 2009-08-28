/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#ifndef _INTERNAL_MACROS_H_
#define _INTERNAL_MACROS_H_

#import <Foundation/Foundation.h>

#define RUBYCOCOA_SUPPRESS_EXCEPTION_LOGGING_P \
  RTEST(rb_gv_get("RUBYCOCOA_SUPPRESS_EXCEPTION_LOGGING"))

extern VALUE rubycocoa_debug;

#define RUBY_DEBUG_P      RTEST(ruby_debug)
#define RUBYCOCOA_DEBUG_P RTEST(rubycocoa_debug)
#define DEBUG_P           (RUBY_DEBUG_P || RUBYCOCOA_DEBUG_P)

#define ASSERT_ALLOC(x) do { if (x == NULL) rb_fatal("can't allocate memory"); } while (0)

#define DLOG(mod, fmt, args...)                  \
  do {                                           \
    if (DEBUG_P) {                             \
      NSAutoreleasePool * pool;                  \
      NSString *          nsfmt;                 \
                                                 \
      pool = [[NSAutoreleasePool alloc] init];   \
      nsfmt = [NSString stringWithFormat:        \
        @"%@",                                   \
        [NSString stringWithFormat:@"%s : %s",   \
          mod, fmt], ##args];                    \
      NSLog(@"%@", nsfmt);                       \
      [pool release];                            \
    }                                            \
  }                                              \
  while (0)

/* syntax: POOL_DO(the_pool) { ... } END_POOL(the_pool); */
#define POOL_DO(POOL)   { id POOL = [[NSAutoreleasePool alloc] init];
#define END_POOL(POOL)  [(POOL) release]; }

/* flag for calling Init_stack frequently */
extern int rubycocoa_frequently_init_stack();
#define FREQUENTLY_INIT_STACK_FLAG rubycocoa_frequently_init_stack()

extern NSThread *rubycocoaThread;
extern NSRunLoop *rubycocoaRunLoop;

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
#import <CoreFoundation/CFRunLoop.h>
extern CFRunLoopRef CFRunLoopGetMain(void);
#define DISPATCH_ON_RUBYCOCOA_THREAD(self, sel) \
  do { \
    assert(rubycocoaRunLoop != nil); \
    if ([rubycocoaRunLoop getCFRunLoop] != CFRunLoopGetMain()) \
      [[NSException exceptionWithName:@"DispatchRubyCocoaThreadError" message:@"cannot forward %s to %@ because RubyCocoa doesn't run in the main thread" userInfo:nil] raise]; \
    else \
      [self performSelectorOnMainThread:sel withObject:nil waitUntilDone:YES]; \
  } \
  while (0) 
#else
#define DISPATCH_ON_RUBYCOCOA_THREAD(self, sel) \
  do { \
    assert(rubycocoaThread != nil); \
    [self performSelector:sel onThread:rubycocoaThread withObject:nil waitUntilDone:YES]; \
  } \
  while (0)
#endif

#endif	// _INTERNAL_MACROS_H_
