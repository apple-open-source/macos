/* 
 * Copyright (c) 2006-2007, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import "cls_objcptr.h"
#import "ocdata_conv.h"
#import "mdl_osxobjc.h"
#import <Foundation/Foundation.h>

static VALUE _kObjcPtr = Qnil;

struct _objcptr_data {
  long    allocated_size;
  void *  cptr;
  const char *  encoding;
};

#define OBJCPTR_DATA_PTR(o) ((struct _objcptr_data*)(DATA_PTR(o)))
#define CPTR_OF(o) (OBJCPTR_DATA_PTR(o)->cptr)
#define ALLOCATED_SIZE_OF(o) (OBJCPTR_DATA_PTR(o)->allocated_size)
#define ENCODING_OF(o) (OBJCPTR_DATA_PTR(o)->encoding)

static void _set_encoding(VALUE obj, const char* enc)
{
  if (ENCODING_OF(obj))
    free ((char*)ENCODING_OF(obj));

  char* buf = malloc(strlen(enc) + 1);
  strcpy(buf, enc);
  ENCODING_OF(obj) = buf;
}

struct _encoding_type_rec {
  struct _encoding_type_rec* next;
  RB_ID        key;
  const char*  encoding;
};

struct _encoding_type_rec* _encoding_type_list = NULL;

static void
_add_encoding_type(const char* key, const char* encstr)
{
  struct _encoding_type_rec* rec;

  if (ocdata_size(encstr) > 0) {
    rec = (struct _encoding_type_rec*)
      malloc(sizeof(struct _encoding_type_rec));
    rec->key      = rb_intern(key);
    rec->encoding = encstr;
    rec->next     = _encoding_type_list;
    _encoding_type_list = rec;
  }
}

static const char* initial_types[][2] = {
  {      "char", "c" },
  {     "uchar", "C" },
  {     "short", "s" },
  {    "ushort", "S" },
  {       "int", "i" },
  {      "uint", "I" },
  {      "long", "l" },
  {     "ulong", "L" },
  {  "longlong", "q" },
  { "ulonglong", "Q" },
  {     "float", "f" },
  {    "double", "d" },
  { NULL, NULL }
};

static void _initialize_encoding_type_list()
{
  const char* (*ip)[2];

  if (_encoding_type_list != NULL) return;
  
  ip = initial_types;
  while ((*ip)[0] != NULL) {
    _add_encoding_type((*ip)[0], (*ip)[1]);
    ip++;
  }
}

static const struct _encoding_type_rec*
_lookup_encoding_type(VALUE key)
{
  struct _encoding_type_rec* rec;

  _initialize_encoding_type_list();
  rec = _encoding_type_list;
  while (rec != NULL) {
    if (SYMBOL_P(key) && rec->key == rb_to_id(key))
      return rec;
    rec = rec->next;
  }
  return NULL;
}


/** for debugging stuff **/
void cptrlog(const char* s, VALUE obj)
{
  NSStringEncoding* p = (NSStringEncoding*) CPTR_OF(obj);
  NSLog(@"%s: < p,*p > %u(%x) %d(%x)", s, p, p, *p, *p);
}
/** **/

static void
_objcptr_data_free(struct _objcptr_data* dp)
{
  if (dp != NULL) {
    if (dp->allocated_size > 0)
      free (dp->cptr);
    if (dp->encoding)
      free ((char*)dp->encoding);
    dp->allocated_size = 0;
    dp->cptr = NULL;
    dp->encoding = NULL;
    free (dp);
  }
}

static struct _objcptr_data*
_objcptr_data_new()
{
  struct _objcptr_data* dp = NULL;
  dp = malloc (sizeof(struct _objcptr_data)); // ALLOC?
  dp->allocated_size = 0;
  dp->cptr = NULL;
  dp->encoding = NULL;
  return dp;
}

