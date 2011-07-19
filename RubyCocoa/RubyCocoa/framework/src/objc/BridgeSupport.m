/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <Foundation/Foundation.h>
#import "osx_ruby.h"
#import "osx_intern.h"
#import "BridgeSupport.h"
#import <dlfcn.h>
#import <st.h>
#import <env.h>
#import <objc/objc-class.h>
#import <objc/objc-runtime.h>
#import "ocdata_conv.h"
#import "ffi.h"
#import "internal_macros.h"
#import "cls_objcid.h"
#import "BridgeSupportLexer.h"
#import "RBClassUtils.h"
#import "mdl_osxobjc.h"
#import "ocexception.h"
#import "objc_compat.h"

static VALUE cOSXBoxed;
static ID ivarEncodingID;

VALUE objboxed_s_class(void)
{
  return cOSXBoxed;
}

static struct st_table *bsBoxed;       // boxed encoding -> struct bsBoxed
static struct st_table *bsCFTypes;     // encoding -> struct bsCFType
static struct st_table *bsCFTypes2;    // CFTypeID -> struct bsCFType
static struct st_table *bsFunctions;   // function name -> struct bsFunction
static struct st_table *bsConstants;   // constant name -> type
static struct st_table *bsMagicCookieConstants;                 // constant value -> struct bsConst
static struct st_table *bsClasses;     // class name -> struct bsClass
static struct st_table *bsInformalProtocolClassMethods;         // selector -> struct bsInformalProtocolMethod
static struct st_table *bsInformalProtocolInstanceMethods;      // selector -> struct bsInformalProtocolMethod

struct bsFunction *current_function = NULL;

#define MAX_ENCODE_LEN 4096

#define CAPITALIZE(x)         \
  do {                        \
    if (islower(x[0]))        \
      x[0] = toupper(x[0]);   \
  }                           \
  while (0)

#define DECAPITALIZE(x)       \
  do {                        \
    if (isupper(x[0]))        \
      x[0] = tolower(x[0]);   \
  }                           \
  while (0)

#if HAS_LIBXML2
#include <libxml/xmlreader.h>

static BOOL
next_node(xmlTextReaderPtr reader)
{
  int   retval;

  retval = xmlTextReaderRead(reader);
  if (retval == 0)
    return NO;

  if (retval < 0)
    rb_raise(rb_eRuntimeError, "parsing error: %d", retval);

  return YES;
}

static inline char *
get_attribute(xmlTextReaderPtr reader, const char *name)
{
  return (char *)xmlTextReaderGetAttribute(reader, (const xmlChar *)name);
}

static inline char *
get_attribute_and_check(xmlTextReaderPtr reader, const char *name)
{
  char *  attribute;

  attribute = get_attribute(reader, name); 
  if (attribute == NULL)
    rb_raise(rb_eRuntimeError, "expected attribute `%s' for element `%s'", name, xmlTextReaderConstName(reader));

  if (strlen(attribute) == 0) {
    free(attribute);
    rb_raise(rb_eRuntimeError, "empty attribute `%s' for element `%s'", name, xmlTextReaderConstName(reader));
  }

  return attribute;
}

static inline char *
get_type_attribute(xmlTextReaderPtr reader)
{
  xmlChar * value;

#if __LP64__
  value = xmlTextReaderGetAttribute(reader, (xmlChar *)"type64");
  if (value == NULL)
#endif
  value = xmlTextReaderGetAttribute(reader, (xmlChar *)"type");

  return (char *)value;
}

static inline char *
get_type_attribute_and_check(xmlTextReaderPtr reader)
{
  char * value;

  value = get_type_attribute(reader);
  if (value == NULL)
    rb_raise(rb_eRuntimeError, "expected attribute `type' for element `%s'", xmlTextReaderConstName(reader));

  if (strlen(value) == 0) {
    free(value);
    rb_raise(rb_eRuntimeError, "empty attribute `type' for element `%s'", xmlTextReaderConstName(reader));
  }

  return value;
}

static inline char *
get_value_and_check(xmlTextReaderPtr reader)
{
  xmlChar * value;
  
  value = xmlTextReaderValue(reader);
  if (value == NULL)
    rb_raise(rb_eRuntimeError, "expected value for element `%s'", xmlTextReaderConstName(reader));

  return (char *)value;
}

static void
get_c_ary_type_attribute(xmlTextReaderPtr reader, bsCArrayArgType *type, int *value)
{
  char *c_ary_type;

  if ((c_ary_type = get_attribute(reader, "c_array_length_in_arg")) != NULL) {
    *type = bsCArrayArgDelimitedByArg;
    *value = atoi(c_ary_type);
  }
  else if ((c_ary_type = get_attribute(reader, "c_array_of_fixed_length")) != NULL) {
    *type = bsCArrayArgFixedLength;
    *value = atoi(c_ary_type);
  }
  else if ((c_ary_type = get_attribute(reader, "c_array_of_variable_length")) != NULL
           && strcmp(c_ary_type, "true") == 0) {
    *type = bsCArrayArgVariableLength;
    *value = -1;
  }
  else if ((c_ary_type = get_attribute(reader, "c_array_delimited_by_null")) != NULL
           && strcmp(c_ary_type, "true") == 0) {
    *type = bsCArrayArgDelimitedByNull;
    *value = -1;
  }
  else {
    *type = bsCArrayArgUndefined;
    *value = -1;
  }

  if (c_ary_type != NULL)
    free(c_ary_type);
}

static inline BOOL
get_boolean_attribute(xmlTextReaderPtr reader, const char *name, BOOL default_value)
{
  char *value;
  BOOL ret;

  value = get_attribute(reader, name);
  if (value == NULL)
    return default_value;
  ret = strcmp(value, "true") == 0;
  free(value);
  return ret;
}

static void
free_bs_call_entry (struct bsCallEntry *entry)
{
  if (entry->argv != NULL) {
    unsigned i;
    for (i = 0; i < entry->argc; i++) {
      if (entry->argv[i].octypestr != NULL)
        free(entry->argv[i].octypestr);
      if (entry->argv[i].sel_of_type != NULL)
        free(entry->argv[i].sel_of_type);
    }
    free(entry->argv);
  }
  if (entry->retval) {
    if (entry->retval->octypestr != NULL)
      free(entry->retval->octypestr);
    free(entry->retval);
  }
}

static void
free_bs_function (struct bsFunction *func)
{
  free_bs_call_entry((struct bsCallEntry *)func);
  free(func->name);
  free(func);
}

static void
free_bs_method (struct bsMethod *method)
{
  free_bs_call_entry((struct bsCallEntry *)method);
  free(method->selector);
  if (method->suggestion != NULL)
    free(method->suggestion);
  free(method);
}

