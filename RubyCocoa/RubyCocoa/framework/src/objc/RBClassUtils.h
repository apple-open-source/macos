/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <objc/objc.h>
#import <Foundation/NSObject.h>
#import "osx_ruby.h"

Class RBObjcClassFromRubyClass (VALUE kls);
VALUE RBRubyClassFromObjcClass (Class cls);

Class RBObjcClassNew(VALUE kls, const char* name, Class super_class);
Class RBObjcDerivedClassNew(VALUE kls, const char* name, Class super_class);

BOOL is_objc_derived_class(VALUE kls);

Class objc_class_alloc(const char* name, Class super_class);

@interface NSObject(RBOverrideMixin)
- __slave__;
- (VALUE) __rbobj__;
+ addRubyMethod: (SEL)a_sel;
+ addRubyMethod: (SEL)a_sel withType:(const char *)typefmt;
@end