static VALUE
_objcptr_s_new(VALUE klass, long len)
{
  VALUE obj;
  obj = Data_Wrap_Struct(klass, 0, _objcptr_data_free, _objcptr_data_new());
  rb_obj_taint(obj);
  if (len > 0) {
    CPTR_OF (obj) = (void*) malloc (len);
    if (CPTR_OF (obj)) {
      ALLOCATED_SIZE_OF(obj) = len;
      memset(CPTR_OF (obj), 0, len);
    }
    rb_obj_untaint(obj);
  }
  return obj;
}

static VALUE
rb_objcptr_s_allocate(int argc, VALUE* argv, VALUE klass)
{
  VALUE  key, cnt, obj;
  size_t length;
  const struct _encoding_type_rec* rec;

  argc = rb_scan_args(argc, argv, "11", &key, &cnt);

  if (argc == 1 && ! SYMBOL_P(key)) {
    length = NUM2LONG(key);
    obj = _objcptr_s_new (klass, length);
    _set_encoding(obj, "C"); /* uchar */
    return obj;
  }

  rec = _lookup_encoding_type(key);
  if (rec == NULL)
    rb_raise(rb_eRuntimeError, "unsupported encoding -- %s", 
	     rb_id2name(rb_to_id(key)));

  length = (argc == 2) ? NUM2LONG(cnt) : 1;
  length *= ocdata_size(rec->encoding);
  obj = _objcptr_s_new (klass, length);
  _set_encoding(obj, rec->encoding);
  return obj;
}

static VALUE
rb_objcptr_s_allocate_as_int8(VALUE klass)
{
  VALUE obj;
  obj = _objcptr_s_new (klass, sizeof(int8_t));
  _set_encoding(obj, "c"); /* char */
  return obj;
}

static VALUE
rb_objcptr_s_allocate_as_int16(VALUE klass)
{
  VALUE obj;
  obj = _objcptr_s_new (klass, sizeof(int16_t));
  _set_encoding(obj, "s"); /* short */
  return obj;
}

static VALUE
rb_objcptr_s_allocate_as_int32(VALUE klass)
{
  VALUE obj;
  obj = _objcptr_s_new (klass, sizeof(int32_t));
  _set_encoding(obj, "i"); /* int */
  return obj;
}

static VALUE
rb_objcptr_inspect(VALUE rcv)
{
  char s[512];
  VALUE rbclass_name;

  rbclass_name = rb_mod_name(CLASS_OF(rcv));
  snprintf(s, sizeof(s), "#<%s:0x%lx cptr=%p allocated_size=%ld encoding=%s>",
           STR2CSTR(rbclass_name),
           NUM2ULONG(rb_obj_id(rcv)),
           CPTR_OF(rcv),
           ALLOCATED_SIZE_OF(rcv),
           ENCODING_OF(rcv) ? ENCODING_OF(rcv) : "(unknown)");
  // cptrlog ("rb_objcptr_inspect", rcv);
  return rb_str_new2(s);
}

static VALUE
rb_objcptr_allocated_size(VALUE rcv)
{
  return UINT2NUM (ALLOCATED_SIZE_OF (rcv));
}

long
objcptr_allocated_size(VALUE rcv)
{
  return ALLOCATED_SIZE_OF (rcv);
}

static VALUE
rb_objcptr_encoding(VALUE rcv)
{
  return ENCODING_OF(rcv) ?
    rb_str_new2(ENCODING_OF(rcv)):
    Qnil;
}

static VALUE
rb_objcptr_regard_as(VALUE rcv, VALUE key)
{
  const struct _encoding_type_rec* rec;
  BOOL ok;
  const char *encoding;

  ok = YES; 
  rec = _lookup_encoding_type(key);
  if (rec == NULL) {
#if __LP64__
    unsigned long size;
#else
    unsigned int size;
#endif
    ok = NO;
    encoding = STR2CSTR(key);
    @try {
      NSGetSizeAndAlignment(STR2CSTR(key), &size, NULL);
      if (size > 0)
        ok = YES;
    }
    @catch (id exception) {}
  }
  else {
    encoding = rec->encoding;
  }

  if (!ok)
    rb_raise(rb_eRuntimeError, "unsupported encoding -- %s", 
	     rb_id2name(rb_to_id(key)));

  _set_encoding(rcv, encoding);
  return rcv;
}