static BOOL 
undecorate_encoding(const char *src, char *dest, size_t dest_len, struct bsStructField *fields, size_t fields_count, int *out_fields_count)
{
  const char *p_src;
  char *p_dst;
  char *pos;
  size_t src_len;
  unsigned field_idx;
  unsigned i;

  p_src = src;
  p_dst = dest;
  src_len = strlen(src);
  field_idx = 0;
  if (out_fields_count != NULL)
    *out_fields_count = 0;

  for (;;) {
    struct bsStructField *field;
    size_t len;

    field = field_idx < fields_count ? &fields[field_idx] : NULL;
    if (field != NULL) {
      field->name = NULL;
      field->encoding = NULL;
    }
    if (field == NULL && fields != NULL) {
      // Not enough fields!
      goto bails;
    }

    // Locate the first field, if any.
    pos = strchr(p_src, '"');

    // Copy what's before the first field, or the rest of the source.
    len = MIN(pos == NULL ? src_len - (p_src - src) + 1 : pos - p_src, dest_len - (p_dst - dest));
    strncpy(p_dst, p_src, len);
    p_dst += len;

    // We can break if there wasn't any field.
    if (pos == NULL)
      break;

    // Jump to the end of the field, saving the field name if necessary.
    p_src = pos + 1;
    pos = strchr(p_src, '"');
    if (pos == NULL) {
      DLOG("MDLOSX", "Can't find the end of field delimiter starting at %d", p_src - src);
      goto bails; 
    }
    if (field != NULL) {
      field->name = (char *)malloc((sizeof(char) * (pos - p_src)) + 1);
      ASSERT_ALLOC(field->name);
      strncpy(field->name, p_src, pos - p_src);
      field->name[pos - p_src] = '\0';
      field_idx++; 
    }
    p_src = pos + 1; 
    pos = NULL;

    // Save the field encoding if necessary.
    if (field != NULL) {
      BOOL is_struct;
      BOOL ok;
      int nested;

      is_struct = *p_src == '{' || *p_src == '(';
      for (i = 0, ok = NO, nested = 0; 
           i < src_len - (p_src - src) && !ok; 
           i++) {

        char c = p_src[i];

        if (is_struct) {
          char opposite = *p_src == '{' ? '}' : ')';
          // Encoding is a structure, we need to match the closing '}',
          // taking into account that other structures can be nested in it.
          if (c == opposite) {
            if (nested == 0)
              ok = YES;
            else
              nested--;  
          }
          else if (c == *p_src && i > 0)
            nested++;
        }
        else {
          // Easy case, just match another field delimiter, or the end
          // of the encoding.
          if (c == '"' || c == '}') {
            i--;
            ok = YES;
          } 
        }
      }

      if (ok == NO) {
        DLOG("MDLOSX", "Can't find the field encoding starting at %d", p_src - src);
        goto bails;
      }

      if (is_struct) {
        char buf[MAX_ENCODE_LEN];
        char buf2[MAX_ENCODE_LEN];
   
        strncpy(buf, p_src, MIN(sizeof buf, i));
        buf[MIN((sizeof buf) - 1, i)] = '\0';        
     
        if (!undecorate_encoding(buf, buf2, sizeof buf2, NULL, 0, NULL)) {
          DLOG("MDLOSX", "Can't un-decode the field encoding '%s'", buf);
          goto bails;
        }

        len = strlen(buf2); 
        field->encoding = (char *)malloc((sizeof(char) * len) + 1);
        ASSERT_ALLOC(field->encoding);
        strncpy(field->encoding, buf2, len);
        field->encoding[len] = '\0';
      }
      else {
        field->encoding = (char *)malloc((sizeof(char) * i) + 1);
        ASSERT_ALLOC(field->encoding);
        strncpy(field->encoding, p_src, i);
        field->encoding[i] = '\0';
        len = i;
      }

      strncpy(p_dst, field->encoding, len);

      p_src += i;
      p_dst += len;
    }
  }

  *p_dst = '\0';
  if (out_fields_count != NULL)
    *out_fields_count = field_idx;
  return YES;

bails:
  // Free what we allocated!
  if (fields != NULL) {
    for (i = 0; i < field_idx; i++) {
      if (fields[i].name != NULL) {
        free(fields[i].name);
      }
      if (fields[i].encoding != NULL) {
        free(fields[i].encoding);
      }
    }
  }
  return NO;
}

static VALUE
rb_bs_boxed_get_encoding (VALUE rcv)
{
  return rb_ivar_get(rcv, ivarEncodingID);  
}

static VALUE
rb_bs_boxed_get_size (VALUE rcv)
{
  struct bsBoxed *boxed;

  boxed = find_bs_boxed_for_klass(rcv);

  return LONG2NUM(bs_boxed_size(boxed));
}

static VALUE
rb_bs_boxed_get_fields (VALUE rcv)
{
  struct bsBoxed *boxed;
  VALUE ary;
  unsigned i;

  boxed = find_bs_boxed_for_klass(rcv);
  ary = rb_ary_new();

  if (boxed->type != bsBoxedStructType)
    return ary;

  for (i = 0; i < boxed->opt.s.field_count; i++) {
    struct bsStructField *  field;

    field = &boxed->opt.s.fields[i];
    rb_ary_push(ary, ID2SYM(rb_intern(field->name))); 
  }

  return ary;
}

static VALUE
rb_bs_boxed_is_opaque (VALUE rcv)
{
  struct bsBoxed *boxed;
  BOOL opaque;

  boxed = find_bs_boxed_for_klass(rcv);
  opaque = boxed->type == bsBoxedStructType ? boxed->opt.s.opaque : YES;

  return opaque ? Qtrue : Qfalse;
}

struct bsBoxed *
find_bs_boxed_for_klass (VALUE klass)
{
  VALUE encoding;

  encoding = rb_ivar_get(klass, ivarEncodingID);
  if (NIL_P(encoding))
    return NULL;

  if (TYPE(encoding) != T_STRING)
    return NULL;

  return find_bs_boxed_by_encoding(StringValuePtr(encoding));
}

size_t 
bs_boxed_size(struct bsBoxed *bs_struct)
{
  if (bs_struct->size == 0 && bs_struct->type == bsBoxedStructType) {
    long size;
    unsigned i;
 
    for (i = 0, size = 0; i < bs_struct->opt.s.field_count; i++)
      size += ocdata_size(bs_struct->opt.s.fields[i].encoding);           

    bs_struct->size = size; 
  }
  return bs_struct->size;
}

static inline struct bsBoxed *
rb_bs_struct_get_bs_struct (VALUE rcv)
{
  struct bsBoxed *bs_struct;

  bs_struct = find_bs_boxed_for_klass(rcv);
  if (bs_struct == NULL) 
    rb_bug("Can't get bridge support structure for the given klass %p", rcv);
  if (bs_struct->type != bsBoxedStructType)
    rb_bug("Invalid bridge support boxed structure type %d", bs_struct->type);

  return bs_struct;
}

static VALUE
rb_bs_struct_new (int argc, VALUE *argv, VALUE rcv)
{
  struct bsBoxed *bs_struct;
  void *data;
  unsigned i;
  unsigned pos;

  bs_struct = rb_bs_struct_get_bs_struct(rcv);
#if 0
  // Probably not necessary.
  if (argc == 1 && TYPE(argv[0]) == T_ARRAY) {
    argc = RARRAY(argv[0])->len;
    argv = RARRAY(argv[0])->ptr;
  }
#endif

  if (argc > 0 && argc != bs_struct->opt.s.field_count)
    rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", argc, bs_struct->opt.s.field_count);

  bs_boxed_size(bs_struct);
  if (bs_struct->size == 0)
    rb_raise(rb_eRuntimeError, "can't instantiate struct '%s' of 0 size", bs_struct->name);

  data = (void *)calloc(1, bs_struct->size);
  ASSERT_ALLOC(data);

  if (argc > 0) {
    for (i = 0, pos = 0; i < bs_struct->opt.s.field_count; i++) {
      const char *field_octype;

      field_octype = bs_struct->opt.s.fields[i].encoding;

      if (!rbobj_to_ocdata(argv[i], field_octype, data + pos, NO))
        rb_raise(rb_eArgError, "Cannot convert arg #%d of type %d to Objective-C", i, field_octype);

      pos += ocdata_size(field_octype);
    }
  }
  
  return Data_Wrap_Struct(rcv, NULL, free, data);
}

VALUE 
rb_bs_boxed_new_from_ocdata (struct bsBoxed *bs_boxed, void *ocdata)
{
  void *data;

  if (ocdata == NULL)
    return Qnil;
  if (bs_boxed->type == bsBoxedOpaqueType) {
    if (*(void **)ocdata == NULL)
      return Qnil;
  }
  
  if (bs_boxed->type == bsBoxedStructType)
    bs_boxed_size(bs_boxed);
  if (bs_boxed->size == 0)
    rb_raise(rb_eRuntimeError, "can't instantiate boxed '%s' of size 0", bs_boxed->name);

  data = (void *)malloc(bs_boxed->size);
  ASSERT_ALLOC(data);
  memcpy(data, ocdata, bs_boxed->size);
  
  return Data_Wrap_Struct(bs_boxed->klass, NULL, free, data);
}

VALUE
rb_bs_boxed_ptr_new_from_ocdata (struct bsBoxed *bs_boxed, void *ocdata)
{
  return Data_Wrap_Struct(bs_boxed->klass, NULL, NULL, ocdata); 
}

static void *
rb_bs_boxed_struct_get_data(VALUE obj, struct bsBoxed *bs_boxed, size_t *size, BOOL *success, BOOL clean_ivars)
{
  void *  data;
  int     i;

  if (success != NULL) 
    *success = NO;
 
  if (NIL_P(obj))
    return NULL;

  // Given Ruby object is not a OSX::Boxed type, let's just pass it to the upstream initializer.
  // This is to keep backward compatibility.
  if (rb_obj_is_kind_of(obj, cOSXBoxed) != Qtrue) {
    if (TYPE(obj) != T_ARRAY) {
      // Calling #to_a is forbidden, as it would split a Range object.
      VALUE ary = rb_ary_new();
      rb_ary_push(ary, obj);
      obj = ary;
    }
    obj = rb_funcall2(bs_boxed->klass, rb_intern("new"), RARRAY(obj)->len, RARRAY(obj)->ptr);
  }

  if (rb_obj_is_kind_of(obj, cOSXBoxed) != Qtrue)
    return NULL;

  // Resync the ivars if necessary.
  // This is required as some fields may nest another structure, which
  // could have been modified as a copy in the Ruby world.
  for (i = 0; i < bs_boxed->opt.s.field_count; i++) {
    char buf[128];
    ID ivar_id;

    snprintf(buf, sizeof buf, "@%s", bs_boxed->opt.s.fields[i].name);
    ivar_id = rb_intern(buf);
    if (rb_ivar_defined(obj, ivar_id) == Qtrue) {
      VALUE val;

      val = rb_ivar_get(obj, ivar_id);
      snprintf(buf, sizeof buf, "%s=", bs_boxed->opt.s.fields[i].name);
      rb_funcall(obj, rb_intern(buf), 1, val);

      if (clean_ivars)
        rb_obj_remove_instance_variable(obj, ID2SYM(ivar_id));
    } 
  }
  Data_Get_Struct(obj, void, data);

  if (size != NULL)
    *size = bs_boxed_size(bs_boxed);
  if (success != NULL)
    *success = YES;

  return data;
}

