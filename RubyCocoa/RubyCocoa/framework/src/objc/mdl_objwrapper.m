/* 
 * Copyright (c) 2006-2007, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import "osx_ruby.h"
#import "ocdata_conv.h"
#import "mdl_osxobjc.h"
#import <Foundation/Foundation.h>
#import <string.h>
#import <stdlib.h>
#import <stdarg.h>
#import <objc/objc-runtime.h>
#import "BridgeSupport.h"
#import "internal_macros.h"
#import "ocexception.h"
#import "objc_compat.h"

#define OCM_AUTO_REGISTER 1

static VALUE _mObjWrapper = Qnil;
static VALUE _mClsWrapper = Qnil;

static VALUE wrapper_ocm_send(int argc, VALUE* argv, VALUE rcv);

#define OBJWRP_LOG(fmt, args...) DLOG("OBJWRP", fmt, ##args)

struct _ocm_retain_context {
  VALUE rcv;
  SEL selector;
};

static void
ocm_retain_arg_if_necessary (VALUE result, BOOL is_result, void *context)
{
  VALUE rcv = ((struct _ocm_retain_context *)context)->rcv;
  SEL selector = ((struct _ocm_retain_context *)context)->selector;

  // Retain if necessary the returned ObjC value unless it was generated 
  // by "alloc/allocWithZone/new/copy/mutableCopy". 
  // Some classes may always return a static dummy object (placeholder) for
  // every [-alloc], so we shouldn't release the return value of these 
  // messages.
  if (!NIL_P(result) && rb_obj_is_kind_of(result, objid_s_class()) == Qtrue) {
    if (!OBJCID_DATA_PTR(result)->retained
        && selector != @selector(alloc)
        && selector != @selector(allocWithZone:)
        && selector != @selector(new)
        && selector != @selector(copy)
        && selector != @selector(mutableCopy)) {

      if (!is_result
          || NIL_P(rcv)
          || strncmp((const char *)selector, "init", 4) != 0
          || OBJCID_ID(rcv) == OBJCID_ID(result)
          || !OBJCID_DATA_PTR(rcv)->retained) { 

        OBJWRP_LOG("retaining %p", OBJCID_ID(result));  
        [OBJCID_ID(result) retain];
      }
    }
    // We assume that the object is retained at that point.
    OBJCID_DATA_PTR(result)->retained = YES; 
    if (selector != @selector(alloc) && selector != @selector(allocWithZone:)) {
      OBJCID_DATA_PTR(result)->can_be_released = YES;
    }
    // Objects that come from an NSObject-based class defined in Ruby have a
    // slave object as an instance variable that serves as the message proxy.
    // However, this RBObject retains the Ruby instance by default, which isn't
    // what we want, because this is a retain circle, and both objects will
    // leak. So we manually release the Ruby object from the slave, so that
    // when the Ruby object will be collected by the Ruby GC, the ObjC object
    // will be properly auto-released.
    //
    // We only do this magic for objects that are explicitely allocated from
    // Ruby.
    if ([OBJCID_ID(result) respondsToSelector:@selector(__trackSlaveRubyObject)]) {
      [OBJCID_ID(result) performSelector:@selector(__trackSlaveRubyObject)];
    }
  }
}

struct ocm_closure_userdata 
{
  VALUE mname;
  VALUE is_predicate;
};

static void
ocm_closure_handler(ffi_cif *cif, void *resp, void **args, void *userdata)
{
  VALUE rcv, argv, mname, is_predicate;

  OBJWRP_LOG("ocm_closure_handler ...");

  rcv = (*(VALUE **)args)[0];
  argv = (*(VALUE **)args)[1];
  mname = ((struct ocm_closure_userdata *)userdata)->mname;
  is_predicate = ((struct ocm_closure_userdata *)userdata)->is_predicate;

  rb_ary_unshift(argv, is_predicate);
  rb_ary_unshift(argv, Qnil);
  rb_ary_unshift(argv, mname);

  *(VALUE *)resp = wrapper_ocm_send(RARRAY(argv)->len, RARRAY(argv)->ptr, rcv); 
  
  OBJWRP_LOG("ocm_closure_handler ok");
}

static void *
ocm_ffi_closure(VALUE mname, VALUE is_predicate)
{
  static ffi_cif *cif = NULL;
  ffi_closure *closure;
  struct ocm_closure_userdata *userdata;

  if (cif == NULL) {
    static ffi_type *args[3];

    cif = (ffi_cif *)malloc(sizeof(ffi_cif));
    ASSERT_ALLOC(cif);

    args[0] = &ffi_type_pointer;
    args[1] = &ffi_type_pointer;
    args[2] = NULL;   

    if (ffi_prep_cif(cif, FFI_DEFAULT_ABI, 2, &ffi_type_pointer, args) 
        != FFI_OK) {
      free(cif);
      return NULL;
    } 
  }

  closure = (ffi_closure *)malloc(sizeof(ffi_closure));
  ASSERT_ALLOC(closure);
 
  userdata = (struct ocm_closure_userdata *)malloc(
    sizeof(struct ocm_closure_userdata));
  ASSERT_ALLOC(userdata);

  userdata->mname = mname;
  userdata->is_predicate = is_predicate;

  if (ffi_prep_closure(closure, cif, ocm_closure_handler, userdata) 
      != FFI_OK)
    return NULL;

  return closure; 
}

static BOOL ignore_ns_override = NO;

static VALUE
wrapper_ignore_ns_override (VALUE rcv)
{
  return ignore_ns_override ? Qtrue : Qfalse;
}

static void
ocm_register(Class klass, VALUE oc_mname, VALUE rb_mname, VALUE is_predicate,
  SEL selector, BOOL is_class_method)
{
  Class c;
  Method (*getMethod)(Class, SEL);
  VALUE rclass;
  RB_ID rclass_id;
  void *closure;
  char *rb_mname_str;
 
  // Let's locate the original class where the method is defined.
  getMethod = is_class_method ? class_getClassMethod : class_getInstanceMethod;
  while ((c = class_getSuperclass(klass)) != NULL 
         && (*getMethod)(c, selector) != NULL) { 
    klass = c; 
  }

  // Find the class.
  rclass_id = rb_intern(class_getName(klass));
  if (!rb_const_defined(osx_s_module(), rclass_id)
      || (rclass = rb_const_get(osx_s_module(), rclass_id)) == Qnil) {
    OBJWRP_LOG("cannot register Ruby method (problem when getting class)");
    return;
  }

  // Create the closure.
  closure = ocm_ffi_closure(oc_mname, is_predicate);
  if (closure == NULL) {
    OBJWRP_LOG("cannot register Ruby method (problem when creating closure)");
    return;
  }

  rb_mname_str = rb_id2name(SYM2ID(rb_mname));
  OBJWRP_LOG("registering Ruby %s method `%s' on `%s'", 
    is_class_method ? "class" : "instance", rb_mname_str, 
    rb_class2name(rclass));

  // Map.
  ignore_ns_override = YES;
  if (is_class_method)
    rb_define_singleton_method(rclass, rb_mname_str, closure, -2); 
  else
    rb_define_method(rclass, rb_mname_str, closure, -2);
  ignore_ns_override = NO;

  // This is a dirty trick to make sure the mname object won't be collected.
  {
    RB_ID   mname_id;
    VALUE   ary;

    mname_id = rb_intern("@__mnames__");
    if (rb_ivar_defined(rclass, mname_id) == Qtrue) {
      ary = rb_ivar_get(rclass, mname_id);
    }
    else {
      ary = rb_ary_new();
      rb_ivar_set(rclass, mname_id, ary);
    }
    rb_ary_push(ary, oc_mname);
  } 

  OBJWRP_LOG("registered Ruby %s method `%s' on `%s'", 
    is_class_method ? "class" : "instance", rb_mname_str, 
    rb_class2name(rclass));
}

static VALUE
ocm_send(int argc, VALUE* argv, VALUE rcv, VALUE* result)
{
  SEL                   selector;
  NSAutoreleasePool *   pool;
  id                    oc_rcv;
  Class                 klass;
  Method                method;
  IMP                   imp;
  NSMethodSignature *   methodSignature;
  unsigned              numberOfArguments;
  unsigned              expected_argc;
  char *                methodReturnType;
  char **               argumentsTypes;
  BOOL                  is_class_method;
  struct bsMethod *     bs_method;
  ffi_type **           arg_types;
  void **               arg_values;
  VALUE                 exception;

  if (argc < 3) 
    return Qfalse;

  pool = [[NSAutoreleasePool alloc] init];

  selector = rbobj_to_nssel(argv[0]);
  exception = Qnil;

  methodReturnType = NULL;
  argumentsTypes = NULL;

  oc_rcv = rbobj_get_ocid(rcv);
  if (oc_rcv == nil) {
    exception = rb_err_new(ocmsgsend_err_class(), "Can't get Objective-C object in %s", RSTRING(rb_inspect(rcv))->ptr);
    goto bails;
  }

  klass = object_getClass(oc_rcv);
  method = class_getInstanceMethod(klass, selector); 
  if (method == NULL) {
    // If we can't get the method signature via the ObjC runtime, let's try the NSObject API,
    // as the target class may override the invocation dispatching methods (as NSUndoManager).
    methodSignature = [oc_rcv methodSignatureForSelector:selector];
    if (methodSignature == nil) {
      exception = rb_err_new(ocmsgsend_err_class(), "Can't get Objective-C method signature for selector '%s' of receiver %s", (char *) selector, RSTRING(rb_inspect(rcv))->ptr);
      goto bails;
    }
    // Let's use the regular message dispatcher.
    imp = objc_msgSend;
  }
  else {
    methodSignature = nil;
    imp = method_getImplementation(method);
  }

  decode_method_encoding(method != NULL ? method_getTypeEncoding(method) : NULL, methodSignature, &numberOfArguments, &methodReturnType, &argumentsTypes, YES);

  // force predicate conversion if required
  if ((*methodReturnType == _C_UCHR || *methodReturnType == _C_CHR)
      && RTEST(argv[2]))
    *methodReturnType = 'B'; // _C_BOOL

  struct _ocm_retain_context context = { rcv, selector };

  is_class_method = TYPE(rcv) == T_CLASS;
  if (is_class_method)
    klass = (Class)oc_rcv;

#if OCM_AUTO_REGISTER 
  if (!NIL_P(argv[1])
      && !rb_obj_is_kind_of(rcv, ocobj_s_class())
      && method != NULL 
      && rb_respond_to(rcv, SYM2ID(argv[1])) == 0)
    ocm_register(klass, argv[0], argv[1], argv[2], selector, is_class_method);
#endif

  argc--; // skip objc method name
  argv++;
  argc--; // skip ruby method name
  argv++;
  argc--; // skip is predicate flag
  argv++;

  OBJWRP_LOG("ocm_send (%s%c%s): args_count=%d ret_type=%s", class_getName(klass), is_class_method ? '.' : '#', selector, argc, methodReturnType);

  // Easy case: a method returning ID (or nothing) _and_ without argument.
  // We don't need libffi here, we can just call it (faster).
  if (numberOfArguments == 0 
      && (*methodReturnType == _C_VOID || *methodReturnType == _C_ID || *methodReturnType == _C_CLASS)) {

    id  val;

    exception = Qnil;
    @try {
      OBJWRP_LOG("direct call easy method %s imp %p", (const char *)selector, imp);
      val = (*imp)(oc_rcv, selector);
    }
    @catch (id oc_exception) {
      OBJWRP_LOG("got objc exception '%@' -- forwarding...", oc_exception);
      exception = oc_err_new(oc_exception);
    }

    if (NIL_P(exception)) {
      if (*methodReturnType != _C_VOID) {
        /* Theoretically, ObjC objects should be removed from the oc2rb
           cache upon dealloc, but it is possible to lose some of them when
           they are allocated within a thread that is directly killed. */
        if (selector == @selector(alloc))
          remove_from_oc2rb_cache(val);
          
        OBJWRP_LOG("got return value %p", val);
        if (!ocdata_to_rbobj(rcv, methodReturnType, (const void *)&val, result, NO)) {
          exception = rb_err_new(ocdataconv_err_class(), "Cannot convert the result as '%s' to Ruby", methodReturnType);
        }
        else {
          ocm_retain_arg_if_necessary(*result, YES, &context);
        }
      }
      else {
        *result = Qnil;
      }
    }
    else {
      *result = Qnil;
    }
    goto success;
  }

  expected_argc = numberOfArguments;

  bs_method = find_bs_method(klass, (const char *) selector, is_class_method); 
  if (bs_method != NULL) {
    OBJWRP_LOG("found metadata description\n");
    if (bs_method->ignore) {
      exception = rb_err_new(rb_eRuntimeError, "Method '%s' is not supported (suggested alternative: '%s')", selector, bs_method->suggestion != NULL ? bs_method->suggestion : "n/a");
      goto bails;
    }
    if (bs_method->is_variadic && argc > numberOfArguments) {
      unsigned i;
      VALUE format_str;      

      expected_argc = argc;
      format_str = Qnil;
      argumentsTypes = (char **)realloc(argumentsTypes, sizeof(char *) * argc);
      ASSERT_ALLOC(argumentsTypes);

      for (i = 0; i < bs_method->argc; i++) {
        struct bsArg *bs_arg = &bs_method->argv[i];
        if (bs_arg->printf_format) {
          assert(bs_arg->index < argc);
          format_str = argv[bs_arg->index];
        }
      }

      if (NIL_P(format_str)) {
        for (i = numberOfArguments; i < argc; i++)
          argumentsTypes[i] = "@"; // _C_ID
      }
      else {
        set_octypes_for_format_str(&argumentsTypes[numberOfArguments],
          argc - numberOfArguments, STR2CSTR(format_str));
      }
    }
  }

  arg_types = (ffi_type **) alloca((expected_argc + 3) * sizeof(ffi_type *));
  ASSERT_ALLOC(arg_types);
  arg_values = (void **) alloca((expected_argc + 3) * sizeof(void *));
  ASSERT_ALLOC(arg_values);

  arg_types[0] = &ffi_type_pointer;
  arg_types[1] = &ffi_type_pointer;
  arg_values[0] = &oc_rcv;
  arg_values[1] = &selector;

  memset(arg_types + 2, 0, (expected_argc + 1) * sizeof(ffi_type *));
  memset(arg_values + 2, 0, (expected_argc + 1) * sizeof(void *));

  exception = rb_ffi_dispatch(
    (struct bsCallEntry *)bs_method, 
    argumentsTypes, 
    expected_argc, 
    argc, 
    2, 
    argv, 
    arg_types, 
    arg_values, 
    methodReturnType, 
    imp, 
    ocm_retain_arg_if_necessary, 
    &context, 
    result);

