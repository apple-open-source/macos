/* 
 * Copyright (c) 2006-2008, The RubyCocoa Project.
 * Copyright (c) 2001-2006, FUJIMOTO Hisakuni.
 * All Rights Reserved.
 *
 * RubyCocoa is free software, covered under either the Ruby's license or the 
 * LGPL. See the COPYRIGHT file for more information.
 */

#import <Foundation/Foundation.h>
#import "libffi.h"
#import "ocdata_conv.h"
#import "BridgeSupport.h"
#import "ocexception.h"
#import "cls_objcid.h"
#import "cls_objcptr.h"
#import "internal_macros.h"
#import <st.h>
#include <sys/mman.h>   // for mmap()

#define FFI_LOG(fmt, args...) DLOG("LIBFFI", fmt, ##args)

ffi_type *
bs_boxed_ffi_type(struct bsBoxed *bs_boxed)
{
  if (bs_boxed->ffi_type == NULL) {
    if (bs_boxed->type == bsBoxedStructType) {
      unsigned i;

      bs_boxed->ffi_type = (ffi_type *)malloc(sizeof(ffi_type));
      ASSERT_ALLOC(bs_boxed->ffi_type);
  
      bs_boxed->ffi_type->size = 0; // IMPORTANT: we need to leave this to 0 and not set the real size
      bs_boxed->ffi_type->alignment = 0;
      bs_boxed->ffi_type->type = FFI_TYPE_STRUCT;
      bs_boxed->ffi_type->elements = malloc((bs_boxed->opt.s.field_count + 1) * sizeof(ffi_type *));
      ASSERT_ALLOC(bs_boxed->ffi_type->elements);
      for (i = 0; i < bs_boxed->opt.s.field_count; i++) {
        char *octypestr;

        octypestr = bs_boxed->opt.s.fields[i].encoding;
        bs_boxed->ffi_type->elements[i] = ffi_type_for_octype(octypestr);
      }
      bs_boxed->ffi_type->elements[bs_boxed->opt.s.field_count] = NULL;
    }
    else if (bs_boxed->type == bsBoxedOpaqueType) {
      // FIXME we assume that boxed types are pointers, but maybe we should analyze the encoding.
      bs_boxed->ffi_type = &ffi_type_pointer;
    }
  }

  return bs_boxed->ffi_type;
}

static struct st_table *ary_ffi_types = NULL;

static ffi_type *
fake_ary_ffi_type (unsigned bytes, unsigned align)
{
  ffi_type *type;
  unsigned i;

  assert(bytes > 0);

  if (ary_ffi_types == NULL)
    ary_ffi_types = st_init_numtable();

  if (st_lookup(ary_ffi_types, (st_data_t)bytes, (st_data_t *)&type))
    return type;

  type = (ffi_type *)malloc(sizeof(ffi_type));
  ASSERT_ALLOC(type);

  type->size = bytes; 
  type->alignment = align;
  type->type = FFI_TYPE_STRUCT;
  type->elements = malloc(bytes * sizeof(ffi_type *));
  ASSERT_ALLOC(type->elements);
  for (i = 0; i < bytes; i++)
    type->elements[i] = &ffi_type_uchar;

  st_insert(ary_ffi_types, (st_data_t)bytes, (st_data_t)type);

  return type;
}