static void *
rb_bs_boxed_opaque_get_data(VALUE obj, struct bsBoxed *bs_boxed, size_t *size, BOOL *success)
{
  void *data;

  if (NIL_P(obj) && bs_boxed_ffi_type(bs_boxed) == &ffi_type_pointer) {
    data = NULL;
  }
  else if (rb_obj_is_kind_of(obj, cOSXBoxed) == Qtrue) {
    Data_Get_Struct(obj, void, data);
  }
  else {
    *success = NO;
    return NULL;
  }

  *size = bs_boxed->size;
  *success = YES;

  return data;
}

void *
rb_bs_boxed_get_data(VALUE obj, const char *encoding, size_t *psize, BOOL *psuccess, BOOL clean_ivars)
{
  struct bsBoxed *bs_boxed;
  void *data;
  size_t size;
  BOOL success;

  size = 0;
  data = NULL;
  success = NO;  

  bs_boxed = find_bs_boxed_by_encoding(encoding);
  if (bs_boxed != NULL) {
    switch (bs_boxed->type) {
      case bsBoxedStructType:
        data = rb_bs_boxed_struct_get_data(obj, bs_boxed, &size, &success, clean_ivars);
        break;
      
      case bsBoxedOpaqueType:
        data = rb_bs_boxed_opaque_get_data(obj, bs_boxed, &size, &success);
        break;
  
      default:
        rb_bug("invalid bridge support boxed structure type %d", bs_boxed->type);
    }
  }

  if (psuccess != NULL)
    *psuccess = success;
  if (psize != NULL)
    *psize = size;

  return data;
}

static void *
rb_bs_struct_get_field_data(VALUE rcv, char **field_encoding_out)
{
  struct bsBoxed *bs_struct;
  const char *field;
  unsigned field_len;
  unsigned i;
  unsigned offset;
  void *struct_data;
  void *data;

  *field_encoding_out = "";
  bs_struct = rb_bs_struct_get_bs_struct(CLASS_OF(rcv));

  if (bs_struct->opt.s.field_count == 0)
    rb_raise(rb_eRuntimeError, "Bridge support structure %p doesn't have any field", bs_struct);

  field = rb_id2name(rb_frame_last_func());
  field_len = strlen(field);
  if (field[field_len - 1] == '=')
    field_len--;

  Data_Get_Struct(rcv, void, struct_data);
  if (struct_data == NULL)
    rb_raise(rb_eRuntimeError, "Given structure %p has null data", rcv);

  for (i = 0, data = NULL, offset = 0; 
       i < bs_struct->opt.s.field_count; 
       i++) {
     
    char *field_octype;

    field_octype = bs_struct->opt.s.fields[i].encoding;
    
    if (strncmp(bs_struct->opt.s.fields[i].name, field, field_len) == 0) {
      *field_encoding_out = field_octype;
      data = struct_data + offset;
      break;
    }

    offset += ocdata_size(field_octype);
  }

  if (data == NULL)
    rb_raise(rb_eRuntimeError, "Can't retrieve data for field '%s'", field);

  return data;
}

static ID
rb_bs_struct_field_ivar_id(void)
{
  char ivar_name[128];
  int len;

  len = snprintf(ivar_name, sizeof ivar_name, "@%s", rb_id2name(rb_frame_last_func()));
  if (ivar_name[len - 1] == '=')
    ivar_name[len - 1] = '\0'; 

  return rb_intern(ivar_name);
}

static VALUE
rb_bs_struct_get (VALUE rcv)
{
  ID ivar_id;  
  VALUE result;

  ivar_id = rb_bs_struct_field_ivar_id();
  if (rb_ivar_defined(rcv, ivar_id) == Qfalse) {
    void *data;
    char *octype;
    BOOL ok;

    data = rb_bs_struct_get_field_data(rcv, &octype);
    if (*octype == _C_ARY_B) {
      // Need to pass a pointer to pointer to the conversion routine, because
      // that's what it expects.
      void *p = &data;
      ok = ocdata_to_rbobj(Qnil, octype, &p, &result, NO);
    }
    else {
      ok = ocdata_to_rbobj(Qnil, octype, data, &result, NO);
    }
    if (!ok)
      rb_raise(rb_eRuntimeError, "Can't convert data %p of type %s to Ruby", 
        data, octype);

    rb_ivar_set(rcv, ivar_id, result);
  }
  else {
    result = rb_ivar_get(rcv, ivar_id);
  }

  return result; 
}

static VALUE
rb_bs_struct_set (VALUE rcv, VALUE val)
{
  void *data;
  char *octype;

  data = rb_bs_struct_get_field_data(rcv, &octype);
  if (!rbobj_to_ocdata(val, octype, data, NO))
    rb_raise(rb_eRuntimeError, "Can't convert Ruby object %p of type %s to Objective-C", val, octype);

  rb_ivar_set(rcv, rb_bs_struct_field_ivar_id(), val);

  return val;
}

static VALUE
rb_bs_struct_to_a (VALUE rcv)
{
  struct bsBoxed *bs_struct;
  unsigned i;
  VALUE ary;

  bs_struct = rb_bs_struct_get_bs_struct(CLASS_OF(rcv));
  ary = rb_ary_new();

  for (i = 0; i < bs_struct->opt.s.field_count; i++) {
    VALUE obj;

    obj = rb_funcall(rcv, rb_intern(bs_struct->opt.s.fields[i].name), 0, NULL);
    rb_ary_push(ary, obj);
  }

  return ary;
}

static VALUE
rb_bs_struct_is_equal (VALUE rcv, VALUE other)
{
  struct bsBoxed *bs_struct;
  unsigned i;

  if (rcv == other)
    return Qtrue;

  if (rb_obj_is_kind_of(other, CLASS_OF(rcv)) == Qfalse)
    return Qfalse;

  bs_struct = rb_bs_struct_get_bs_struct(CLASS_OF(rcv));

  for (i = 0; i < bs_struct->opt.s.field_count; i++) {
    VALUE lval, rval;
    ID msg;

    msg = rb_intern(bs_struct->opt.s.fields[i].name);
    lval = rb_funcall(rcv, msg, 0, NULL);
    rval = rb_funcall(other, msg, 0, NULL);
    
    if (rb_equal(lval, rval) == Qfalse)
      return Qfalse;
  }

  return Qtrue;
}

static VALUE
rb_bs_struct_dup (VALUE rcv)
{
  struct bsBoxed * bs_struct;
  void  *data;

  bs_struct = rb_bs_struct_get_bs_struct(CLASS_OF(rcv));
  data = rb_bs_boxed_struct_get_data(rcv, bs_struct, NULL, NULL, NO);

  return rb_bs_boxed_new_from_ocdata(bs_struct, data);
}

static VALUE
rb_define_bs_boxed_class (VALUE mOSX, const char *name, const char *encoding)
{
  VALUE klass;

  // FIXME make sure we don't define the same class twice!
  klass = rb_define_class_under(mOSX, name, cOSXBoxed);
  rb_ivar_set(klass, ivarEncodingID, rb_str_new2(encoding)); 
  
  return klass;
}

static struct bsBoxed *
init_bs_boxed (bsBoxedType type, const char *name, const char *encoding, VALUE klass)
{
  struct bsBoxed *bs_boxed;

  bs_boxed = (struct bsBoxed *)malloc(sizeof(struct bsBoxed)); 
  ASSERT_ALLOC(bs_boxed);

  bs_boxed->type = type; 
  bs_boxed->name = (char *)name;
  bs_boxed->size = 0; // lazy determined
  bs_boxed->encoding = strdup(encoding);
  bs_boxed->klass = klass;
  bs_boxed->ffi_type = NULL; // lazy determined

  return bs_boxed;
}

