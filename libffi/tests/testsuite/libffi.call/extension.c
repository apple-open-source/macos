/* Area:	ffi_call
   Purpose:	Check sign extension of small ints passed in big registers
   Limitations:	none.
   PR:		rdar://7755419, rdar://8042065
   Originator:	Greg Parker <gparker@apple.com>  */

/* { dg-do run } */
#include "ffitest.h"
#include <stdint.h>

int64_t s8_fn(int8_t arg) {
    return arg;
}

int64_t s16_fn(int16_t arg) {
    return arg;
}

int64_t s32_fn(int32_t arg) {
    return arg;
}

int64_t s64_fn(int64_t arg) {
    return arg;
}

int main(void)
{
    ffi_status err;
    ffi_cif cif8, cif16, cif32, cif64;
    ffi_type *argtype8[]  = { &ffi_type_sint8 };
    ffi_type *argtype16[] = { &ffi_type_sint16 };
    ffi_type *argtype32[] = { &ffi_type_sint32 };
    ffi_type *argtype64[] = { &ffi_type_sint64 };
    int8_t  arg8  = -0x12;
    int16_t arg16 = -0x1234;
    int32_t arg32 = -0x12345678;
    int64_t arg64 = -0x1234567890abcdefLL;
    void *argvalue8[]  = { &arg8 };
    void *argvalue16[] = { &arg16 };
    void *argvalue32[] = { &arg32 };
    void *argvalue64[] = { &arg64 };
    int64_t ret;

    CHECK(ffi_prep_cif(&cif8,  FFI_DEFAULT_ABI, 1, &ffi_type_sint64, argtype8) == FFI_OK);

    CHECK(ffi_prep_cif(&cif16, FFI_DEFAULT_ABI, 1, &ffi_type_sint64, argtype16) == FFI_OK);

    CHECK(ffi_prep_cif(&cif32, FFI_DEFAULT_ABI, 1, &ffi_type_sint64, argtype32) == FFI_OK);

    CHECK(ffi_prep_cif(&cif64, FFI_DEFAULT_ABI, 1, &ffi_type_sint64, argtype64) == FFI_OK);

    ffi_call(&cif8,  FFI_FN(s8_fn),  &ret, argvalue8);
    CHECK(ret == (int64_t)arg8);

    ffi_call(&cif16, FFI_FN(s16_fn), &ret, argvalue16);
    CHECK(ret == (int64_t)arg16);

    ffi_call(&cif32, FFI_FN(s32_fn), &ret, argvalue32);
    CHECK(ret == (int64_t)arg32);

    ffi_call(&cif64, FFI_FN(s64_fn), &ret, argvalue64);
    CHECK(ret == (int64_t)arg64);

    return 0;
}
