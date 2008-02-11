/* 
 * Copyright (c) 2006-2007, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <objc/objc-class.h>
#import <Foundation/NSObject.h>
#import "osx_ruby.h"

void init_ovmix(void);

void install_ovmix_ivars(Class c);
void install_ovmix_methods(Class c);
void install_ovmix_class_methods(Class c);
void install_ovmix_hooks(Class c);

void release_slave(id rcv);

void ovmix_register_ruby_method(Class klass, SEL method, BOOL override);

@interface NSObject (__rbobj__)
+ (VALUE)__rbclass__;
@end
