/*****************************************
 * ffidl (Darwin 9 Universal version)
 *
 * A combination of libffi, for foreign function
 * interface, and libdl, for dynamic library loading and
 * symbol listing,  packaged with hints from ::dll, and
 * exported to Tcl.
 *
 * Ffidl - Copyright (c) 1999 by Roger E Critchlow Jr,
 * Santa Fe, NM, USA, rec@elf.org
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the ``Software''), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL ROGER E CRITCHLOW JR BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Note that this distribution of Ffidl contains a modified copy of libffi
 * which has its own Copyright notice and License.
 *
 */

/*
 * Changes since ffidl 0.6:
 *  - support for 4-way universal builds on Darwin
 *  - support for Leopard libffi
 *  - remove ffcall and other code unused in Darwin universal build
 *  - support for Darwin Intel
 *  - ObjType bugfixes
 *  - TEA 3.6 buildsystem
 *
 * Changes since ffidl 0.5:
 *  - updates for 2005 version of libffi
 *  - TEA 3.2 buildsystem, testsuite
 *  - support for Tcl 8.4, Tcl_WideInt, TclpDlopen
 *  - support for Darwin PowerPC
 *  - fixes for 64bit (LP64)
 *  - callouts & callbacks are created/used relative to current namespace (for unqualified names)
 *  - addition of [ffidl::stubsymbol] for Tcl/Tk symbol resolution via stubs tables
 *  - callbacks can be called anytime, not just from inside callouts (using Tcl_BackgroundError to report errors)
 *
 * These changes are under BSD License and are
 * Copyright (c) 2005-2008, Daniel A. Steffen <das@users.sourceforge.net>
 *
 */

#include <tcl.h>
#include <tclInt.h>
#include <tclPort.h>

#ifdef LOOKUP_TK_STUBS
static const char *MyTkInitStubs(Tcl_Interp *interp, char *version, int exact);
static void *tkStubsPtr, *tkPlatStubsPtr, *tkIntStubsPtr, *tkIntPlatStubsPtr, *tkIntXlibStubsPtr;
#else
#define tkStubsPtr NULL
#define tkPlatStubsPtr NULL
#define tkIntStubsPtr NULL
#define tkIntPlatStubsPtr NULL
#define tkIntXlibStubsPtr NULL
#endif

#include <string.h>
#include <stdlib.h>

#include <ffi.h>

#ifdef FFI_NO_RAW_API
#undef FFI_NATIVE_RAW_API
#define FFI_NATIVE_RAW_API 0
#endif

#ifndef FFI_CLOSURES
#define HAVE_CLOSURES 0
#else
#define HAVE_CLOSURES FFI_CLOSURES
#endif

#define lib_type_void	&ffi_type_void
#define lib_type_uint8	&ffi_type_uint8
#define lib_type_sint8	&ffi_type_sint8
#define lib_type_uint16	&ffi_type_uint16
#define lib_type_sint16	&ffi_type_sint16
#define lib_type_uint32	&ffi_type_uint32
#define lib_type_sint32	&ffi_type_sint32
#define lib_type_uint64	&ffi_type_uint64
#define lib_type_sint64	&ffi_type_sint64
#define lib_type_float	&ffi_type_float
#define lib_type_double	&ffi_type_double
#define lib_type_longdouble	&ffi_type_longdouble
#define lib_type_pointer	&ffi_type_pointer

#define lib_type_schar	&ffi_type_schar
#define lib_type_uchar	&ffi_type_uchar
#define lib_type_ushort	&ffi_type_ushort
#define lib_type_sshort	&ffi_type_sshort
#define lib_type_uint	&ffi_type_uint
#define lib_type_sint	&ffi_type_sint
/* ffi_type_ulong & ffi_type_slong are always 64bit ! */
#if SIZEOF_LONG == 2
#define lib_type_ulong	&ffi_type_uint16
#define lib_type_slong	&ffi_type_sint16
#elif SIZEOF_LONG == 4
#define lib_type_ulong	&ffi_type_uint32
#define lib_type_slong	&ffi_type_sint32
#elif SIZEOF_LONG == 8
#define lib_type_ulong	&ffi_type_uint64
#define lib_type_slong	&ffi_type_sint64
#endif
#if HAVE_LONG_LONG
#if SIZEOF_LONG_LONG == 2
#define lib_type_ulonglong	&ffi_type_uint16
#define lib_type_slonglong	&ffi_type_sint16
#elif SIZEOF_LONG_LONG == 4
#define lib_type_ulonglong	&ffi_type_uint32
#define lib_type_slonglong	&ffi_type_sint32
#elif SIZEOF_LONG_LONG == 8
#define lib_type_ulonglong	&ffi_type_uint64
#define lib_type_slonglong	&ffi_type_sint64
#endif
#endif

#ifdef __CHAR_UNSIGNED__
#define lib_type_char	&ffi_type_uint8
#else
#define lib_type_char	&ffi_type_sint8
#endif

/*
 * Turn callbacks off if they're not implemented
 */
#if defined USE_CALLBACKS
#if ! HAVE_CLOSURES
#undef USE_CALLBACKS
#endif
#endif

/*****************************************
 *
 * ffidlopen, ffidlsym, and ffidlclose abstractions
 * of dlopen(), dlsym(), and dlclose().
 */
#ifndef NO_DLFCN_H
#include <dlfcn.h>

static void *ffidlopen(char *library, const char **error)
{
  void *handle = dlopen(library, RTLD_NOW | RTLD_GLOBAL);
  *error = dlerror();
  return handle;
}
static void *ffidlsym(void *handle, char *name, const char **error)
{
  void *address = dlsym(handle, name);
  *error = dlerror();
  return address;
}
static void ffidlclose(void *handle, const char **error)
{
  dlclose(handle);
  *error = dlerror();
}

#endif

/*****************************************
 *
 * Functions exported from this file.
 */
EXTERN void *	ffidl_pointer_pun(void *p);
EXTERN int	Ffidl_Init(Tcl_Interp *interp);

/*****************************************
 *
 * Definitions.
 */
/*
 * values for ffidl_type.type
 */
#define FFIDL_VOID		0
#define FFIDL_INT		1
#define FFIDL_FLOAT		2
#define FFIDL_DOUBLE		3
#define FFIDL_LONGDOUBLE	4
#define FFIDL_UINT8		5
#define FFIDL_SINT8		6
#define FFIDL_UINT16		7
#define FFIDL_SINT16		8
#define FFIDL_UINT32		9
#define FFIDL_SINT32		10
#define FFIDL_UINT64		11
#define FFIDL_SINT64		12
#define FFIDL_STRUCT		13
#define FFIDL_PTR		14	/* integer value pointer */
#define FFIDL_PTR_BYTE		15	/* byte array pointer */
#define FFIDL_PTR_UTF8		16	/* UTF-8 string pointer */
#define FFIDL_PTR_UTF16		17	/* UTF-16 string pointer */
#define FFIDL_PTR_VAR		18	/* byte array in variable */
#define FFIDL_PTR_OBJ		19	/* Tcl_Obj pointer */
#define FFIDL_PTR_PROC		20	/* Pointer to Tcl proc */

/*
 * aliases for unsized type names
 */
#ifdef __CHAR_UNSIGNED__
#define FFIDL_CHAR	FFIDL_UINT8
#else
#define FFIDL_CHAR	FFIDL_SINT8
#endif

#define FFIDL_SCHAR	FFIDL_SINT8
#define FFIDL_UCHAR	FFIDL_UINT8

#if SIZEOF_SHORT == 2
#define FFIDL_USHORT	FFIDL_UINT16
#define FFIDL_SSHORT	FFIDL_SINT16
#elif SIZEOF_SHORT == 4
#define FFIDL_USHORT	FFIDL_UINT32
#define FFIDL_SSHORT	FFIDL_SINT32
#define UINT_T
#elif SIZEOF_SHORT == 8
#define FFIDL_USHORT	FFIDL_UINT64
#define FFIDL_SSHORT	FFIDL_SINT64
#else
#error "no short type"
#endif

#if SIZEOF_INT == 2
#define FFIDL_UINT	FFIDL_UINT16
#define FFIDL_SINT	FFIDL_SINT16
#elif SIZEOF_INT == 4
#define FFIDL_UINT	FFIDL_UINT32
#define FFIDL_SINT	FFIDL_SINT32
#elif SIZEOF_INT == 8
#define FFIDL_UINT	FFIDL_UINT64
#define FFIDL_SINT	FFIDL_SINT64
#else
#error "no int type"
#endif

#if SIZEOF_LONG == 2
#define FFIDL_ULONG	FFIDL_UINT16
#define FFIDL_SLONG	FFIDL_SINT16
#elif SIZEOF_LONG == 4
#define FFIDL_ULONG	FFIDL_UINT32
#define FFIDL_SLONG	FFIDL_SINT32
#elif SIZEOF_LONG == 8
#define FFIDL_ULONG	FFIDL_UINT64
#define FFIDL_SLONG	FFIDL_SINT64
#else
#error "no long type"
#endif

#if HAVE_LONG_LONG
#if SIZEOF_LONG_LONG == 2
#define FFIDL_ULONGLONG	FFIDL_UINT16
#define FFIDL_SLONGLONG	FFIDL_SINT16
#elif SIZEOF_LONG_LONG == 4
#define FFIDL_ULONGLONG	FFIDL_UINT32
#define FFIDL_SLONGLONG	FFIDL_SINT32
#elif SIZEOF_LONG_LONG == 8
#define FFIDL_ULONGLONG	FFIDL_UINT64
#define FFIDL_SLONGLONG	FFIDL_SINT64
#else
#error "no long long type"
#endif
#endif

/*
 * Once more through, decide the alignment and C types
 * for the sized ints
 */

#define ALIGNOF_INT8	1
#define UINT8_T		unsigned char
#define SINT8_T		signed char

#if SIZEOF_SHORT == 2
#define ALIGNOF_INT16	ALIGNOF_SHORT
#define UINT16_T	unsigned short
#define SINT16_T	signed short
#elif SIZEOF_INT == 2
#define ALIGNOF_INT16	ALIGNOF_INT
#define UINT16_T	unsigned int
#define SINT16_T	signed int
#elif SIZEOF_LONG == 2
#define ALIGNOF_INT16	ALIGNOF_LONG
#define UINT16_T	unsigned long
#define SINT16_T	signed long
#else
#error "no 16 bit int"
#endif

#if SIZEOF_SHORT == 4
#define ALIGNOF_INT32	ALIGNOF_SHORT
#define UINT32_T	unsigned short
#define SINT32_T	signed short
#elif SIZEOF_INT == 4
#define ALIGNOF_INT32	ALIGNOF_INT
#define UINT32_T	unsigned int
#define SINT32_T	signed int
#elif SIZEOF_LONG == 4
#define ALIGNOF_INT32	ALIGNOF_LONG
#define UINT32_T	unsigned long
#define SINT32_T	signed long
#else
#error "no 32 bit int"
#endif

#if SIZEOF_SHORT == 8
#define ALIGNOF_INT64	ALIGNOF_SHORT
#define UINT64_T	unsigned short
#define SINT64_T	signed short
#elif SIZEOF_INT == 8
#define ALIGNOF_INT64	ALIGNOF_INT
#define UINT64_T	unsigned int
#define SINT64_T	signed int
#elif SIZEOF_LONG == 8
#define ALIGNOF_INT64	ALIGNOF_LONG
#define UINT64_T	unsigned long
#define SINT64_T	signed long
#elif HAVE_LONG_LONG && SIZEOF_LONG_LONG == 8
#define ALIGNOF_INT64	ALIGNOF_LONG_LONG
#define UINT64_T	unsigned long long
#define SINT64_T	signed long long
#endif

#ifdef ALIGNOF_INT64
#define HAVE_INT64	1
#endif

/*
 * values for ffidl_type.class
 */