ffi_type *
ffi_type_for_octype (const char *octypestr)
{
  octypestr = encoding_skip_qualifiers(octypestr);

  switch (*octypestr) {
    case _C_ID:
    case _C_CLASS:
    case _C_SEL:
    case _C_CHARPTR:
    case _C_PTR:
      return &ffi_type_pointer;

    case _C_BOOL:
    case _C_UCHR:
      return &ffi_type_uchar;

    case _C_CHR:
      return &ffi_type_schar;

    case _C_SHT:
      return &ffi_type_sshort;

    case _C_USHT:
      return &ffi_type_ushort;

    case _C_INT:
      return &ffi_type_sint;

    case _C_UINT:
      return &ffi_type_uint;

    case _C_LNG:
      return sizeof(int) == sizeof(long) ? &ffi_type_sint : &ffi_type_slong;

#if defined(_C_LNG_LNG)
    case _C_LNG_LNG: 
      return &ffi_type_sint64;
#endif

    case _C_ULNG:
      return sizeof(unsigned int) == sizeof(unsigned long) ? &ffi_type_uint : &ffi_type_ulong;

#if defined(_C_ULNG_LNG)
    case _C_ULNG_LNG: 
      return &ffi_type_uint64;
#endif

    case _C_FLT:
      return &ffi_type_float;

    case _C_DBL:
      return &ffi_type_double;

    case _C_VOID:
      return &ffi_type_void;  

    case _C_BFLD:
      {
        unsigned int size;

        size = ocdata_size(octypestr);
        if (size > 0) {
          if (size == 1)
            return &ffi_type_uchar;
          else if (size == 2)
            return &ffi_type_ushort;
          else if (size <= 4)
            return &ffi_type_uint; 
          else
            return fake_ary_ffi_type(size, 0);
        }
      }
      break;

    case _C_ARY_B:
      {
#if __LP64__
        unsigned long size, align;
#else
        unsigned int size, align;
#endif

        @try {
          NSGetSizeAndAlignment(octypestr, &size, &align);
        }
        @catch (id exception) {
          rb_raise(rb_eRuntimeError, "Cannot compute size of type `%s' : %s",
            octypestr, [[exception description] UTF8String]);
        }

        if (size > 0)  
          return fake_ary_ffi_type(size, align);
      }
      break;

    default:
      {
        struct bsBoxed *bs_boxed;

        bs_boxed = find_bs_boxed_by_encoding(octypestr);
        if (bs_boxed != NULL)
          return bs_boxed_ffi_type(bs_boxed);
      }
      break;
  }

  NSLog (@"XXX returning ffi type void for unrecognized encoding '%s'", octypestr);

  return &ffi_type_void;
}

static inline BOOL
__is_in(int elem, int *array, unsigned count)
{
  unsigned i;
  for (i = 0; i < count; i++) {
    if (array[i] == elem)
      return YES;
  }
  return NO;
}