static struct bsBoxed *
init_bs_boxed_struct (VALUE mOSX, const char *name, const char *decorated_encoding, BOOL is_opaque)
{
  char encoding[MAX_ENCODE_LEN];
  struct bsStructField fields[128];
  int field_count = 0;
  VALUE klass;
  unsigned i;
  struct bsBoxed *bs_boxed;

  // Undecorate the encoding and its fields.
  if (!undecorate_encoding(decorated_encoding, encoding, MAX_ENCODE_LEN, fields, 128, &field_count)) {
    DLOG("MDLOSX", "Can't handle structure '%s' with encoding '%s'", name, decorated_encoding);
    return NULL;
  }

  // Define proxy class.
  klass = rb_define_bs_boxed_class(mOSX, name, encoding);
  if (NIL_P(klass))
    return NULL;
  if (!is_opaque) {
    for (i = 0; i < field_count; i++) {
      char setter[128];

      snprintf(setter, sizeof setter, "%s=", fields[i].name);
      rb_define_method(klass, fields[i].name, rb_bs_struct_get, 0);
      rb_define_method(klass, setter, rb_bs_struct_set, 1);
    }
    rb_define_method(klass, "to_a", rb_bs_struct_to_a, 0);
  }
  rb_define_singleton_method(klass, "new", rb_bs_struct_new, -1);
  rb_define_method(klass, "==", rb_bs_struct_is_equal, 1);
  rb_define_method(klass, "dup", rb_bs_struct_dup, 0);
  rb_define_method(klass, "clone", rb_bs_struct_dup, 0);

  // Allocate and return bs_boxed entry.
  bs_boxed = init_bs_boxed(bsBoxedStructType, name, encoding, klass);
  bs_boxed->opt.s.fields = (struct bsStructField *)malloc(sizeof(struct bsStructField) * field_count);
  ASSERT_ALLOC(bs_boxed->opt.s.fields);
  memcpy(bs_boxed->opt.s.fields, fields, sizeof(struct bsStructField) * field_count); 
  bs_boxed->opt.s.field_count = field_count;
  bs_boxed->opt.s.opaque = is_opaque;

  return bs_boxed;
}

static struct bsBoxed *
init_bs_boxed_opaque (VALUE mOSX, const char *name, const char *encoding)
{
  VALUE klass;
  struct bsBoxed *bs_boxed;
  
  klass = rb_define_bs_boxed_class(mOSX, name, encoding);
  if (NIL_P(klass))
    return NULL;

  bs_boxed = init_bs_boxed(bsBoxedOpaqueType, name, encoding, klass);
  if (bs_boxed != NULL)
    bs_boxed->size = sizeof(void *);

  return bs_boxed;
}

static Class
bs_cf_type_create_proxy(const char *name)
{
  Class klass, superclass;

  superclass = objc_getClass("NSCFType");
  if (superclass == NULL)
    rb_bug("can't locate ObjC class NSCFType");
#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
  klass = objc_class_alloc(name, superclass);
  objc_addClass(klass);
#else
  klass = objc_allocateClassPair(superclass, name, 0);
  objc_registerClassPair(klass); 
#endif
  return klass;
}

static void
func_dispatch_retain_if_necessary(VALUE arg, BOOL is_retval, void *ctx)
{
  struct bsFunction *func = (struct bsFunction *)ctx;

  // retain the new ObjC object, that will be released once the Ruby object is collected
  if (!NIL_P (arg) 
      && (*encoding_skip_to_first_type(func->retval->octypestr) == _C_ID 
          || find_bs_cf_type_by_encoding(func->retval->octypestr) != NULL)) {
    if (func->retval->should_be_retained && !OBJCID_DATA_PTR(arg)->retained) {
      DLOG("MDLOSX", "retaining objc value");
      [OBJCID_ID(arg) retain];
    }
    OBJCID_DATA_PTR(arg)->retained = YES;
    OBJCID_DATA_PTR(arg)->can_be_released = YES;
  }
}

static VALUE
bridge_support_dispatcher (int argc, VALUE *argv, VALUE rcv)
{
  const char *func_name;
  struct bsFunction *func;
  int expected_argc;
  ffi_type **arg_types;
  void **arg_values;
  char **arg_octypesstr;
  VALUE exception;
  VALUE result;
  NSAutoreleasePool *pool;

  // lookup structure
  func_name = rb_id2name(rb_frame_last_func());
  DLOG("MDLOSX", "dispatching function '%s'", func_name);
  if (!st_lookup(bsFunctions, (st_data_t)func_name, (st_data_t *)&func))
    rb_fatal("Unrecognized function '%s'", func_name);
  if (func == NULL)
    rb_fatal("Retrieved func structure is invalid");

  // lookup function symbol
  if (func->sym == NULL) {
    func->sym = dlsym(RTLD_DEFAULT, func_name);
    if (func->sym == NULL) 
      rb_fatal("Can't locate function symbol '%s' : %s", func->name, dlerror());
  }

  // allocate arg types/values
  expected_argc = func->is_variadic && argc > func->argc ? argc : func->argc;
  arg_types = (ffi_type **) alloca((expected_argc + 1) * sizeof(ffi_type *));
  arg_values = (void **) alloca((expected_argc + 1) * sizeof(void *));
  if (arg_types == NULL || arg_values == NULL)
    rb_fatal("can't allocate memory");

  memset(arg_types, 0, (expected_argc + 1) * sizeof(ffi_type *));
  memset(arg_values, 0, (expected_argc + 1) * sizeof(void *));

  if (func->is_variadic && argc > func->argc) {
    unsigned i;
    VALUE format_str;

    DLOG("MDLOSX", "function is variadic, %d min argc, %d additional argc", func->argc, argc - func->argc);
    arg_octypesstr = (char **)alloca((expected_argc + 1) * sizeof(char *));
    format_str = Qnil;
    for (i = 0; i < func->argc; i++) {
      arg_octypesstr[i] = func->argv[i].octypestr;
      if (func->argv[i].printf_format)
        format_str = argv[i];
    }
    if (NIL_P(format_str)) {
      for (i = func->argc; i < argc; i++)
        arg_octypesstr[i] = "@"; // _C_ID;
    }
    else {
      set_octypes_for_format_str(&arg_octypesstr[func->argc], 
        argc - func->argc, StringValuePtr(format_str)); 
    }
  }
  else {
    arg_octypesstr = NULL;
  } 

  pool = [[NSAutoreleasePool alloc] init];

  current_function = func;

  // and dispatch!
  exception = rb_ffi_dispatch(
    (struct bsCallEntry *)func, 
    arg_octypesstr, 
    expected_argc,
    argc, 
    0, 
    argv, 
    arg_types, 
    arg_values, 
    func->retval->octypestr, 
    func->sym, 
    func_dispatch_retain_if_necessary, 
    (void *)func, 
    &result);

  current_function = NULL;

  [pool release];

  if (!NIL_P(exception))
    rb_exc_raise(exception);

  DLOG("MDLOSX", "dispatching function '%s' done", func_name);

  return result;
}

static struct bsRetval default_func_retval = { bsCArrayArgUndefined, -1, "v", NO };  

static VALUE 
osx_load_bridge_support_dylib (VALUE rcv, VALUE path)
{
  const char *cpath;

  cpath = StringValuePtr(path);
  if (dlopen(cpath, RTLD_LAZY) == NULL)
    rb_raise(rb_eArgError, "Can't load the bridge support dylib file `%s' : %s", cpath, dlerror());

  return Qnil;
}

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_4
// DO NOT SUPPORT formal protocols with bridgesupport on 10.4 or earlier.
// there is no objc runtime-api for gathering all protocols in objective-c 1.0.
#define reload_protocols()
#else