#define FFIDL_ARG		0x001	/* type parser in argument context */
#define FFIDL_RET		0x002	/* type parser in return context */
#define FFIDL_ELT		0x004	/* type parser in element context */
#define FFIDL_CBARG		0x008	/* type parser in callback argument context */
#define FFIDL_CBRET		0x010	/* type parser in callback return context */
#define FFIDL_ALL		(FFIDL_ARG|FFIDL_RET|FFIDL_ELT|FFIDL_CBARG|FFIDL_CBRET)
#define FFIDL_ARGRET		(FFIDL_ARG|FFIDL_RET)
#define FFIDL_GETINT		0x020	/* arg needs an int value */
#define FFIDL_GETDOUBLE		0x040	/* arg needs a double value */
#define FFIDL_GETBYTES		0x080	/* arg needs a bytearray value */
#define FFIDL_STATIC_TYPE	0x100	/* do not free this type */
#define FFIDL_GETWIDEINT	0x200	/* arg needs a wideInt value */

/*****************************************
 *
 * Type definitions for ffidl.
 */
/*
 * forward declarations.
 */
typedef union ffidl_value ffidl_value;
typedef struct ffidl_type ffidl_type;
typedef struct ffidl_client ffidl_client;
typedef struct ffidl_cif ffidl_cif;
typedef struct ffidl_callout ffidl_callout;
typedef struct ffidl_callback ffidl_callback;
typedef struct ffidl_closure ffidl_closure;

/*
 * The ffidl_value structure contains a union used
 * for converting to/from Tcl type.
 */
union ffidl_value {
  int v_int;
  float v_float;
  double v_double;
#if HAVE_LONG_DOUBLE
  long double v_longdouble;
#endif
  UINT8_T v_uint8;
  SINT8_T v_sint8;
  UINT16_T v_uint16;
  SINT16_T v_sint16;
  UINT32_T v_uint32;
  SINT32_T v_sint32;
#if HAVE_INT64
  UINT64_T v_uint64;
  SINT64_T v_sint64;
#endif
  void *v_struct;
  void *v_pointer;
};

/*
 * The ffidl_type structure contains a type code, a class,
 * the size of the type, the structure element alignment of
 * the class, and a pointer to the underlying ffi_type.
 */
struct ffidl_type {
   size_t size;
   unsigned short typecode;
   unsigned short class;
   unsigned short alignment;
   unsigned short nelts;
   ffidl_type **elements;
   ffi_type *lib_type;
};

/*
 * The ffidl_client contains
 * a hashtable for ffidl-typedef definitions,
 * a hashtable for ffidl-callout definitions,
 * a hashtable for cif's keyed by signature,
 * a hashtable of libs loaded by ffidl-symbol,
 * a hashtable of callbacks keyed by proc name
 */
struct ffidl_client {
  Tcl_HashTable types;
  Tcl_HashTable cifs;
  Tcl_HashTable callouts;
  Tcl_HashTable libs;
  Tcl_HashTable callbacks;
};

/*
 * The ffidl_cif structure contains an ffi_cif,
 * an array of ffidl_types used to construct the
 * cif and convert arguments, and an array of void*
 * used to pass converted arguments into ffi_call.
 */
struct ffidl_cif {
   int refs;
   ffidl_client *client;
   ffidl_type *rtype;
   ffidl_value rvalue;
   void *ret;
   int argc;
   ffidl_type **atypes;
   ffidl_value *avalues;
   void **args;
   int use_raw_api;
   ffi_type **lib_atypes;
   ffi_cif lib_cif;
};

/*
 * The ffidl_callout contains a cif pointer,
 * a function address, the ffidl_client
 * which defined the callout, and a usage
 * string.
 */
struct ffidl_callout {
  ffidl_cif *cif;
  void (*fn)();
  ffidl_client *client;
  char usage[1];
};

#if USE_CALLBACKS
/*
 * The ffidl_closure contains a ffi_closure structure,
 * a Tcl_Interp pointer, and a pointer to the callback binding.
 */
struct ffidl_closure {
   ffi_closure lib_closure;
   Tcl_Interp *interp;
   ffidl_callback *callback;
};
/*
 * The ffidl_callback binds a ffidl_cif pointer to
 * a Tcl proc name, it defines the signature of the
 * c function call to the Tcl proc.
 */
struct ffidl_callback {
  ffidl_cif *cif;
  Tcl_Obj *proc;
  ffidl_closure closure;
};
#endif

/*****************************************
 *
 * Data defined in this file.
 * In addition to the version string above
 */

static Tcl_ObjType *ffidl_bytearray_ObjType;
static Tcl_ObjType *ffidl_int_ObjType;
#if HAVE_INT64
static Tcl_ObjType *ffidl_wideInt_ObjType;
#endif
static Tcl_ObjType *ffidl_double_ObjType;

/*
 * base types, the ffi base types and some additional bits.
 */
#define init_type(size,type,class,alignment,libtype) { size,type,class|FFIDL_STATIC_TYPE,alignment,0,0,libtype }

static ffidl_type ffidl_type_void = init_type(0, FFIDL_VOID, FFIDL_RET|FFIDL_CBRET, 0, lib_type_void);
static ffidl_type ffidl_type_char = init_type(SIZEOF_CHAR, FFIDL_CHAR, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_CHAR, lib_type_char);
static ffidl_type ffidl_type_schar = init_type(SIZEOF_CHAR, FFIDL_SCHAR, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_CHAR, lib_type_schar);
static ffidl_type ffidl_type_uchar = init_type(SIZEOF_CHAR, FFIDL_UCHAR, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_CHAR, lib_type_uchar);
static ffidl_type ffidl_type_sshort = init_type(SIZEOF_SHORT, FFIDL_SSHORT, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_SHORT, lib_type_sshort);
static ffidl_type ffidl_type_ushort = init_type(SIZEOF_SHORT, FFIDL_USHORT, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_SHORT, lib_type_ushort);
static ffidl_type ffidl_type_sint = init_type(SIZEOF_INT, FFIDL_SINT, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_INT, lib_type_sint);
static ffidl_type ffidl_type_uint = init_type(SIZEOF_INT, FFIDL_UINT, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_INT, lib_type_uint);
#if SIZEOF_LONG == 8
static ffidl_type ffidl_type_slong = init_type(SIZEOF_LONG, FFIDL_SLONG, FFIDL_ALL|FFIDL_GETWIDEINT, ALIGNOF_LONG, lib_type_slong);
static ffidl_type ffidl_type_ulong = init_type(SIZEOF_LONG, FFIDL_ULONG, FFIDL_ALL|FFIDL_GETWIDEINT, ALIGNOF_LONG, lib_type_ulong);
#else
static ffidl_type ffidl_type_slong = init_type(SIZEOF_LONG, FFIDL_SLONG, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_LONG, lib_type_slong);
static ffidl_type ffidl_type_ulong = init_type(SIZEOF_LONG, FFIDL_ULONG, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_LONG, lib_type_ulong);
#endif
#if HAVE_LONG_LONG
static ffidl_type ffidl_type_slonglong = init_type(SIZEOF_LONG_LONG, FFIDL_SLONGLONG, FFIDL_ALL|FFIDL_GETWIDEINT, ALIGNOF_LONG_LONG, lib_type_slonglong);
static ffidl_type ffidl_type_ulonglong = init_type(SIZEOF_LONG_LONG, FFIDL_ULONGLONG, FFIDL_ALL|FFIDL_GETWIDEINT, ALIGNOF_LONG_LONG, lib_type_ulonglong );
#endif
static ffidl_type ffidl_type_float = init_type(SIZEOF_FLOAT, FFIDL_FLOAT, FFIDL_ALL|FFIDL_GETDOUBLE, ALIGNOF_FLOAT, lib_type_float);
static ffidl_type ffidl_type_double = init_type(SIZEOF_DOUBLE, FFIDL_DOUBLE, FFIDL_ALL|FFIDL_GETDOUBLE, ALIGNOF_DOUBLE, lib_type_double);
#if HAVE_LONG_DOUBLE
static ffidl_type ffidl_type_longdouble = init_type(SIZEOF_LONG_DOUBLE, FFIDL_LONGDOUBLE, FFIDL_ALL|FFIDL_GETDOUBLE, ALIGNOF_LONG_DOUBLE, lib_type_longdouble );
#endif
static ffidl_type ffidl_type_sint8 = init_type(1, FFIDL_SINT8, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_INT8, lib_type_sint8);
static ffidl_type ffidl_type_uint8 = init_type(1, FFIDL_UINT8, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_INT8, lib_type_uint8);
static ffidl_type ffidl_type_sint16 = init_type(2, FFIDL_SINT16, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_INT16, lib_type_sint16);
static ffidl_type ffidl_type_uint16 = init_type(2, FFIDL_UINT16, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_INT16, lib_type_uint16);
static ffidl_type ffidl_type_sint32 = init_type(4, FFIDL_SINT32, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_INT32, lib_type_sint32);
static ffidl_type ffidl_type_uint32 = init_type(4, FFIDL_UINT32, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_INT32, lib_type_uint32);
#if HAVE_INT64
static ffidl_type ffidl_type_sint64 = init_type(8, FFIDL_SINT64, FFIDL_ALL|FFIDL_GETWIDEINT, ALIGNOF_INT64, lib_type_sint64);
static ffidl_type ffidl_type_uint64 = init_type(8, FFIDL_UINT64, FFIDL_ALL|FFIDL_GETWIDEINT, ALIGNOF_INT64, lib_type_uint64);
#endif
static ffidl_type ffidl_type_pointer = init_type(SIZEOF_VOID_P, FFIDL_PTR, FFIDL_ALL|FFIDL_GETINT, ALIGNOF_VOID_P, lib_type_pointer);
static ffidl_type ffidl_type_pointer_obj = init_type(SIZEOF_VOID_P, FFIDL_PTR_OBJ, FFIDL_ARGRET|FFIDL_CBARG|FFIDL_CBRET, ALIGNOF_VOID_P, lib_type_pointer);
static ffidl_type ffidl_type_pointer_utf8 = init_type(SIZEOF_VOID_P, FFIDL_PTR_UTF8, FFIDL_ARGRET|FFIDL_CBARG, ALIGNOF_VOID_P, lib_type_pointer);
static ffidl_type ffidl_type_pointer_utf16 = init_type(SIZEOF_VOID_P, FFIDL_PTR_UTF16, FFIDL_ARGRET|FFIDL_CBARG, ALIGNOF_VOID_P, lib_type_pointer);
static ffidl_type ffidl_type_pointer_byte = init_type(SIZEOF_VOID_P, FFIDL_PTR_BYTE, FFIDL_ARG, ALIGNOF_VOID_P, lib_type_pointer);
static ffidl_type ffidl_type_pointer_var = init_type(SIZEOF_VOID_P, FFIDL_PTR_VAR, FFIDL_ARG, ALIGNOF_VOID_P, lib_type_pointer);
#if USE_CALLBACKS
static ffidl_type ffidl_type_pointer_proc = init_type(SIZEOF_VOID_P, FFIDL_PTR_PROC, FFIDL_ARG, ALIGNOF_VOID_P, lib_type_pointer);
#endif

/*****************************************
 *
 * Functions defined in this file.
 */
/*
 * hash table management
 */
/* define a hashtable entry */
static void entry_define(Tcl_HashTable *table, char *name, void *datum)
{
  int dummy;
  Tcl_SetHashValue(Tcl_CreateHashEntry(table,name,&dummy), datum);
}
/* lookup an existing entry */
static void *entry_lookup(Tcl_HashTable *table, char *name)
{
  Tcl_HashEntry *entry = Tcl_FindHashEntry(table,name);
  return entry ? Tcl_GetHashValue(entry) : NULL;
}
/* find an entry by it's hash value */
static Tcl_HashEntry *entry_find(Tcl_HashTable *table, void *datum)
{
  Tcl_HashSearch search;
  Tcl_HashEntry *entry = Tcl_FirstHashEntry(table, &search);
  while (entry != NULL) {
    if (Tcl_GetHashValue(entry) == datum)
      return entry;
    entry = Tcl_NextHashEntry(&search);
  }
  return NULL;
}
/*
 * type management
 */