VALUE
rb_ffi_dispatch (
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
  void (*retain_if_necessary)(VALUE arg, BOOL retval, void *ctx), 
  void *retain_if_necessary_ctx, 
  VALUE *result)
{
  int         length_args[MAX_ARGS];
  unsigned    length_args_count;
  int         pointers_args[MAX_ARGS];
  unsigned    pointers_args_count;
  unsigned    skipped;
  int         i;
  ffi_type *  ret_type;
  void *      retval = NULL;
  ffi_cif     cif;
  VALUE       exception;

#define ARG_OCTYPESTR(i) \
  (arg_octypes != NULL ? arg_octypes[i] : call_entry->argv[i].octypestr) 

#define IS_POINTER_ARG(idx) \
  (__is_in(idx, pointers_args, pointers_args_count)) 

#define IS_LENGTH_ARG(idx) \
  (__is_in(idx, length_args, length_args_count)) 

  FFI_LOG("argc expected %d given %d delta %d", expected_argc, given_argc, 
    argc_delta);

  // Check arguments count.
  length_args_count = pointers_args_count = 0;
  if (call_entry != NULL) {
    for (i = 0; i < call_entry->argc; i++) {
      struct bsArg *arg;
  
      arg = &call_entry->argv[i];
      // The given argument is a C array with a length determined by the value
      // of another argument, like:
      //   [NSArray +arrayWithObjects:length:]
      // If 'in' or 'inout, the 'length' argument is not necessary (but the 
      // 'array' is). If 'out', the 'array' argument is not necessary (but the 
      // 'length' is).
      if (arg->c_ary_type == bsCArrayArgDelimitedByArg) {
        unsigned j;
        BOOL already;
        
        // Some methods may accept multiple 'array' 'in' arguments that refer 
        // to the same 'length' argument, like:
        //   [NSDictionary +dictionaryWithObjects:forKeys:count:]
        for (j = 0, already = NO; j < length_args_count; j++) {
          if (length_args[j] == arg->c_ary_type_value) {
            already = YES;
            break;
          }
        }
        if (already)
          continue;
        
        length_args[length_args_count++] = arg->c_ary_type_value;
      }
    }
    FFI_LOG("detected %d array length argument(s)", length_args_count);
  }

  if (expected_argc - length_args_count != given_argc) {
    for (i = 0; i < expected_argc; i++) {
      char *type = ARG_OCTYPESTR(i);
      type = (char *)encoding_skip_qualifiers(type);
      if (given_argc + pointers_args_count < expected_argc
          && (i >= given_argc || !NIL_P(argv[i]))
          && ((*type == _C_PTR && find_bs_cf_type_by_encoding(type) == NULL) 
               || *type == _C_ARY_B)) {
        struct bsArg *bs_arg;

        bs_arg = find_bs_arg_by_index(call_entry, i, expected_argc);
        if (bs_arg == NULL || bs_arg->type_modifier == bsTypeModifierOut)
          pointers_args[pointers_args_count++] = i;
      }
    }
    FFI_LOG("detected %d omitted pointer(s)", pointers_args_count);
    if (pointers_args_count + given_argc != expected_argc)
      return rb_err_new(rb_eArgError, 
        "wrong number of argument(s) (expected %d, got %d)", expected_argc, 
        given_argc);
  }

  for (i = skipped = 0; i < expected_argc; i++) {
    const char *octype_str;
    
    octype_str = ARG_OCTYPESTR(i);
    // C-array-length-like argument, which should be already defined
    // at the same time than the C-array-like argument, unless it's
    // returned by reference or specifically provided.
    if (IS_LENGTH_ARG(i) && *octype_str != _C_PTR) {
      if (given_argc + skipped < expected_argc) {
        skipped++;
      }
      else {
        VALUE arg;
        int *value;
        int *prev_len;

        arg = argv[i - skipped];
        Check_Type(arg, T_FIXNUM);

        value = OCDATA_ALLOCA(octype_str);
        if (!rbobj_to_ocdata(arg, octype_str, value, NO))
          return rb_err_new(ocdataconv_err_class(), "Cannot convert the argument #%d as '%s' to Objective-C", i, octype_str); 
        
        prev_len = arg_values[i + argc_delta];
        if (prev_len != NULL && (*prev_len < *value || *value < 0))
          return rb_err_new(rb_eArgError, "Incorrect array length of argument #%d (expected a non negative value greater or equal to %d, got %d)", i, *prev_len, *value);
        
        arg_types[i + argc_delta] = ffi_type_for_octype(octype_str);
        arg_values[i + argc_delta] = value;
      }
    } 
    // Omitted pointer.
    else if (IS_POINTER_ARG(i)) {
      void *value;
      arg_types[i + argc_delta] = &ffi_type_pointer;
      if (*octype_str == _C_PTR) {
        // Regular pointer.
        value = alloca(sizeof(void *));
        *(void **)value = OCDATA_ALLOCA(octype_str+1);
      }
      else {
        // C_ARY.
        value = alloca(sizeof(void *));
        *(void **)value = OCDATA_ALLOCA(octype_str);
      }
      void **p = *(void ***)value;
      *p = NULL;
      arg_values[i + argc_delta] = value; 
      FFI_LOG("omitted_pointer[%d] (%p) : %s", i, arg_values[i + argc_delta], 
        octype_str);
      skipped++;
    }
    // Regular argument.
    else {
      volatile VALUE arg;
      void *value;
      BOOL is_c_array;
      int len;
      struct bsArg *bs_arg;

      arg = argv[i - skipped];
      bs_arg = find_bs_arg_by_index(call_entry, i, expected_argc);

      if (bs_arg != NULL) {
        if (!bs_arg->null_accepted && NIL_P(arg))
          return rb_err_new(rb_eArgError, "Argument #%d cannot be nil", i);
        if (bs_arg->octypestr != NULL) 
          octype_str = bs_arg->octypestr;
        is_c_array = bs_arg->c_ary_type != bsCArrayArgUndefined;
      }
      else {
        is_c_array = NO;
      }

      // C-array-like argument.
      if (is_c_array) {
        const char * ptype;

        ptype = octype_str;
        ptype = encoding_skip_qualifiers(ptype);
        if (*ptype != _C_PTR && *ptype != _C_ARY_B && *ptype != _C_CHARPTR)
          return rb_err_new(rb_eRuntimeError, "Internal error: argument #%d is not a defined as a pointer in the runtime or it is described as such in the metadata", i);
        ptype++;

        if (NIL_P(arg))
          len = 0;
        else if (TYPE(arg) == T_STRING)
          len = RSTRING(arg)->len;
        else if (TYPE(arg) == T_ARRAY)
          len = RARRAY(arg)->len; // XXX should be RARRAY(arg)->len * ocdata_sizeof(...)
        else if (rb_obj_is_kind_of(arg, objcptr_s_class()))
          len = objcptr_allocated_size(arg); 
        else {
          return rb_err_new(rb_eArgError, "Expected either String/Array/ObjcPtr for argument #%d (but got %s).", i, rb_obj_classname(arg));
        }

        if (bs_arg->c_ary_type == bsCArrayArgFixedLength) {
          int expected_len = bs_arg->c_ary_type_value * ocdata_size(ptype);
          if (expected_len != len)
            return rb_err_new(rb_eArgError, "Argument #%d has an invalid length (expected %d, got %d)", i, expected_len, len); 
        }
        else if (bs_arg->c_ary_type == bsCArrayArgDelimitedByArg) {
          int * prev_len;
        
          prev_len = arg_values[bs_arg->c_ary_type_value + argc_delta];
          if (prev_len != NULL && (*prev_len > len || *prev_len < 0))
            return rb_err_new(rb_eArgError, "Incorrect array length of argument #%d (expected a non negative value greater or equal to %d, got %d)", i, len, *prev_len);
          FFI_LOG("arg[%d] (%p) : %s (defined as a C array delimited by arg #%d in the metadata)", i, arg, octype_str, bs_arg->c_ary_type_value);
        }
        value = OCDATA_ALLOCA(octype_str);
        if (len > 0)
          *(void **) value = alloca(ocdata_size(ptype) * len);
      }
      // Regular argument. 
      else {
        FFI_LOG("arg[%d] (%p) : %s", i, arg, octype_str);
        len = 0;
        value = OCDATA_ALLOCA(octype_str);
      }

      if (!rbobj_to_ocdata(arg, octype_str, value, NO))
        return rb_err_new(ocdataconv_err_class(), "Cannot convert the argument #%d as '%s' to Objective-C", i, octype_str); 

      // Register the selector value as an informal protocol, based on the metadata annotation.
      if (*octype_str == _C_SEL && bs_arg != NULL && bs_arg->sel_of_type != NULL && *(char **)value != NULL) {
        struct bsInformalProtocolMethod * inf_prot_method;

        inf_prot_method = find_bs_informal_protocol_method(*(char **)value, NO);
        if (inf_prot_method != NULL) {
          if (strcmp(inf_prot_method->encoding, bs_arg->sel_of_type) != 0)
            return rb_err_new(ocdataconv_err_class(), "Cannot register the given selector '%s' as an informal protocol method of type '%s', because another informal protocol method is already registered with the same signature (but with another type, '%s'. Please rename your selector.", *(char **)value, bs_arg->sel_of_type, inf_prot_method->encoding);
        }
        else {
          inf_prot_method = (struct bsInformalProtocolMethod *)malloc(sizeof(struct bsInformalProtocolMethod));
          ASSERT_ALLOC(inf_prot_method);
          inf_prot_method->selector = *(char **)value; // no need to dup it, selectors are unique
          inf_prot_method->is_class_method = NO;
          inf_prot_method->encoding = bs_arg->sel_of_type;
          inf_prot_method->protocol_name = NULL; // unnamed
          register_bs_informal_protocol_method(inf_prot_method);
          FFI_LOG("registered informal protocol method '%s' of type '%s'", *(char **)value, bs_arg->sel_of_type);
        }
      }
      
      arg_types[i + argc_delta] = ffi_type_for_octype(octype_str);
      arg_values[i + argc_delta] = value;

      if (is_c_array && bs_arg->c_ary_type == bsCArrayArgDelimitedByArg) {
        int * plen;

        FFI_LOG("arg[%d] defined as the array length (%d)", bs_arg->c_ary_type_value, len);
        plen = (int *) alloca(sizeof(int));
        *plen = len;
        arg_values[bs_arg->c_ary_type_value + argc_delta] = plen;
        arg_types[bs_arg->c_ary_type_value + argc_delta] = &ffi_type_uint;
      }
    }
  }

  // Prepare return type/val.
  if (call_entry != NULL 
      && call_entry->retval != NULL 
      && call_entry->retval->octypestr != NULL
      && strcmp(call_entry->retval->octypestr, ret_octype) != 0) {

    FFI_LOG("coercing result from octype '%s' to octype '%s'", ret_octype, call_entry->retval->octypestr);
    ret_octype = call_entry->retval->octypestr;
  }
  FFI_LOG("retval : %s", ret_octype);
  ret_type = ffi_type_for_octype(ret_octype);
  if (ret_type != &ffi_type_void) {
    size_t ret_len = MAX(sizeof(long), ocdata_size(ret_octype));
    FFI_LOG("allocated %ld bytes for the result", ret_len);
    retval = alloca(ret_len);
  }

  // Prepare cif.
  int cif_ret_status = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, expected_argc + argc_delta, ret_type, arg_types);
  if (cif_ret_status != FFI_OK)
    rb_fatal("Can't prepare the cif");

  // Call function.
  exception = Qnil;
  @try {
    FFI_LOG("ffi_call %p with %d args", func_sym, expected_argc + argc_delta);
    ffi_call(&cif, FFI_FN(func_sym), (ffi_arg *)retval, arg_values);
    FFI_LOG("ffi_call done");
  }
  @catch (id oc_exception) {
    FFI_LOG("got objc exception '%@' -- forwarding...", oc_exception);
    exception = oc_err_new(oc_exception);
  }

  // Return exception if catched.
  if (!NIL_P(exception))
    return exception;

  // Get result as argument.
  for (i = 0; i < expected_argc; i++) {
    VALUE arg;

    arg = (i < given_argc) ? argv[i] : Qnil;
    if (arg == Qnil)
      continue;
    if (!is_id_ptr(ARG_OCTYPESTR(i)))
      continue;
    if (rb_obj_is_kind_of(arg, objid_s_class()) != Qtrue)
      continue;
    FFI_LOG("got passed-by-reference argument %d", i);
    (*retain_if_necessary)(arg, NO, retain_if_necessary_ctx);
  }

  // Get result
  if (ret_type != &ffi_type_void) {
    FFI_LOG("getting return value (%p of type '%s')", retval, ret_octype);

    if (!ocdata_to_rbobj(Qnil, ret_octype, retval, result, YES))
      return rb_err_new(ocdataconv_err_class(), "Cannot convert the result as '%s' to Ruby", ret_octype); 
    
    FFI_LOG("got return value");
    (*retain_if_necessary)(*result, YES, retain_if_necessary_ctx);
  } 
  else {
    *result = Qnil;
  }

  // Get omitted pointers result, and pack them with the result in an array.
  if (pointers_args_count > 0) {
    volatile VALUE retval_ary;

    retval_ary = rb_ary_new();
    if (*ret_octype != _C_VOID) { 
      // Don't test if *result is nil, as nil may have been returned!
      rb_ary_push(retval_ary, *result);
    }

    for (i = 0; i < expected_argc; i++) {
      void *value;

      if (!IS_POINTER_ARG(i))
        continue;

      value = arg_values[i + argc_delta];
      if (value != NULL) {
        volatile VALUE rbval;
        const char *octype_str;
        struct bsArg *bs_arg;
        char fake_octype_str[512];

        octype_str = ARG_OCTYPESTR(i);
        octype_str = encoding_skip_qualifiers(octype_str);
        if (*octype_str == _C_PTR)
          octype_str++;
        octype_str = encoding_skip_qualifiers(octype_str);
        FFI_LOG("got omitted_pointer[%d] : %s (%p)", i, octype_str, value);
        rbval = Qnil;
        if ((*octype_str == _C_PTR || *octype_str == _C_ARY_B) 
            && (bs_arg = find_bs_arg_by_index(call_entry, i, expected_argc)) 
              != NULL) {

          switch (bs_arg->c_ary_type) {
            case bsCArrayArgDelimitedByArg:
              {
                void *length_data;
                long length_value;

                length_data = 
                  arg_values[bs_arg->c_ary_type_value + argc_delta];
                if (length_data == NULL) {
                  length_value = 0;
                }
                else if (IS_POINTER_ARG(bs_arg->c_ary_type_value)) {
                  long *p = *(long **)length_data;
                  length_value = *p;
                }
                else {
                  length_value = *(long *)length_data;
                } 

                if (length_value > 0) {
                  if (*octype_str == _C_ARY_B) {
                    char *p = (char *)octype_str;
                    do { p++; } while (isdigit(*p));
                    snprintf(fake_octype_str, sizeof fake_octype_str, 
                      "[%ld%s", length_value, p);
                    octype_str = fake_octype_str;
                  }
                  else {
                    rbval = rb_str_new((char *)value, 
                      length_value * ocdata_size(octype_str));
                  }
                }
                else {
                  FFI_LOG("array length should have been returned by argument #%d, but it's invalid (%ld), defaulting on ObjCPtr", bs_arg->c_ary_type_value, length_value);
                }
              }
              break;

            case bsCArrayArgFixedLength:
              rbval = rb_str_new((char *)value, bs_arg->c_ary_type_value);
              break;

            case bsCArrayArgDelimitedByNull:
              rbval = rb_str_new2((char *)value);
              break;

            default:
              // Do nothing.
              break;
          }
        }

        if (NIL_P(rbval)) {
          void *p;
          if (*octype_str == _C_ARY_B)
            p = &value;
          else
            p = *(void **)value;
          if (!ocdata_to_rbobj(Qnil, octype_str, p, (VALUE*)&rbval, YES))
            return rb_err_new(ocdataconv_err_class(), "Cannot convert the passed-by-reference argument #%d as '%s' to Ruby", i, octype_str);
        }
        (*retain_if_necessary)(rbval, NO, retain_if_necessary_ctx);
        rb_ary_push(retval_ary, rbval);
      }
      else {
        FFI_LOG("omitted pointer[%d] is nil, skipping...", i);
      }
    }

    *result = RARRAY(retval_ary)->len == 1 ? RARRAY(retval_ary)->ptr[0] : RARRAY(retval_ary)->len == 0 ? Qnil : retval_ary;
  }
  
  FFI_LOG("ffi dispatch done");

  return Qnil;
}

