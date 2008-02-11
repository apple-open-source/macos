/* 
 * Copyright (c) 2006-2007, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import "mdl_osxobjc.h"
#import "osx_ruby.h"
#import <Foundation/Foundation.h>
#import "RubyCocoa.h"
#import "Version.h"
#import "RBThreadSwitcher.h"
#import "RBObject.h"
#import "RBClassUtils.h"
#import "ocdata_conv.h"
#import <mach-o/dyld.h>
#import <string.h>
#import "BridgeSupport.h"
#import <objc/objc-runtime.h>
#import "cls_objcid.h"
#import "objc_compat.h"
#import "OverrideMixin.h"

#define OSX_MODULE_NAME "OSX"

static VALUE _cOCObject = Qnil;
ID _relaxed_syntax_ID;

static VALUE init_module_OSX()
{
  VALUE module;
  RB_ID id_osx = rb_intern(OSX_MODULE_NAME);

  if (rb_const_defined(rb_cObject, id_osx))
    module = rb_const_get(rb_cObject, id_osx);
  else
    module = rb_define_module(OSX_MODULE_NAME);
  return module;
}

static VALUE init_cls_OCObject(VALUE mOSX)
{
  VALUE kObjcID;
  VALUE kOCObject;
  VALUE mOCObjWrapper;

  kObjcID = rb_const_get(mOSX, rb_intern("ObjcID"));
  kOCObject = rb_define_class_under(mOSX, "OCObject", kObjcID);
  mOCObjWrapper = rb_const_get(mOSX, rb_intern("OCObjWrapper"));
  rb_include_module(kOCObject, mOCObjWrapper);

  return kOCObject;
}

// def OSX.objc_proxy_class_new (kls, kls_name)
// ex1.  OSX.objc_proxy_class_new (AA::BB::AppController, "AppController")
static VALUE
osx_mf_objc_proxy_class_new(VALUE mdl, VALUE kls, VALUE kls_name)
{
  kls_name = rb_obj_as_string(kls_name);
  RBObjcClassNew(kls, STR2CSTR(kls_name), [RBObject class]);
  return Qnil;
}

// def OSX.objc_derived_class_new (kls, kls_name, super_name)
// ex1.  OSX.objc_derived_class_new (AA::BB::CustomView, "CustomView", "NSView")
static VALUE
osx_mf_objc_derived_class_new(VALUE mdl, VALUE kls, VALUE kls_name, VALUE super_name)
{
  Class super_class;
  Class new_cls = nil;

  kls_name = rb_obj_as_string(kls_name);
  super_name = rb_obj_as_string(super_name);
  super_class = objc_getClass(STR2CSTR(super_name));
  if (super_class)
    new_cls = RBObjcDerivedClassNew(kls, STR2CSTR(kls_name), super_class);

  if (new_cls)
    return ocobj_s_new(new_cls);
  return Qnil;
}

// def OSX.objc_class_method_add (kls, method_name)
// ex1.  OSX.objc_class_method_add (AA::BB::CustomView, "drawRect:")
static VALUE
osx_mf_objc_class_method_add(VALUE mdl, VALUE kls, VALUE method_name, VALUE class_method, VALUE types)
{
  Class a_class;
  SEL a_sel;
  char *kls_name;
  BOOL direct_override;

  method_name = rb_obj_as_string(method_name);
  a_sel = sel_registerName(STR2CSTR(method_name));
  if (a_sel == NULL)
    return Qnil;
  kls_name = rb_class2name(kls);
  if (strncmp(kls_name, "OSX::", 5) == 0 
      && (a_class = objc_lookUpClass(kls_name + 5)) != NULL 
      && !is_objc_derived_class(kls)) {
    // override in the current class
    direct_override = YES;
  }
  else {
    // override in the super class 
    a_class = RBObjcClassFromRubyClass(kls);
    direct_override = NO;
  }
  if (a_class != NULL) {
    id rcv;

    rcv = RTEST(class_method) ? a_class->isa : a_class;
    if (NIL_P(types))
      ovmix_register_ruby_method(rcv, a_sel, direct_override);
    else
      [rcv addRubyMethod:a_sel withType:STR2CSTR(types)];
  }
  return Qnil;
}

static VALUE
osx_mf_ruby_thread_switcher_start(int argc, VALUE* argv, VALUE mdl)
{
  VALUE arg_interval, arg_wait;
  double interval, wait;

  rb_scan_args(argc, argv, "02", &arg_interval, &arg_wait);

  if (arg_interval == Qnil) {
    [RBThreadSwitcher start];
  }
  else {
    Check_Type(arg_interval, T_FLOAT);
    interval = RFLOAT(arg_interval)->value;

    if (arg_wait == Qnil) {
      [RBThreadSwitcher start: interval];
    }
    else {
      Check_Type(arg_wait, T_FLOAT);
      wait = RFLOAT(arg_wait)->value;
      [RBThreadSwitcher start: interval wait: wait];
    }
  }
  return Qnil;
}

static VALUE
osx_mf_ruby_thread_switcher_stop(VALUE mdl)
{
  [RBThreadSwitcher stop];
  return Qnil;
}

static VALUE
ns_autorelease_pool(VALUE mdl)
{
  id pool = [[NSAutoreleasePool alloc] init];
  rb_yield(Qnil);
  [pool release];
  return Qnil;
}

static void
thread_switcher_start()
{
  [RBThreadSwitcher start];
}

/******************/