/* define a new type */
static void type_define(ffidl_client *client, char *tname, ffidl_type *ttype)
{
  entry_define(&client->types,tname,(void*)ttype);
}
/* lookup an existing type */
static ffidl_type *type_lookup(ffidl_client *client, char *tname)
{
  return entry_lookup(&client->types,tname);
}
/* find a type by it's ffidl_type */
/*
static Tcl_HashEntry *type_find(ffidl_client *client, ffidl_type *type)
{
  return entry_find(&client->types,(void *)type);
}
*/
/* parse an argument or return type specification */
static int type_parse(Tcl_Interp *interp, ffidl_client *client, unsigned context, Tcl_Obj *obj,
		      ffidl_type **type1, ffidl_value *type2, void **argp)
{
  char *arg = Tcl_GetString(obj);
  char buff[128];

  /* lookup the type */
  *type1 = type_lookup(client, arg);
  if (*type1 == NULL) {
    Tcl_AppendResult(interp, "no type defined for: ", arg, NULL);
    return TCL_ERROR;
  }
  /* test the context */
  if ((context & (*type1)->class) == 0) {
    Tcl_AppendResult(interp, "type ", arg, " is not permitted in ",
		     (context&FFIDL_ARG) ? "argument" :  "return",
		     " context.", NULL);
    return TCL_ERROR;
  }
  /* set arg value pointer */
  switch ((*type1)->typecode) {
  case FFIDL_VOID:		*argp = NULL; break; /* libffi depends on this being NULL on some platforms ! */
  case FFIDL_INT:		*argp = (void *)&type2->v_int; break;
  case FFIDL_FLOAT:		*argp = (void *)&type2->v_float; break;
  case FFIDL_DOUBLE:		*argp = (void *)&type2->v_double; break;
#if HAVE_LONG_DOUBLE
  case FFIDL_LONGDOUBLE:	*argp = (void *)&type2->v_longdouble; break;
#endif
  case FFIDL_UINT8:		*argp = (void *)&type2->v_uint8; break;
  case FFIDL_SINT8:		*argp = (void *)&type2->v_sint8; break;
  case FFIDL_UINT16:		*argp = (void *)&type2->v_uint16; break;
  case FFIDL_SINT16:		*argp = (void *)&type2->v_sint16; break;
  case FFIDL_UINT32:		*argp = (void *)&type2->v_uint32; break;
  case FFIDL_SINT32:		*argp = (void *)&type2->v_sint32; break;
#if HAVE_INT64
  case FFIDL_UINT64:		*argp = (void *)&type2->v_uint64; break;
  case FFIDL_SINT64:		*argp = (void *)&type2->v_sint64; break;
#endif
  case FFIDL_PTR:		*argp = (void *)&type2->v_pointer; break;
  case FFIDL_PTR_BYTE:		*argp = (void *)&type2->v_pointer; break;
  case FFIDL_PTR_OBJ:		*argp = (void *)&type2->v_pointer; break;
  case FFIDL_PTR_UTF8:		*argp = (void *)&type2->v_pointer; break;
  case FFIDL_PTR_UTF16:		*argp = (void *)&type2->v_pointer; break;
  case FFIDL_PTR_VAR:		*argp = (void *)&type2->v_pointer; break;
  case FFIDL_PTR_PROC:		*argp = (void *)&type2->v_pointer; break;
  case FFIDL_STRUCT:		*argp = (void *)&type2->v_struct; break;
  default:
    sprintf(buff, "unknown ffidl_type.t = %d", (*type1)->typecode);
    Tcl_AppendResult(interp, buff, NULL);
    return TCL_ERROR;
  }
  return TCL_OK;
}

/* Determine correct binary formats */
#if defined WORDS_BIGENDIAN
#define FFIDL_WIDEINT_FORMAT	"W"
#define FFIDL_INT_FORMAT	"I"
#define FFIDL_SHORT_FORMAT	"S"
#else
#define FFIDL_WIDEINT_FORMAT	"w"
#define FFIDL_INT_FORMAT	"i"
#define FFIDL_SHORT_FORMAT	"s"
#endif

/* build a binary format string */
static int type_format(Tcl_Interp *interp, ffidl_type *type, int *offset)
{
  int i;
  char buff[128];
  /* Insert alignment padding */
  while ((*offset % type->alignment) != 0) {
    Tcl_AppendResult(interp, "x", NULL);
    *offset += 1;
  }
  switch (type->typecode) {
  case FFIDL_INT:
  case FFIDL_UINT8:
  case FFIDL_SINT8:
  case FFIDL_UINT16:
  case FFIDL_SINT16:
  case FFIDL_UINT32:
  case FFIDL_SINT32:
#if HAVE_INT64
  case FFIDL_UINT64:
  case FFIDL_SINT64:
#endif
  case FFIDL_PTR:
  case FFIDL_PTR_BYTE:
  case FFIDL_PTR_OBJ:
  case FFIDL_PTR_UTF8:
  case FFIDL_PTR_UTF16:
  case FFIDL_PTR_VAR:
  case FFIDL_PTR_PROC:
    if (type->size == sizeof(Tcl_WideInt)) {
      *offset += 8;
      Tcl_AppendResult(interp, FFIDL_WIDEINT_FORMAT, NULL);
      return TCL_OK;
    } else if (type->size == sizeof(int)) {
      *offset += 4;
      Tcl_AppendResult(interp, FFIDL_INT_FORMAT, NULL);
      return TCL_OK;
    } else if (type->size == sizeof(short)) {
      *offset += 2;
      Tcl_AppendResult(interp, FFIDL_SHORT_FORMAT, NULL);
      return TCL_OK;
    } else if (type->size == sizeof(char)) {
      *offset += 1;
      Tcl_AppendResult(interp, "c", NULL);
      return TCL_OK;
    } else {
      *offset += type->size;
      sprintf(buff, "c%lu", (long)(type->size));
      Tcl_AppendResult(interp, buff, NULL);
      return TCL_OK;
    }
  case FFIDL_FLOAT:
  case FFIDL_DOUBLE:
#if HAVE_LONG_DOUBLE
  case FFIDL_LONGDOUBLE:
#endif
    if (type->size == sizeof(double)) {
      *offset += 8;
      Tcl_AppendResult(interp, "d", NULL);
      return TCL_OK;
    } else if (type->size == sizeof(float)) {
      *offset += 4;
      Tcl_AppendResult(interp, "f", NULL);
      return TCL_OK;
    } else {
      *offset += type->size;
      sprintf(buff, "c%lu", (long)(type->size));
      Tcl_AppendResult(interp, buff, NULL);
      return TCL_OK;
    }
  case FFIDL_STRUCT:
    for (i = 0; i < type->nelts; i += 1)
      if (type_format(interp, type->elements[i], offset) != TCL_OK)
	return TCL_ERROR;
    /* Insert tail padding */
    while (*offset < type->size) {
      Tcl_AppendResult(interp, "x", NULL);
      *offset += 1;
    }
    return TCL_OK;
  default:
    sprintf(buff, "cannot format ffidl_type: %d", type->typecode);
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, buff, NULL);
    return TCL_ERROR;
  }
}
static ffidl_type *type_alloc(ffidl_client *client, int nelts)
{
  ffidl_type *newtype;
  newtype = (ffidl_type *)Tcl_Alloc(sizeof(ffidl_type)
				  +nelts*sizeof(ffidl_type*)
				  +sizeof(ffi_type)+(nelts+1)*sizeof(ffi_type *)
				  );
  if (newtype == NULL) {
    return NULL;
  }
  /* initialize aggregate type */
  newtype->size = 0;
  newtype->typecode = FFIDL_STRUCT;
  newtype->class = FFIDL_ALL;
  newtype->alignment = 0;
  newtype->nelts = nelts;
  newtype->elements = (ffidl_type **)(newtype+1);
  newtype->lib_type = (ffi_type *)(newtype->elements+nelts);
  newtype->lib_type->size = 0;
  newtype->lib_type->alignment = 0;
  newtype->lib_type->type = FFI_TYPE_STRUCT;
  newtype->lib_type->elements = (ffi_type **)(newtype->lib_type+1);
  return newtype;
}
/* free a type */
static void type_free(ffidl_type *type)
{
  Tcl_Free((void *)type);
}
/* prep a type for use by the library */
static int type_prep(ffidl_type *type)
{
  ffi_cif cif;
  int i;
  for (i = 0; i < type->nelts; i += 1)
    type->lib_type->elements[i] = type->elements[i]->lib_type;
  type->lib_type->elements[i] = NULL;
  /* try out new type in a temporary cif, which should set size and alignment */
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 0, type->lib_type, NULL) != FFI_OK)
    return TCL_ERROR;
  if (type->size != type->lib_type->size) {
    fprintf(stderr, "ffidl disagrees with libffi about aggregate size of type %hu! %lu != %lu\n", type->typecode, (long)(type->size), (long)(type->lib_type->size));
  }
  if (type->alignment != type->lib_type->alignment) {
    fprintf(stderr, "ffidl disagrees with libffi about aggregate alignment of type  %hu! %hu != %hu\n", type->typecode, type->alignment, type->lib_type->alignment);
  }
  return TCL_OK;
}
/*
 * cif, ie call signature, management.
 */
/* define a new cif */
static void cif_define(ffidl_client *client, char *cname, ffidl_cif *cif)
{
  entry_define(&client->cifs,cname,(void*)cif);
}
/* lookup an existing cif */
static ffidl_cif *cif_lookup(ffidl_client *client, char *cname)
{
  return entry_lookup(&client->cifs,cname);
}
/* find a cif by it's ffidl_cif */
static Tcl_HashEntry *cif_find(ffidl_client *client, ffidl_cif *cif)
{
  return entry_find(&client->cifs,(void *)cif);
}
/* allocate a cif and its parts */
static ffidl_cif *cif_alloc(ffidl_client *client, int argc)
{
  /* allocate storage for:
     the ffidl_cif,
     the argument ffi_type pointers,
     the argument ffidl_types,
     the argument values,
     and the argument value pointers.
  */
  ffidl_cif *cif;
  cif = (ffidl_cif *)Tcl_Alloc(sizeof(ffidl_cif)
			     +argc*sizeof(ffidl_type*)
			     +argc*sizeof(ffidl_value)
			     +argc*sizeof(void*)
			     +argc*sizeof(ffi_type*)
			     );
  if (cif == NULL) {
    return NULL;
  }
  /* initialize the cif */
  cif->refs = 0;
  cif->client = client;
  cif->argc = argc;
  cif->atypes = (ffidl_type **)(cif+1);
  cif->avalues = (ffidl_value *)(cif->atypes+argc);
  cif->args = (void **)(cif->avalues+argc);
  cif->lib_atypes = (ffi_type **)(cif->args+argc);
  return cif;
}
/* free a cif */
void cif_free(ffidl_cif *cif)
{
  Tcl_Free((void *)cif);
}
/* maintain reference counts on cif's */
static void cif_inc_ref(ffidl_cif *cif)
{
  cif->refs += 1;
}
static void cif_dec_ref(ffidl_cif *cif)
{
  if (--cif->refs == 0) {
    Tcl_DeleteHashEntry(cif_find(cif->client, cif));
    cif_free(cif);
  }
}
/* do any library dependent prep for this cif */
static int cif_prep(ffidl_cif *cif, int protocol)
{
  ffi_type *rtype;
  int i;
  cif->use_raw_api = 0;
  rtype = cif->rtype->lib_type;
#if FFI_NATIVE_RAW_API
  cif->use_raw_api = 1;
  if (cif->rtype->typecode == FFIDL_STRUCT)
    cif->use_raw_api = 0;
#endif
  for (i = 0; i < cif->argc; i += 1) {
    cif->lib_atypes[i] = cif->atypes[i]->lib_type;
#if FFI_NATIVE_RAW_API
    if (cif->atypes[i]->typecode == FFIDL_STRUCT
	|| cif->atypes[i]->typecode == FFIDL_UINT64
	|| cif->atypes[i]->typecode == FFIDL_SINT64)
      cif->use_raw_api = 0;
#endif
  }
  if (ffi_prep_cif(&cif->lib_cif, protocol, cif->argc, rtype, cif->lib_atypes) != FFI_OK) {
    return TCL_ERROR;
  }
#if FFI_NATIVE_RAW_API
  if (cif->use_raw_api) {
    /* rewrite cif->args[i] into a stack image */
    int offset = 0, bytes = ffi_raw_size(&cif->lib_cif);
    /* fprintf(stderr, "using raw api for %d args\n", cif->argc); */
    for (i = 0; i < cif->argc; i += 1) {
      /* set args[i] to args[0]+offset */
      /* fprintf(stderr, "  arg[%d] was %08x ...", i, cif->args[i]); */
      cif->args[i] = (void *)(((char *)cif->args[0])+offset);
      /* fprintf(stderr, " becomes %08x\n", cif->args[i]); */
      /* increment offset */
      offset += cif->atypes[i]->size;
      /* align offset, so total bytes is correct */
      if (offset & (FFI_SIZEOF_ARG-1))
	offset = (offset|(FFI_SIZEOF_ARG-1))+1;
    }
    /* fprintf(stderr, "  final offset %d, bytes %d\n", offset, bytes); */
    if (offset != bytes) {
      fprintf(stderr, "ffidl and libffi disagree about bytes of argument! %d != %d\n", offset, bytes);
    }
  }
#endif
  return TCL_OK;
}
/* find the protocol, ie abi, for this cif */
static int cif_protocol(Tcl_Interp *interp, Tcl_Obj *obj, int *protocolp, char **protocolnamep)
{
  *protocolp = FFI_DEFAULT_ABI;
  *protocolnamep = NULL;
  if (obj != NULL) {
    *protocolnamep = Tcl_GetString(obj);
    if (*protocolp == FFI_DEFAULT_ABI)
      *protocolnamep = NULL;
  }
  return TCL_OK;
}
/*
 * parse a cif argument list, return type, and protocol,
 * and find or create it in the cif table.
 */