static VALUE
rb_objcptr_bytestr_at(VALUE rcv, VALUE offset, VALUE length)
{
  return rb_tainted_str_new ((char*)CPTR_OF(rcv) + NUM2LONG(offset), NUM2LONG(length));
}

static VALUE
rb_objcptr_bytestr(int argc, VALUE* argv, VALUE rcv)
{
  VALUE  rb_length;
  long length;

  length = ALLOCATED_SIZE_OF(rcv);
  rb_scan_args(argc, argv, "01", &rb_length);
  if (length == 0 || rb_length != Qnil) {
    if (! FIXNUM_P(rb_length))
      Check_Type(rb_length, T_BIGNUM);
    length = NUM2LONG(rb_length);
  }
  return rb_tainted_str_new (CPTR_OF(rcv), length);
}

static VALUE
rb_objcptr_int8_at(VALUE rcv, VALUE index)
{
  int8_t* ptr = (int8_t*) CPTR_OF(rcv);
  return INT2NUM ( ptr [NUM2LONG(index)] );
}

static VALUE
rb_objcptr_uint8_at(VALUE rcv, VALUE index)
{
  u_int8_t* ptr = (u_int8_t*) CPTR_OF(rcv);
  return UINT2NUM ( ptr [NUM2LONG(index)] );
}

static VALUE
rb_objcptr_int16_at(VALUE rcv, VALUE index)
{
  int16_t* ptr = (int16_t*) CPTR_OF(rcv);
  return INT2NUM ( ptr [NUM2LONG(index)] );
}

static VALUE
rb_objcptr_uint16_at(VALUE rcv, VALUE index)
{
  u_int16_t* ptr = (u_int16_t*) CPTR_OF(rcv);
  return UINT2NUM ( ptr [NUM2LONG(index)] );
}

static VALUE
rb_objcptr_int32_at(VALUE rcv, VALUE index)
{
  int32_t* ptr = (int32_t*) CPTR_OF(rcv);
  return INT2NUM ( ptr [NUM2LONG(index)] );
}

static VALUE
rb_objcptr_uint32_at(VALUE rcv, VALUE index)
{
  u_int32_t* ptr = (u_int32_t*) CPTR_OF(rcv);
  return UINT2NUM ( ptr [NUM2LONG(index)] );
}


static VALUE
rb_objcptr_int8(VALUE rcv)
{
  return INT2NUM (* (int8_t*) CPTR_OF(rcv));
}

static VALUE
rb_objcptr_uint8(VALUE rcv)
{
  return UINT2NUM (* (u_int8_t*) CPTR_OF(rcv));
}

static VALUE
rb_objcptr_int16(VALUE rcv)
{
  return INT2NUM (* (int16_t*) CPTR_OF(rcv));
}

static VALUE
rb_objcptr_uint16(VALUE rcv)
{
  return UINT2NUM (* (u_int16_t*) CPTR_OF(rcv));
}

static VALUE
rb_objcptr_int32(VALUE rcv)
{
  return INT2NUM (* (int32_t*) CPTR_OF(rcv));
}

static VALUE
rb_objcptr_uint32(VALUE rcv)
{
  return UINT2NUM (* (u_int32_t*) CPTR_OF(rcv));
}


/** class methods called from the Objc World **/
VALUE
objcptr_s_class ()
{
  return _kObjcPtr;
}

VALUE
objcptr_s_new_with_cptr (void* cptr, const char* encoding)
{
  VALUE obj;
  obj = _objcptr_s_new (_kObjcPtr, 0);
  CPTR_OF(obj) = cptr;
  _set_encoding(obj, encoding + 1); // skipping the first type  
  return obj;
}

