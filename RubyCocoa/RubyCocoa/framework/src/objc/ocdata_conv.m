/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <objc/objc-class.h>
#import <Foundation/Foundation.h>
#import "ocdata_conv.h"
#import "RBObject.h"
#import "mdl_osxobjc.h"
#import <CoreFoundation/CFString.h> // CFStringEncoding
#import "st.h"
#import "BridgeSupport.h"
#import "internal_macros.h"

#define CACHE_LOCKING 0

#define DATACONV_LOG(fmt, args...) DLOG("DATACNV", fmt, ##args)

static struct st_table *rb2ocCache;
static struct st_table *oc2rbCache;

static VALUE _ocid_to_rbobj (VALUE context_obj, id ocid, BOOL is_class);

#if CACHE_LOCKING
static pthread_mutex_t rb2ocCacheLock;
static pthread_mutex_t oc2rbCacheLock;
# define CACHE_LOCK(x)      (pthread_mutex_lock(x))
# define CACHE_UNLOCK(x)    (pthread_mutex_unlock(x))
#else
# define CACHE_LOCK(x)
# define CACHE_UNLOCK(x)
#endif

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
// On MacOS X 10.4 or earlier, +signatureWithObjCTypes: is a SPI 
@interface NSMethodSignature (WarningKiller)
+ (id) signatureWithObjCTypes:(const char*)types;
@end
#endif

@interface RBObject (Private)
- (id)_initWithRubyObject: (VALUE)rbobj retains: (BOOL) flag;
@end

void init_rb2oc_cache(void)
{
  rb2ocCache = st_init_numtable();
#if CACHE_LOCKING
  pthread_mutex_init(&rb2ocCacheLock, NULL);
#endif
}

void init_oc2rb_cache(void)
{
  oc2rbCache = st_init_numtable();
#if CACHE_LOCKING
  pthread_mutex_init(&oc2rbCacheLock, NULL);
#endif
}

void remove_from_oc2rb_cache(id ocid)
{
  CACHE_LOCK(&oc2rbCacheLock);
  st_delete(oc2rbCache, (st_data_t *)&ocid, NULL);
  CACHE_UNLOCK(&oc2rbCacheLock);
}

void remove_from_rb2oc_cache(VALUE rbobj)
{
  CACHE_LOCK(&rb2ocCacheLock);
  st_delete(rb2ocCache, (st_data_t *)&rbobj, NULL);
  CACHE_UNLOCK(&rb2ocCacheLock);
}

static BOOL
convert_cary(VALUE *result, void *ocdata, char *octype_str, BOOL to_ruby)
{
  long i, count, size, pos;
  VALUE ary;
  BOOL ok;

  octype_str++;

  // first, get the number of entries  
  count = 0;
  while (isdigit(*octype_str)) {
    count *= 10;
    count += (long)(*octype_str - '0');
    octype_str++;
  }

  // second, remove the trailing ']'
  pos = strlen(octype_str) - 1;
  octype_str[pos] = '\0';      /*  ((char*)octype_str)[pos] = '\0'; */
  size = ocdata_size(octype_str);

  // third, do the conversion
  if (to_ruby) {
    ary = rb_ary_new();
    for (i = 0; i < count; i++) {
      VALUE entry;
      void *p;

      p = *(void **)ocdata + (i * size);
      if (!ocdata_to_rbobj(Qnil, octype_str, p, &entry, NO)) {
        *result = Qnil;
        ok = NO;
        goto bail;
      }
      rb_ary_push(ary, entry);
    }
    
    *result = ary;
    ok = YES;
  }
  else {
    volatile VALUE ary;

    ary = *result;

    Check_Type(ary, T_ARRAY);
    if (RARRAY(ary)->len > count)
      rb_raise(rb_eArgError, 
        "Given Array expected with maximum %d elements, but got %d",
        count, RARRAY(ary)->len);

    for (i = 0; i < RARRAY(ary)->len; i++) {
      VALUE val;
      void *p;

      val = RARRAY(ary)->ptr[i];
      p = ocdata + (i * size);
      if (!rbobj_to_ocdata(val, octype_str, p, NO)) {
        ok = NO;
        goto bail;
      } 
    }
    ok = YES;
  }

bail:
  // put back the trailing ']'
  octype_str[pos] = ']'; /* ((char*)octype_str)[pos] = ']'; */
  return ok;
}

static BOOL
rbobj_to_cary (VALUE obj, void *data, const char *octype_str)
{
  return convert_cary(&obj, data, (char *)octype_str, NO);
}

static BOOL
cary_to_rbary (void *data, const char *octype_str, VALUE *result)
{
  return convert_cary(result, data, (char *)octype_str, YES);
}

size_t ocdata_alloc_size(const char* octype_str)
{
#if BYTE_ORDER == BIG_ENDIAN
  size_t size = ocdata_size(octype_str);
  if (size == 0) return 0;
  return size < sizeof(void*) ? sizeof(void*) : size;
#else
  return ocdata_size(octype_str);
#endif
}