success:
  OBJWRP_LOG("ocm_send (%s) done%s", (const char *)selector, NIL_P(exception) ? "" : " with exception");

bails:
  if (methodReturnType != NULL)
    free(methodReturnType);
  if (argumentsTypes != NULL) {
    unsigned i;
    for (i = 0; i < numberOfArguments; i++)
      free(argumentsTypes[i]);
    free(argumentsTypes);
  }

  [pool release];

  return exception;
}

/*************************************************/

static VALUE
wrapper_ocm_responds_p(VALUE rcv, VALUE sel)
{
  id oc_rcv = rbobj_get_ocid(rcv);
  SEL oc_sel = rbobj_to_nssel(sel);
  return [oc_rcv respondsToSelector: oc_sel] ? Qtrue : Qfalse;
}

#if 0
// Disabled, because we don't have a working implementation for systems
// equal or below than Tiger.
static VALUE
wrapper_ocm_conforms_p(VALUE rcv, VALUE name)
{
  Protocol *protocol = objc_getProtocol(STR2CSTR(name));
  if (protocol == NULL)
    rb_raise(rb_eArgError, "Invalid protocol name `%s'", STR2CSTR(name));
  return class_conformsToProtocol(rbobj_get_ocid(rcv), protocol) ? Qtrue : Qfalse;
}
#endif