VALUE
rb_osx_class_const (const char* name)
{
  VALUE mOSX;
  VALUE constant;
  ID name_id;
 
  if (strlen(name) == 0)
    return Qnil;

  mOSX = osx_s_module();
  if (NIL_P(mOSX)) 
    return Qnil;

  name_id = rb_intern(name);
  if (!rb_is_const_id(name_id)) {
    // If the class name can't be a constant, let's use the superclass name.
    Class klass = objc_getClass(name);
    if (klass != NULL) {
      Class superklass = class_getSuperclass(klass);
      if (superklass != NULL)
        return rb_osx_class_const(class_getName(superklass));
    }
 
    return Qnil;
  }

  // Get the class constant, triggering an import if necessary.
  // Don't import the class if we are called within NSClassFromString, just return the constant 
  // if it exists (otherwise it would cause an infinite loop).
  if (rb_const_defined(mOSX, name_id)) {
    constant = rb_const_get(mOSX, name_id);
  }
  else if (current_function == NULL || strcmp(current_function->name, "NSClassFromString") != 0) {
    constant = rb_funcall(mOSX, rb_intern("ns_import"), 1, rb_str_new2(name));
  }
  else {
    constant = Qnil;
  }

  return constant;
}

VALUE
ocobj_s_class (void)
{
  return _cOCObject;
}

VALUE
rb_cls_ocobj (const char* name)
{
  VALUE cls = rb_osx_class_const(name);
  if (cls == Qnil) 
    cls = _cOCObject;
  return cls;
}

static id
rb_obj_ocid(VALUE rcv)
{
  VALUE val = rb_funcall(rcv, rb_intern("__ocid__"), 0);
  return NUM2OCID(val);
}

static VALUE
osx_mf_objc_symbol_to_obj(VALUE mdl, VALUE const_name, VALUE const_type)
{
  rb_raise(rb_eRuntimeError, "#objc_symbol_to_obj has been obsoleted");
  return Qnil;
}

/***/

VALUE
osx_s_module()
{
  RB_ID rid;

  rid = rb_intern("OSX");
  if (! rb_const_defined(rb_cObject, rid))
    return rb_define_module("OSX");
  return rb_const_get(rb_cObject, rid);
}

VALUE
ocobj_s_new_with_class_name(id ocid, const char *cls_name)
{
  // Try to determine from the metadata if a given NSCFType object cannot be promoted to a better class.
  if (strcmp(cls_name, "NSCFType") == 0) {
    struct bsCFType *bs_cf_type;
    
    bs_cf_type = find_bs_cf_type_by_type_id(CFGetTypeID((CFTypeRef)ocid));
    if (bs_cf_type != NULL)
      cls_name = bs_cf_type->bridged_class_name;
  }
  
  return objcid_new_with_ocid(rb_cls_ocobj(cls_name), ocid);
}

VALUE
ocobj_s_new(id ocid)
{
  return ocobj_s_new_with_class_name(ocid, object_getClassName(ocid));
}

id
rbobj_get_ocid (VALUE obj)
{
  RB_ID mtd;

  if (rb_obj_is_kind_of(obj, objid_s_class()) == Qtrue)
    return OBJCID_ID(obj);

  mtd = rb_intern("__ocid__");
  if (rb_respond_to(obj, mtd))
    return rb_obj_ocid(obj);

#if 0
  if (rb_respond_to(obj, rb_intern("to_nsobj"))) {
    VALUE nso = rb_funcall(obj, rb_intern("to_nsobj"), 0);
    return rb_obj_ocid(nso);
  }
#endif

  return nil;
}