static int cif_parse(Tcl_Interp *interp, ffidl_client *client, Tcl_Obj *args, Tcl_Obj *ret, Tcl_Obj *pro, ffidl_cif **cifp, int callbackp)
{
  int argc, protocol, i;
  Tcl_Obj **argv;
  char *protocolname;
  Tcl_DString signature;
  ffidl_cif *cif;
  /* fetch argument types */
  if (Tcl_ListObjGetElements(interp, args, &argc, &argv) == TCL_ERROR) return TCL_ERROR;
  /* fetch protocol */
  if (cif_protocol(interp, pro, &protocol, &protocolname) == TCL_ERROR) return TCL_ERROR;
  /* build the cif signature key */
  Tcl_DStringInit(&signature);
  if (protocolname != NULL) {
    Tcl_DStringAppend(&signature, protocolname, -1);
    Tcl_DStringAppend(&signature, " ", 1);
  }
  Tcl_DStringAppend(&signature, Tcl_GetString(ret), -1);
  Tcl_DStringAppend(&signature, "(", 1);
  for (i = 0; i < argc; i += 1) {
    if (i != 0) Tcl_DStringAppend(&signature, ",", 1);
    Tcl_DStringAppend(&signature, Tcl_GetString(argv[i]), -1);
  }
  Tcl_DStringAppend(&signature, ")", 1);
  /* lookup the signature in the cif hash */
  cif = cif_lookup(client, Tcl_DStringValue(&signature));
  if (cif == NULL) {
    cif = cif_alloc(client, argc);
    if (cif == NULL) {
      Tcl_AppendResult(interp, "couldn't allocate the ffidl_cif", NULL); 
      Tcl_DStringFree(&signature);
      return TCL_ERROR;
    }
    /* parse return value spec */
    if (type_parse(interp, client, callbackp ? FFIDL_CBRET : FFIDL_RET, ret,
		   &cif->rtype, &cif->rvalue, &cif->ret) == TCL_ERROR) {
      cif_free(cif);
      Tcl_DStringFree(&signature);
      return TCL_ERROR;
    }
    /* parse arg specs */
    for (i = 0; i < argc; i += 1)
      if (type_parse(interp, client, callbackp ? FFIDL_CBARG : FFIDL_ARG, argv[i],
		     &cif->atypes[i], &cif->avalues[i], &cif->args[i]) == TCL_ERROR) {
	cif_free(cif);
	Tcl_DStringFree(&signature);
	return TCL_ERROR;
      }
    /* see if we done right */
    if (cif_prep(cif, protocol) != TCL_OK) {
      Tcl_AppendResult(interp, "type definition error", NULL);
      cif_free(cif);
      Tcl_DStringFree(&signature);
      return TCL_ERROR;
    }
    /* define the cif */
    cif_define(client, Tcl_DStringValue(&signature), cif);
    Tcl_ResetResult(interp);
  }
  /* free the signature string */
  Tcl_DStringFree(&signature);
  /* mark the cif as referenced */
  cif_inc_ref(cif);
  /* return success */
  *cifp = cif;
  return TCL_OK;
}
/*
 * callout management
 */
/* define a new callout */
static void callout_define(ffidl_client *client, char *pname, ffidl_callout *callout)
{
  entry_define(&client->callouts,pname,(void*)callout);
}
/* lookup an existing callout */
static ffidl_callout *callout_lookup(ffidl_client *client, char *pname)
{
  return entry_lookup(&client->callouts,pname);
}
/* find a callout by it's ffidl_callout */
static Tcl_HashEntry *callout_find(ffidl_client *client, ffidl_callout *callout)
{
  return entry_find(&client->callouts,(void *)callout);
}
/* cleanup on ffidl_callout_call deletion */
static void callout_delete(ClientData clientData)
{
  ffidl_callout *callout = (ffidl_callout *)clientData;
  Tcl_HashEntry *entry = callout_find(callout->client, callout);
  if (entry) {
    cif_dec_ref(callout->cif);
    Tcl_Free((void *)callout);
    Tcl_DeleteHashEntry(entry);
  }
}
/* make a call */
/* consider what happens if we reenter using the same cif */  
static void callout_call(ffidl_callout *callout)
{
  ffidl_cif *cif = callout->cif;
#if FFI_NATIVE_RAW_API
  if (cif->use_raw_api)
    ffi_raw_call(&cif->lib_cif, callout->fn, cif->ret, (ffi_raw *)cif->args[0]);
  else
    ffi_call(&cif->lib_cif, callout->fn, cif->ret, cif->args);
#else
  ffi_call(&cif->lib_cif, callout->fn, cif->ret, cif->args);
#endif
}
/*
 * lib management, but note we never free a lib
 * because we cannot know how often it is used.
 */
/* define a new lib */
static void lib_define(ffidl_client *client, char *lname, void *handle, void* unload)
{
  void** libentry = (void**)Tcl_Alloc(2*sizeof(void*));
  libentry[0] = handle; libentry[1] = unload; 
  entry_define(&client->libs,lname,libentry);
}
/* lookup an existing type */
static void *lib_lookup(ffidl_client *client, char *lname, void** unload)
{
  void** libentry = entry_lookup(&client->libs,lname);
  if (libentry) {
      if (unload) *unload = libentry[1];
      return libentry[0];
  } else {
      return NULL;
  }
}
#if USE_CALLBACKS
/*
 * callback management
 */
/* define a new callback */
static void callback_define(ffidl_client *client, char *cname, ffidl_callback *callback)
{
  entry_define(&client->callbacks,cname,(void*)callback);
}
/* lookup an existing callback */
static ffidl_callback *callback_lookup(ffidl_client *client, char *cname)
{
  return entry_lookup(&client->callbacks,cname);
}
/* find a callback by it's ffidl_callback */
/*
static Tcl_HashEntry *callback_find(ffidl_client *client, ffidl_callback *callback)
{
  return entry_find(&client->callbacks,(void *)callback);
}
*/
/* delete a callback definition */
/*
static void callback_delete(ffidl_client *client, ffidl_callback *callback)
{
  Tcl_HashEntry *entry = callback_find(client, callback);
  if (entry) {
    cif_dec_ref(callback->cif);
    Tcl_DecrRefCount(callback->proc);
    Tcl_Free((void *)callback);
    Tcl_DeleteHashEntry(entry);
  }
}
*/
/* call a tcl proc from a libffi closure */
static void callback_callback(ffi_cif *fficif, void *ret, void **args, void *user_data)
{
  ffidl_closure *closure = (ffidl_closure *)user_data;
  ffidl_callback *callback = closure->callback;
  Tcl_Interp *interp = closure->interp;
  ffidl_cif *cif = callback->cif;
  Tcl_Obj **objv, *obj, *list;
  char buff[128];
  int i, status;
  long ltmp;
  double dtmp;
#if HAVE_INT64
  Tcl_WideInt wtmp;
#endif
  /* test for valid scope */
  if (interp == NULL) {
    Tcl_Panic("callback called out of scope!\n");
  }
  /* initialize command list */
  list = Tcl_NewListObj(1, &callback->proc);
  Tcl_IncrRefCount(list);
  /* fetch and convert argument values */
  for (i = 0; i < cif->argc; i += 1) {
    void *argp;
#if FFI_NATIVE_RAW_API
    if (cif->use_raw_api) {
      int offset = ((int)cif->args[i])-((int)cif->args[0]);
      argp = (void *)(((char *)args)+offset);
    } else {
      argp = args[i];
    }
#else
    argp = args[i];
#endif
    switch (cif->atypes[i]->typecode) {
    case FFIDL_INT:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewLongObj((long)(*(int *)argp)));
      continue;
    case FFIDL_FLOAT:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewDoubleObj((double)(*(float *)argp)));
      continue;
    case FFIDL_DOUBLE:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewDoubleObj(*(double *)argp));
      continue;
#if HAVE_LONG_DOUBLE
    case FFIDL_LONGDOUBLE:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewDoubleObj((double)(*(long double *)argp)));
      continue;
#endif
    case FFIDL_UINT8:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewLongObj((long)(*(UINT8_T *)argp)));
      continue;
    case FFIDL_SINT8:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewLongObj((long)(*(SINT8_T *)argp)));
      continue;
    case FFIDL_UINT16:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewLongObj((long)(*(UINT16_T *)argp)));
      continue;
    case FFIDL_SINT16:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewLongObj((long)(*(SINT16_T *)argp)));
      continue;
    case FFIDL_UINT32:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewLongObj((long)(*(UINT32_T *)argp)));
      continue;
    case FFIDL_SINT32:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewLongObj((long)(*(SINT32_T *)argp)));
      continue;
#if HAVE_INT64
    case FFIDL_UINT64:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewWideIntObj((Tcl_WideInt)(*(UINT64_T *)argp)));
      continue;
    case FFIDL_SINT64:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewWideIntObj((Tcl_WideInt)(*(SINT64_T *)argp)));
      continue;