static void
reload_protocols(void) 
{
    Protocol **prots; 
    unsigned int i, prots_count; 
 
    prots = objc_copyProtocolList(&prots_count); 
    for (i = 0; i < prots_count; i++) { 
        Protocol *p; 
        struct objc_method_description *methods; 
        unsigned j, methods_count; 
 
        p = prots[i]; 
 
#define REGISTER_MDESCS(cmethods) \
    do { \
	struct st_table *t = cmethods ? bsInformalProtocolClassMethods : bsInformalProtocolInstanceMethods; \
        for (j = 0; j < methods_count; j++) { \
            struct bsInformalProtocolMethod *informal_method; \
            informal_method = (struct bsInformalProtocolMethod *)malloc(sizeof(struct bsInformalProtocolMethod)); \
            ASSERT_ALLOC(informal_method); \
            informal_method->selector = (char *)methods[j].name; \
            informal_method->is_class_method = cmethods; \
            informal_method->encoding = strdup(methods[j].types); \
            informal_method->protocol_name = strdup(protocol_getName(p)); \
            st_insert(t, (st_data_t)methods[j].name, (st_data_t)informal_method); \
        } \
	if (methods != NULL) { \
	    free(methods); \
	} \
    } \
    while (0)
 
        methods = protocol_copyMethodDescriptionList(p, true, true, &methods_count); 
        REGISTER_MDESCS(false); 
        methods = protocol_copyMethodDescriptionList(p, false, true, &methods_count); 
        REGISTER_MDESCS(false);
        methods = protocol_copyMethodDescriptionList(p, true, false, &methods_count); 
        REGISTER_MDESCS(true);
        methods = protocol_copyMethodDescriptionList(p, false, false, &methods_count); 
        REGISTER_MDESCS(true);
 
#undef REGISTER_MDESCS 
    }
    if (prots != NULL) { 
	free(prots); 
    }
} 

#endif

static int
compare_bs_arg(const void *a, const void *b)
{
    struct bsArg *arg_a = (struct bsArg *)a;
    struct bsArg *arg_b = (struct bsArg *)b;
    return arg_a->index == arg_b->index ? 0 : (arg_a->index > arg_b->index ? 1 : -1);
}

