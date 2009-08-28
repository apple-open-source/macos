/*
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import "libffi.h"

typedef enum {
    bsTypeModifierIn,
    bsTypeModifierOut,
    bsTypeModifierInout
} bsTypeModifier;

typedef enum {
    bsCArrayArgUndefined,
    bsCArrayArgDelimitedByArg,
    bsCArrayArgFixedLength,
    bsCArrayArgVariableLength,
    bsCArrayArgDelimitedByNull
} bsCArrayArgType;

#define MAX_ARGS 128

struct bsArg {
  unsigned          index;
  bsTypeModifier    type_modifier;
  bsCArrayArgType   c_ary_type;
  int               c_ary_type_value;  // not set if arg_type is bsCArrayArgUndefined
  BOOL              null_accepted;
  char *            octypestr;
  char *            sel_of_type;
  BOOL              printf_format;
};

struct bsRetval {
  bsCArrayArgType   c_ary_type;
  int               c_ary_type_value;  // not set if arg_type is bsCArrayArgUndefined
  char *            octypestr;
  BOOL              should_be_retained;
};

struct bsCallEntry {
  int               argc;
  struct bsArg *    argv;
  struct bsRetval * retval;
  BOOL              is_variadic;
};

struct bsFunction {
  int               argc;
  struct bsArg *    argv;
  struct bsRetval * retval;
  BOOL              is_variadic;
  char *            name;
  void *            sym;
};

struct bsClass {
  char *              name;
  struct st_table *   class_methods;
  struct st_table *   instance_methods;
};

struct bsMethod {
  int               argc;
  struct bsArg *    argv;
  struct bsRetval * retval; // can be NULL
  BOOL              is_variadic;
  char *            selector;
  BOOL              is_class_method;
  BOOL              ignore;
  char *            suggestion;   // only if ignore is true
};

struct bsInformalProtocolMethod {
  char *  selector;
  BOOL    is_class_method;
  char *  encoding;
  char *  protocol_name;
};

#define BS_BOXED_OCTYPE_THRESHOLD  1300

struct bsStructField {
  char *    name;
  char *    encoding;
};

struct bsStruct {
  struct bsStructField *fields;
  int field_count;
  BOOL opaque;
};

struct bsOpaque {
  // Nothing there yet.
};

typedef enum {
    bsBoxedStructType,
    bsBoxedOpaqueType
} bsBoxedType;

struct bsBoxed {
  bsBoxedType   type;
  char *        name;
  char *        encoding;
  size_t        size;
  ffi_type *    ffi_type;
  VALUE         klass;
  union {
    struct bsStruct s;
    struct bsOpaque o;
  } opt;
};

struct bsCFType {
  char *    name;
  char *    encoding;
  char *    bridged_class_name;
  CFTypeID  type_id;
};

struct bsConst {
  char *    name;
  char *    encoding;
  BOOL      is_magic_cookie;
  char *    class_name;       // set lazily, and only for magic cookies
  BOOL      ignored;
  char *    suggestion;
};

extern struct bsFunction *current_function;

VALUE objboxed_s_class(void);
struct bsBoxed *find_bs_boxed_by_encoding(const char *encoding);
struct bsBoxed *find_bs_boxed_for_klass (VALUE klass);
VALUE rb_bs_boxed_new_from_ocdata(struct bsBoxed *bs_boxed, void *ocdata);
VALUE rb_bs_boxed_ptr_new_from_ocdata(struct bsBoxed *bs_boxed, void *ocdata);
void *rb_bs_boxed_get_data(VALUE obj, const char *encoding, size_t *size, BOOL *success, BOOL clean_ivars);
size_t bs_boxed_size(struct bsBoxed *bs_struct);

struct bsCFType *find_bs_cf_type_by_encoding(const char *encoding);
struct bsCFType *find_bs_cf_type_by_type_id(CFTypeID type_id);

struct bsMethod *find_bs_method(id klass, const char *selector, BOOL is_class_method);
struct bsArg *find_bs_arg_by_index(struct bsCallEntry *entry, unsigned index, unsigned argc);
struct bsArg *find_bs_arg_by_c_array_len_arg_index(struct bsCallEntry *entry, unsigned index);

void register_bs_informal_protocol_method(struct bsInformalProtocolMethod *method);
struct bsInformalProtocolMethod *find_bs_informal_protocol_method(const char *selector, BOOL is_class_method);

struct bsConst *find_magic_cookie_const_by_value(void *value);

void initialize_bridge_support(VALUE mOSX);