size_t
ocdata_size(const char* octype_str)
{
  size_t result;
  struct bsBoxed *bs_boxed;

  if (*octype_str == _C_CONST)
    octype_str++;  

  bs_boxed = find_bs_boxed_by_encoding(octype_str);
  if (bs_boxed != NULL)
    return bs_boxed_size(bs_boxed);

  if (find_bs_cf_type_by_encoding(octype_str) != NULL)
    octype_str = "@";

  result = 0;

  switch (*octype_str) {
    case _C_ID:
    case _C_CLASS:
      result = sizeof(id); 
      break;

    case _C_SEL:
      result = sizeof(SEL); 
      break;

    case _C_CHR:
    case _C_UCHR:
      result = sizeof(char); 
      break;

    case _C_SHT:
    case _C_USHT:
      result = sizeof(short); 
      break;

    case _C_INT:
    case _C_UINT:
      result = sizeof(int); 
      break;

    case _C_LNG:
    case _C_ULNG:
      result = sizeof(long); 
      break;

#if HAVE_LONG_LONG
    case _C_LNG_LNG:
      result = sizeof(long long); 
      break;

    case _C_ULNG_LNG:
      result = sizeof(unsigned long long); 
      break;
#endif

    case _C_FLT:
      result = sizeof(float); 
      break;

    case _C_DBL:
      result = sizeof(double); 
      break;

    case _C_CHARPTR:
      result = sizeof(char*); 
      break;

    case _C_VOID:
      result = 0; 
      break;

    case _C_BOOL:
      result = sizeof(BOOL); 
      break; 

    case _C_PTR:
      result = sizeof(void*); 
      break;

    case _C_BFLD:
      if (octype_str != NULL) {
        char *type;
        long lng;
  
        type = (char *)octype_str;
        lng  = strtol(type, &type, 10);
  
        // while next type is a bit field
        while (*type == _C_BFLD) {
          long next_lng;
  
          // skip over _C_BFLD
          type++;
  
          // get next bit field length
          next_lng = strtol(type, &type, 10);
  
          // if spans next word then align to next word
          if ((lng & ~31) != ((lng + next_lng) & ~31))
            lng = (lng + 31) & ~31;
  
          // increment running length
          lng += next_lng;
        }
        result = (lng + 7) / 8;
      }
      break;

    default:
      @try {
        NSGetSizeAndAlignment(octype_str, 
#if __LP64__
          (unsigned long *)&result, 
#else
          (unsigned int *)&result, 
#endif
          NULL);
        }
        @catch (id exception) {
          rb_raise(rb_eRuntimeError, "Cannot compute size of type `%s' : %s",
            octype_str, [[exception description] UTF8String]);
        }
      break;
  }

  return result;
}

void *
ocdata_malloc(const char* octype_str)
{
  size_t s = ocdata_alloc_size(octype_str);
  if (s == 0) return NULL;
  return malloc(s);
}

BOOL
ocdata_to_rbobj (VALUE context_obj, const char *octype_str, const void *ocdata, VALUE *result, BOOL from_libffi)
{
  BOOL f_success = YES;
  volatile VALUE rbval = Qnil;
  struct bsBoxed *bs_boxed;

#if BYTE_ORDER == BIG_ENDIAN
  // libffi casts all types as a void pointer, which is problematic on PPC for types sized less than a void pointer (char, uchar, short, ushort, ...), as we have to shift the bytes to get the real value.
  if (from_libffi) {
    int delta = sizeof(void *) - ocdata_size(octype_str);
    if (delta > 0)
      ocdata += delta; 
  }
#endif

  octype_str = encoding_skip_qualifiers(octype_str);

  bs_boxed = find_bs_boxed_by_encoding(octype_str);
  if (bs_boxed != NULL) {
    *result = rb_bs_boxed_new_from_ocdata(bs_boxed, (void *)ocdata);
    return YES;
  }
  
  if (find_bs_cf_type_by_encoding(octype_str) != NULL)
    octype_str = "@";

  switch (*octype_str) {
    case _C_ID:
    case _C_CLASS:
      rbval = _ocid_to_rbobj(context_obj, *(id*)ocdata, *octype_str == _C_CLASS);
      break;

    case _C_PTR:
      if (is_boxed_ptr(octype_str, &bs_boxed)) {
        rbval = rb_bs_boxed_ptr_new_from_ocdata(bs_boxed, *(void **)ocdata);
      }
      else {
        void *cptr = *(void**)ocdata;
        rbval = cptr == NULL ? Qnil : objcptr_s_new_with_cptr (cptr, octype_str);
      }
      break;
  
    case _C_ARY_B:
      f_success = cary_to_rbary(*(void **)ocdata, octype_str, (VALUE*)&rbval); 
      break;

    case _C_BOOL:
      rbval = bool_to_rbobj(*(BOOL*)ocdata);
      break;

    case _C_SEL:
      rbval = rb_str_new2(sel_getName(*(SEL*)ocdata));
      break;

    case _C_CHR:
      rbval = INT2NUM(*(char*)ocdata); 
      break;

    case _C_UCHR:
      rbval = UINT2NUM(*(unsigned char*)ocdata); 
      break;

    case _C_SHT:
      rbval = INT2NUM(*(short*)ocdata); 
      break;

    case _C_USHT:
      rbval = UINT2NUM(*(unsigned short*)ocdata); 
      break;

    case _C_INT:
      rbval = INT2NUM(*(int*)ocdata); 
      break;

    case _C_UINT:
      rbval = UINT2NUM(*(unsigned int*)ocdata);
      break;

    case _C_LNG:
      rbval = INT2NUM(*(long*)ocdata); 
      break;

    case _C_ULNG:
      rbval = UINT2NUM(*(unsigned long*)ocdata); 
      break;

#if HAVE_LONG_LONG
    case _C_LNG_LNG:
      rbval = LL2NUM(*(long long*)ocdata); 
      break;

    case _C_ULNG_LNG:
      rbval = ULL2NUM(*(unsigned long long*)ocdata); 
      break;
#endif

    case _C_FLT:
      rbval = rb_float_new((double)(*(float*)ocdata)); 
      break;

    case _C_DBL:
      rbval = rb_float_new(*(double*)ocdata); 
      break;

    case _C_CHARPTR:
      if (*(void **)ocdata == NULL)
        rbval = Qnil;
      else
        rbval = rb_str_new2(*(char **)ocdata); 
      break;
  
    default:
      f_success = NO;
      rbval = Qnil;
      break;
	}

  if (f_success) 
    *result = rbval;

  return f_success;
}