static VALUE
osx_load_bridge_support_file (VALUE mOSX, VALUE path)
{
  const char *        cpath;
  xmlTextReaderPtr    reader;
  struct bsFunction * func;
  struct bsClass *    klass;
  struct bsMethod *   method;
  unsigned int        i;
  struct bsArg        args[MAX_ARGS];
  char *              protocol_name;
  BOOL                within_func_ptr_arg;
  struct {
    char *    retval;
    char *    argv[MAX_ARGS];
    unsigned  argc;
  } func_ptr;

  cpath = StringValuePtr(path);

#define RESET_FUNC_PTR_CTX()      \
  do {                            \
    func_ptr.retval = NULL;       \
    func_ptr.argc = 0;            \
    within_func_ptr_arg = NO;     \
  }                               \
  while (0)

  RESET_FUNC_PTR_CTX();

  DLOG("MDLOSX", "Loading bridge support file `%s'", cpath);
  
  reader = xmlNewTextReaderFilename(cpath);
  if (reader == NULL)
    rb_raise(rb_eRuntimeError, "cannot create XML text reader for file at path `%s'", cpath);

  func = NULL;
  klass = NULL;
  method = NULL;
  protocol_name = NULL;

  while (YES) {
    const char *name;
    unsigned int namelen;
    int node_type = -1;
    BOOL eof;
    struct bs_xml_atom *atom;

    do {
      if ((eof = !next_node(reader)))
        break;
      
      node_type = xmlTextReaderNodeType(reader);
    }
    while (node_type != XML_READER_TYPE_ELEMENT && node_type != XML_READER_TYPE_END_ELEMENT);    
    
    if (eof)
      break;

    name = (const char *)xmlTextReaderConstName(reader);
    namelen = strlen(name); 

    if (node_type == XML_READER_TYPE_ELEMENT) {
      atom = bs_xml_element(name, namelen);
      if (atom == NULL)
        continue;
      switch (atom->val) {
      case BS_XML_CONSTANT: { 
        char *            const_name;
        struct bsConst *  bs_const;

        const_name = get_attribute_and_check(reader, "name");

        if (st_lookup(bsConstants, (st_data_t)const_name, NULL)) {
          DLOG("MDLOSX", "Constant '%s' already registered, skipping...", const_name);
          free(const_name);
        }
        else {
          char *  const_type;
          char *  const_magic_cookie;
          
          const_type = get_type_attribute_and_check(reader);
          bs_const = (struct bsConst *)malloc(sizeof(struct bsConst));
          ASSERT_ALLOC(bs_const);

          bs_const->name = const_name;
          bs_const->encoding = const_type;
          bs_const->class_name = NULL;
          bs_const->ignored = NO;
          bs_const->suggestion = NULL;

          const_magic_cookie = get_attribute(reader, "magic_cookie");
          if (const_magic_cookie != NULL) {
            bs_const->is_magic_cookie = strcmp(const_magic_cookie, "true") == 0;
            free(const_magic_cookie);
          }
          else {
            bs_const->is_magic_cookie = NO;
          }

          st_insert(bsConstants, (st_data_t)const_name, (st_data_t)bs_const);
        }
      }
      break;

      case BS_XML_STRING_CONSTANT: {
        char *  strconst_name;

        strconst_name = get_attribute_and_check(reader, "name");
        if (rb_const_defined(mOSX, rb_intern(strconst_name))) {
          DLOG("MDLOSX", "String constant '%s' already registered, skipping...", strconst_name);
          free(strconst_name);
        }
        else { 
          char *  strconst_value;
          char *  strconst_nsstring;
          BOOL    strconst_is_nsstring;
          VALUE   value;

          strconst_value = get_attribute_and_check(reader, "value");
          strconst_nsstring = get_attribute(reader, "nsstring");
          if (strconst_nsstring != NULL) {
            strconst_is_nsstring = strcmp(strconst_nsstring, "true") == 0;
            free(strconst_nsstring);
          }
          else {
            strconst_is_nsstring = NO;
          }

          value = Qnil;
          if (strconst_is_nsstring) {
            NSString *nsvalue;
            
            nsvalue = [[NSString alloc] initWithUTF8String:strconst_value];
            value = ocid_to_rbobj(Qnil, nsvalue);
          }
          else {
            value = rb_str_new2(strconst_value);
          }

          CAPITALIZE(strconst_name);

          if (!NIL_P(value))
            rb_define_const(mOSX, strconst_name, value);

          free(strconst_name);
          free(strconst_value);   
        }
      }
      break;

      case BS_XML_ENUM: { 
        char *  enum_name;
        BOOL    ignore;

        ignore = NO;
        enum_name = get_attribute_and_check(reader, "name");
        if (rb_const_defined(mOSX, rb_intern(enum_name)) || strcmp(enum_name, "Nil") == 0) {
          DLOG("MDLOSX", "Enum '%s' already registered, skipping...", enum_name);
        }
        else {
          char *  ignored;

          ignored = get_attribute(reader, "ignore");
          if (ignored != NULL) {
            ignore = strcmp(ignored, "true") == 0;
            free(ignored);
          }

          if (ignore) {
            struct bsConst *  fake_bs_const;

            fake_bs_const = (struct bsConst *)malloc(sizeof(struct bsConst));
            ASSERT_ALLOC(fake_bs_const);

            fake_bs_const->name = enum_name;
            fake_bs_const->encoding = NULL;
            fake_bs_const->is_magic_cookie = NO;
            fake_bs_const->ignored = YES;
            fake_bs_const->suggestion = get_attribute(reader, "suggestion");

            st_insert(bsConstants, (st_data_t)enum_name, (st_data_t)fake_bs_const); 
          }
          else {
            char *  enum_value = NULL;
            VALUE   value;

#if __LP64__
            enum_value = get_attribute(reader, "value64");
#endif
            if (enum_value == NULL) {
              enum_value = get_attribute(reader, "value");
	    }
#if BYTE_ORDER == BIG_ENDIAN
            if (enum_value == NULL)
              enum_value = get_attribute(reader, "be_value");
#else
            if (enum_value == NULL)
              enum_value = get_attribute(reader, "le_value");
#endif
            if (enum_value != NULL) {
              /* Because rb_cstr_to_dbl() might warn in case the given float
               * is out of range. */
              VALUE old_ruby_verbose = ruby_verbose;    
              ruby_verbose = Qnil;

              value = strchr(enum_value, '.') != NULL
                ? rb_float_new(rb_cstr_to_dbl(enum_value, 0))
                : rb_cstr_to_inum(enum_value, 10, 0); 

              ruby_verbose = old_ruby_verbose;            

              CAPITALIZE(enum_name);
              ID enum_id = rb_intern(enum_name);
              if (!rb_const_defined(mOSX, enum_id)) {
                rb_const_set(mOSX, enum_id, value);
              }

              free (enum_value);
            }
            else {
              DLOG("MDLOSX", "Enum '%s' doesn't have a compatible value attribute, skipping...", enum_name);
            }
          }
        }
        if (!ignore)
          free (enum_name);
      }
      break;

      case BS_XML_STRUCT: {
        char *           struct_decorated_encoding;
        char *           struct_name;
        char *           is_opaque_s;
        BOOL             is_opaque;
        struct bsBoxed * bs_boxed;

        struct_decorated_encoding = get_type_attribute_and_check(reader);
        struct_name = get_attribute_and_check(reader, "name");
        is_opaque_s = get_attribute(reader, "opaque");
        if (is_opaque_s != NULL) {
          is_opaque = strcmp(is_opaque_s, "true") == 0;
          free(is_opaque_s);
        }
        else {
          is_opaque = NO;
        }

        bs_boxed = init_bs_boxed_struct(mOSX, struct_name, struct_decorated_encoding, is_opaque);
        if (bs_boxed == NULL) {
          DLOG("MDLOSX", "Can't init structure '%s' -- skipping...", struct_decorated_encoding);
          free(struct_name);
        }
        else {
          if (st_lookup(bsBoxed, (st_data_t)bs_boxed->encoding, NULL)) {
            DLOG("MDLOSX", "Another C structure already registered under the encoding '%s', skipping...", bs_boxed->encoding); 
          }
          else {
            st_insert(bsBoxed, (st_data_t)bs_boxed->encoding, (st_data_t)bs_boxed);
            DLOG("MDLOSX", "Imported boxed type of name `%s' encoding `%s'", struct_name, bs_boxed->encoding);
          }
        }

        free(struct_decorated_encoding);
      }
      break;

      case BS_XML_OPAQUE: {
        char *  opaque_encoding;

        opaque_encoding = get_type_attribute_and_check(reader);
        if (st_lookup(bsBoxed, (st_data_t)opaque_encoding, NULL)) {
          DLOG("MDLOSX", "Opaque type with encoding '%s' already defined -- skipping...", opaque_encoding);
          free(opaque_encoding);
        }
        else {
          char *            opaque_name;
          struct bsBoxed *  bs_boxed;
  
          opaque_name = get_attribute_and_check(reader, "name");

          bs_boxed = init_bs_boxed_opaque(mOSX, opaque_name, opaque_encoding);
          if (bs_boxed == NULL) {
            DLOG("MDLOSX", "Can't init opaque '%s' -- skipping...", opaque_encoding);
          }
          else {
            st_insert(bsBoxed, (st_data_t)bs_boxed->encoding, (st_data_t)bs_boxed);
          }
          free(opaque_encoding);
        }      
      }
      break;

      case BS_XML_CFTYPE: {
        char *typeid_encoding;

        typeid_encoding = get_type_attribute_and_check(reader);
        if (st_lookup(bsCFTypes, (st_data_t)typeid_encoding, NULL)) {
          DLOG("MDLOSX", "CFType with encoding '%s' already defined -- skipping...", typeid_encoding);
          free(typeid_encoding);
        }
        else {
          struct bsCFType *bs_cf_type;
          char *gettypeid_func;
          char *toll_free;

          bs_cf_type = (struct bsCFType *)malloc(sizeof(struct bsCFType));
          ASSERT_ALLOC(bs_cf_type);

          bs_cf_type->name = get_attribute_and_check(reader, "name");
          bs_cf_type->encoding = typeid_encoding;
          
          gettypeid_func = get_attribute(reader, "gettypeid_func");
          if (gettypeid_func != NULL) {
            void *sym;

            sym = dlsym(RTLD_DEFAULT, gettypeid_func);
            if (sym == NULL) {
              DLOG("MDLOSX", "Cannot locate GetTypeID function '%s' for given CFType '%s' -- ignoring it...", gettypeid_func, bs_cf_type->name);
              bs_cf_type->type_id = 0; /* not a type */
            }
            else {
              int (*cb)(void) = sym;
              bs_cf_type->type_id = (*cb)();
            }

            free(gettypeid_func);
          }
          else {
            bs_cf_type->type_id = 0; /* not a type */
          }

          bs_cf_type->bridged_class_name = NULL; 
          toll_free = get_attribute(reader, "tollfree");
          if (toll_free != NULL) {
            if (objc_getClass(toll_free) != nil) {
              bs_cf_type->bridged_class_name = toll_free;
            }
            else {
              DLOG("MDLOSX", "Given CFType toll-free class '%s' doesn't exist -- creating a proxy...", toll_free);
              free(toll_free);
            }
          }
          if (bs_cf_type->bridged_class_name == NULL) {
            bs_cf_type_create_proxy(bs_cf_type->name);
            bs_cf_type->bridged_class_name = bs_cf_type->name;
          }
 
          st_insert(bsCFTypes, (st_data_t)typeid_encoding, (st_data_t)bs_cf_type);
          if (bs_cf_type->type_id > 0) 
            st_insert(bsCFTypes2, (st_data_t)bs_cf_type->type_id, (st_data_t)bs_cf_type);
        }
      }
      break;

      case BS_XML_INFORMAL_PROTOCOL: {
        protocol_name = get_attribute_and_check(reader, "name");
      }
      break;

      case BS_XML_FUNCTION: {
        char *  func_name;
        
        func_name = get_attribute_and_check(reader, "name");
        if (st_lookup(bsFunctions, (st_data_t)func_name, (st_data_t *)&func)) {
          st_delete(bsFunctions, (st_data_t *)&func->name, (st_data_t *)&func);
          DLOG("MDLOSX", "Re-defining function '%s'", func_name);
          free_bs_function(func);
        }

        func = (struct bsFunction *)calloc(1, sizeof(struct bsFunction));
        ASSERT_ALLOC(func);

        st_insert(bsFunctions, (st_data_t)func_name, (st_data_t)func);
        rb_undef_method(CLASS_OF(mOSX), func_name);
        rb_define_module_function(mOSX, func_name, bridge_support_dispatcher, -1);

        func->name = func_name;
        func->is_variadic = get_boolean_attribute(reader, "variadic", NO);
        func->argc = 0;
        func->argv = NULL;
        func->retval = &default_func_retval;
      }
      break;

      case BS_XML_FUNCTION_ALIAS: {
        char *  alias_name;
        char *  alias_original;

        alias_name = get_attribute_and_check(reader, "name"); 
        alias_original = get_attribute_and_check(reader, "original");

        rb_undef_method(CLASS_OF(mOSX), alias_name);
        rb_define_alias(CLASS_OF(mOSX), alias_name, alias_original);

        free(alias_name);
        free(alias_original);
      }
      break;

      case BS_XML_CLASS: {
        char *  class_name;
        
        class_name = get_attribute_and_check(reader, "name");
      
        if (st_lookup(bsClasses, (st_data_t)class_name, (st_data_t *)&klass)) {
          free (class_name);
        }
        else {
          klass = (struct bsClass *)malloc(sizeof(struct bsClass));
          ASSERT_ALLOC(klass);
          
          klass->name = class_name;
          klass->class_methods = st_init_strtable();
          klass->instance_methods = st_init_strtable();
          
          st_insert(bsClasses, (st_data_t)class_name, (st_data_t)klass);
        }
      }
      break;

      case BS_XML_ARG: {
        if (within_func_ptr_arg) {
          if (func_ptr.argc > MAX_ARGS) {
              DLOG("MDLOSX", "Maximum number of arguments reached for function pointer (%d), skipping...", MAX_ARGS);
          }
          else {
            func_ptr.argv[func_ptr.argc++] = get_type_attribute_and_check(reader);
          }
        }
        else if (func != NULL || method != NULL) {
          int * argc;

          argc = func != NULL ? &func->argc : &method->argc;

          if (*argc >= MAX_ARGS) {
            if (func != NULL)
              DLOG("MDLOSX", "Maximum number of arguments reached for function '%s' (%d), skipping...", func->name, MAX_ARGS);
            else
              DLOG("MDLOSX", "Maximum number of arguments reached for method '%s' (%d), skipping...", method->selector, MAX_ARGS);
          } 
          else {
            char *  type_modifier;
            struct bsArg * arg; 
            char *  func_ptr;
 
            arg = &args[(*argc)++];

            if (method != NULL) {
              char * index = get_attribute_and_check(reader, "index");
              arg->index = atoi(index);
              free(index);
            }
            else {
              arg->index = -1;
            }
    
            type_modifier = get_attribute(reader, "type_modifier");
            if (type_modifier != NULL) {
              switch (*type_modifier) {
                case 'n':
                  arg->type_modifier = bsTypeModifierIn;
                  break;
                case 'o':
                  arg->type_modifier = bsTypeModifierOut;
                  break;
                case 'N':
                  arg->type_modifier = bsTypeModifierInout;
                  break;
                default:
                  DLOG("MDLOSX", "Given type modifier '%s' is invalid, default'ing to 'out'", type_modifier);
                  arg->type_modifier = bsTypeModifierOut;
              }
              free(type_modifier);
            }
            else {
              arg->type_modifier = bsTypeModifierOut;
            } 
#if __LP64__
            arg->sel_of_type = get_attribute(reader, "sel_of_type64");
            if (arg->sel_of_type == NULL)
#endif
              arg->sel_of_type = get_attribute(reader, "sel_of_type");

            arg->printf_format = get_boolean_attribute(reader, "printf_format", NO); 
            arg->null_accepted = get_boolean_attribute(reader, "null_accepted", YES);
            get_c_ary_type_attribute(reader, &arg->c_ary_type, &arg->c_ary_type_value); 
  
            arg->octypestr = get_type_attribute(reader);

            func_ptr = get_attribute(reader, "function_pointer");
            if (func_ptr != NULL) {
              within_func_ptr_arg = strcmp(func_ptr, "true") == 0;
              free(func_ptr);
            }
            else {
              within_func_ptr_arg = NO;
            }
          }
        }
        else {
          DLOG("MDLOSX", "Argument defined outside of a function/method/function_pointer, skipping...");
        }
      }
      break;

      case BS_XML_RETVAL: {
        if (within_func_ptr_arg) {
          if (func_ptr.retval != NULL) {
            DLOG("MDLOSX", "Function pointer return value defined more than once, skipping...");
          } 
          else {
            func_ptr.retval = get_type_attribute(reader);
          }
        }
        else if (func != NULL || method != NULL) {
          if (func != NULL && func->retval != NULL && func->retval != &default_func_retval) {
            DLOG("MDLOSX", "Function '%s' return value defined more than once, skipping...", func->name);
          }
          else if (method != NULL && method->retval != NULL) {
            DLOG("MDLOSX", "Method '%s' return value defined more than once, skipping...", method->selector);
          }
          else {
            bsCArrayArgType type;
            int value;
            struct bsRetval *retval;  
            char *func_ptr;

            get_c_ary_type_attribute(reader, &type, &value);
  
            retval = (struct bsRetval *)malloc(sizeof(struct bsRetval));
            ASSERT_ALLOC(retval);
            
            retval->c_ary_type = type;
            retval->c_ary_type_value = value;
            retval->octypestr = get_type_attribute(reader);

            if (func != NULL) {
              if (retval->octypestr != NULL) {
                retval->should_be_retained = 
                  *encoding_skip_to_first_type(retval->octypestr) == _C_ID
                  || find_bs_cf_type_by_encoding(retval->octypestr) != NULL
                    ? !get_boolean_attribute(reader, "already_retained", NO) 
                    : YES;
                func->retval = retval;
              }
              else {
                DLOG("MDLOSX", "Function '%s' return value defined without type, using default return type...", func->name);
                free(retval);
              }
            }
            else {
              method->retval = retval;
            }
            
            func_ptr = get_attribute(reader, "function_pointer");
            if (func_ptr != NULL) {
              within_func_ptr_arg = strcmp(func_ptr, "true") == 0;
              free(func_ptr);
            }
            else {
              within_func_ptr_arg = NO;
            }
          }
        }
        else {
          DLOG("MDLOSX", "Return value defined outside a function/method, skipping...");
        }
      }
      break;

      case BS_XML_METHOD: {
        if (protocol_name != NULL) {
          char * selector;
          BOOL   is_class_method;
          struct st_table *hash;

          selector = get_attribute_and_check(reader, "selector");
          is_class_method = get_boolean_attribute(reader, "class_method", NO);
          hash = is_class_method ? bsInformalProtocolClassMethods : bsInformalProtocolInstanceMethods;         
          if (st_lookup(hash, (st_data_t)selector, NULL)) {
            DLOG("MDLOSX", "Informal protocol method [NSObject %c%s] already defined, skipping...", is_class_method ? '+' : '-', selector);
            free(selector);
          }
          else {
            struct bsInformalProtocolMethod *informal_method;

            informal_method = (struct bsInformalProtocolMethod *)malloc(sizeof(struct bsInformalProtocolMethod));
            ASSERT_ALLOC(informal_method);

            informal_method->selector = selector;
            informal_method->is_class_method = is_class_method;
            informal_method->encoding = get_type_attribute_and_check(reader);
            informal_method->protocol_name = protocol_name;

            st_insert(hash, (st_data_t)selector, (st_data_t)informal_method);            
          }
        }
        else if (klass == NULL) {
          DLOG("MDLOSX", "Method defined outside a class or informal protocol, skipping...");
        }
        else {
          char * selector;
          BOOL is_class_method;
          struct st_table * methods_hash;

          selector = get_attribute_and_check(reader, "selector");
          is_class_method = get_boolean_attribute(reader, "class_method", NO);

          methods_hash = is_class_method ? klass->class_methods : klass->instance_methods;
          if (st_lookup(methods_hash, (st_data_t)selector, (st_data_t *)&method)) {
            st_delete(methods_hash, (st_data_t *)&method->selector, (st_data_t *)&method);
            DLOG("MDLOSX", "Re-defining method '%s' in class '%s'", selector, klass->name);
            free_bs_method(method);
          }

          method = (struct bsMethod *)malloc(sizeof(struct bsMethod));
          ASSERT_ALLOC(method);

          method->selector = selector;
          method->is_class_method = is_class_method;
          method->is_variadic = get_boolean_attribute(reader, "variadic", NO);
          method->ignore = get_boolean_attribute(reader, "ignore", NO);
          method->suggestion = method->ignore ? get_attribute(reader, "suggestion") : NULL;
          method->argc = 0;
          method->argv = NULL;
          method->retval = NULL;
          
          st_insert(methods_hash, (st_data_t)selector, (st_data_t)method);
        }
      }
      break;

      default: break; // Do nothing.
      } // End of switch. 
    }
    else if (node_type == XML_READER_TYPE_END_ELEMENT) {
      atom = bs_xml_element(name, namelen);
      if (atom == NULL)
        continue;
      switch (atom->val) {
      case BS_XML_INFORMAL_PROTOCOL: {
        protocol_name = NULL;
      }
      break;

      case BS_XML_RETVAL:
      case BS_XML_ARG: {
        if (within_func_ptr_arg) {
          size_t len;
          struct bsCallEntry *call_entry;
          char new_type[1028];

          new_type[0] = '^';
          new_type[1] = '?';
          new_type[2] = '\0';
          len = sizeof(new_type) - 2;
          strncat(new_type, func_ptr.retval, len);
          len -= strlen(func_ptr.retval);
          free(func_ptr.retval);
          for (i = 0; i < func_ptr.argc; i++) {
            strncat(new_type, func_ptr.argv[i], len);
            len -= strlen(func_ptr.argv[i]);
            free(func_ptr.argv[i]);
          }

          call_entry = func != NULL 
            ? (struct bsCallEntry *)func : (struct bsCallEntry *)method;

          if (atom->val == BS_XML_RETVAL) {
            struct bsRetval *retval;
            retval = call_entry->retval;
            if (retval == &default_func_retval) {
              struct bsRetval *new_retval =
                (struct bsRetval *)malloc(sizeof(struct bsRetval));
              ASSERT_ALLOC(new_retval);
              memcpy(new_retval, retval, sizeof(struct bsRetval));
              retval = new_retval;
            }
            else {
              free(retval->octypestr);
            }
            retval->octypestr = (char *)strdup(new_type);
          }
          else {
            struct bsArg *arg;
            arg = &args[call_entry->argc - 1];
            free(arg->octypestr);
            arg->octypestr = (char *)strdup(new_type);
          }

          RESET_FUNC_PTR_CTX();
        }
      }
      break;

      case BS_XML_FUNCTION: { 
        BOOL all_args_ok;
  
        all_args_ok = YES;
  
        for (i = 0; i < func->argc; i++) {
          if (args[i].octypestr == NULL) {
            DLOG("MDLOSX", "Function '%s' argument #%d type has not been provided, skipping...", func->name, i);
            all_args_ok = NO;
            break;
          }
        }
  
        if (all_args_ok) {
          if (func->argc > 0) {
            size_t len;
    
            len = sizeof(struct bsArg) * func->argc;
            func->argv = (struct bsArg *)malloc(len);
            ASSERT_ALLOC(func->argv);
            memcpy(func->argv, args, len);
          }
          if (func->retval == NULL)
            func->retval = &default_func_retval;
        } 
        else {
          rb_undef_method(mOSX, func->name);
          st_delete(bsFunctions, (st_data_t *)&func->name, NULL);
          free_bs_function(func);
        }

        func = NULL;
      }
      break;

      case BS_XML_METHOD: {
        if (method->argc > 0) {
          size_t len;
    
          len = sizeof(struct bsArg) * method->argc;
          method->argv = (struct bsArg *)malloc(len);
          ASSERT_ALLOC(method->argv);
          memcpy(method->argv, args, len);
          qsort(method->argv, method->argc, sizeof(struct bsArg), compare_bs_arg);
        }

        method = NULL;
      }
      break;

      case BS_XML_CLASS: {
        klass = NULL;
      }
      break;

      default: break; // Do nothing.
      } // End of switch.
    }
  }

  xmlFreeTextReader(reader);

  reload_protocols(); // TODO this should probably be done somewhere else

  return mOSX;
}