static VALUE
wrapper_ocm_send(int argc, VALUE* argv, VALUE rcv)
{
  VALUE result;
  VALUE exc;
  exc = ocm_send(argc, argv, rcv, &result);
  if (!NIL_P(exc)) {
    if (exc == Qfalse)
      exc = rb_err_new(ocmsgsend_err_class(), "cannot forward message.");
    rb_exc_raise (exc);
  }
  return result;
}

static VALUE
wrapper_to_s (VALUE rcv)
{
  VALUE ret;
  id oc_rcv;
  id pool;

  oc_rcv = rbobj_get_ocid(rcv);
  pool = [[NSAutoreleasePool alloc] init];
  oc_rcv = [oc_rcv description];
  ret = ocstr_to_rbstr(oc_rcv);
  [pool release];
  return ret;
}

static void
_ary_push_objc_methods (VALUE ary, Class cls, int recur)
{
  Class superclass = class_getSuperclass(cls);
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_4
  Method *methods;
  unsigned int i, count;
  methods = class_copyMethodList(cls, &count);
  for (i = 0; i < count; i++)
    rb_ary_push(ary, rb_str_new2((const char *)method_getName(methods[i])));
  free(methods);
#else
  void* iterator = NULL;
  struct objc_method_list* list;

  while (list = class_nextMethodList(cls, &iterator)) {
    int i;
    struct objc_method* methods = list->method_list;
    
    for (i = 0; i < list->method_count; i++) {
      rb_ary_push (ary, rb_str_new2((const char*)(methods[i].method_name)));
    }
  }
#endif

  if (recur && superclass != NULL && !class_isMetaClass(cls))
    _ary_push_objc_methods (ary, superclass, recur);
  rb_funcall(ary, rb_intern("uniq!"), 0);
}