#endif
    case FFIDL_STRUCT:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewByteArrayObj((unsigned char *)argp, cif->atypes[i]->size));
      continue;
    case FFIDL_PTR:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewLongObj((long)(*(void **)argp)));
      continue;
    case FFIDL_PTR_OBJ:
      Tcl_ListObjAppendElement(interp, list, *(Tcl_Obj **)argp);
      continue;
    case FFIDL_PTR_UTF8:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewStringObj(*(char **)argp, -1));
      continue;
    case FFIDL_PTR_UTF16:
      Tcl_ListObjAppendElement(interp, list, Tcl_NewUnicodeObj(*(Tcl_UniChar **)argp, -1));
      continue;
    default:
      sprintf(buff, "unimplemented type for callback argument: %d", cif->atypes[i]->typecode);
      Tcl_AppendResult(interp, buff, NULL);
      Tcl_DecrRefCount(list);
      goto escape;
      continue;
    }
  }
  /* get command */
  Tcl_ListObjGetElements(interp, list, &i, &objv);
  /* call */
  status = Tcl_EvalObjv(interp, cif->argc+1, objv, TCL_EVAL_GLOBAL);
  /* clean up arguments */
  Tcl_DecrRefCount(list);
  if (status == TCL_ERROR) {
    goto escape;
  }
  /* fetch return value */
  obj = Tcl_GetObjResult(interp);
  if (cif->rtype->class & FFIDL_GETINT) {
    if (ffidl_double_ObjType && obj->typePtr == ffidl_double_ObjType) {
      if (Tcl_GetDoubleFromObj(interp, obj, &dtmp) == TCL_ERROR) {
	Tcl_AppendResult(interp, ", converting callback return value", NULL);
	goto escape;
      }
      ltmp = (long)dtmp;
      if (dtmp != ltmp)
	if (Tcl_GetLongFromObj(interp, obj, &ltmp) == TCL_ERROR) {
	  Tcl_AppendResult(interp, ", converting callback return value", NULL);
	  goto escape;
	}
    } else if (Tcl_GetLongFromObj(interp, obj, &ltmp) == TCL_ERROR) {
      Tcl_AppendResult(interp, ", converting callback return value", NULL);
      goto escape;
    }
#if HAVE_INT64
  } else if (cif->rtype->class & FFIDL_GETWIDEINT) {
    if (ffidl_double_ObjType && obj->typePtr == ffidl_double_ObjType) {
      if (Tcl_GetDoubleFromObj(interp, obj, &dtmp) == TCL_ERROR) {
	Tcl_AppendResult(interp, ", converting callback return value", NULL);
	goto escape;
      }
      wtmp = (Tcl_WideInt)dtmp;
      if (dtmp != wtmp)
	if (Tcl_GetWideIntFromObj(interp, obj, &wtmp) == TCL_ERROR) {
	  Tcl_AppendResult(interp, ", converting callback return value", NULL);
	  goto escape;
	}
    } else if (Tcl_GetWideIntFromObj(interp, obj, &wtmp) == TCL_ERROR) {
      Tcl_AppendResult(interp, ", converting callback return value", NULL);
      goto escape;
    }
#endif
  } else if (cif->rtype->class & FFIDL_GETDOUBLE) {
    if (ffidl_int_ObjType && obj->typePtr == ffidl_int_ObjType) {
      if (Tcl_GetLongFromObj(interp, obj, &ltmp) == TCL_ERROR) {
	Tcl_AppendResult(interp, ", converting callback return value", NULL);
	goto escape;
      }
      dtmp = (double)ltmp;
      if (dtmp != ltmp)
	if (Tcl_GetDoubleFromObj(interp, obj, &dtmp) == TCL_ERROR) {
	  Tcl_AppendResult(interp, ", converting callback return value", NULL);
	  goto escape;
	}
#if HAVE_INT64
    } else if (ffidl_wideInt_ObjType && obj->typePtr == ffidl_wideInt_ObjType) {
      if (Tcl_GetWideIntFromObj(interp, obj, &wtmp) == TCL_ERROR) {
	Tcl_AppendResult(interp, ", converting callback return value", NULL);
	goto escape;
      }
      dtmp = (double)wtmp;
      if (dtmp != wtmp)
	if (Tcl_GetDoubleFromObj(interp, obj, &dtmp) == TCL_ERROR) {
	  Tcl_AppendResult(interp, ", converting callback return value", NULL);
	  goto escape;
	}
#endif
    } else if (Tcl_GetDoubleFromObj(interp, obj, &dtmp) == TCL_ERROR) {
      Tcl_AppendResult(interp, ", converting callback return value", NULL);
      goto escape;
    }
  }
  
  /* convert return value */
  switch (cif->rtype->typecode) {
  case FFIDL_VOID:	break;
  case FFIDL_INT:	*(int *)ret = (int)ltmp; break;
  case FFIDL_FLOAT:	*(float *)ret = (float)dtmp; break;
  case FFIDL_DOUBLE:	*(double *)ret = dtmp; break;
#if HAVE_LONG_DOUBLE
  case FFIDL_LONGDOUBLE:*(long double *)ret = dtmp; break;
#endif
#ifdef POWERPC_DARWIN
  case FFIDL_UINT8:	*(UINT32_T *)ret = (UINT8_T)ltmp; break;
  case FFIDL_SINT8:	*(SINT32_T *)ret = (SINT8_T)ltmp; break;
  case FFIDL_UINT16:	*(UINT32_T *)ret = (UINT16_T)ltmp; break;
  case FFIDL_SINT16:	*(SINT32_T *)ret = (SINT16_T)ltmp; break;
#else
  case FFIDL_UINT8:	*(UINT8_T *)ret = (UINT8_T)ltmp; break;
  case FFIDL_SINT8:	*(SINT8_T *)ret = (SINT8_T)ltmp; break;
  case FFIDL_UINT16:	*(UINT16_T *)ret = (UINT16_T)ltmp; break;
  case FFIDL_SINT16:	*(SINT16_T *)ret = (SINT16_T)ltmp; break;
#endif
  case FFIDL_UINT32:	*(UINT32_T *)ret = (UINT32_T)ltmp; break;
  case FFIDL_SINT32:	*(SINT32_T *)ret = (SINT32_T)ltmp; break;
#if HAVE_INT64
  case FFIDL_UINT64:	*(UINT64_T *)ret = (UINT64_T)wtmp; break;
  case FFIDL_SINT64:	*(SINT64_T *)ret = (SINT64_T)wtmp; break;
#endif
  case FFIDL_STRUCT:
    {
      int len;
      void *bytes = Tcl_GetByteArrayFromObj(obj, &len);
      if (len != cif->rtype->size) {
	Tcl_ResetResult(interp);
	sprintf(buff, "byte array for callback struct return has %u bytes instead of %lu", len, (long)(cif->rtype->size));
	Tcl_AppendResult(interp, buff, NULL);
	goto escape;
      }
      memcpy(ret, bytes, cif->rtype->size);
      break;
    }
  case FFIDL_PTR:	*(void **)ret = (void *)ltmp; break;
  case FFIDL_PTR_OBJ:	*(Tcl_Obj **)ret = obj; break;
  default:
    Tcl_ResetResult(interp);
    sprintf(buff, "unimplemented type for callback return: %d", cif->rtype->typecode);
    Tcl_AppendResult(interp, buff, NULL);
    goto escape;
  }
  /* done */
  return;
escape:
  Tcl_BackgroundError(interp);
  memset(ret, 0, cif->rtype->size);
}
#endif
/*
 * Client management.
 */
/* client interp deletion callback for cleanup */
static void client_delete(ClientData clientData, Tcl_Interp *interp)
{
  ffidl_client *client = (ffidl_client *)clientData;
  Tcl_HashSearch search;
  Tcl_HashEntry *entry;

  /* there should be no callouts left */
  for (entry = Tcl_FirstHashEntry(&client->callouts, &search); entry != NULL; entry = Tcl_NextHashEntry(&search)) {
    char *name = Tcl_GetHashKey(&client->callouts, entry);
    /* Couldn't do this while traversing the hash table anyway */
    /* Tcl_DeleteCommand(interp, name); */
    fprintf(stderr, "error - dangling callout in client_delete: %s\n", name);
  }

#if USE_CALLBACKS
  /* free all callbacks */
  for (entry = Tcl_FirstHashEntry(&client->callbacks, &search); entry != NULL; entry = Tcl_NextHashEntry(&search)) {
    ffidl_callback *callback = Tcl_GetHashValue(entry);
    cif_dec_ref(callback->cif);
    Tcl_DecrRefCount(callback->proc);
    Tcl_Free((void *)callback);
  }
#endif

  /* there should be no cifs left */
  for (entry = Tcl_FirstHashEntry(&client->cifs, &search); entry != NULL; entry = Tcl_NextHashEntry(&search)) {
    char *signature = Tcl_GetHashKey(&client->cifs, entry);
    fprintf(stderr, "error - dangling ffidl_cif in client_delete: %s\n",signature);
  }

  /* free all allocated typedefs */
  for (entry = Tcl_FirstHashEntry(&client->types, &search); entry != NULL; entry = Tcl_NextHashEntry(&search)) {
    ffidl_type *type = Tcl_GetHashValue(entry);
    if ((type->class & FFIDL_STATIC_TYPE) == 0)
      type_free(type);
  }

  /* free all libs */
  for (entry = Tcl_FirstHashEntry(&client->libs, &search); entry != NULL; entry = Tcl_NextHashEntry(&search)) {
    void **libentry = Tcl_GetHashValue(entry);
    const char *error;
    ffidlclose(libentry[0], &error);
    Tcl_Free((char*)libentry);
  }

  /* free hashtables */
  Tcl_DeleteHashTable(&client->callouts);
#if USE_CALLBACKS
  Tcl_DeleteHashTable(&client->callbacks);
#endif
  Tcl_DeleteHashTable(&client->cifs);
  Tcl_DeleteHashTable(&client->types);
  Tcl_DeleteHashTable(&client->libs);

  /* free client structure */
  Tcl_Free((void *)client);
}
/* client allocation and initialization */
static ffidl_client *client_alloc(Tcl_Interp *interp)
{
  ffidl_client *client;

  /* allocate client data structure */
  client = (ffidl_client *)Tcl_Alloc(sizeof(ffidl_client));

  /* allocate hashtables for this load */
  Tcl_InitHashTable(&client->types, TCL_STRING_KEYS);
  Tcl_InitHashTable(&client->callouts, TCL_STRING_KEYS);
  Tcl_InitHashTable(&client->cifs, TCL_STRING_KEYS);
  Tcl_InitHashTable(&client->libs, TCL_STRING_KEYS);
#if USE_CALLBACKS
  Tcl_InitHashTable(&client->callbacks, TCL_STRING_KEYS);
#endif

  /* initialize types */
  type_define(client, "void", &ffidl_type_void);
  type_define(client, "char", &ffidl_type_char);
  type_define(client, "signed char", &ffidl_type_schar);
  type_define(client, "unsigned char", &ffidl_type_uchar);
  type_define(client, "short", &ffidl_type_sshort);
  type_define(client, "unsigned short", &ffidl_type_ushort);
  type_define(client, "int", &ffidl_type_sint);
  type_define(client, "unsigned", &ffidl_type_uint);
  type_define(client, "long", &ffidl_type_slong);
  type_define(client, "unsigned long", &ffidl_type_ulong);
#if HAVE_LONG_LONG
  type_define(client, "long long", &ffidl_type_slonglong);
  type_define(client, "unsigned long long", &ffidl_type_ulonglong);
#endif
  type_define(client, "float", &ffidl_type_float);
  type_define(client, "double", &ffidl_type_double);
#if HAVE_LONG_DOUBLE
  type_define(client, "long double", &ffidl_type_longdouble);
#endif
  type_define(client, "sint8", &ffidl_type_sint8);
  type_define(client, "uint8", &ffidl_type_uint8);
  type_define(client, "sint16", &ffidl_type_sint16);
  type_define(client, "uint16", &ffidl_type_uint16);
  type_define(client, "sint32", &ffidl_type_sint32);
  type_define(client, "uint32", &ffidl_type_uint32);
#if HAVE_INT64
  type_define(client, "sint64", &ffidl_type_sint64);
  type_define(client, "uint64", &ffidl_type_uint64);
#endif
  type_define(client, "pointer", &ffidl_type_pointer);
  type_define(client, "pointer-obj", &ffidl_type_pointer_obj);
  type_define(client, "pointer-utf8", &ffidl_type_pointer_utf8);
  type_define(client, "pointer-utf16", &ffidl_type_pointer_utf16);
  type_define(client, "pointer-byte", &ffidl_type_pointer_byte);
  type_define(client, "pointer-var", &ffidl_type_pointer_var);
#if USE_CALLBACKS
  type_define(client, "pointer-proc", &ffidl_type_pointer_proc);
#endif

  /* arrange for cleanup on interpreter deletion */
  Tcl_CallWhenDeleted(interp, client_delete, (ClientData)client);

  /* finis */
  return client;
}
/*****************************************
 *
 * Functions exported as tcl commands.
 */