/** instance methods called from the Objc World **/
void* objcptr_cptr (VALUE rcv)
{
  if (CLASS_OF(rcv) == _kObjcPtr) {
    OBJ_TAINT(rcv);                // A raw C pointer is passed to the C world, so it may taint.
    return CPTR_OF(rcv);
  }
  return NULL;
}

static VALUE
objcptr_at (VALUE rcv, VALUE key)
{
  unsigned offset;
  VALUE val;

  Check_Type(key, T_FIXNUM);
  offset = FIX2INT(key);  
  offset *= ocdata_size(ENCODING_OF(rcv));

  if (!ocdata_to_rbobj(Qnil, ENCODING_OF(rcv), CPTR_OF(rcv) + offset, &val, NO))
    rb_raise(rb_eRuntimeError, "Can't convert element of type '%s' at index %d offset %d", ENCODING_OF(rcv), FIX2INT(key), offset);

  return val; 
}

static VALUE
rb_objcptr_set_at (VALUE rcv, VALUE key, VALUE val)
{
  unsigned offset;

  Check_Type(key, T_FIXNUM);

  if (ENCODING_OF(rcv) == NULL)
    rb_raise(rb_eRuntimeError, "#[]= can't be called on this instance");

  offset = FIX2INT(key);  
  offset *= ocdata_size(ENCODING_OF(rcv));
 
  if (!rbobj_to_ocdata(val, ENCODING_OF(rcv), CPTR_OF(rcv) + offset, NO))
    rb_raise(rb_eRuntimeError, "Can't convert given object to type '%s' at index %d offset %d", ENCODING_OF(rcv), FIX2INT(key), offset);

  return val;
}

static VALUE
rb_objcptr_assign (VALUE rcv, VALUE obj)
{
  if (ENCODING_OF(rcv) == NULL)
    rb_raise(rb_eRuntimeError, "#assign can't be called on this instance");

  if (!rbobj_to_ocdata(obj, ENCODING_OF(rcv), CPTR_OF(rcv), NO))
    rb_raise(rb_eArgError, "Can't convert object to type '%s'", ENCODING_OF(rcv));
  
  return rcv;
}

static VALUE
rb_objcptr_cast_as (VALUE rcv, VALUE encoding)
{
  VALUE val;
  const char* enc = StringValuePtr(encoding);
  void* ptr;
  
  if (*enc == '^') {
    enc++;
    ptr = CPTR_OF(rcv);
  } else {
    ptr = &CPTR_OF(rcv);
  }
  
  if (!ocdata_to_rbobj(Qnil, enc, ptr, &val, NO))
    rb_raise(rb_eArgError, "Can't convert object to type '%s'", StringValuePtr(encoding));
  return val;
}

static VALUE
objcptr_to_a (VALUE rcv, VALUE index, VALUE count)
{
  size_t type_size, offset;
  long   i, max;
  void*  ptr;
  const char* type;
  VALUE val, ary;

  if (!FIXNUM_P(index)) Check_Type(index, T_BIGNUM);
  if (!FIXNUM_P(count)) Check_Type(count, T_BIGNUM);

  type = ENCODING_OF(rcv);
  type_size = ocdata_size(type);
  offset = NUM2UINT(index) * type_size;
  ptr = CPTR_OF(rcv);
  max = NUM2LONG(count);
  ary = rb_ary_new2(max);

  for (i = 0; i < max; i++) {
    val = Qnil;
    if (! ocdata_to_rbobj(Qnil, type, ptr + offset, &val, NO))
      rb_raise(rb_eRuntimeError,
               "Can't convert element of type '%s' at index %d offset %d", 
               type, i, offset);
    rb_ary_store(ary, i, val);
    offset += type_size;
  }
  return ary; 
}

static VALUE
rb_objcptr_at (int argc, VALUE* argv, VALUE rcv)
{
  VALUE key, count;
  
  if (ENCODING_OF(rcv) == NULL)
    rb_raise(rb_eRuntimeError, "#[] can't be called on this instance");

  argc = rb_scan_args(argc, argv, "11", &key, &count);
  if (argc == 1)
    return objcptr_at(rcv, key);
  else if (argc == 2)
    return objcptr_to_a(rcv, key, count);
  return Qnil;
}

