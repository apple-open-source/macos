/*
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <Foundation/Foundation.h>
#import <ctype.h>
#import "RBObject.h"
#import "mdl_osxobjc.h"
#import "ocdata_conv.h"
#import "BridgeSupport.h"
#import "internal_macros.h"
#import "OverrideMixin.h"

#define RBOBJ_LOG(fmt, args...) DLOG("RBOBJ", fmt, ##args)

extern ID _relaxed_syntax_ID;

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
// On MacOS X 10.4 or earlier, +signatureWithObjCTypes: is a SPI
@interface NSMethodSignature (WarningKiller)
+ (id) signatureWithObjCTypes:(const char*)types;
@end
#endif

static RB_ID sel_to_mid(SEL a_sel)
{
  int i, length;
  const char *selName;
  char mname[1024];

  selName = sel_getName(a_sel);
  memset(mname, 0, sizeof(mname));
  strncpy(mname, selName, sizeof(mname) - 1);

  // selstr.sub(/:/,'_')
  length = strlen(selName);
  for (i = 0; i < length; i++)
    if (mname[i] == ':') mname[i] = '_';

  if (RTEST(rb_ivar_get(osx_s_module(), _relaxed_syntax_ID))) {
    // sub(/^(.*)_$/, "\1")
    for (i = length - 1; i >= 0; i--) {
      if (mname[i] != '_') break;
      mname[i] = '\0';
    }
  }

  return rb_intern(mname);
}

static RB_ID sel_to_mid_as_setter(SEL a_sel)
{
  volatile VALUE str = rb_str_new2(sel_getName(a_sel));

  // if str.sub!(/^set([A-Z][^:]*):$/, '\1=') then
  //   str = str[0].chr.downcase + str[1..-1]
  // end
  const char* re_pattern = "^set([A-Z][^:]*):$";
  VALUE re = rb_reg_new(re_pattern, strlen(re_pattern), 0);
  if (rb_funcall(str, rb_intern("sub!"), 2, re, rb_str_new2("\\1=")) != Qnil) {
    int c = (int)(RSTRING(str)->ptr[0]);
    c = tolower(c);
    RSTRING(str)->ptr[0] = (char) c;
  }

  return rb_to_id(str);
}

static RB_ID rb_obj_sel_to_mid(VALUE rcv, SEL a_sel)
{
  RB_ID mid = sel_to_mid(a_sel);
  if (rb_respond_to(rcv, mid) == 0)
    mid = sel_to_mid_as_setter(a_sel);
  return mid;
}

static int rb_obj_arity_of_method(VALUE rcv, SEL a_sel, BOOL *ok)
{
  VALUE mstr;
  RB_ID mid;
  VALUE method;
  VALUE argc;

  mid = rb_obj_sel_to_mid(rcv, a_sel);
  if (rb_respond_to(rcv, mid) == 0) {
    *ok = NO;
    return 0;
  }
  mstr = rb_str_new2(rb_id2name(mid)); // mstr = sel_to_rbobj (a_sel);
  method = rb_funcall(rcv, rb_intern("method"), 1, mstr);
  *ok = YES;
  argc = rb_funcall(method, rb_intern("arity"), 0);
  return NUM2INT(argc);
}

@interface __RBObjectThreadDispatcher : NSObject
{
  id _returned_ocid;
  RBObject * _rbobj;
  NSInvocation * _invocation;
}
+ (void)dispatchInvocation:(NSInvocation *)invocation toRBObject:(RBObject *)rbobj;
@end

@implementation RBObject

// private methods

- (BOOL)rbobjRespondsToSelector: (SEL)a_sel
{
  BOOL ret;
  RB_ID mid;
  int state;
  extern void Init_stack(VALUE*);

  if (FREQUENTLY_INIT_STACK_FLAG) {
    RBOBJ_LOG("rbobjRespondsToSelector(%s) w/Init_stack(%08lx)", a_sel, (void*)&state);
    Init_stack((void*)&state);
  }
  mid = rb_obj_sel_to_mid(m_rbobj, a_sel);
  ret = (rb_respond_to(m_rbobj, mid) != 0);
  RBOBJ_LOG("   --> %d", ret);
  return ret;
}

- (VALUE)fetchForwardArgumentsOf: (NSInvocation*)an_inv
{
  int i;
  NSMethodSignature* msig = [an_inv methodSignature];
  int arg_cnt = ([msig numberOfArguments] - 2);
  VALUE args = rb_ary_new2(arg_cnt);
  for (i = 0; i < arg_cnt; i++) {
    VALUE arg_val;
    const char* octstr = [msig getArgumentTypeAtIndex: (i+2)];
    void* ocdata = OCDATA_ALLOCA(octstr);
    BOOL f_conv_success;

    RBOBJ_LOG("arg[%d] of type '%s'", i, octstr);
    [an_inv getArgument: ocdata atIndex: (i+2)];
    f_conv_success = ocdata_to_rbobj(Qnil, octstr, ocdata, &arg_val, NO);
    if (f_conv_success == NO) {
      arg_val = Qnil;
    }
    rb_ary_store(args, i, arg_val);
  }
  return args;
}

- (BOOL)stuffForwardResult: (VALUE)result to: (NSInvocation*)an_inv returnedOcid: (id *) returnedOcid
{
  NSMethodSignature* msig = [an_inv methodSignature];
  const char* octype_str = encoding_skip_to_first_type([msig methodReturnType]);
  BOOL f_success;

  RBOBJ_LOG("stuff forward result of type '%s'", octype_str);

  *returnedOcid = nil;

  if (*octype_str == _C_VOID) {
    f_success = true;
  }
  else if ((*octype_str == _C_ID) || (*octype_str == _C_CLASS)) {
    id ocdata = rbobj_get_ocid(result);
    if (ocdata == nil) {
      if (result == m_rbobj) {
        ocdata = self;
      }
      else {
        rbobj_to_nsobj(result, &ocdata);
        *returnedOcid = ocdata;
      }
    }
    [an_inv setReturnValue: &ocdata];
    f_success = YES;
  }
  else {
    void* ocdata = OCDATA_ALLOCA(octype_str);
    f_success = rbobj_to_ocdata (result, octype_str, ocdata, YES);
    if (f_success) [an_inv setReturnValue: ocdata];
  }
  return f_success;
}

static void
rbobjRaiseRubyException (void)
{
  VALUE lasterr = rb_gv_get("$!");
  RB_ID mtd = rb_intern("nsexception");
  if (rb_respond_to(lasterr, mtd)) {
      VALUE nso = rb_funcall(lasterr, mtd, 0);
      NSException *exc = rbobj_get_ocid(nso);
      [exc raise];
      return; // not reached
  }

  NSMutableDictionary *info = [NSMutableDictionary dictionary];

  id ocdata = rbobj_get_ocid(lasterr);
  if (ocdata == nil) {
      rbobj_to_nsobj(lasterr, &ocdata);
  }
  [info setObject: ocdata forKey: @"$!"];

  VALUE klass = rb_class_path(CLASS_OF(lasterr));
  NSString *rbclass = [NSString stringWithUTF8String:StringValuePtr(klass)];

  VALUE rbmessage = rb_obj_as_string(lasterr);
  NSString *message = [NSString stringWithUTF8String:StringValuePtr(rbmessage)];

  NSMutableArray *backtraceArray = [NSMutableArray array];
  volatile VALUE ary = rb_funcall(ruby_errinfo, rb_intern("backtrace"), 0);
  int c;
  for (c=0; c<RARRAY(ary)->len; c++) {
      const char *path = StringValuePtr(RARRAY(ary)->ptr[c]);
      NSString *nspath = [NSString stringWithUTF8String:path];
      [backtraceArray addObject: nspath];
  }

  [info setObject: backtraceArray forKey: @"backtrace"];

  NSException* myException = [NSException
      exceptionWithName:[@"RBException_" stringByAppendingString: rbclass]
                         reason:message
                         userInfo:info];
  [myException raise];
}

static VALUE rbobject_protected_apply(VALUE a)
{
  VALUE *args = (VALUE*) a;
  return rb_apply(args[0],(RB_ID)args[1],(VALUE)args[2]);
}

static void notify_error(VALUE rcv, RB_ID mid)
{
  extern int RBNotifyException(const char* title, VALUE err);
  char title[128];
  snprintf(title, sizeof(title), "%s#%s", rb_obj_classname(rcv), rb_id2name(mid));
  RBNotifyException(title, ruby_errinfo);
}

VALUE rbobj_call_ruby(id rbobj, SEL selector, VALUE args)
{
  VALUE m_rbobj;
  RB_ID mid;
  VALUE stub_args[3];
  VALUE rb_result;
  int err;

  if ([rbobj respondsToSelector:@selector(__rbobj__)]) {
    m_rbobj = [rbobj __rbobj__];
  }
  else if ([rbobj respondsToSelector:@selector(__rbclass__)]) {
    m_rbobj = [rbobj __rbclass__];
  }
  else {
    // Not an RBObject class, try to get the value from the cache.
    m_rbobj = ocid_to_rbobj_cache_only(rbobj);
    if (NIL_P(m_rbobj)) {
      // Nothing in the cache, it means that this Objective-C object never
      // crossed the bridge yet. Let's create the Ruby proxy.
      m_rbobj = ocid_to_rbobj(Qnil, rbobj);
    }
  }

  mid = rb_obj_sel_to_mid(m_rbobj, selector);
  stub_args[0] = m_rbobj;
  stub_args[1] = mid;
  stub_args[2] = args;

  RBOBJ_LOG("calling method %s on Ruby object %p with %d args", rb_id2name(mid), m_rbobj, RARRAY(args)->len);

  if (rb_respond_to(m_rbobj, mid) == 0) {
    VALUE str = rb_inspect(m_rbobj);
    rb_raise(rb_eRuntimeError, "Ruby object `%s' doesn't respond to the ObjC selector `%s', the method either doesn't exist or is private", StringValuePtr(str), (char *)selector);
  }

  rb_result = rb_protect(rbobject_protected_apply, (VALUE)stub_args, &err);
  if (err) {
    notify_error(m_rbobj, mid);
    RBOBJ_LOG("got Ruby exception, raising Objective-C exception");
    rbobjRaiseRubyException();
    return Qnil; /* to be sure */
  }

  return rb_result;
}

