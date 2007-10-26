/* 
 * Copyright (c) 2006-2007, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <objc/objc-class.h>
#import <stdarg.h>
#import "osx_ruby.h"

#if HAVE_LONG_LONG
/* long long is missing from objc-class.h
   _C_LLNG and _C_ULLNG are kept for historical reasons, but the official 
   constants are _C_LNG_LNG and _C_ULNG_LNG */
# ifndef _C_LLNG
#  define _C_LLNG 'q'
# endif
# ifndef _C_LNG_LNG
#  define _C_LNG_LNG 'q'
# endif
# ifndef _C_ULLNG
#  define _C_ULLNG 'Q'
# endif
# ifndef _C_ULNG_LNG
#  define _C_ULNG_LNG 'Q'
# endif
/* NUM2ULL is missing from ruby.h */
# ifndef NUM2ULL
#  define NUM2ULL(x) (FIXNUM_P(x)?FIX2ULONG(x):rb_num2ull((VALUE)x))
# endif
#endif	/* HAVE_LONG_LONG */

#if !defined(_C_BOOL)
# define _C_BOOL 'B'
#endif

#if !defined(_C_CONST)
# define _C_CONST 'r'
#endif

enum osxobjc_nsdata_type {
  _PRIV_C_BOOL = 1024,
  _PRIV_C_PTR,
  _PRIV_C_ID_PTR,
  _PRIV_C_FUNC_PTR
};

size_t  ocdata_alloc_size (const char* octype_str);
size_t  ocdata_size       (const char* octype_str);
void*   ocdata_malloc     (const char* octype_str);
#define OCDATA_ALLOCA(s)  alloca(ocdata_alloc_size(s))

const char *encoding_skip_modifiers(const char *type);
BOOL is_id_ptr (const char *type);
struct bsBoxed;
BOOL is_boxed_ptr (const char *type, struct bsBoxed **boxed);

SEL          rbobj_to_nssel    (VALUE obj);
BOOL         rbobj_to_nsobj    (VALUE obj, id* nsobj);
BOOL         rbobj_to_bool     (VALUE obj);
id           rbstr_to_ocstr    (VALUE obj);

VALUE    sel_to_rbobj (SEL val);
VALUE    int_to_rbobj (int val);
VALUE   uint_to_rbobj (unsigned int val);
VALUE double_to_rbobj (double val);
VALUE   bool_to_rbobj (BOOL val);
VALUE   ocid_to_rbobj (VALUE context_obj, id ocid);
VALUE  ocstr_to_rbstr (id ocstr);

BOOL  ocdata_to_rbobj (VALUE context_obj,
		       const char *octype, const void* ocdata, VALUE* result, BOOL from_libffi);
BOOL  rbobj_to_ocdata (VALUE obj, const char *octype, void* ocdata, BOOL to_libffi);

void init_rb2oc_cache(void);
void init_oc2rb_cache(void);
void remove_from_rb2oc_cache(VALUE rbobj);
void remove_from_oc2rb_cache(id ocid);
VALUE ocid_to_rbobj_cache_only(id ocid);

@class NSMethodSignature;
void decode_method_encoding(const char *encoding, NSMethodSignature *methodSignature, unsigned *argc, char **retval_type, char ***arg_types, BOOL strip_first_two_args);
void set_octypes_for_format_str (char **octypes, unsigned len, char *format_str);