/* usage: ::ffidl::info option ?...? */
static int tcl_ffidl_info(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int i;
  char *arg;
  Tcl_HashTable *table;
  Tcl_HashSearch search;
  Tcl_HashEntry *entry;
  ffidl_type *type;
  ffidl_client *client = (ffidl_client *)clientData;
  static const char *options[] = {
#define INFO_ALIGNOF 0
    "alignof",
#define INFO_CALLBACKS 1
    "callbacks",
#define INFO_CALLOUTS 2
    "callouts",
#define INFO_CANONICAL_HOST 3
    "canonical-host",
#define INFO_FORMAT 4
    "format",
#define INFO_HAVE_INT64 5
    "have-int64",
#define INFO_HAVE_LONG_DOUBLE 6
    "have-long-double",
#define INFO_HAVE_LONG_LONG 7
    "have-long-long",
#define INFO_INTERP 8
    "interp",
#define INFO_LIBRARIES 9
    "libraries",
#define INFO_SIGNATURES 10
    "signatures",
#define INFO_SIZEOF 11
    "sizeof",
#define INFO_TYPEDEFS 12
    "typedefs",
#define INFO_USE_CALLBACKS 13
    "use-callbacks",
#define INFO_USE_FFCALL 14
    "use-ffcall",
#define INFO_USE_LIBFFI 15
    "use-libffi",
#define INFO_USE_LIBFFI_RAW 16
    "use-libffi-raw",
    NULL
  };

  if (objc < 2) {
    Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
    return TCL_ERROR;
  }

  if (Tcl_GetIndexFromObj(interp, objv[1], options, "option", TCL_EXACT, &i) == TCL_ERROR)
    return TCL_ERROR;

  switch (i) {
  case INFO_CALLOUTS:		/* return list of callout names */
    table = &client->callouts;
  list_table_keys:		/* list the keys in a hash table */
    if (objc != 2) {
      Tcl_WrongNumArgs(interp,2,objv,"");
      return TCL_ERROR;
    }
    for (entry = Tcl_FirstHashEntry(table, &search); entry != NULL; entry = Tcl_NextHashEntry(&search))
      Tcl_ListObjAppendElement(interp, Tcl_GetObjResult(interp), Tcl_NewStringObj(Tcl_GetHashKey(table,entry),-1));
    return TCL_OK;
  case INFO_TYPEDEFS:		/* return list of typedef names */
    table = &client->types;
    goto list_table_keys;
  case INFO_SIGNATURES:		/* return list of ffi signatures */
    table = &client->cifs;
    goto list_table_keys;
  case INFO_LIBRARIES:		/* return list of lib names */
    table = &client->libs;
    goto list_table_keys;
  case INFO_CALLBACKS:		/* return list of callback names */
#if USE_CALLBACKS
    table = &client->callbacks;
    goto list_table_keys;
#else
    Tcl_AppendResult(interp, "callbacks are not supported in this configuration", NULL);
    return TCL_ERROR;
#endif

  case INFO_SIZEOF:		/* return sizeof type */
  case INFO_ALIGNOF:		/* return alignof type */
  case INFO_FORMAT:		/* return binary format of type */
    if (objc != 3) {
      Tcl_WrongNumArgs(interp,2,objv,"type");
      return TCL_ERROR;
    }
    arg = Tcl_GetString(objv[2]);
    type = type_lookup(client, arg);
    if (type == NULL) {
      Tcl_AppendResult(interp, "undefined type: ", arg, NULL);
      return TCL_ERROR;
    }
    if (i == INFO_SIZEOF) {
      Tcl_SetObjResult(interp, Tcl_NewIntObj(type->size));
      return TCL_OK;
    }
    if (i == INFO_ALIGNOF) {
      Tcl_SetObjResult(interp, Tcl_NewIntObj(type->alignment));
      return TCL_OK;
    }
    if (i == INFO_FORMAT) {
      i = 0;
      return type_format(interp, type, &i);
    }
    Tcl_AppendResult(interp, "lost in ::ffidl::info?", NULL);
    return TCL_ERROR;
  case INFO_INTERP:
    /* return the interp as integer */
    if (objc != 2) {
      Tcl_WrongNumArgs(interp,2,objv,"");
      return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, Tcl_NewLongObj((long)interp));
    return TCL_OK;
  case INFO_USE_FFCALL:
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
    return TCL_OK;
  case INFO_USE_LIBFFI:
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
    return TCL_OK;
  case INFO_USE_CALLBACKS:
#if USE_CALLBACKS
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
#else
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
#endif
    return TCL_OK;
  case INFO_HAVE_LONG_LONG:
#if HAVE_LONG_LONG
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
#else
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
#endif
    return TCL_OK;
  case INFO_HAVE_LONG_DOUBLE:
#if HAVE_LONG_DOUBLE
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
#else
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
#endif
    return TCL_OK;
  case INFO_USE_LIBFFI_RAW:
#if FFI_NATIVE_RAW_API
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
#else
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
#endif
    return TCL_OK;
  case INFO_HAVE_INT64:
#if HAVE_INT64
    Tcl_SetObjResult(interp, Tcl_NewIntObj(1));
#else
    Tcl_SetObjResult(interp, Tcl_NewIntObj(0));
#endif
    return TCL_OK;
  case INFO_CANONICAL_HOST:
    Tcl_SetObjResult(interp, Tcl_NewStringObj(CANONICAL_HOST,-1));
    return TCL_OK;
  }
  
  /* return an error */
  Tcl_AppendResult(interp, "missing option implementation: ", Tcl_GetString(objv[1]), NULL);
  return TCL_ERROR;
}

/* usage: ffidl-typedef name type1 ?type2 ...? */
static int tcl_ffidl_typedef(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  char *tname1, *tname2;
  ffidl_type *newtype, *ttype2;
  int nelts, i;
  ffidl_client *client = (ffidl_client *)clientData;
  /* check number of args */
  if (objc < 3) {
    Tcl_WrongNumArgs(interp,1,objv,"name type ?...?");
    return TCL_ERROR;
  }
  /* fetch new type name, verify that it is new */
  tname1 = Tcl_GetString(objv[1]);
  if (type_lookup(client, tname1) != NULL) {
    Tcl_AppendResult(interp, "type is already defined: ", tname1, NULL);
    return TCL_ERROR;
  }
  /* define tname1 as an alias for tname2 */
  if (objc == 3) {
    tname2 = Tcl_GetString(objv[2]);
    ttype2 = type_lookup(client, tname2);
    if (ttype2 == NULL) {
      Tcl_AppendResult(interp, "undefined type: ", tname2, NULL);
      return TCL_ERROR;
    }
    type_define(client, tname1, ttype2);
    return TCL_OK;
  }
  /* allocate an aggregate type */
  nelts = objc-2;
  newtype = type_alloc(client, nelts);
  if (newtype == NULL) {
    Tcl_AppendResult(interp, "couldn't allocate the ffi_type", NULL); 
    return TCL_ERROR;
  }
  /* parse aggregate types */
  newtype->size = 0;
  newtype->alignment = 0;
  for (i = 0; i < nelts; i += 1) {
    tname2 = Tcl_GetString(objv[2+i]);
    ttype2 = type_lookup(client, tname2);
    if (ttype2 == NULL) {
      type_free(newtype);
      Tcl_AppendResult(interp, "undefined element type: ", tname2, NULL);
      return TCL_ERROR;
    }
    if ((ttype2->class & FFIDL_ELT) == 0) {
      type_free(newtype);
      Tcl_AppendResult(interp, "type ", tname2, " is not permitted in element context", NULL);
      return TCL_ERROR;
    }
    newtype->elements[i] = ttype2;
    /* accumulate the aggregate size and alignment */
    /* align current size to element's alignment */
    if ((ttype2->alignment-1) & newtype->size)
      newtype->size = ((newtype->size-1) | (ttype2->alignment-1)) + 1;
    /* add the element's size */
    newtype->size += ttype2->size;
    /* bump the aggregate alignment as required */
    if (ttype2->alignment > newtype->alignment)
      newtype->alignment = ttype2->alignment;
  }
  newtype->size = ((newtype->size-1) | (newtype->alignment-1)) + 1; /* tail padding as in libffi */
  if (type_prep(newtype) != TCL_OK) {
    type_free(newtype);
    Tcl_AppendResult(interp, "type definition error", NULL);
    return TCL_ERROR;
  }
  /* define new type */
  type_define(client, tname1, newtype);
  /* return success */
  return TCL_OK;
}