static BOOL 
rbary_to_nsary (VALUE rbary, id* nsary)
{
  long i, len;
  id *objects;

  len = RARRAY(rbary)->len;
  objects = (id *)alloca(sizeof(id) * len);
  ASSERT_ALLOC(objects);
  
  for (i = 0; i < len; i++)
    if (!rbobj_to_nsobj(RARRAY(rbary)->ptr[i], &objects[i]))
      return NO;
  
  *nsary = [[[NSMutableArray alloc] initWithObjects:objects count:len] autorelease];
  return YES;
}

// FIXME: we should use the CoreFoundation API for x_to_y functions
// (should be faster than Foundation)

static BOOL 
rbhash_to_nsdic (VALUE rbhash, id* nsdic)
{
  volatile VALUE ary_keys;
  VALUE* keys;
  VALUE val;
  long i, len;
  id *nskeys, *nsvals;

  ary_keys = rb_funcall(rbhash, rb_intern("keys"), 0);
  len = RARRAY(ary_keys)->len;
  keys = RARRAY(ary_keys)->ptr;

  nskeys = (id *)alloca(sizeof(id) * len);
  ASSERT_ALLOC(nskeys);
  nsvals = (id *)alloca(sizeof(id) * len);
  ASSERT_ALLOC(nsvals);

  for (i = 0; i < len; i++) {
    if (!rbobj_to_nsobj(keys[i], &nskeys[i])) 
      return NO;
    val = rb_hash_aref(rbhash, keys[i]);
    if (!rbobj_to_nsobj(val, &nsvals[i])) 
      return NO;
  }

  *nsdic = [[[NSMutableDictionary alloc] initWithObjects:nsvals forKeys:nskeys count:len] autorelease];
  return YES;
}

static BOOL 
rbbool_to_nsnum (VALUE rbval, id* nsval)
{
  *nsval = [NSNumber numberWithBool:RTEST(rbval)];
  return YES;
}

static BOOL 
rbint_to_nsnum (VALUE rbval, id* nsval)
{
#if HAVE_LONG_LONG
  long long val;
  val = NUM2LL(rbval);
  *nsval = [NSNumber numberWithLongLong:val];
#else
  long val;
  val = NUM2LONG(rbval);
  *nsval = [NSNumber numberWithLong:val];
#endif
  return YES;
}

static BOOL 
rbfloat_to_nsnum (VALUE rbval, id* nsval)
{
  double val;
  val = NUM2DBL(rbval);
  *nsval = [NSNumber numberWithDouble:val];
  return YES; 
}

static BOOL
rbtime_to_nsdate (VALUE rbval, id* nsval)
{
  NSTimeInterval seconds;
  seconds = NUM2LONG(rb_funcall(rbval, rb_intern("to_i"), 0));
  *nsval = [NSDate dateWithTimeIntervalSince1970:seconds]; 
  return [(*nsval) isKindOfClass: [NSDate class]];
}

static BOOL 
rbobj_convert_to_nsobj (VALUE obj, id* nsobj)
{
  switch (TYPE(obj)) {
    case T_NIL:
      *nsobj = nil;
      return YES;

    case T_STRING:
      obj = rb_obj_as_string(obj);
      *nsobj = rbstr_to_ocstr(obj);
      return YES;

    case T_SYMBOL:
      obj = rb_obj_as_string(obj);
      *nsobj = [NSString stringWithUTF8String: RSTRING(obj)->ptr];
      return YES;

    case T_ARRAY:
      return rbary_to_nsary(obj, nsobj);

    case T_HASH:
      return rbhash_to_nsdic(obj, nsobj);

    case T_TRUE:
    case T_FALSE:
      return rbbool_to_nsnum(obj, nsobj);     

    case T_FIXNUM:
    case T_BIGNUM:
      return rbint_to_nsnum(obj, nsobj);

    case T_FLOAT:
      return rbfloat_to_nsnum(obj, nsobj);

    default:
      if (rb_obj_is_kind_of(obj, rb_cTime))
        return rbtime_to_nsdate(obj, nsobj);

      *nsobj = [[[RBObject alloc] initWithRubyObject:obj] autorelease];
      return YES;
  }
  return YES;
}