#else /* !HAS_LIBXML2 */

static VALUE
osx_load_bridge_support_file (VALUE rcv, VALUE path)
{
  rb_warn("libxml2 is not available, bridge support file `%s' cannot be read", StringValuePtr(path));
  return rcv;
}

#endif

struct bsConst *
find_magic_cookie_const_by_value(void *value)
{
  struct bsConst *bs_const;

  if (!st_lookup(bsMagicCookieConstants, (st_data_t)value, (st_data_t *)&bs_const))
    return NULL;

  return bs_const;
}

static VALUE
osx_import_c_constant (VALUE self, VALUE sym)
{
  const char *      name;
  char *            real_name;
  struct bsConst *  bs_const;
  void *            cvalue;
  VALUE             value;
  
  name = rb_id2name(SYM2ID(sym));
  real_name = (char *)name;
  if (!st_lookup(bsConstants, (st_data_t)name, (st_data_t *)&bs_const)) {
    // Decapitalize the string and try again.
    real_name = strdup(name);
    DECAPITALIZE(real_name);
    if (!st_lookup(bsConstants, (st_data_t)real_name, (st_data_t *)&bs_const)) {
      free(real_name);
      rb_raise(rb_eLoadError, "C constant '%s' not found", name);
    }
  }

  if (bs_const->ignored)
    rb_raise(rb_eRuntimeError, "Constant '%s' is not supported (suggested alternative: '%s')", bs_const->name, bs_const->suggestion != NULL ? bs_const->suggestion : "n/a");

  cvalue = dlsym(RTLD_DEFAULT, real_name);
  value = Qnil;
  if (cvalue != NULL) {
    DLOG("MDLOSX", "Importing C constant `%s' of type '%s'", name, bs_const->encoding);
    if (bs_const->is_magic_cookie) { 
      struct bsCFType *bs_cftype;

      bs_cftype = find_bs_cf_type_by_encoding(bs_const->encoding);
      bs_const->class_name = bs_cftype != NULL
        ? bs_cftype->bridged_class_name : "OCObject";

      DLOG("MDLOSX", "Constant is a magic-cookie of fixed value %p, guessed class name '%s'", *(void **)cvalue, bs_const->class_name);

      st_insert(bsMagicCookieConstants, (st_data_t)*(void **)cvalue, (st_data_t)bs_const);
    }
    if (!ocdata_to_rbobj(Qnil, bs_const->encoding, cvalue, &value, NO))
      rb_raise(ocdataconv_err_class(), "Cannot convert the Objective-C constant '%s' as '%s' to Ruby", name, bs_const->encoding);
    rb_define_const(self, name, value);
    DLOG("MDLOSX", "Imported C constant `%s' with value %p", name, value);
  }

  if (name != real_name)
    free(real_name);
  
  if (cvalue == NULL)
    rb_bug("Can't locate constant symbol '%s' : %s", name, dlerror());
  
  return value;
}