/* usage: depends on the signature defining the ffidl-callout */
static int tcl_ffidl_call(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  ffidl_callout *callout = (ffidl_callout *)clientData;
  ffidl_cif *cif = callout->cif;
  int i, itmp;
  long ltmp;
  double dtmp;
#if HAVE_INT64
  Tcl_WideInt wtmp;
#endif
  Tcl_Obj *obj = NULL;
  char buff[128];
  /* usage check */
  if (objc-1 != cif->argc) {
    Tcl_WrongNumArgs(interp, 1, objv, callout->usage);
    return TCL_ERROR;
  }
  /* fetch and convert argument values */
  for (i = 0; i < cif->argc; i += 1) {
    /* fetch object */
    obj = objv[1+i];
    /* fetch value from object and store value into arg value array */
    if (cif->atypes[i]->class & FFIDL_GETINT) {
      if (ffidl_double_ObjType && obj->typePtr == ffidl_double_ObjType) {
	if (Tcl_GetDoubleFromObj(interp, obj, &dtmp) == TCL_ERROR)
	  goto cleanup;
	ltmp = (long)dtmp;
	if (dtmp != ltmp)
	  if (Tcl_GetLongFromObj(interp, obj, &ltmp) == TCL_ERROR)
	    goto cleanup;
      } else if (Tcl_GetLongFromObj(interp, obj, &ltmp) == TCL_ERROR)
	goto cleanup;
#if HAVE_INT64
    } else if (cif->atypes[i]->class & FFIDL_GETWIDEINT) {
      if (ffidl_double_ObjType && obj->typePtr == ffidl_double_ObjType) {
	if (Tcl_GetDoubleFromObj(interp, obj, &dtmp) == TCL_ERROR)
	  goto cleanup;
	wtmp = (Tcl_WideInt)dtmp;
	if (dtmp != wtmp)
	  if (Tcl_GetWideIntFromObj(interp, obj, &wtmp) == TCL_ERROR)
	    goto cleanup;
      } else if (Tcl_GetWideIntFromObj(interp, obj, &wtmp) == TCL_ERROR)
	goto cleanup;
#endif
    } else if (cif->atypes[i]->class & FFIDL_GETDOUBLE) {
      if (ffidl_int_ObjType && obj->typePtr == ffidl_int_ObjType) {
	if (Tcl_GetLongFromObj(interp, obj, &ltmp) == TCL_ERROR)
	  goto cleanup;
	dtmp = (double)ltmp;
	if (dtmp != ltmp)
	  if (Tcl_GetDoubleFromObj(interp, obj, &dtmp) == TCL_ERROR)
	    goto cleanup;
#if HAVE_INT64
      } else if (ffidl_wideInt_ObjType && obj->typePtr == ffidl_wideInt_ObjType) {
	if (Tcl_GetWideIntFromObj(interp, obj, &wtmp) == TCL_ERROR)
	  goto cleanup;
	dtmp = (double)wtmp;
	if (dtmp != wtmp)
	  if (Tcl_GetDoubleFromObj(interp, obj, &dtmp) == TCL_ERROR)
	    goto cleanup;
#endif
      } else if (Tcl_GetDoubleFromObj(interp, obj, &dtmp) == TCL_ERROR)
	goto cleanup;
    }
    switch (cif->atypes[i]->typecode) {
    case FFIDL_INT:
      *(int *)cif->args[i] = (int)ltmp;
      continue;
    case FFIDL_FLOAT:
      *(float *)cif->args[i] = (float)dtmp;
      continue;
    case FFIDL_DOUBLE:
      *(double *)cif->args[i] = (double)dtmp;
      continue;
#if HAVE_LONG_DOUBLE
    case FFIDL_LONGDOUBLE:
      *(long double *)cif->args[i] = (long double)dtmp;
      continue;
#endif
    case FFIDL_UINT8:
      *(UINT8_T *)cif->args[i] = (UINT8_T)ltmp;
      continue;
    case FFIDL_SINT8:
      *(SINT8_T *)cif->args[i] = (SINT8_T)ltmp;
      continue;
    case FFIDL_UINT16:
      *(UINT16_T *)cif->args[i] = (UINT16_T)ltmp;
      continue;
    case FFIDL_SINT16:
      *(SINT16_T *)cif->args[i] = (SINT16_T)ltmp;
      continue;
    case FFIDL_UINT32:
      *(UINT32_T *)cif->args[i] = (UINT32_T)ltmp;
      continue;
    case FFIDL_SINT32:
      *(SINT32_T *)cif->args[i] = (SINT32_T)ltmp;
      continue;
#if HAVE_INT64
    case FFIDL_UINT64:
      *(UINT64_T *)cif->args[i] = (UINT64_T)wtmp;
      continue;
    case FFIDL_SINT64:
      *(SINT64_T *)cif->args[i] = (SINT64_T)wtmp;
      continue;
#endif
    case FFIDL_STRUCT:
      if (ffidl_bytearray_ObjType && obj->typePtr != ffidl_bytearray_ObjType) {
	sprintf(buff, "parameter %d must be a binary string", i);
	Tcl_AppendResult(interp, buff, NULL);
	goto cleanup;
      }
      cif->args[i] = (void *)Tcl_GetByteArrayFromObj(obj, &itmp);
      if (itmp != cif->atypes[i]->size) {
	sprintf(buff, "parameter %d is the wrong size, %u bytes instead of %lu.", i, itmp, (long)(cif->atypes[i]->size));
	Tcl_AppendResult(interp, buff, NULL);
	goto cleanup;
      }
      continue;
    case FFIDL_PTR:
      *(void **)cif->args[i] = (void *)ltmp;
      continue;
    case FFIDL_PTR_OBJ:
      *(void **)cif->args[i] = (void *)obj;
      continue;
    case FFIDL_PTR_UTF8:
      *(void **)cif->args[i] = (void *)Tcl_GetString(obj);
      continue;
    case FFIDL_PTR_UTF16:
      *(void **)cif->args[i] = (void *)Tcl_GetUnicode(obj);
      continue;
    case FFIDL_PTR_BYTE:
      if (ffidl_bytearray_ObjType && obj->typePtr != ffidl_bytearray_ObjType) {
	sprintf(buff, "parameter %d must be a binary string", i);
	Tcl_AppendResult(interp, buff, NULL);
	goto cleanup;
      }
      *(void **)cif->args[i] = (void *)Tcl_GetByteArrayFromObj(obj, &itmp);
      continue;
    case FFIDL_PTR_VAR:
      obj = Tcl_ObjGetVar2(interp, objv[1+i], NULL, TCL_LEAVE_ERR_MSG);
      if (obj == NULL) return TCL_ERROR;
      if (ffidl_bytearray_ObjType && obj->typePtr != ffidl_bytearray_ObjType) {
	sprintf(buff, "parameter %d must be a binary string", i);
	Tcl_AppendResult(interp, buff, NULL);
	goto cleanup;
      }
      if (Tcl_IsShared(obj)) {
	obj = Tcl_ObjSetVar2(interp, objv[1+i], NULL, Tcl_DuplicateObj(obj), TCL_LEAVE_ERR_MSG);
	if (obj == NULL)
	  goto cleanup;
      }
      *(void **)cif->args[i] = (void *)Tcl_GetByteArrayFromObj(obj, &itmp);
      /* printf("pointer-var -> %d\n", cif->avalues[i].v_pointer); */
      Tcl_InvalidateStringRep(obj);
      continue;
#if USE_CALLBACKS
    case FFIDL_PTR_PROC: {
      ffidl_callback *callback;
      ffidl_closure *closure;
      Tcl_DString ds;
      char *name = Tcl_GetString(objv[1+i]);
      Tcl_DStringInit(&ds);
      if (!strstr(name, "::")) {
        Tcl_Namespace *ns;
        ns = Tcl_GetCurrentNamespace(interp);
        if (ns != Tcl_GetGlobalNamespace(interp)) {
          Tcl_DStringAppend(&ds, ns->fullName, -1);
        }
        Tcl_DStringAppend(&ds, "::", 2);
        Tcl_DStringAppend(&ds, name, -1);
        name = Tcl_DStringValue(&ds);
      }
      callback = callback_lookup(callout->client, name);
      Tcl_DStringFree(&ds);
      if (callback == NULL) {
	Tcl_AppendResult(interp, "no callback named \"", Tcl_GetString(objv[1+i]), "\" is defined", NULL);
	goto cleanup;
      }
      closure = &(callback->closure);
      *(void **)cif->args[i] = (void *)&closure->lib_closure;
    }
    continue;
#endif
    default:
      sprintf(buff, "unknown type for argument: %d", cif->atypes[i]->typecode);
      Tcl_AppendResult(interp, buff, NULL);
      goto cleanup;
    }
  }
  /* prepare for structure return */
  if (cif->rtype->typecode == FFIDL_STRUCT) {
    obj = Tcl_NewByteArrayObj((unsigned char*)"", cif->rtype->size);
    Tcl_IncrRefCount(obj);
    cif->ret = Tcl_GetByteArrayFromObj(obj, &itmp);
  }
  /* call */
  callout_call(callout);
  /* convert return value */
  switch (cif->rtype->typecode) {
  case FFIDL_VOID:	break;
  case FFIDL_INT:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_int)); break;
  case FFIDL_FLOAT:	Tcl_SetObjResult(interp, Tcl_NewDoubleObj((double)cif->rvalue.v_float)); break;
  case FFIDL_DOUBLE:	Tcl_SetObjResult(interp, Tcl_NewDoubleObj((double)cif->rvalue.v_double)); break;
#if HAVE_LONG_DOUBLE
  case FFIDL_LONGDOUBLE:Tcl_SetObjResult(interp, Tcl_NewDoubleObj((double)cif->rvalue.v_longdouble)); break;
#endif
#ifdef POWERPC_DARWIN
  case FFIDL_UINT8:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_uint32)); break;
  case FFIDL_SINT8:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_sint32)); break;
  case FFIDL_UINT16:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_uint32)); break;
  case FFIDL_SINT16:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_sint32)); break;
#else
  case FFIDL_UINT8:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_uint8)); break;
  case FFIDL_SINT8:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_sint8)); break;
  case FFIDL_UINT16:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_uint16)); break;
  case FFIDL_SINT16:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_sint16)); break;
#endif
  case FFIDL_UINT32:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_uint32)); break;
  case FFIDL_SINT32:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_sint32)); break;
#if HAVE_INT64
  case FFIDL_UINT64:	Tcl_SetObjResult(interp, Tcl_NewWideIntObj((Tcl_WideInt)cif->rvalue.v_uint64)); break;
  case FFIDL_SINT64:	Tcl_SetObjResult(interp, Tcl_NewWideIntObj((Tcl_WideInt)cif->rvalue.v_sint64)); break;
#endif
  case FFIDL_STRUCT:	Tcl_SetObjResult(interp, obj); Tcl_DecrRefCount(obj); break;
  case FFIDL_PTR:	Tcl_SetObjResult(interp, Tcl_NewLongObj((long)cif->rvalue.v_pointer)); break;
  case FFIDL_PTR_OBJ:	Tcl_SetObjResult(interp, (Tcl_Obj *)cif->rvalue.v_pointer); break;
  case FFIDL_PTR_UTF8:	Tcl_SetObjResult(interp, Tcl_NewStringObj(cif->rvalue.v_pointer, -1)); break;
  case FFIDL_PTR_UTF16:	Tcl_SetObjResult(interp, Tcl_NewUnicodeObj(cif->rvalue.v_pointer, -1)); break;
  default:
    sprintf(buff, "Invalid return type: %d", cif->rtype->typecode);
    Tcl_AppendResult(interp, buff, NULL);
    goto cleanup;
    return TCL_ERROR;
  }    
  /* done */
  return TCL_OK;
  /* blew it */
 cleanup:
  return TCL_ERROR;
}

/* usage: ffidl-callout name {?argument_type ...?} return_type address ?protocol? */
static int tcl_ffidl_callout(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  char *name;
  void (*fn)();
  int argc, i;
  long tmp;
  Tcl_Obj **argv;
  Tcl_DString usage, ds;
  Tcl_Command res;
  ffidl_cif *cif;
  ffidl_callout *callout;
  ffidl_client *client = (ffidl_client *)clientData;
  /* usage check */
  if (objc != 5 && objc != 6) {
    Tcl_WrongNumArgs(interp, 1, objv, "name {?argument_type ...?} return_type address ?protocol?");
    return TCL_ERROR;
  }
  /* fetch name */
  Tcl_DStringInit(&ds);
  name = Tcl_GetString(objv[1]);
  if (!strstr(name, "::")) {
    Tcl_Namespace *ns;
    ns = Tcl_GetCurrentNamespace(interp);
    if (ns != Tcl_GetGlobalNamespace(interp)) {
      Tcl_DStringAppend(&ds, ns->fullName, -1);
    }
    Tcl_DStringAppend(&ds, "::", 2);
    Tcl_DStringAppend(&ds, name, -1);
    name = Tcl_DStringValue(&ds);
  }
  /* fetch cif */
  if (cif_parse(interp, client, objv[2], objv[3], objc==5 ? NULL : objv[5], &cif, 0) == TCL_ERROR) return TCL_ERROR;
  /* fetch function pointer */
  if (Tcl_GetLongFromObj(interp, objv[4], &tmp) == TCL_ERROR) return TCL_ERROR;
  fn = (void (*)())tmp;
  /* if callout is already defined, redefine it */
  if ((callout = callout_lookup(client, name))) {
    Tcl_DeleteCommand(interp, name);
  }
  /* build the usage string */
  Tcl_ListObjGetElements(interp, objv[2], &argc, &argv);
  Tcl_DStringInit(&usage);
  for (i = 0; i < argc; i += 1) {
    if (i != 0) Tcl_DStringAppend(&usage, " ", 1);
    Tcl_DStringAppend(&usage, Tcl_GetString(argv[i]), -1);
  }
  /* allocate the callout structure */
  callout = (ffidl_callout *)Tcl_Alloc(sizeof(ffidl_callout)+Tcl_DStringLength(&usage)+1);
  if (callout == NULL) {
    Tcl_DStringFree(&usage);
    cif_dec_ref(cif);
    Tcl_AppendResult(interp, "can't allocate ffidl_callout for: ", name, NULL);
    return TCL_ERROR;
  }
  /* initialize the callout */
  callout->cif = cif;
  callout->fn = fn;
  callout->client = client;
  strcpy(callout->usage, Tcl_DStringValue(&usage));
  /* free the usage string */
  Tcl_DStringFree(&usage);
  /* define the callout */
  callout_define(client, name, callout);
  /* create the tcl command */
  res = Tcl_CreateObjCommand(interp, name, tcl_ffidl_call, (ClientData) callout, callout_delete);
  Tcl_DStringFree(&ds);
  return (res ? TCL_OK : TCL_ERROR);
}

