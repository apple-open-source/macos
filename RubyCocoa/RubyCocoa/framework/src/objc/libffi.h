/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import "osx_ruby.h"

/* Ruby and FFI use both autoconf, and some variables collide. */
#if defined(PACKAGE_BUGREPORT)
# undef PACKAGE_BUGREPORT
#endif

#if defined(PACKAGE_NAME) 
# undef PACKAGE_NAME
#endif

#if defined(PACKAGE_STRING)
# undef PACKAGE_STRING
#endif

#if defined(PACKAGE_TARNAME)
# undef PACKAGE_TARNAME
#endif

#if defined(PACKAGE_VERSION)
# undef PACKAGE_VERSION
#endif

#if defined(WORDS_BIGENDIAN)
# undef WORDS_BIGENDIAN
#endif

#import "ffi.h"

struct bsBoxed;
struct bsCallEntry;

ffi_type *ffi_type_for_octype(const char *octypestr);
ffi_type *bs_boxed_ffi_type(struct bsBoxed *bs_boxed);

VALUE rb_ffi_dispatch(
  struct bsCallEntry *call_entry, 
  char **arg_octypes, 
  int expected_argc, 
  int given_argc, 
  int argc_delta, 
  VALUE *argv, 
  ffi_type **arg_types, 
  void **arg_values, 
  char *ret_octype, 
  void *func_sym, 
  void (*retain_if_necessary)(VALUE arg, BOOL is_retval, void *ctx), 
  void *retain_if_necessary_ctx, 
  VALUE *result);

void *ffi_make_closure(
  const char *rettype, 
  const char **argtypes, 
  unsigned argc, 
  void (*handler)(ffi_cif *,void *,void **,void *), 
  void *context);

void ffi_dispatch_closure_in_main_thread(
  void (*handler)(ffi_cif *,void *,void **,void *), 
  ffi_cif *cif, 
  void *resp, 
  void **args, 
  void *userdata,
  void (*finished_handler)(ffi_cif *,void *,void **,void *));