- (id)rbobjForwardInvocation: (NSInvocation *)an_inv
{
  VALUE rb_args;
  VALUE rb_result;
  VALUE rb_result_inspect;
  id returned_ocid;

  RBOBJ_LOG("rbobjForwardInvocation(%@)", an_inv);
  rb_args = [self fetchForwardArgumentsOf: an_inv];
  rb_result = rbobj_call_ruby(self, [an_inv selector], rb_args);
  [self stuffForwardResult: rb_result to: an_inv returnedOcid: &returned_ocid];
  rb_result_inspect = rb_inspect(rb_result);
  RBOBJ_LOG("   --> rb_result=%s", StringValuePtr(rb_result_inspect));

  return returned_ocid;
}

// public class methods
+ RBObjectWithRubyScriptCString: (const char*) cstr
{
  return [[[self alloc] initWithRubyScriptCString: cstr] autorelease];
}

+ RBObjectWithRubyScriptString: (NSString*) str
{
  return [[[self alloc] initWithRubyScriptString: str] autorelease];
}

// public methods

- (VALUE) __rbobj__  { return m_rbobj; }

- (void) trackRetainReleaseOfRubyObject
{
  VALUE str = rb_inspect(m_rbobj);
  RBOBJ_LOG("start tracking retain/release of Ruby object `%s'",  
    StringValuePtr(str));
  m_rbobj_retain_release_track = YES;
}

