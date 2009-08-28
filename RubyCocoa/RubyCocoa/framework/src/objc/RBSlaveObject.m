/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <Foundation/Foundation.h>
#import "RBObject.h"
#import "RBSlaveObject.h"
#import "RBClassUtils.h"
#import "osx_ruby.h"
#import "ocdata_conv.h"
#import "mdl_osxobjc.h"

static VALUE rbobj_for(VALUE rbclass, id master)
{
  static BOOL in_rbobj_for = NO;
  VALUE val;
  if (in_rbobj_for == YES) return Qnil;
  in_rbobj_for = YES;
  val = rb_funcall(rbclass, rb_intern("new_with_ocid"), 1, OCID2NUM(master));
  in_rbobj_for = NO;
  return val;
}

@implementation RBObject(RBSlaveObject)

- initWithMasterObject: master
{
  return [self initWithClass: [self class] masterObject: master];
}

- initWithClass: (Class)occlass masterObject: master
{
  VALUE rb_class = RBRubyClassFromObjcClass (occlass);
  self = [self initWithRubyClass: rb_class masterObject: master];
  oc_master = occlass;
  return self;
}

///////

- initWithRubyClass: (VALUE)rbclass masterObject: master
{
  VALUE rbobj;
  rbobj = rbobj_for(rbclass, master);
  return [self initWithRubyObject: rbobj];
}

@end