void *
ffi_make_closure(const char *rettype, const char **argtypes, unsigned argc, void (*handler)(ffi_cif *,void *,void **,void *), void *context)
{
  const char *error;
  unsigned i;
  ffi_type *retval_ffi_type;
  ffi_type **arg_ffi_types;
  ffi_cif *cif;
  ffi_closure *closure;

  error = NULL;
  cif = NULL;
  closure = NULL;

  FFI_LOG("make closure argc %d", argc);

  arg_ffi_types = (ffi_type **)malloc(sizeof(ffi_type *) * (argc + 1));
  if (arg_ffi_types == NULL) {
    error = "Can't allocate memory";
    goto bails;
  }

  for (i = 0; i < argc; i++) {
    arg_ffi_types[i] = ffi_type_for_octype(argtypes[i]);
    FFI_LOG("arg[%d] -> ffi_type %p", i, arg_ffi_types[i]);
  }
  retval_ffi_type = ffi_type_for_octype(rettype);
  arg_ffi_types[argc] = NULL;

  cif = (ffi_cif *)malloc(sizeof(ffi_cif));
  if (cif == NULL) {
    error = "Can't allocate memory";
    goto bails;
  }

  if (ffi_prep_cif(cif, FFI_DEFAULT_ABI, argc, retval_ffi_type, arg_ffi_types) != FFI_OK) {
    error = "Can't prepare cif";
    goto bails;
  }

  // Allocate a page to hold the closure with read and write permissions.
  if ((closure = mmap(NULL, sizeof(ffi_closure), PROT_READ | PROT_WRITE,
				  MAP_ANON | MAP_PRIVATE, -1, 0)) == (void*)-1)
  {
    error = "Can't allocate memory";
    goto bails;
  }

  if (ffi_prep_closure(closure, cif, handler, context) != FFI_OK) {
    error = "Can't prepare closure";
    goto bails;
  }

  // Ensure that the closure will execute on all architectures.
  if (mprotect(closure, sizeof(closure), PROT_READ | PROT_EXEC) == -1)
  {
    error = "Can't mark the closure with PROT_EXEC";
    goto bails;
  }

  goto done;

bails:
  if (arg_ffi_types != NULL)
    free(arg_ffi_types);
  if (cif != NULL)
    free(cif);
  if (closure != NULL)
    free(closure);
  if (error != NULL)
    rb_raise(rb_eRuntimeError, error);

done:
  return closure;
}