BOOL 
rbobj_to_nsobj (VALUE obj, id* nsobj)
{
  BOOL  ok;

  if (obj == Qnil) {
    *nsobj = nil;
    return YES;
  }

  // Cache new Objective-C object addresses in an internal table to 
  // avoid duplication.
  //
  // We are locking the access to the cache twice (lookup + insert) as
  // rbobj_convert_to_nsobj is succeptible to call us again, to avoid
  // a deadlock.

  CACHE_LOCK(&rb2ocCacheLock);
  ok = st_lookup(rb2ocCache, (st_data_t)obj, (st_data_t *)nsobj);
  CACHE_UNLOCK(&rb2ocCacheLock);

  if (!ok) {
    *nsobj = rbobj_get_ocid(obj);
    if (*nsobj != nil || rbobj_convert_to_nsobj(obj, nsobj)) {
      BOOL  magic_cookie;
      if (*nsobj == nil) return YES;
      
      magic_cookie = find_magic_cookie_const_by_value(*nsobj) != NULL;
      if (magic_cookie || ([*nsobj isProxy] && [*nsobj isRBObject])) {
        CACHE_LOCK(&rb2ocCacheLock);
        // Check out that the hash is still empty for us, to avoid a race condition.
        if (!st_lookup(rb2ocCache, (st_data_t)obj, (st_data_t *)nsobj))
          st_insert(rb2ocCache, (st_data_t)obj, (st_data_t)*nsobj);
        CACHE_UNLOCK(&rb2ocCacheLock);
      }
      ok = YES;
    }
  }

  return ok;
}

BOOL 
rbobj_to_bool (VALUE obj)
{
  return RTEST(obj) ? YES : NO;
}

VALUE 
bool_to_rbobj (BOOL val)
{
  return (val ? Qtrue : Qfalse);
}

VALUE 
sel_to_rbobj (SEL val)
{
  VALUE rbobj;

  // FIXME: this should be optimized

  if (ocdata_to_rbobj(Qnil, ":", &val, &rbobj, NO)) {
    rbobj = rb_obj_as_string(rbobj);
    // str.tr!(':','_')
    rb_funcall(rbobj, rb_intern("tr!"), 2, rb_str_new2(":"), rb_str_new2("_"));
    // str.sub!(/_+$/,'')
    rb_funcall(rbobj, rb_intern("sub!"), 2, rb_str_new2("_+$"), rb_str_new2(""));
  }
  else {
    rbobj = Qnil;
  }
  return rbobj;
}

VALUE 
int_to_rbobj (int val)
{
  return INT2NUM(val);
}

VALUE 
uint_to_rbobj (unsigned int val)
{
  return UINT2NUM(val);
}

VALUE 
double_to_rbobj (double val)
{
  return rb_float_new(val);
}

VALUE
ocid_to_rbobj_cache_only (id ocid)
{
  VALUE result;
  BOOL  ok;

  CACHE_LOCK(&oc2rbCacheLock);
  ok = st_lookup(oc2rbCache, (st_data_t)ocid, (st_data_t *)&result);
  CACHE_UNLOCK(&oc2rbCacheLock);

  return ok ? result : Qnil;
}

static VALUE
_ocid_to_rbobj (VALUE context_obj, id ocid, BOOL is_class)
{
  VALUE result;
  BOOL  ok, shouldCache;
  struct bsConst *  bs_const;

  if (ocid == nil) 
    return Qnil;

  // Cache new Ruby object addresses in an internal table to 
  // avoid duplication.
  //
  // We are locking the access to the cache twice (lookup + insert) as
  // ocobj_s_new is succeptible to call us again, to avoid a deadlock.

  bs_const = find_magic_cookie_const_by_value(ocid);

  if (bs_const == NULL 
      && (is_class 
          || [ocid isProxy] 
          || find_bs_cf_type_by_type_id(CFGetTypeID(ocid)) != NULL)) {
    // We don't cache CF-based objects because we don't have yet a reliable
    // way to remove them from the cache.
    ok = shouldCache = NO;
  }
  else {
    CACHE_LOCK(&oc2rbCacheLock);
    ok = st_lookup(oc2rbCache, (st_data_t)ocid, (st_data_t *)&result);
    CACHE_UNLOCK(&oc2rbCacheLock);
    shouldCache = context_obj != Qfalse;
  }

  if (!ok) {
    if (bs_const != NULL) {
      result = ocobj_s_new_with_class_name(ocid, bs_const->class_name);
    }
    else {
      result = ocid_get_rbobj(ocid);
      if (result == Qnil)
        result = rbobj_get_ocid(context_obj) == ocid 
          ? context_obj : ocobj_s_new(ocid);
    }

    if (shouldCache) {
      CACHE_LOCK(&oc2rbCacheLock);
      // Check out that the hash is still empty for us, to avoid a race 
      // condition.
      if (!st_lookup(oc2rbCache, (st_data_t)ocid, (st_data_t *)&result))
        st_insert(oc2rbCache, (st_data_t)ocid, (st_data_t)result);
      CACHE_UNLOCK(&oc2rbCacheLock);
    }
  }

  return result;
}