- (void) retainRubyObject
{
  if (m_rbobj_retain_release_track && !m_rbobj_retained) {
    VALUE str = rb_inspect(m_rbobj);
    RBOBJ_LOG("retaining Ruby object `%s'", StringValuePtr(str));
    rb_gc_register_address(&m_rbobj);
    m_rbobj_retained = YES;
  }
}

- (void) releaseRubyObject
{
  if (m_rbobj_retain_release_track && m_rbobj_retained) {
    RBOBJ_LOG("releasing Ruby object `#<%s:%p>'", rb_obj_classname(m_rbobj), m_rbobj);
    rb_gc_unregister_address(&m_rbobj);
    m_rbobj_retained = NO;
  }
}

- (void) dealloc
{
  RBOBJ_LOG("deallocating RBObject %p", self);
  remove_from_rb2oc_cache(m_rbobj);
  [self releaseRubyObject];
  [super dealloc];
}

- _initWithRubyObject: (VALUE)rbobj retains: (BOOL) flag
{
  m_rbobj = rbobj;
  m_rbobj_retained = flag;
  m_rbobj_retain_release_track = NO;
  oc_master = nil;
  if (flag)
    rb_gc_register_address(&m_rbobj);
  return self;
}

- initWithRubyObject: (VALUE)rbobj
{
  return [self _initWithRubyObject: rbobj retains: YES];
}

- initWithRubyScriptCString: (const char*) cstr
{
  return [self initWithRubyObject: rb_eval_string(cstr)];
}

- initWithRubyScriptString: (NSString*) str
{
  return [self initWithRubyScriptCString: [str UTF8String]];
}

- (NSString*) _copyDescription
{
  VALUE str = rb_inspect(m_rbobj);
  return [[[NSString alloc] initWithUTF8String: StringValuePtr(str)] autorelease];
}