static VALUE
wrapper_objc_methods (VALUE rcv)
{
  VALUE ary;
  id oc_rcv;

  ary = rb_ary_new();
  oc_rcv = rbobj_get_ocid (rcv);
  _ary_push_objc_methods (ary, oc_rcv->isa, 1);
  return ary;
}

static VALUE
wrapper_objc_instance_methods (int argc, VALUE* argv, VALUE rcv)
{
  VALUE ary;
  id oc_rcv;
  int recur;

  recur = (argc == 0) ? 1 : RTEST(argv[0]);
  ary = rb_ary_new();
  oc_rcv = rbobj_get_ocid (rcv);
  _ary_push_objc_methods (ary, oc_rcv, recur);
  return ary;
}

static VALUE
wrapper_objc_class_methods (int argc, VALUE* argv, VALUE rcv)
{
  VALUE ary;
  id oc_rcv;
  int recur;

  recur = (argc == 0) ? 1 : RTEST(argv[0]);
  ary = rb_ary_new();
  oc_rcv = rbobj_get_ocid (rcv);
  _ary_push_objc_methods (ary, oc_rcv->isa, recur);
  return ary;
}

static const char*
_objc_method_type (Class cls, const char* name)
{
  Method method;

  method = class_getInstanceMethod(cls, sel_registerName(name));
  if (!method)
    return NULL;
  return method_getTypeEncoding(method);
}  