VALUE
ocid_to_rbobj (VALUE context_obj, id ocid)
{
  return _ocid_to_rbobj(context_obj, ocid, NO);
}

static SEL 
rbobj_to_cselstr (VALUE obj)
{
  int i;
  volatile VALUE str;
  char *sel;
 
  str = rb_obj_is_kind_of(obj, rb_cString)
    ? obj : rb_obj_as_string(obj);

  if (rb_ivar_defined(str, rb_intern("@__is_sel__")) == Qtrue)
    return sel_registerName(RSTRING(str)->ptr);

  sel = (char *)alloca(RSTRING(str)->len + 1);
  sel[0] = RSTRING(str)->ptr[0];
  for (i = 1; i < RSTRING(str)->len; i++) {
    char c = RSTRING(str)->ptr[i];
    if (c == '_')
      c = ':';
    sel[i] = c;  
  }
  sel[RSTRING(str)->len] = '\0';

  return sel_registerName(sel);
}

SEL 
rbobj_to_nssel (VALUE obj)
{
  return NIL_P(obj) ? NULL : rbobj_to_cselstr(obj);
}

struct funcptr_closure_context {
  char *    rettype;
  char **   argtypes;
  unsigned  argc;
  VALUE     block;
};

static void
funcptr_closure_handler (ffi_cif *cif, void *resp, void **args, void *userdata)
{
  struct funcptr_closure_context *context;
  volatile VALUE rb_args;
  unsigned i;
  VALUE retval;

  context = (struct funcptr_closure_context *)userdata;
  rb_args = rb_ary_new2(context->argc);

  for (i = 0; i < context->argc; i++) {
    VALUE arg;

    if (!ocdata_to_rbobj(Qnil, context->argtypes[i], args[i], &arg, NO))
      rb_raise(rb_eRuntimeError, "Can't convert Objective-C argument #%d of octype '%s' to Ruby value", i, context->argtypes[i]);

    DATACONV_LOG("converted arg #%d of type %s to Ruby value %p", i, context->argtypes[i], arg);

    rb_ary_store(rb_args, i, arg);
  }

  DATACONV_LOG("calling Ruby block with %d args...", RARRAY(rb_args)->len);
  retval = rb_funcall2(context->block, rb_intern("call"), RARRAY(rb_args)->len, RARRAY(rb_args)->ptr);
  DATACONV_LOG("called Ruby block");

  if (*encoding_skip_to_first_type(context->rettype) != _C_VOID) {
    if (!rbobj_to_ocdata(retval, context->rettype, resp, YES))
      rb_raise(rb_eRuntimeError, "Can't convert return Ruby value to Objective-C value of octype '%s'", context->rettype);
  }
}

static BOOL
rbobj_to_funcptr (VALUE obj, void **cptr, const char *octype_str)
{
  unsigned  argc;
  unsigned  i;
  char *    rettype;
  char **   argtypes; 
  int       block_arity;
  struct funcptr_closure_context *  context;

  if (TYPE(obj) == T_NIL) {
    *cptr = NULL;
    return YES;
  }
  
  if (rb_obj_is_kind_of(obj, rb_cProc) == Qfalse)
    return NO;

  if (*octype_str != '?')
    return NO;
  octype_str++;
  if (octype_str == NULL)
    return NO;

  decode_method_encoding(octype_str, nil, &argc, &rettype, &argtypes, NO); 

  block_arity = FIX2INT(rb_funcall(obj, rb_intern("arity"), 0));
  if (block_arity != argc) {
    free(rettype);
    if (argtypes != NULL) {
      for (i = 0; i < argc; i++)
        free(argtypes[i]);
      free(argtypes);
    }
    // Should we return NO there? Probably better to raise an exception directly.
    rb_raise(rb_eArgError, "Given Proc object has an invalid number of arguments (expected %d, got %d)", 
             argc, block_arity); 
    return NO;  // to be sure... 
  }

  context = (struct funcptr_closure_context *)malloc(sizeof(struct funcptr_closure_context));
  ASSERT_ALLOC(context);
  context->rettype = rettype;
  context->argtypes = argtypes;
  context->argc = argc;
  context->block = obj; 

  *cptr = ffi_make_closure(rettype, (const char **)argtypes, argc, funcptr_closure_handler, context);

  return YES;
}