VALUE
ocid_get_rbobj (id ocid)
{
  VALUE result = Qnil;

  @try {  
    if (([ocid isProxy] && [ocid isRBObject])
        || [ocid respondsToSelector:@selector(__rbobj__)])
      result = [ocid __rbobj__];
  } 
  @catch (id exception) {}

  return result;
}

// FIXME: this is a silly hack.

struct RB_METHOD {
  VALUE klass, rklass;
  // ...
};

static VALUE
osx_mf_rebind_umethod(VALUE rcv, VALUE klass, VALUE umethod)
{
  struct RB_METHOD *data;

  Data_Get_Struct(umethod, struct RB_METHOD, data);
  data->rklass = klass;
  
  return Qnil;
}

static VALUE
osx_rbobj_to_nsobj (VALUE rcv, VALUE obj)
{
  id ocid, pool;
  VALUE val;

  pool = [[NSAutoreleasePool alloc] init];
  if (!rbobj_to_nsobj(obj, &ocid) || ocid == nil) {
    [pool release];
    return Qnil;
  }

  val = ocid_to_rbobj(Qnil, ocid);
  [ocid retain];
  OBJCID_DATA_PTR(val)->retained = YES;
  OBJCID_DATA_PTR(val)->can_be_released = YES;

  [pool release];

  return val;
}

NSThread *rubycocoaThread;
NSRunLoop *rubycocoaRunLoop;

/******************/

void initialize_mdl_osxobjc()
{
  char* framework_resources_path();
  VALUE mOSX;

  mOSX = init_module_OSX();
  init_cls_ObjcPtr(mOSX);
  init_cls_ObjcID(mOSX);
  init_mdl_OCObjWrapper(mOSX);
  _cOCObject = init_cls_OCObject(mOSX);

  _relaxed_syntax_ID = rb_intern("@relaxed_syntax");
  rb_ivar_set(mOSX, _relaxed_syntax_ID, Qtrue);

  rb_define_module_function(mOSX, "objc_proxy_class_new", 
			    osx_mf_objc_proxy_class_new, 2);
  rb_define_module_function(mOSX, "objc_derived_class_new", 
			    osx_mf_objc_derived_class_new, 3);
  rb_define_module_function(mOSX, "objc_class_method_add",
			    osx_mf_objc_class_method_add, 4);

  rb_define_module_function(mOSX, "ruby_thread_switcher_start",
			    osx_mf_ruby_thread_switcher_start, -1);
  rb_define_module_function(mOSX, "ruby_thread_switcher_stop",
			    osx_mf_ruby_thread_switcher_stop, 0);

  rb_define_module_function(mOSX, "ns_autorelease_pool",
			    ns_autorelease_pool, 0);

  rb_define_const(mOSX, "RUBYCOCOA_VERSION", 
		  rb_obj_freeze(rb_str_new2(RUBYCOCOA_VERSION)));
  rb_define_const(mOSX, "RUBYCOCOA_RELEASE_DATE", 
		  rb_obj_freeze(rb_str_new2(RUBYCOCOA_RELEASE_DATE)));
  rb_define_const(mOSX, "RUBYCOCOA_SVN_REVISION", 
		  rb_obj_freeze(rb_str_new2(RUBYCOCOA_SVN_REVISION)));

  char *p = framework_resources_path();
  rb_define_const(mOSX, "RUBYCOCOA_RESOURCES_PATH",
		  rb_obj_freeze(rb_str_new2(p)));
  free(p);

  rb_define_const(mOSX, "RUBYCOCOA_SIGN_PATHS", rb_ary_new());
  rb_define_const(mOSX, "RUBYCOCOA_FRAMEWORK_PATHS", rb_ary_new());

  rb_define_module_function(mOSX, "objc_symbol_to_obj", osx_mf_objc_symbol_to_obj, 2);

  rb_define_module_function(mOSX, "__rebind_umethod__", osx_mf_rebind_umethod, 2);

  rb_define_module_function(mOSX, "rbobj_to_nsobj", osx_rbobj_to_nsobj, 1);
  
  thread_switcher_start();
  
  initialize_bridge_support(mOSX);

  rubycocoaThread = [NSThread currentThread];
  rubycocoaRunLoop = [NSRunLoop currentRunLoop];
}