/*******/

VALUE
init_cls_ObjcPtr(VALUE outer)
{
  _kObjcPtr = rb_define_class_under (outer, "ObjcPtr", rb_cObject);

  rb_define_singleton_method (_kObjcPtr, "new", rb_objcptr_s_allocate, -1);
  rb_define_singleton_method (_kObjcPtr, "allocate_as_int8", rb_objcptr_s_allocate_as_int8, 0);
  rb_define_singleton_method (_kObjcPtr, "allocate_as_int16", rb_objcptr_s_allocate_as_int16, 0);
  rb_define_singleton_method (_kObjcPtr, "allocate_as_int32", rb_objcptr_s_allocate_as_int32, 0);
  rb_define_singleton_method (_kObjcPtr, "allocate_as_int", rb_objcptr_s_allocate_as_int32, 0);
  rb_define_singleton_method (_kObjcPtr, "allocate_as_bool", rb_objcptr_s_allocate_as_int8, 0);

  rb_define_method (_kObjcPtr, "inspect", rb_objcptr_inspect, 0);
  rb_define_method (_kObjcPtr, "allocated_size", rb_objcptr_allocated_size, 0);

  rb_define_method (_kObjcPtr, "encoding", rb_objcptr_encoding, 0);
  rb_define_method (_kObjcPtr, "__regard_as__", rb_objcptr_regard_as, 1);
  rb_define_alias  (_kObjcPtr, "regard_as", "__regard_as__");

  rb_define_method (_kObjcPtr, "bytestr_at", rb_objcptr_bytestr_at, 2);
  rb_define_method (_kObjcPtr, "bytestr", rb_objcptr_bytestr, -1);

  rb_define_method (_kObjcPtr, "int8_at", rb_objcptr_int8_at, 1);
  rb_define_method (_kObjcPtr, "uint8_at", rb_objcptr_uint8_at, 1);
  rb_define_method (_kObjcPtr, "int16_at", rb_objcptr_int16_at, 1);
  rb_define_method (_kObjcPtr, "uint16_at", rb_objcptr_uint16_at, 1);
  rb_define_method (_kObjcPtr, "int32_at", rb_objcptr_int32_at, 1);
  rb_define_method (_kObjcPtr, "uint32_at", rb_objcptr_uint32_at, 1);
  rb_define_alias (_kObjcPtr, "int_at", "int32_at");
  rb_define_alias (_kObjcPtr, "uint_at", "uint32_at");
  rb_define_alias (_kObjcPtr, "bool_at", "uint8_at");

  rb_define_method (_kObjcPtr, "int8", rb_objcptr_int8, 0);
  rb_define_method (_kObjcPtr, "uint8", rb_objcptr_uint8, 0);
  rb_define_method (_kObjcPtr, "int16", rb_objcptr_int16, 0);
  rb_define_method (_kObjcPtr, "uint16", rb_objcptr_uint16, 0);
  rb_define_method (_kObjcPtr, "int32", rb_objcptr_int32, 0);
  rb_define_method (_kObjcPtr, "uint32", rb_objcptr_uint32, 0);
  rb_define_alias (_kObjcPtr, "int", "int32");
  rb_define_alias (_kObjcPtr, "uint", "uint32");
  rb_define_alias (_kObjcPtr, "bool", "uint8");

  rb_define_method (_kObjcPtr, "[]", rb_objcptr_at, -1);
  rb_define_method (_kObjcPtr, "[]=", rb_objcptr_set_at, 2);

  rb_define_method (_kObjcPtr, "assign", rb_objcptr_assign, 1);
  
  rb_define_method (_kObjcPtr, "cast_as", rb_objcptr_cast_as, 1);

  return _kObjcPtr;
}