static BOOL 
rbobj_to_objcptr (VALUE obj, void** cptr, const char *octype_str)
{
  if (TYPE(obj) == T_NIL) {
    *cptr = NULL;
  }
  else if (TYPE(obj) == T_STRING) {
    *cptr = RSTRING(obj)->ptr;
  }
  else if (TYPE(obj) == T_ARRAY) {
    if (RARRAY(obj)->len > 0) {
      size_t len;
      void *ary;
      unsigned i;
      
      len = ocdata_size(octype_str);
      ary = *cptr;

      for (i = 0; i < RARRAY(obj)->len; i++) {
        if (!rbobj_to_ocdata(RARRAY(obj)->ptr[i], octype_str, ary + (i * len), NO))
          return NO;
      }
    }
    else {
      *cptr = NULL;
    }
  }
  else if (rb_obj_is_kind_of(obj, objid_s_class()) == Qtrue) {
    *cptr = OBJCID_ID(obj);
  }
  else if (rb_obj_is_kind_of(obj, objcptr_s_class()) == Qtrue) {
    *cptr = objcptr_cptr(obj);
  }
  else if (rb_obj_is_kind_of(obj, objboxed_s_class()) == Qtrue) {
    struct bsBoxed *bs_boxed;
    void *data;
    BOOL ok;

    bs_boxed = find_bs_boxed_for_klass(CLASS_OF(obj));
    if (bs_boxed == NULL)
      return NO;

    data = rb_bs_boxed_get_data(obj, bs_boxed->encoding, NULL, &ok, YES);
    if (!ok)
      return NO;
    *cptr = data;
  } 
  else {
    return NO;
  }
  return YES;
}

static BOOL 
rbobj_to_idptr (VALUE obj, id** idptr)
{
  if (TYPE(obj) == T_NIL) {
    *idptr = nil;
  }
  else if (TYPE(obj) == T_ARRAY) {
    if (RARRAY(obj)->len > 0) {
      id *ary;
      unsigned i;

      ary = *idptr;
      for (i = 0; i < RARRAY(obj)->len; i++) {
        if (!rbobj_to_nsobj(RARRAY(obj)->ptr[i], &ary[i])) {
          *idptr = nil;
          return NO;
        }
      }
    }
    else {
      *idptr = nil;
    }
  }
  else if (rb_obj_is_kind_of(obj, objid_s_class()) == Qtrue) {
    id old_id = OBJCID_ID(obj);
    if (old_id) [old_id release];
    OBJCID_ID(obj) = nil;
    *idptr = OBJCID_IDPTR(obj);
  }
  else {
    return NO;
  }
  return YES;
}

BOOL
rbobj_to_ocdata (VALUE obj, const char *octype_str, void* ocdata, BOOL to_libffi)
{
  BOOL f_success = YES;

#if BYTE_ORDER == BIG_ENDIAN
  // libffi casts all types as a void pointer, which is problematic on PPC for types sized less than a void pointer (char, uchar, short, ushort, ...), as we have to shift the bytes to get the real value.
  if (to_libffi) {
    int delta = sizeof(void *) - ocdata_size(octype_str);
    if (delta > 0) {
      memset(ocdata, 0, delta);
      ocdata += delta;
    }
  }
#endif
  
  octype_str = encoding_skip_qualifiers(octype_str);

  // Make sure we convert booleans to NSNumber booleans.
  if (*octype_str != _C_ID && *octype_str != _C_BOOL) {
    if (TYPE(obj) == T_TRUE) {
      obj = INT2NUM(1);
    }
    else if (TYPE(obj) == T_FALSE) {
      obj = INT2NUM(0);
    }
  }

  if (find_bs_boxed_by_encoding(octype_str) != NULL) {
    void *data;
    size_t size;

    data = rb_bs_boxed_get_data(obj, octype_str, &size, &f_success, YES);
    if (f_success) {
      if (data == NULL)
        *(void **)ocdata = NULL;
      else
        memcpy(ocdata, data, size);
      return YES;
    }
  }

  if (find_bs_cf_type_by_encoding(octype_str) != NULL)
    octype_str = "@";

  switch (*octype_str) {
    case _C_ID:
    case _C_CLASS: 
    {
      id nsobj;
      f_success = rbobj_to_nsobj(obj, &nsobj);
      if (f_success) *(id*)ocdata = nsobj;
      break;
    }

    case _C_SEL:
      *(SEL*)ocdata = rbobj_to_nssel(obj);
      break;

    case _C_UCHR:
      *(unsigned char*)ocdata = (unsigned char) NUM2UINT(rb_Integer(obj));
      break;

    case _C_BOOL:
      {
        unsigned char v;

        switch (TYPE(obj)) {
          case T_FALSE:
          case T_NIL:
            v = 0;
            break;
          case T_TRUE:
          // All other types should be converted as true, to follow the
          // Ruby semantics (where for example any integer is always true,
          // even 0).
          default:
            v = 1;
            break;
        }
        *(unsigned char*)ocdata = v;
      } 
      break;

    case _C_CHR:
      *(char*)ocdata = (char) NUM2INT(rb_Integer(obj));
      break;

    case _C_SHT:
      *(short*)ocdata = (short) NUM2INT(rb_Integer(obj));
      break;

    case _C_USHT:
      *(unsigned short*)ocdata = (unsigned short) NUM2UINT(rb_Integer(obj));
      break;

    case _C_INT:
      *(int*)ocdata = (int) NUM2INT(rb_Integer(obj));
      break;

    case _C_UINT:
      *(unsigned int*)ocdata = (unsigned int) NUM2UINT(rb_Integer(obj));
      break;

    case _C_LNG:
      *(long*)ocdata = (long) NUM2LONG(rb_Integer(obj));
      break;

    case _C_ULNG:
      *(unsigned long*)ocdata = (unsigned long) NUM2ULONG(rb_Integer(obj));
      break;

#if HAVE_LONG_LONG
    case _C_LNG_LNG:
      *(long long*)ocdata = (long long) NUM2LL(rb_Integer(obj));
      break;

    case _C_ULNG_LNG:
      *(unsigned long long*)ocdata = (unsigned long long) NUM2ULL(rb_Integer(obj));
      break;
#endif

    case _C_FLT:
      *(float*)ocdata = (float) RFLOAT(rb_Float(obj))->value;
      break;

    case _C_DBL:
      *(double*)ocdata = RFLOAT(rb_Float(obj))->value;
      break;

    case _C_CHARPTR:
      {
        VALUE str = rb_obj_as_string(obj);
        *(char**)ocdata = StringValuePtr(str);
      }
      break;

    case _C_PTR:
      if (is_id_ptr(octype_str)) {
        f_success = rbobj_to_idptr(obj, ocdata);
      }
      else {
        f_success = rbobj_to_objcptr(obj, ocdata, octype_str + 1);
        if (!f_success)
          f_success = rbobj_to_funcptr(obj, ocdata, octype_str + 1);
      }
      break;

    case _C_ARY_B:
      f_success = rbobj_to_cary(obj, ocdata, octype_str);
      break;   

    default:
      f_success = NO;
      break;
  }

  return f_success;
}