struct bsBoxed *
find_bs_boxed_by_encoding (const char *encoding)
{
  struct bsBoxed *bs_boxed;

  if (!st_lookup(bsBoxed, (st_data_t)encoding, (st_data_t *)&bs_boxed))
    return NULL;

  return bs_boxed;
}

struct bsCFType *
find_bs_cf_type_by_encoding(const char *encoding)
{
  struct bsCFType *cf_type;

  if (!st_lookup(bsCFTypes, (st_data_t)encoding, (st_data_t *)&cf_type))
    return NULL;

  return cf_type;
}

struct bsCFType *
find_bs_cf_type_by_type_id(CFTypeID typeid)
{
  struct bsCFType *cf_type;

  if (!st_lookup(bsCFTypes2, (st_data_t)typeid, (st_data_t *)&cf_type))
    return NULL;

  return cf_type;
}

static struct bsMethod *
__find_bs_method(const char *class_name, const char *selector, BOOL is_class_method)
{
  struct bsClass *bs_class;
  struct bsMethod *method;

  if (!st_lookup(bsClasses, (st_data_t)class_name, (st_data_t *)&bs_class))
    return NULL;

  if (!st_lookup(is_class_method ? bs_class->class_methods : bs_class->instance_methods, (st_data_t)selector, (st_data_t *)&method))
    return NULL;

  return method;
}

struct bsMethod *
find_bs_method(id klass, const char *selector, BOOL is_class_method)
{
  if (klass == nil || selector == NULL)
    return NULL;

  do {
    struct bsMethod *method;

    method = __find_bs_method(class_getName(klass), selector, is_class_method);
    if (method != NULL)
      return method;
   
    klass = class_getSuperclass(klass);
  }
  while (klass != NULL);

  return NULL;
}

struct bsArg *
find_bs_arg_by_index(struct bsCallEntry *entry, unsigned index, unsigned argc)
{
  unsigned i;

  if (entry == NULL)
    return NULL;

  if (argc == entry->argc)
    return &entry->argv[index];

  for (i = 0; i < entry->argc; i++)
    if (entry->argv[i].index == index)
      return &entry->argv[i];  

  return NULL;
}

struct bsArg *
find_bs_arg_by_c_array_len_arg_index(struct bsCallEntry *entry, unsigned index)
{
  unsigned i;
  
  if (entry == NULL)
    return NULL;

  for (i = 0; i < entry->argc; i++)
    if (entry->argv[i].c_ary_type == bsCArrayArgDelimitedByArg && entry->argv[i].c_ary_type_value == index)
      return &entry->argv[i];  

  return NULL;
}

void
register_bs_informal_protocol_method(struct bsInformalProtocolMethod *method)
{
  struct st_table *hash;

  hash = method->is_class_method ? bsInformalProtocolClassMethods : bsInformalProtocolInstanceMethods;

  st_insert(hash, (st_data_t)method->selector, (st_data_t)method);
}

struct bsInformalProtocolMethod *
find_bs_informal_protocol_method(const char *selector, BOOL is_class_method)
{
  struct st_table *hash;
  struct bsInformalProtocolMethod *method;

  hash = is_class_method 
    ? bsInformalProtocolClassMethods : bsInformalProtocolInstanceMethods;

  return st_lookup(hash, (st_data_t)selector, (st_data_t *)&method) 
    ? method : NULL;
}

static VALUE
osx_lookup_informal_protocol_method_type (VALUE rcv, VALUE sel, 
  VALUE is_class_method)
{
  struct bsInformalProtocolMethod *method;

  method = find_bs_informal_protocol_method(StringValuePtr(sel), 
    RTEST(is_class_method));

  return method == NULL ? Qnil : rb_str_new2(method->encoding);
}

void
initialize_bridge_support (VALUE mOSX)
{
  cOSXBoxed = rb_define_class_under(mOSX, "Boxed", rb_cObject);
  ivarEncodingID = rb_intern("@__encoding__");
  rb_define_singleton_method(cOSXBoxed, "encoding", rb_bs_boxed_get_encoding, 0);
  rb_define_singleton_method(cOSXBoxed, "size", rb_bs_boxed_get_size, 0);
  rb_define_singleton_method(cOSXBoxed, "fields", rb_bs_boxed_get_fields, 0);
  rb_define_singleton_method(cOSXBoxed, "opaque?", rb_bs_boxed_is_opaque, 0);

  bsBoxed = st_init_strtable();
  bsCFTypes = st_init_strtable();
  bsCFTypes2 = st_init_numtable();
  bsConstants = st_init_strtable();
  bsMagicCookieConstants = st_init_numtable();
  bsFunctions = st_init_strtable();
  bsClasses = st_init_strtable();
  bsInformalProtocolClassMethods = st_init_strtable();
  bsInformalProtocolInstanceMethods = st_init_strtable();

  rb_define_module_function(mOSX, "load_bridge_support_file",
    osx_load_bridge_support_file, 1);
  
  rb_define_module_function(mOSX, "load_bridge_support_dylib",
    osx_load_bridge_support_dylib, 1);

  rb_define_module_function(mOSX, "import_c_constant",
    osx_import_c_constant, 1);
  
  rb_define_module_function(mOSX, "lookup_informal_protocol_method_type",
    osx_lookup_informal_protocol_method_type, 2);
}
