/* 
 * Copyright (c) 2006-2007, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import "cls_objcid.h"

#import "osx_ruby.h"
#import "ocdata_conv.h"
#import <Foundation/Foundation.h>
#import <string.h>
#import <stdlib.h>
#import "RBObject.h"
#import "internal_macros.h"
#import "BridgeSupport.h"
#import "mdl_osxobjc.h"

static VALUE _kObjcID = Qnil;

static void
_objcid_data_free(struct _objcid_data* dp)
{
  id pool = [[NSAutoreleasePool alloc] init];
  if (dp != NULL) {
    if (dp->ocid != nil) {
      remove_from_oc2rb_cache(dp->ocid);
      if (dp->retained && dp->can_be_released) {
        DLOG("CLSOBJ", "releasing %p", dp->ocid);
        [dp->ocid release];
      }
    }
    free(dp);
  }
  [pool release];
}

static struct _objcid_data*
_objcid_data_new()
{
  struct _objcid_data* dp;
  dp = malloc(sizeof(struct _objcid_data));
  dp->ocid = nil;
  dp->retained = NO;
  dp->can_be_released = NO;
  return dp;
}

static VALUE
objcid_s_new(int argc, VALUE* argv, VALUE klass)
{
  VALUE obj;
  obj = Data_Wrap_Struct(klass, NULL, _objcid_data_free, _objcid_data_new());
  rb_obj_call_init(obj, argc, argv);
  return obj;
}

VALUE
objcid_new_with_ocid(VALUE klass, id ocid)
{
  VALUE obj;

  obj = Data_Wrap_Struct(klass, 0, _objcid_data_free, _objcid_data_new());

  // The retention of the ObjC instance is delayed in ocm_send, to not
  // violate the "init-must-follow-alloc" initialization pattern.
  // Retaining here could message in the middle. 
  if (ocid != nil) {
    OBJCID_DATA_PTR(obj)->ocid = ocid;
    OBJCID_DATA_PTR(obj)->retained = NO;
  }

  rb_obj_call_init(obj, 0, NULL);
  return obj;
}

static VALUE
wrapper_objcid_s_new_with_ocid(VALUE klass, VALUE rbocid)
{
  return objcid_new_with_ocid(klass, NUM2OCID(rbocid));
}

static VALUE
objcid_release(VALUE rcv)
{
  if (OBJCID_DATA_PTR(rcv)->can_be_released) {
    [OBJCID_ID(rcv) release];
    OBJCID_DATA_PTR(rcv)->can_be_released = NO;
  }
  return rcv;
}

static VALUE
objcid_initialize(int argc, VALUE* argv, VALUE rcv)
{
  return rcv;
}

static VALUE
objcid_ocid(VALUE rcv)
{
  return OCID2NUM(OBJCID_ID(rcv));
}

static VALUE
objcid_inspect(VALUE rcv)
{
  char              s[512];
  id                ocid;
  struct bsConst *  bs_const;
  const char *      class_desc;
  id                pool;

  ocid = OBJCID_ID(rcv);
  bs_const = find_magic_cookie_const_by_value(ocid);
  if (bs_const != NULL) {
    pool = nil;
    class_desc = bs_const->class_name;
  }
  else {
    pool = [[NSAutoreleasePool alloc] init];
    class_desc = [[[ocid class] description] UTF8String];
  }

  snprintf(s, sizeof(s), "#<%s:0x%lx class='%s' id=%p>",
    rb_class2name(CLASS_OF(rcv)),
    NUM2ULONG(rb_obj_id(rcv)), 
    class_desc, ocid);

  if (pool != nil)
    [pool release];

  return rb_str_new2(s);
}

/** class methods **/

VALUE
objid_s_class ()
{
  return _kObjcID;
}

/*******/

VALUE
init_cls_ObjcID(VALUE outer)
{
  _kObjcID = rb_define_class_under(outer, "ObjcID", rb_cObject);

  rb_define_singleton_method(_kObjcID, "new", objcid_s_new, -1);
  rb_define_singleton_method(_kObjcID, "new_with_ocid", wrapper_objcid_s_new_with_ocid, 1);

  rb_define_method(_kObjcID, "initialize", objcid_initialize, -1);
  rb_define_method(_kObjcID, "__ocid__", objcid_ocid, 0);
  rb_define_method(_kObjcID, "__inspect__", objcid_inspect, 0);
  rb_define_method(_kObjcID, "release", objcid_release, 0);
  rb_define_method(_kObjcID, "inspect", objcid_inspect, 0);

  return _kObjcID;
}