static 
NSStringEncoding kcode_to_nsencoding (const char* kcode) 
{ 
  if (strcmp(kcode, "UTF8") == 0)
    return NSUTF8StringEncoding;
  else if (strcmp(kcode, "SJIS") == 0)
    return CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingMacJapanese);
  else if (strcmp(kcode, "EUC") == 0)
    return NSJapaneseEUCStringEncoding;
  else // "NONE"
    return NSUTF8StringEncoding;
}
#define KCODE_NSSTRENCODING kcode_to_nsencoding(rb_get_kcode()) 

id
rbstr_to_ocstr(VALUE obj)
{
  return [[[NSMutableString alloc] initWithData:[NSData dataWithBytes:RSTRING(obj)->ptr
			    			 length: RSTRING(obj)->len]
			    encoding:KCODE_NSSTRENCODING] autorelease];
}

VALUE
ocstr_to_rbstr(id ocstr)
{
  NSData * data = [(NSString *)ocstr dataUsingEncoding:KCODE_NSSTRENCODING
				     allowLossyConversion:YES];
  return rb_str_new ([data bytes], [data length]);
}

static void
__decode_method_encoding_with_method_signature(NSMethodSignature *methodSignature, unsigned *argc, char **retval_type, char ***arg_types, BOOL strip_first_two_args)
{
  *argc = [methodSignature numberOfArguments];
  if (strip_first_two_args)
    *argc -= 2;
  *retval_type = strdup([methodSignature methodReturnType]);
  if (*argc > 0) {
    unsigned i;
    char **l_arg_types;
    l_arg_types = (char **)malloc(sizeof(char *) * *argc);
    for (i = 0; i < *argc; i++)
      l_arg_types[i] = strdup([methodSignature getArgumentTypeAtIndex:i + (strip_first_two_args ? 2 : 0)]);
    *arg_types = l_arg_types;
  }
  else {
    *arg_types = NULL;
  }
}

static inline const char *
__iterate_until(const char *type, char end)
{
  char begin;
  unsigned nested;

  begin = *type;
  nested = 0;

  do {
    type++;
    if (*type == begin) {
      nested++;
    }
    else if (*type == end) {
      if (nested == 0)
        return type;
      nested--;
    }
  }
  while (YES);

  return NULL;
}

BOOL 
is_id_ptr (const char *type)
{
  if (*type != _C_PTR)
    return NO;

  type++;
  type = encoding_skip_to_first_type(type);

  return *type == _C_ID; 
}

BOOL
is_boxed_ptr (const char *type, struct bsBoxed **boxed)
{
  struct bsBoxed *b;

  if (*type != _C_PTR)
    return NO;

  type++;

  b = find_bs_boxed_by_encoding(type);
  if (b != NULL) {
    if (boxed != NULL)
      *boxed = b;
    return YES;
  }

  return NO;
}