static VALUE
_name_to_selstr (VALUE name)
{
  VALUE re;
  const char* patstr = "([^^])_";
  const char* repstr = "\\1:";

  name = rb_obj_as_string (name);
  re = rb_reg_new (patstr, strlen(patstr), 0);
  rb_funcall (name, rb_intern("gsub!"), 2, re, rb_str_new2(repstr));
  return name;
}

static VALUE
wrapper_objc_method_type (VALUE rcv, VALUE name)
{
  id oc_rcv;
  const char* str;

  oc_rcv = rbobj_get_ocid (rcv);
  name = _name_to_selstr (name);
  str = _objc_method_type (oc_rcv->isa, STR2CSTR(name));
  if (str == NULL) return Qnil;
  return rb_str_new2(str);
}

static VALUE
wrapper_objc_instance_method_type (VALUE rcv, VALUE name)
{
  id oc_rcv;
  const char* str;

  oc_rcv = rbobj_get_ocid (rcv);
  name = _name_to_selstr (name);
  str = _objc_method_type (oc_rcv, STR2CSTR(name));
  if (str == NULL) return Qnil;
  return rb_str_new2(str);
}

static VALUE
wrapper_objc_class_method_type (VALUE rcv, VALUE name)
{
  id oc_rcv;
  const char* str;

  oc_rcv = rbobj_get_ocid (rcv);
  name = _name_to_selstr (name);
  str = _objc_method_type (oc_rcv->isa, STR2CSTR(name));
  if (str == NULL) return Qnil;
  return rb_str_new2(str);
}