- (BOOL)isKindOfClass: (Class)klass
{
  BOOL ret;
  RBOBJ_LOG("isKindOfClass(%@)", NSStringFromClass(klass));
  ret = NO;
  RBOBJ_LOG("   --> %d", ret);
  return ret;
}

- (BOOL)isRBObject
{
  return YES;
}

- (void)forwardInvocation: (NSInvocation *)an_inv
{
  RBOBJ_LOG("forwardInvocation(%@)", an_inv);
  if ([self rbobjRespondsToSelector: [an_inv selector]]) {
    RBOBJ_LOG("   -> forward to Ruby Object");
    if (is_ruby_native_thread()) {
      [self rbobjForwardInvocation: an_inv];
    }
    else {
      rb_warning("Invocation `%s' received from another thread - forwarding it to the main thread", [[an_inv description] UTF8String]);
      [__RBObjectThreadDispatcher dispatchInvocation:an_inv toRBObject:self];
    }
  }
  else {
    RBOBJ_LOG("   -> forward to super Objective-C Object");
    [super forwardInvocation: an_inv];
  }
}

- (NSMethodSignature*)methodSignatureForSelector: (SEL)a_sel
{
  NSMethodSignature* ret = nil;
  RBOBJ_LOG("methodSignatureForSelector(%s)", a_sel);
  if (a_sel == NULL)
    return nil;
  // Try the master object.
  if (oc_master != nil) {
    ret = [oc_master instanceMethodSignatureForSelector:a_sel];
    if (ret != nil)
      RBOBJ_LOG("\tgot method signature from the master object");
  }
  // Try the metadata.
  if (ret == nil) {
    struct bsInformalProtocolMethod *method;

    method = find_bs_informal_protocol_method((const char *)a_sel, NO);
    if (method != NULL) {
      ret = [NSMethodSignature signatureWithObjCTypes:method->encoding];
      RBOBJ_LOG("\tgot method signature from metadata (types: '%s')", method->encoding);
    }
  }
  // Ensure a dummy method signature ('id' for everything).
  if (ret == nil) {
    int argc;
    BOOL ok;

    argc = rb_obj_arity_of_method(m_rbobj, a_sel, &ok);
    if (ok) {
      char encoding[128], *p;

      if (argc < 0)
        argc = -1 - argc;
      argc = MIN(sizeof(encoding) - 4, argc);

      strcpy(encoding, "@@:");
      p = &encoding[3];
      while (argc-- > 0) {
        *p++ = '@';
      }
      *p = '\0';
      ret = [NSMethodSignature signatureWithObjCTypes:encoding];
      RBOBJ_LOG("\tgenerated dummy method signature");
    }
    else {
      VALUE str = rb_inspect(m_rbobj);
      RBOBJ_LOG("\tcan't generate a dummy method signature because receiver %s doesn't respond to the selector", StringValuePtr(str));
    }
  }
  RBOBJ_LOG("   --> %@", ret);
  return ret;
}

- (BOOL)respondsToSelector: (SEL)a_sel
{
  BOOL ret;
  if (a_sel == @selector(__rbobj__))
    return YES;
  RBOBJ_LOG("respondsToSelector(%s)", a_sel);
  ret = [self rbobjRespondsToSelector: a_sel];
  RBOBJ_LOG("   --> %d", ret);
  return ret;
}

@end

@implementation __RBObjectThreadDispatcher

- (id)initWithInvocation:(NSInvocation *)invocation RBObject:(RBObject *)rbobj
{
  self = [super init];
  if (self != NULL) {
    _returned_ocid = nil;
    _invocation = invocation; // no retain
    _rbobj = rbobj; // no retain
  }
  return self;
}

extern NSThread *rubycocoaThread;

- (void)dispatch
{
  DISPATCH_ON_RUBYCOCOA_THREAD(self, @selector(syncDispatch));
  if (_returned_ocid != nil)
    [_returned_ocid autorelease];
}

- (void)syncDispatch
{
  _returned_ocid = [_rbobj rbobjForwardInvocation:_invocation];
  if (_returned_ocid != nil)
    [_returned_ocid retain];
}

+ (void)dispatchInvocation:(NSInvocation *)invocation toRBObject:(RBObject *)rbobj
{
  __RBObjectThreadDispatcher *  dispatcher;

  dispatcher = [[__RBObjectThreadDispatcher alloc] initWithInvocation:invocation RBObject:rbobj];
  [dispatcher dispatch];
  [dispatcher release];
}

@end

@implementation NSProxy (RubyCocoaEx)

- (BOOL)isRBObject
{
  return NO;
}

@end