@interface __OVMIXThreadDispatcher : NSObject
{
  void (*_handler)(ffi_cif *,void *,void **,void *);
  void (*_finished_handler)(ffi_cif *,void *,void **,void *);
  ffi_cif * _cif;
  void * _resp;
  void ** _args;
  void * _userdata;
}
@end

@implementation __OVMIXThreadDispatcher

- (id)initWithClosure:(void (*)(ffi_cif *,void *,void **,void *))handler 
  cif:(ffi_cif *)cif 
  resp:(void *)resp 
  args:(void **)args 
  userdata:(void *)userdata 
  finished_handler:(void (*)(ffi_cif *,void *,void **,void *))finished_handler
{
  self = [super init];
  if (self != NULL) {
    _handler = handler;
    _cif = cif;
    _resp = resp;
    _args = args;
    _userdata = userdata;
    _finished_handler = finished_handler;
  }
  return self;
}

extern NSThread *rubycocoaThread;

- (void)dispatch
{
  DISPATCH_ON_RUBYCOCOA_THREAD(self, @selector(syncDispatch));
}

- (void)syncDispatch
{
  (*_handler)(_cif, _resp, _args, _userdata);
  (*_finished_handler)(_cif, _resp, _args, _userdata);
}

@end

void ffi_dispatch_closure_in_main_thread(
  void (*handler)(ffi_cif *,void *,void **,void *),
  ffi_cif *cif,
  void *resp,
  void **args,
  void *userdata,
  void (*finished_handler)(ffi_cif *,void *,void **,void *))
{
  __OVMIXThreadDispatcher * dispatcher;

  dispatcher = [[__OVMIXThreadDispatcher alloc] 
    initWithClosure:handler 
    cif:cif 
    resp:resp 
    args:args 
    userdata:userdata 
    finished_handler:finished_handler];
  [dispatcher dispatch];
  [dispatcher release]; 
}
