// -*- mode:objc; indent-tabs-mode:nil; coding:utf-8 -*-
//
//  VPRubyPluginEnabler.m
//  RubyPluginEnabler
//
//  Created by Fujimoto Hisa on 07/02/02.
//  Copyright 2007 Fujimoto Hisa. All rights reserved.

#import <Cocoa/Cocoa.h>
#import <VPPlugin/VPPlugin.h>
#import <RubyCocoa/RubyCocoa.h>

@interface VPRubyPluginEnabler : VPPlugin
- (void) didRegister;
@end

@implementation VPRubyPluginEnabler
- (void) didRegister {
  static int installed = 0;
  if (! installed) {
    if (RBBundleInit("vpr_init.rb", [self class], self))
      NSLog(@"VPRubyPluginEnabler#didRegister failed.");
    else
      installed = 1;
  }
}
@end
