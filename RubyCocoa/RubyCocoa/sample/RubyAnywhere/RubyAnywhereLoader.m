// -*- mode:objc; indent-tabs-mode:nil; coding:utf-8 -*-
//
//  RubyAnywhereLoader.m
//  RubyAnywhere
//
//  Created by Fujimoto Hisa on 07/02/01.
//  Copyright 2007 FOBJ SYSTEMS. All rights reserved.

#import <RubyCocoa/RubyCocoa.h>

@interface RubyAnywhereLoader : NSObject
+ (void) install;
@end

@implementation RubyAnywhereLoader
+ (void) install {
  static int installed = 0;
  if (! installed) {
    if (RBBundleInit("ruby_anywhere_init.rb", self, nil))
      NSLog(@"RubyAnywhereLoader.install: failed");
    else
      installed = 1;
  }
}
@end