static id 
_objc_alias_method (Class klass, VALUE new, VALUE old)
{
  Method me;
  SEL new_name;
  SEL old_name;

  old_name = rbobj_to_nssel(old);
  new_name = rbobj_to_nssel(new);
  me = class_getInstanceMethod(klass, old_name);

  // warn if trying to alias a method that isn't a member of the specified class
  if (me == NULL)
    rb_raise(rb_eRuntimeError, "could not alias '%s' for '%s' to class '%s': Objective-C cannot find it in the class", (char *)new_name, (char *)old_name, class_getName(klass));
  
  class_addMethod(klass, new_name, method_getImplementation(me), method_getTypeEncoding(me));
  
  return nil;
}

static VALUE
wrapper_objc_alias_method (VALUE rcv, VALUE new, VALUE old)
{
  Class klass = rbobj_get_ocid (rcv);
  _objc_alias_method(klass, new, old);
  return rcv;
}

static VALUE
wrapper_objc_alias_class_method (VALUE rcv, VALUE new, VALUE old)
{
  Class klass = (rbobj_get_ocid (rcv))->isa;
  _objc_alias_method(klass, new, old);
  return rcv;
}

/*****************************************/

VALUE
init_mdl_OCObjWrapper(VALUE outer)
{
  _mObjWrapper = rb_define_module_under(outer, "OCObjWrapper");

  rb_define_method(_mObjWrapper, "ocm_responds?", wrapper_ocm_responds_p, 1);
	//rb_define_method(_mObjWrapper, "ocm_conforms?", wrapper_ocm_conforms_p, 1);
  rb_define_method(_mObjWrapper, "ocm_send", wrapper_ocm_send, -1);
  rb_define_method(_mObjWrapper, "to_s", wrapper_to_s, 0);

  rb_define_method(_mObjWrapper, "objc_methods", wrapper_objc_methods, 0);
  rb_define_method(_mObjWrapper, "objc_method_type", wrapper_objc_method_type, 1);

  _mClsWrapper = rb_define_module_under(outer, "OCClsWrapper");
  rb_define_method(_mClsWrapper, "objc_instance_methods", wrapper_objc_instance_methods, -1);
  rb_define_method(_mClsWrapper, "objc_class_methods", wrapper_objc_class_methods, -1);
  rb_define_method(_mClsWrapper, "objc_instance_method_type", wrapper_objc_instance_method_type, 1);
  rb_define_method(_mClsWrapper, "objc_class_method_type", wrapper_objc_class_method_type, 1);

  rb_define_method(_mClsWrapper, "_objc_alias_method", wrapper_objc_alias_method, 2);
  rb_define_method(_mClsWrapper, "_objc_alias_class_method", wrapper_objc_alias_class_method, 2);

  rb_define_module_function(outer, "_ignore_ns_override", wrapper_ignore_ns_override, 0);

  return Qnil;
}