#if USE_CALLBACKS
/* usage: ffidl-callback name {?argument_type ...?} return_type ?protocol? -> */
static int tcl_ffidl_callback(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  char *name;
  ffidl_cif *cif;
  int tmp;
  Tcl_DString ds;
  ffidl_callback *callback;
  ffidl_client *client = (ffidl_client *)clientData;
  ffidl_closure *closure;
  /* usage check */
  if (objc != 4 && objc != 5) {
    Tcl_WrongNumArgs(interp, 1, objv, "name {?argument_type ...?} return_type ?protocol?");
    return TCL_ERROR;
  }
  /* fetch name */
  Tcl_DStringInit(&ds);
  name = Tcl_GetString(objv[1]);
  if (!strstr(name, "::")) {
    Tcl_Namespace *ns;
    ns = Tcl_GetCurrentNamespace(interp);
    if (ns != Tcl_GetGlobalNamespace(interp)) {
      Tcl_DStringAppend(&ds, ns->fullName, -1);
    }
    Tcl_DStringAppend(&ds, "::", 2);
    Tcl_DStringAppend(&ds, name, -1);
    name = Tcl_DStringValue(&ds);
  }
  /* fetch cif */
  if (cif_parse(interp, client, objv[2], objv[3], objc == 4 ? NULL : objv[4], &cif, 1) == TCL_ERROR) return TCL_ERROR;
  /* if callback is already defined, redefine it */
  if ((callback = callback_lookup(client, name))) {
    cif_dec_ref(callback->cif);
    Tcl_DecrRefCount(callback->proc);
    Tcl_Free((void *)callback);
  }
  /* allocate the callback structure */
  Tcl_ListObjLength(interp, objv[2], &tmp);
  callback = (ffidl_callback *)Tcl_Alloc(sizeof(ffidl_callback)+tmp*sizeof(Tcl_Obj *));
  if (callback == NULL) {
    cif_dec_ref(cif);
    Tcl_AppendResult(interp, "can't allocate ffidl_callback for: ", name, NULL);
    return TCL_ERROR;
  }
  /* initialize the callback */
  callback->cif = cif;
  callback->proc = Tcl_NewStringObj(name, -1);
  Tcl_IncrRefCount(callback->proc);

  closure = &(callback->closure);
  closure->interp = interp;
  closure->callback = callback;
#if FFI_NATIVE_RAW_API
  if (cif->use_raw_api) {
    if (ffi_prep_raw_closure((ffi_raw_closure *)&closure->lib_closure, &callback->cif->lib_cif,
                             (void (*)(ffi_cif*,void*,ffi_raw*,void*))callback_callback,
                             (void *)closure) != FFI_OK) {
      Tcl_AppendResult(interp, "libffi can't make raw closure for: ", name, NULL);
      return TCL_ERROR;
    }
  } else
#endif
    if (ffi_prep_closure(&closure->lib_closure, &callback->cif->lib_cif,
                         (void (*)(ffi_cif*,void*,void**,void*))callback_callback,
                         (void *)closure) != FFI_OK) {
      Tcl_AppendResult(interp, "libffi can't make closure for: ", name, NULL);
      return TCL_ERROR;
    }
#endif
  /* define the callback */
  callback_define(client, name, callback);
  Tcl_DStringFree(&ds);
  return TCL_OK;
}
/* usage: ffidl-symbol library symbol -> address */
static int tcl_ffidl_symbol(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  char *library, *symbol, *native;
  const char *error;
  void *address;
  Tcl_DString ds;
  Tcl_DString newName;
  void *handle, *unload;
  ffidl_client *client = (ffidl_client *)clientData;

  if (objc != 3) {
    Tcl_WrongNumArgs(interp,1,objv,"library symbol");
    return TCL_ERROR;
  }

  library = Tcl_GetString(objv[1]);
  handle = lib_lookup(client, library, NULL);

  if (handle == NULL) {
    native = Tcl_UtfToExternalDString(NULL, library, -1, &ds);
    handle = ffidlopen(strlen(native)?native:NULL, &error);
    Tcl_DStringFree(&ds);
    if (handle == NULL) {
      Tcl_AppendResult(interp, "couldn't load file \"", library, "\" : ", error, (char *) NULL);
      return TCL_ERROR;
    }
    unload = NULL;
    lib_define(client, library, handle, unload);
  }

  symbol = Tcl_GetString(objv[2]);
  native = Tcl_UtfToExternalDString(NULL, symbol, -1, &ds);
  address = ffidlsym(handle, native, &error);	
  if (error) {
  /* 
   * Some platforms still add an underscore to the beginning of symbol
   * names.  If we can't find a name without an underscore, try again
   * with the underscore.
   */
    Tcl_DStringInit(&newName);
    Tcl_DStringAppend(&newName, "_", 1);
    native = Tcl_DStringAppend(&newName, native, -1);
    address = ffidlsym(handle, native, &error);
    Tcl_DStringFree(&newName);
  }
  Tcl_DStringFree(&ds);

  if (error) {
    Tcl_AppendResult(interp, "couldn't find symbol \"", symbol, "\" : ", error, NULL);
    return TCL_ERROR;
  }

  Tcl_SetObjResult(interp, Tcl_NewLongObj((long)address));
  return TCL_OK;
}
/* usage: ffidl-stubsymbol library stubstable symbolnumber -> address */
static int tcl_ffidl_stubsymbol(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int library, stubstable, symbolnumber; 
  void **stubs = NULL, *address;
  static const char *library_names[] = {
    "tcl", 
#ifdef LOOKUP_TK_STUBS
    "tk",
#endif
    NULL
  };
  enum libraries {
    LIB_TCL, LIB_TK,
  };
  static const char *stubstable_names[] = {
    "stubs", "intStubs", "platStubs", "intPlatStubs", "intXLibStubs", NULL
  };
  enum stubstables {
    STUBS, INTSTUBS, PLATSTUBS, INTPLATSTUBS, INTXLIBSTUBS,
  };

  if (objc != 4) {
    Tcl_WrongNumArgs(interp,1,objv,"library stubstable symbolnumber");
    return TCL_ERROR;
  }
  if (Tcl_GetIndexFromObj(interp, objv[1], library_names, "library", 0, &library) != TCL_OK) {
    return TCL_ERROR;
  }
  if (Tcl_GetIndexFromObj(interp, objv[2], stubstable_names, "stubstable", 0, &stubstable) != TCL_OK) {
    return TCL_ERROR;
  }
  if (Tcl_GetIntFromObj(interp, objv[3], &symbolnumber) != TCL_OK || symbolnumber < 0) {
    return TCL_ERROR;
  }

#ifdef LOOKUP_TK_STUBS
  if (library == LIB_TK) {
    if (MyTkInitStubs(interp, TCL_VERSION, 0) == NULL) {
      return TCL_ERROR;
    }
  }
#endif
  switch (stubstable) {
    case STUBS:
      stubs = (void**)(library == LIB_TCL ? tclStubsPtr : tkStubsPtr); break;
    case INTSTUBS:
      stubs = (void**)(library == LIB_TCL ? tclIntStubsPtr : tkIntStubsPtr); break;
    case PLATSTUBS:
      stubs = (void**)(library == LIB_TCL ? tclPlatStubsPtr : tkPlatStubsPtr); break;
    case INTPLATSTUBS:
      stubs = (void**)(library == LIB_TCL ? tclIntPlatStubsPtr : tkIntPlatStubsPtr); break;
    case INTXLIBSTUBS:
      stubs = (void**)(library == LIB_TCL ? NULL : tkIntXlibStubsPtr); break;
  }

  if (!stubs) {
    Tcl_AppendResult(interp, "no stubs table \"", Tcl_GetString(objv[2]), 
        "\" in library \"", Tcl_GetString(objv[1]), "\"", NULL);
    return TCL_ERROR;
  }
  address = *(stubs + 2 + symbolnumber);
  if (!address) {
    Tcl_AppendResult(interp, "couldn't find symbol number ", Tcl_GetString(objv[3]),
        " in stubs table \"", Tcl_GetString(objv[2]), "\"", NULL);
    return TCL_ERROR;
  }

  Tcl_SetObjResult(interp, Tcl_NewLongObj((long)address));
  return TCL_OK;
}

/*
 * One function exported for pointer punning with ffidl-callout.
 */
void *ffidl_pointer_pun(void *p) { return p; }

/*
 *--------------------------------------------------------------
 *
 * Ffidl_Init
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None
 *
 *--------------------------------------------------------------
 */
int Ffidl_Init(Tcl_Interp *interp)
{
  ffidl_client *client;

  if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
    return TCL_ERROR;
  }
  if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 0) == NULL) {
      return TCL_ERROR;
  }
  if (Tcl_PkgProvide(interp, "Ffidl", FFIDL_VERSION) != TCL_OK) {
    return TCL_ERROR;
  }

  /* allocate and initialize client for this interpreter */
  client = client_alloc(interp);

  /* initialize commands */
  Tcl_CreateObjCommand(interp,"::ffidl::info", tcl_ffidl_info, (ClientData) client, NULL);
  Tcl_CreateObjCommand(interp,"::ffidl::typedef", tcl_ffidl_typedef, (ClientData) client, NULL);
  Tcl_CreateObjCommand(interp,"::ffidl::symbol", tcl_ffidl_symbol, (ClientData) client, NULL);
  Tcl_CreateObjCommand(interp,"::ffidl::stubsymbol", tcl_ffidl_stubsymbol, (ClientData) client, NULL);
  Tcl_CreateObjCommand(interp,"::ffidl::callout", tcl_ffidl_callout, (ClientData) client, NULL);
#if USE_CALLBACKS
  Tcl_CreateObjCommand(interp,"::ffidl::callback", tcl_ffidl_callback, (ClientData) client, NULL);
#endif

  /* determine Tcl_ObjType * for some types */
  ffidl_bytearray_ObjType = Tcl_GetObjType("bytearray");
  ffidl_int_ObjType = Tcl_GetObjType("int");
#if HAVE_INT64
  ffidl_wideInt_ObjType = Tcl_GetObjType("wideInt");
#endif
  ffidl_double_ObjType = Tcl_GetObjType("double");

  /* done */
  return TCL_OK;
}

#ifdef LOOKUP_TK_STUBS
typedef struct MyTkStubHooks {
    void *tkPlatStubs;
    void *tkIntStubs;
    void *tkIntPlatStubs;
    void *tkIntXlibStubs;
} MyTkStubHooks;

typedef struct MyTkStubs {
    int magic;
    struct MyTkStubHooks *hooks;
} MyTkStubs;

/* private copy of Tk_InitStubs to avoid having to depend on Tk at build time */
static const char *
MyTkInitStubs(interp, version, exact)
    Tcl_Interp *interp;
    char *version;
    int exact;
{
    const char *actualVersion;

    actualVersion = Tcl_PkgRequireEx(interp, "Tk", version, exact,
		(ClientData *) &tkStubsPtr);
    if (!actualVersion) {
	return NULL;
    }

    if (!tkStubsPtr) {
	Tcl_SetResult(interp,
		"This implementation of Tk does not support stubs",
		TCL_STATIC);
	return NULL;
    }
    
    tkPlatStubsPtr =    ((MyTkStubs*)tkStubsPtr)->hooks->tkPlatStubs;
    tkIntStubsPtr =     ((MyTkStubs*)tkStubsPtr)->hooks->tkIntStubs;
    tkIntPlatStubsPtr = ((MyTkStubs*)tkStubsPtr)->hooks->tkIntPlatStubs;
    tkIntXlibStubsPtr = ((MyTkStubs*)tkStubsPtr)->hooks->tkIntXlibStubs;
    
    return actualVersion;
}
#endif