const char *
encoding_skip_to_first_type(const char *type)
{
  while (YES) {
    switch (*type) {
      case _C_CONST:
      case _C_PTR:
      case 'O': // bycopy
      case 'n': // in
      case 'o': // out
      case 'N': // inout
      case 'V': // oneway
        type++;
        break;

      default:
        return type;
    }
  }
  return NULL;
}

const char *
encoding_skip_qualifiers(const char *type)
{
  while (YES) {
    switch (*type) {
      case _C_CONST:
      case 'O': // bycopy
      case 'n': // in
      case 'o': // out
      case 'N': // inout
      case 'V': // oneway
        type++;
        break;

      default:
        return type;
    }
  }
  return NULL;
}

static const char *
__get_first_encoding(const char *type, char *buf, size_t buf_len)
{
  const char *orig_type;
  const char *p;

  orig_type = type;

  type = encoding_skip_to_first_type(type);

  switch (*type) {
    case '\0':
      return NULL;
    case _C_ARY_B:
      type = __iterate_until(type, _C_ARY_E);
      break;
    case _C_STRUCT_B:
      type = __iterate_until(type, _C_STRUCT_E);
      break;
    case _C_UNION_B:
      type = __iterate_until(type, _C_UNION_E);
      break;
  }

  type++;
  p = type;
  while (*p >= '0' && *p <= '9') { p++; }

  if (buf != NULL) {
    size_t len = (long)(type - orig_type);
    assert(len < buf_len);
    strncpy(buf, orig_type, len);
    buf[len] = '\0';
  }

  return p;
}

// 10.4 or lower, use NSMethodSignature.
// Otherwise, use the Objective-C runtime API, which is faster and more reliable with structures encoding.
void
decode_method_encoding(const char *encoding, NSMethodSignature *methodSignature, unsigned *argc, char **retval_type, char ***arg_types, BOOL strip_first_two_args)
{
  assert(encoding != NULL || methodSignature != nil);

  if (encoding == NULL) {
    DATACONV_LOG("decoding method encoding using method signature %p", methodSignature);
    __decode_method_encoding_with_method_signature(methodSignature, argc, retval_type, arg_types, strip_first_two_args);   
  }
  else {
    char buf[1024];

    DATACONV_LOG("decoding method encoding '%s' manually", encoding);
    encoding = __get_first_encoding(encoding, buf, sizeof buf);
    DATACONV_LOG("retval -> %s", buf);
    *retval_type = strdup(buf);
    if (strip_first_two_args) {
      DATACONV_LOG("skipping first two args");
      encoding = __get_first_encoding(encoding, NULL, 0);    
      encoding = __get_first_encoding(encoding, NULL, 0);    
    }
    *argc = 0;
    // Do a first pass to know the argc 
    if (encoding != NULL) {
      const char *p = encoding;
      while ((p = __get_first_encoding(p, NULL, 0)) != NULL) { (*argc)++; }
    }
    DATACONV_LOG("argc -> %d", *argc);
    if (*argc > 0) {
      unsigned i;
      char **p;
      i = 0;
      p = (char **)malloc(sizeof(char *) * (*argc));
      while ((encoding = __get_first_encoding(encoding, buf, sizeof buf)) != NULL) {
        DATACONV_LOG("arg[%d] -> %s", i, buf);
        p[i++] = strdup(buf);
      }
      *arg_types = p;
    }
    else {
      *arg_types = NULL;
    }
  }
}

void
set_octypes_for_format_str (char **octypes, unsigned len, char *format_str)
{
  unsigned i, j, format_str_len;

  // We cannot display this log because the given `format_str' format string will be evaluated within
  // the output message. Fortunately, Ruby format string APIs do not do that.
  //DATACONV_LOG("decoding format string `%s' types for %d argument(s)", format_str, len);

  format_str_len = strlen(format_str);
  i = j = 0;

  while (i < format_str_len) {
    if (format_str[i++] != '%')
      continue;
    if (i < format_str_len && format_str[i] == '%') {
      i++;
      continue;
    }
    while (i < format_str_len) {
      char *type = NULL;
      switch (format_str[i++]) {
        case 'd':
        case 'i':
        case 'o':
        case 'u':
        case 'x':
        case 'X':
        case 'c':
        case 'C':
          type = "i"; // _C_INT;
          break;

        case 'D':
        case 'O':
        case 'U':
          type = "l"; // _C_LNG;
          break;

        case 'f':       
        case 'F':
        case 'e':       
        case 'E':
        case 'g':       
        case 'G':
        case 'a':
        case 'A':
          type = "d"; // _C_DBL;
          break;

        case 's':
        case 'S':
          type = "*"; // _C_CHARPTR;
          break;

        case 'p':
          type = "^"; // _C_PTR;
          break;

        case '@':
          type = "@"; // _C_ID;
          break;
      }

      if (type != NULL) {
        DATACONV_LOG("found format string token #%d of type '%s'", j, type);

        if (len == 0 || j >= len)
          rb_raise(rb_eArgError, "Too much tokens in the format string `%s' for the given %d argument(s)", format_str, len);

        octypes[j++] = type;

        break;
      }
    }
  }
  for (; j < len; j++)
    octypes[j] = "@"; // _C_ID;
}
