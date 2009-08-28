/*
 * a set of functions to test arguments and returns in ffidl.
 * bugs found:
 *	libffi x86 set sizeof(long) to 8
 *	libffi x86 bad return for unsigned long long
 *	ffidl  confused GetInt and GetLong sometimes
 *	2nd long long in callback gets trashed
 */

#include <tcl.h>

typedef struct {
  signed char v_schar;
  signed short v_sshort;
  signed int v_sint;
  signed long v_slong;
  signed long long v_slonglong;
  float v_float;
  double v_double;
  void *v_pointer;
  unsigned char v_bytes[8];
} ffidl_test_struct;
ffidl_test_struct astruct = { 1, 2, 3, 4, 5, 6, 7, (void*)8, "0123456" };

EXTERN void ffidl_schar_to_void(signed char a) { return ; }
EXTERN void ffidl_uchar_to_void(unsigned char a) { return ; }
EXTERN void ffidl_sshort_to_void(signed short a) { return ; }
EXTERN void ffidl_ushort_to_void(unsigned short a) { return ; }
EXTERN void ffidl_sint_to_void(signed int a) { return ; }
EXTERN void ffidl_uint_to_void(unsigned int a) { return ; }
EXTERN void ffidl_slong_to_void(signed long a) { return ; }
EXTERN void ffidl_ulong_to_void(unsigned long a) { return ; }
EXTERN void ffidl_slonglong_to_void(signed long long a) { return ; }
EXTERN void ffidl_ulonglong_to_void(unsigned long long a) { return ; }
EXTERN void ffidl_float_to_void(float a) { return ; }
EXTERN void ffidl_double_to_void(double a) { return ; }
EXTERN void ffidl_longdouble_to_void(long double a) { return ; }
EXTERN void ffidl_pointer_to_void(void *a) { return ; }

EXTERN signed char ffidl_schar_to_schar(signed char a) { return a; }
EXTERN signed char ffidl_uchar_to_schar(unsigned char a) { return a; }
EXTERN signed char ffidl_sshort_to_schar(signed short a) { return a; }
EXTERN signed char ffidl_ushort_to_schar(unsigned short a) { return a; }
EXTERN signed char ffidl_sint_to_schar(signed int a) { return a; }
EXTERN signed char ffidl_uint_to_schar(unsigned int a) { return a; }
EXTERN signed char ffidl_slong_to_schar(signed long a) { return a; }
EXTERN signed char ffidl_ulong_to_schar(unsigned long a) { return a; }
EXTERN signed char ffidl_slonglong_to_schar(signed long long a) { return a; }
EXTERN signed char ffidl_ulonglong_to_schar(unsigned long long a) { return a; }
EXTERN signed char ffidl_float_to_schar(float a) { return a; }
EXTERN signed char ffidl_double_to_schar(double a) { return a; }
EXTERN signed char ffidl_longdouble_to_schar(long double a) { return a; }
EXTERN signed char ffidl_pointer_to_schar(void *a) { return (signed char)(long)a; }

EXTERN unsigned char ffidl_schar_to_uchar(signed char a) { return a; }
EXTERN unsigned char ffidl_uchar_to_uchar(unsigned char a) { return a; }
EXTERN unsigned char ffidl_sshort_to_uchar(signed short a) { return a; }
EXTERN unsigned char ffidl_ushort_to_uchar(unsigned short a) { return a; }
EXTERN unsigned char ffidl_sint_to_uchar(signed int a) { return a; }
EXTERN unsigned char ffidl_uint_to_uchar(unsigned int a) { return a; }
EXTERN unsigned char ffidl_slong_to_uchar(signed long a) { return a; }
EXTERN unsigned char ffidl_ulong_to_uchar(unsigned long a) { return a; }
EXTERN unsigned char ffidl_slonglong_to_uchar(signed long long a) { return a; }
EXTERN unsigned char ffidl_ulonglong_to_uchar(unsigned long long a) { return a; }
EXTERN unsigned char ffidl_float_to_uchar(float a) { return a; }
EXTERN unsigned char ffidl_double_to_uchar(double a) { return a; }
EXTERN unsigned char ffidl_longdouble_to_uchar(long double a) { return a; }
EXTERN unsigned char ffidl_pointer_to_uchar(void *a) { return (unsigned char)(long)a; }

EXTERN signed short ffidl_schar_to_sshort(signed char a) { return a; }
EXTERN signed short ffidl_uchar_to_sshort(unsigned char a) { return a; }
EXTERN signed short ffidl_sshort_to_sshort(signed short a) { return a; }
EXTERN signed short ffidl_ushort_to_sshort(unsigned short a) { return a; }
EXTERN signed short ffidl_sint_to_sshort(signed int a) { return a; }
EXTERN signed short ffidl_uint_to_sshort(unsigned int a) { return a; }
EXTERN signed short ffidl_slong_to_sshort(signed long a) { return a; }
EXTERN signed short ffidl_ulong_to_sshort(unsigned long a) { return a; }
EXTERN signed short ffidl_slonglong_to_sshort(signed long long a) { return a; }
EXTERN signed short ffidl_ulonglong_to_sshort(unsigned long long a) { return a; }
EXTERN signed short ffidl_float_to_sshort(float a) { return a; }
EXTERN signed short ffidl_double_to_sshort(double a) { return a; }
EXTERN signed short ffidl_longdouble_to_sshort(long double a) { return a; }
EXTERN signed short ffidl_pointer_to_sshort(void *a) { return (signed short)(long)a; }

EXTERN unsigned short ffidl_schar_to_ushort(signed char a) { return a; }
EXTERN unsigned short ffidl_uchar_to_ushort(unsigned char a) { return a; }
EXTERN unsigned short ffidl_sshort_to_ushort(signed short a) { return a; }
EXTERN unsigned short ffidl_ushort_to_ushort(unsigned short a) { return a; }
EXTERN unsigned short ffidl_sint_to_ushort(signed int a) { return a; }
EXTERN unsigned short ffidl_uint_to_ushort(unsigned int a) { return a; }
EXTERN unsigned short ffidl_slong_to_ushort(signed long a) { return a; }
EXTERN unsigned short ffidl_ulong_to_ushort(unsigned long a) { return a; }
EXTERN unsigned short ffidl_slonglong_to_ushort(signed long long a) { return a; }
EXTERN unsigned short ffidl_ulonglong_to_ushort(unsigned long long a) { return a; }
EXTERN unsigned short ffidl_float_to_ushort(float a) { return a; }
EXTERN unsigned short ffidl_double_to_ushort(double a) { return a; }
EXTERN unsigned short ffidl_longdouble_to_ushort(long double a) { return a; }
EXTERN unsigned short ffidl_pointer_to_ushort(void *a) { return (unsigned short)(long)a; }

EXTERN signed int ffidl_schar_to_sint(signed char a) { return a; }
EXTERN signed int ffidl_uchar_to_sint(unsigned char a) { return a; }
EXTERN signed int ffidl_sshort_to_sint(signed short a) { return a; }
EXTERN signed int ffidl_ushort_to_sint(unsigned short a) { return a; }
EXTERN signed int ffidl_sint_to_sint(signed int a) { return a; }
EXTERN signed int ffidl_uint_to_sint(unsigned int a) { return a; }
EXTERN signed int ffidl_slong_to_sint(signed long a) { return a; }
EXTERN signed int ffidl_ulong_to_sint(unsigned long a) { return a; }
EXTERN signed int ffidl_slonglong_to_sint(signed long long a) { return a; }
EXTERN signed int ffidl_ulonglong_to_sint(unsigned long long a) { return a; }
EXTERN signed int ffidl_float_to_sint(float a) { return a; }
EXTERN signed int ffidl_double_to_sint(double a) { return a; }
EXTERN signed int ffidl_longdouble_to_sint(long double a) { return a; }
EXTERN signed int ffidl_pointer_to_sint(void *a) { return (signed int)(long)a; }

EXTERN unsigned int ffidl_schar_to_uint(signed char a) { return a; }
EXTERN unsigned int ffidl_uchar_to_uint(unsigned char a) { return a; }
EXTERN unsigned int ffidl_sshort_to_uint(signed short a) { return a; }
EXTERN unsigned int ffidl_ushort_to_uint(unsigned short a) { return a; }
EXTERN unsigned int ffidl_sint_to_uint(signed int a) { return a; }
EXTERN unsigned int ffidl_uint_to_uint(unsigned int a) { return a; }
EXTERN unsigned int ffidl_slong_to_uint(signed long a) { return a; }
EXTERN unsigned int ffidl_ulong_to_uint(unsigned long a) { return a; }
EXTERN unsigned int ffidl_slonglong_to_uint(signed long long a) { return a; }
EXTERN unsigned int ffidl_ulonglong_to_uint(unsigned long long a) { return a; }
EXTERN unsigned int ffidl_float_to_uint(float a) { return a; }
EXTERN unsigned int ffidl_double_to_uint(double a) { return a; }
EXTERN unsigned int ffidl_longdouble_to_uint(long double a) { return a; }
EXTERN unsigned int ffidl_pointer_to_uint(void *a) { return (unsigned int)(long)a; }

EXTERN signed long ffidl_schar_to_slong(signed char a) { return a; }
EXTERN signed long ffidl_uchar_to_slong(unsigned char a) { return a; }
EXTERN signed long ffidl_sshort_to_slong(signed short a) { return a; }
EXTERN signed long ffidl_ushort_to_slong(unsigned short a) { return a; }
EXTERN signed long ffidl_sint_to_slong(signed int a) { return a; }
EXTERN signed long ffidl_uint_to_slong(unsigned int a) { return a; }
EXTERN signed long ffidl_slong_to_slong(signed long a) { return a; }
EXTERN signed long ffidl_ulong_to_slong(unsigned long a) { return a; }
EXTERN signed long ffidl_slonglong_to_slong(signed long long a) { return a; }
EXTERN signed long ffidl_ulonglong_to_slong(unsigned long long a) { return a; }
EXTERN signed long ffidl_float_to_slong(float a) { return a; }
EXTERN signed long ffidl_double_to_slong(double a) { return a; }
EXTERN signed long ffidl_longdouble_to_slong(long double a) { return a; }
EXTERN signed long ffidl_pointer_to_slong(void *a) { return (signed long)(long)a; }

EXTERN unsigned long ffidl_schar_to_ulong(signed char a) { return a; }
EXTERN unsigned long ffidl_uchar_to_ulong(unsigned char a) { return a; }
EXTERN unsigned long ffidl_sshort_to_ulong(signed short a) { return a; }
EXTERN unsigned long ffidl_ushort_to_ulong(unsigned short a) { return a; }
EXTERN unsigned long ffidl_sint_to_ulong(signed int a) { return a; }
EXTERN unsigned long ffidl_uint_to_ulong(unsigned int a) { return a; }
EXTERN unsigned long ffidl_slong_to_ulong(signed long a) { return a; }
EXTERN unsigned long ffidl_ulong_to_ulong(unsigned long a) { return a; }
EXTERN unsigned long ffidl_slonglong_to_ulong(signed long long a) { return a; }
EXTERN unsigned long ffidl_ulonglong_to_ulong(unsigned long long a) { return a; }
EXTERN unsigned long ffidl_float_to_ulong(float a) { return a; }
EXTERN unsigned long ffidl_double_to_ulong(double a) { return a; }
EXTERN unsigned long ffidl_longdouble_to_ulong(long double a) { return a; }
EXTERN unsigned long ffidl_pointer_to_ulong(void *a) { return (unsigned long)(long)a; }

EXTERN signed long long ffidl_schar_to_slonglong(signed char a) { return a; }
EXTERN signed long long ffidl_uchar_to_slonglong(unsigned char a) { return a; }
EXTERN signed long long ffidl_sshort_to_slonglong(signed short a) { return a; }
EXTERN signed long long ffidl_ushort_to_slonglong(unsigned short a) { return a; }
EXTERN signed long long ffidl_sint_to_slonglong(signed int a) { return a; }
EXTERN signed long long ffidl_uint_to_slonglong(unsigned int a) { return a; }
EXTERN signed long long ffidl_slong_to_slonglong(signed long a) { return a; }
EXTERN signed long long ffidl_ulong_to_slonglong(unsigned long a) { return a; }
EXTERN signed long long ffidl_slonglong_to_slonglong(signed long long a) { return a; }
EXTERN signed long long ffidl_ulonglong_to_slonglong(unsigned long long a) { return a; }
EXTERN signed long long ffidl_float_to_slonglong(float a) { return a; }
EXTERN signed long long ffidl_double_to_slonglong(double a) { return a; }
EXTERN signed long long ffidl_longdouble_to_slonglong(long double a) { return a; }
EXTERN signed long long ffidl_pointer_to_slonglong(void *a) { return (signed long long)(long)a; }

EXTERN unsigned long long ffidl_schar_to_ulonglong(signed char a) { return a; }
EXTERN unsigned long long ffidl_uchar_to_ulonglong(unsigned char a) { return a; }
EXTERN unsigned long long ffidl_sshort_to_ulonglong(signed short a) { return a; }
EXTERN unsigned long long ffidl_ushort_to_ulonglong(unsigned short a) { return a; }
EXTERN unsigned long long ffidl_sint_to_ulonglong(signed int a) { return a; }
EXTERN unsigned long long ffidl_uint_to_ulonglong(unsigned int a) { return a; }
EXTERN unsigned long long ffidl_slong_to_ulonglong(signed long a) { return a; }
EXTERN unsigned long long ffidl_ulong_to_ulonglong(unsigned long a) { return a; }
EXTERN unsigned long long ffidl_slonglong_to_ulonglong(signed long long a) { return a; }
EXTERN unsigned long long ffidl_ulonglong_to_ulonglong(unsigned long long a) { return a; }
EXTERN unsigned long long ffidl_float_to_ulonglong(float a) { return a; }
EXTERN unsigned long long ffidl_double_to_ulonglong(double a) { return a; }
EXTERN unsigned long long ffidl_longdouble_to_ulonglong(long double a) { return a; }
EXTERN unsigned long long ffidl_pointer_to_ulonglong(void *a) { return (signed long long)(long)a; }

EXTERN float ffidl_schar_to_float(signed char a) { return a; }
EXTERN float ffidl_uchar_to_float(unsigned char a) { return a; }
EXTERN float ffidl_sshort_to_float(signed short a) { return a; }
EXTERN float ffidl_ushort_to_float(unsigned short a) { return a; }
EXTERN float ffidl_sint_to_float(signed int a) { return a; }
EXTERN float ffidl_uint_to_float(unsigned int a) { return a; }
EXTERN float ffidl_slong_to_float(signed long a) { return a; }
EXTERN float ffidl_ulong_to_float(unsigned long a) { return a; }
EXTERN float ffidl_slonglong_to_float(signed long long a) { return a; }
EXTERN float ffidl_ulonglong_to_float(unsigned long long a) { return a; }
EXTERN float ffidl_float_to_float(float a) { return a; }
EXTERN float ffidl_double_to_float(double a) { return a; }
EXTERN float ffidl_longdouble_to_float(long double a) { return a; }
EXTERN float ffidl_pointer_to_float(void *a) { return (float)(long)a; }

EXTERN double ffidl_schar_to_double(signed char a) { return a; }
EXTERN double ffidl_uchar_to_double(unsigned char a) { return a; }
EXTERN double ffidl_sshort_to_double(signed short a) { return a; }
EXTERN double ffidl_ushort_to_double(unsigned short a) { return a; }
EXTERN double ffidl_sint_to_double(signed int a) { return a; }
EXTERN double ffidl_uint_to_double(unsigned int a) { return a; }
EXTERN double ffidl_slong_to_double(signed long a) { return a; }
EXTERN double ffidl_ulong_to_double(unsigned long a) { return a; }
EXTERN double ffidl_slonglong_to_double(signed long long a) { return a; }
EXTERN double ffidl_ulonglong_to_double(unsigned long long a) { return a; }
EXTERN double ffidl_float_to_double(float a) { return a; }
EXTERN double ffidl_double_to_double(double a) { return a; }
EXTERN double ffidl_longdouble_to_double(long double a) { return a; }
EXTERN double ffidl_pointer_to_double(void *a) { return (double)(long)a; }

EXTERN long double ffidl_schar_to_longdouble(signed char a) { return a; }
EXTERN long double ffidl_uchar_to_longdouble(unsigned char a) { return a; }
EXTERN long double ffidl_sshort_to_longdouble(signed short a) { return a; }
EXTERN long double ffidl_ushort_to_longdouble(unsigned short a) { return a; }
EXTERN long double ffidl_sint_to_longdouble(signed int a) { return a; }
EXTERN long double ffidl_uint_to_longdouble(unsigned int a) { return a; }
EXTERN long double ffidl_slong_to_longdouble(signed long a) { return a; }
EXTERN long double ffidl_ulong_to_longdouble(unsigned long a) { return a; }
EXTERN long double ffidl_slonglong_to_longdouble(signed long long a) { return a; }
EXTERN long double ffidl_ulonglong_to_longdouble(unsigned long long a) { return a; }
EXTERN long double ffidl_float_to_longdouble(float a) { return a; }
EXTERN long double ffidl_double_to_longdouble(double a) { return a; }
EXTERN long double ffidl_longdouble_to_longdouble(long double a) { return a; }
EXTERN long double ffidl_pointer_to_longdouble(void *a) { return (long double)(long)a; }

EXTERN void *ffidl_schar_to_pointer(signed char a) { return (void *)(long)a; }
EXTERN void *ffidl_uchar_to_pointer(unsigned char a) { return (void *)(long)a; }
EXTERN void *ffidl_sshort_to_pointer(signed short a) { return (void *)(long)a; }
EXTERN void *ffidl_ushort_to_pointer(unsigned short a) { return (void *)(long)a; }
EXTERN void *ffidl_sint_to_pointer(signed int a) { return (void *)(long)a; }
EXTERN void *ffidl_uint_to_pointer(unsigned int a) { return (void *)(long)a; }
EXTERN void *ffidl_slong_to_pointer(signed long a) { return (void *)(long)a; }
EXTERN void *ffidl_ulong_to_pointer(unsigned long a) { return (void *)(long)a; }
EXTERN void *ffidl_slonglong_to_pointer(signed long long a) { return (void *)(long)a; }
EXTERN void *ffidl_ulonglong_to_pointer(unsigned long long a) { return (void *)(long)a; }
EXTERN void *ffidl_float_to_pointer(float a) { return (void *)(long)a; }
EXTERN void *ffidl_double_to_pointer(double a) { return (void *)(long)a; }
EXTERN void *ffidl_longdouble_to_pointer(long double a) { return (void *)(long)a; }
EXTERN void *ffidl_pointer_to_pointer(void *a) { return a; }

EXTERN ffidl_test_struct ffidl_fill_struct() { return astruct; }
EXTERN ffidl_test_struct ffidl_struct_to_struct(ffidl_test_struct a) { return a; }

EXTERN char * ffidl_test_signatures() {
  return
    "void ffidl_schar_to_void(signed char )\n"
    "void ffidl_uchar_to_void(unsigned char )\n"
    "void ffidl_sshort_to_void(signed short )\n"
    "void ffidl_ushort_to_void(unsigned short )\n"
    "void ffidl_sint_to_void(signed int )\n"
    "void ffidl_uint_to_void(unsigned int )\n"
    "void ffidl_slong_to_void(signed long )\n"
    "void ffidl_ulong_to_void(unsigned long )\n"
    "void ffidl_slonglong_to_void(signed long long )\n"
    "void ffidl_ulonglong_to_void(unsigned long long )\n"
    "void ffidl_float_to_void(float )\n"
    "void ffidl_double_to_void(double )\n"
    "void ffidl_longdouble_to_void(long double )\n"
    "void ffidl_pointer_to_void(void *)\n"

    "signed char ffidl_schar_to_schar(signed char )\n"
    "signed char ffidl_uchar_to_schar(unsigned char )\n"
    "signed char ffidl_sshort_to_schar(signed short )\n"
    "signed char ffidl_ushort_to_schar(unsigned short )\n"
    "signed char ffidl_sint_to_schar(signed int )\n"
    "signed char ffidl_uint_to_schar(unsigned int )\n"
    "signed char ffidl_slong_to_schar(signed long )\n"
    "signed char ffidl_ulong_to_schar(unsigned long )\n"
    "signed char ffidl_slonglong_to_schar(signed long long )\n"
    "signed char ffidl_ulonglong_to_schar(unsigned long long )\n"
    "signed char ffidl_float_to_schar(float )\n"
    "signed char ffidl_double_to_schar(double )\n"
    "signed char ffidl_longdouble_to_schar(long double )\n"
    "signed char ffidl_pointer_to_schar(void *)\n"

    "unsigned char ffidl_schar_to_uchar(signed char )\n"
    "unsigned char ffidl_uchar_to_uchar(unsigned char )\n"
    "unsigned char ffidl_sshort_to_uchar(signed short )\n"
    "unsigned char ffidl_ushort_to_uchar(unsigned short )\n"
    "unsigned char ffidl_sint_to_uchar(signed int )\n"
    "unsigned char ffidl_uint_to_uchar(unsigned int )\n"
    "unsigned char ffidl_slong_to_uchar(signed long )\n"
    "unsigned char ffidl_ulong_to_uchar(unsigned long )\n"
    "unsigned char ffidl_slonglong_to_uchar(signed long long )\n"
    "unsigned char ffidl_ulonglong_to_uchar(unsigned long long )\n"
    "unsigned char ffidl_float_to_uchar(float )\n"
    "unsigned char ffidl_double_to_uchar(double )\n"
    "unsigned char ffidl_longdouble_to_uchar(long double )\n"
    "unsigned char ffidl_pointer_to_uchar(void *)\n"

    "signed short ffidl_schar_to_sshort(signed char )\n"
    "signed short ffidl_uchar_to_sshort(unsigned char )\n"
    "signed short ffidl_sshort_to_sshort(signed short )\n"
    "signed short ffidl_ushort_to_sshort(unsigned short )\n"
    "signed short ffidl_sint_to_sshort(signed int )\n"
    "signed short ffidl_uint_to_sshort(unsigned int )\n"
    "signed short ffidl_slong_to_sshort(signed long )\n"
    "signed short ffidl_ulong_to_sshort(unsigned long )\n"
    "signed short ffidl_slonglong_to_sshort(signed long long )\n"
    "signed short ffidl_ulonglong_to_sshort(unsigned long long )\n"
    "signed short ffidl_float_to_sshort(float )\n"
    "signed short ffidl_double_to_sshort(double )\n"
    "signed short ffidl_longdouble_to_sshort(long double )\n"
    "signed short ffidl_pointer_to_sshort(void *)\n"

    "unsigned short ffidl_schar_to_ushort(signed char )\n"
    "unsigned short ffidl_uchar_to_ushort(unsigned char )\n"
    "unsigned short ffidl_sshort_to_ushort(signed short )\n"
    "unsigned short ffidl_ushort_to_ushort(unsigned short )\n"
    "unsigned short ffidl_sint_to_ushort(signed int )\n"
    "unsigned short ffidl_uint_to_ushort(unsigned int )\n"
    "unsigned short ffidl_slong_to_ushort(signed long )\n"
    "unsigned short ffidl_ulong_to_ushort(unsigned long )\n"
    "unsigned short ffidl_slonglong_to_ushort(signed long long )\n"
    "unsigned short ffidl_ulonglong_to_ushort(unsigned long long )\n"
    "unsigned short ffidl_float_to_ushort(float )\n"
    "unsigned short ffidl_double_to_ushort(double )\n"
    "unsigned short ffidl_longdouble_to_ushort(long double )\n"
    "unsigned short ffidl_pointer_to_ushort(void *)\n"

    "signed int ffidl_schar_to_sint(signed char )\n"
    "signed int ffidl_uchar_to_sint(unsigned char )\n"
    "signed int ffidl_sshort_to_sint(signed short )\n"
    "signed int ffidl_ushort_to_sint(unsigned short )\n"
    "signed int ffidl_sint_to_sint(signed int )\n"
    "signed int ffidl_uint_to_sint(unsigned int )\n"
    "signed int ffidl_slong_to_sint(signed long )\n"
    "signed int ffidl_ulong_to_sint(unsigned long )\n"
    "signed int ffidl_slonglong_to_sint(signed long long )\n"
    "signed int ffidl_ulonglong_to_sint(unsigned long long )\n"
    "signed int ffidl_float_to_sint(float )\n"
    "signed int ffidl_double_to_sint(double )\n"
    "signed int ffidl_longdouble_to_sint(long double )\n"
    "signed int ffidl_pointer_to_sint(void *)\n"

    "unsigned int ffidl_schar_to_uint(signed char )\n"
    "unsigned int ffidl_uchar_to_uint(unsigned char )\n"
    "unsigned int ffidl_sshort_to_uint(signed short )\n"
    "unsigned int ffidl_ushort_to_uint(unsigned short )\n"
    "unsigned int ffidl_sint_to_uint(signed int )\n"
    "unsigned int ffidl_uint_to_uint(unsigned int )\n"
    "unsigned int ffidl_slong_to_uint(signed long )\n"
    "unsigned int ffidl_ulong_to_uint(unsigned long )\n"
    "unsigned int ffidl_slonglong_to_uint(signed long long )\n"
    "unsigned int ffidl_ulonglong_to_uint(unsigned long long )\n"
    "unsigned int ffidl_float_to_uint(float )\n"
    "unsigned int ffidl_double_to_uint(double )\n"
    "unsigned int ffidl_longdouble_to_uint(long double )\n"
    "unsigned int ffidl_pointer_to_uint(void *)\n"

    "signed long ffidl_schar_to_slong(signed char )\n"
    "signed long ffidl_uchar_to_slong(unsigned char )\n"
    "signed long ffidl_sshort_to_slong(signed short )\n"
    "signed long ffidl_ushort_to_slong(unsigned short )\n"
    "signed long ffidl_sint_to_slong(signed int )\n"
    "signed long ffidl_uint_to_slong(unsigned int )\n"
    "signed long ffidl_slong_to_slong(signed long )\n"
    "signed long ffidl_ulong_to_slong(unsigned long )\n"
    "signed long ffidl_slonglong_to_slong(signed long long )\n"
    "signed long ffidl_ulonglong_to_slong(unsigned long long )\n"
    "signed long ffidl_float_to_slong(float )\n"
    "signed long ffidl_double_to_slong(double )\n"
    "signed long ffidl_longdouble_to_slong(long double )\n"
    "signed long ffidl_pointer_to_slong(void *)\n"

    "unsigned long ffidl_schar_to_ulong(signed char )\n"
    "unsigned long ffidl_uchar_to_ulong(unsigned char )\n"
    "unsigned long ffidl_sshort_to_ulong(signed short )\n"
    "unsigned long ffidl_ushort_to_ulong(unsigned short )\n"
    "unsigned long ffidl_sint_to_ulong(signed int )\n"
    "unsigned long ffidl_uint_to_ulong(unsigned int )\n"
    "unsigned long ffidl_slong_to_ulong(signed long )\n"
    "unsigned long ffidl_ulong_to_ulong(unsigned long )\n"
    "unsigned long ffidl_slonglong_to_ulong(signed long long )\n"
    "unsigned long ffidl_ulonglong_to_ulong(unsigned long long )\n"
    "unsigned long ffidl_float_to_ulong(float )\n"
    "unsigned long ffidl_double_to_ulong(double )\n"
    "unsigned long ffidl_longdouble_to_ulong(long double )\n"
    "unsigned long ffidl_pointer_to_ulong(void *)\n"

    "signed long long ffidl_schar_to_slonglong(signed char )\n"
    "signed long long ffidl_uchar_to_slonglong(unsigned char )\n"
    "signed long long ffidl_sshort_to_slonglong(signed short )\n"
    "signed long long ffidl_ushort_to_slonglong(unsigned short )\n"
    "signed long long ffidl_sint_to_slonglong(signed int )\n"
    "signed long long ffidl_uint_to_slonglong(unsigned int )\n"
    "signed long long ffidl_slong_to_slonglong(signed long )\n"
    "signed long long ffidl_ulong_to_slonglong(unsigned long )\n"
    "signed long long ffidl_slonglong_to_slonglong(signed long long )\n"
    "signed long long ffidl_ulonglong_to_slonglong(unsigned long long )\n"
    "signed long long ffidl_float_to_slonglong(float )\n"
    "signed long long ffidl_double_to_slonglong(double )\n"
    "signed long long ffidl_longdouble_to_slonglong(long double )\n"
    "signed long long ffidl_pointer_to_slonglong(void *)\n"

    "unsigned long long ffidl_schar_to_ulonglong(signed char )\n"
    "unsigned long long ffidl_uchar_to_ulonglong(unsigned char )\n"
    "unsigned long long ffidl_sshort_to_ulonglong(signed short )\n"
    "unsigned long long ffidl_ushort_to_ulonglong(unsigned short )\n"
    "unsigned long long ffidl_sint_to_ulonglong(signed int )\n"
    "unsigned long long ffidl_uint_to_ulonglong(unsigned int )\n"
    "unsigned long long ffidl_slong_to_ulonglong(signed long )\n"
    "unsigned long long ffidl_ulong_to_ulonglong(unsigned long )\n"
    "unsigned long long ffidl_slonglong_to_ulonglong(signed long long )\n"
    "unsigned long long ffidl_ulonglong_to_ulonglong(unsigned long long )\n"
    "unsigned long long ffidl_float_to_ulonglong(float )\n"
    "unsigned long long ffidl_double_to_ulonglong(double )\n"
    "unsigned long long ffidl_longdouble_to_ulonglong(long double )\n"
    "unsigned long long ffidl_pointer_to_ulonglong(void *)\n"

    "float ffidl_schar_to_float(signed char )\n"
    "float ffidl_uchar_to_float(unsigned char )\n"
    "float ffidl_sshort_to_float(signed short )\n"
    "float ffidl_ushort_to_float(unsigned short )\n"
    "float ffidl_sint_to_float(signed int )\n"
    "float ffidl_uint_to_float(unsigned int )\n"
    "float ffidl_slong_to_float(signed long )\n"
    "float ffidl_ulong_to_float(unsigned long )\n"
    "float ffidl_slonglong_to_float(signed long long )\n"
    "float ffidl_ulonglong_to_float(unsigned long long )\n"
    "float ffidl_float_to_float(float )\n"
    "float ffidl_double_to_float(double )\n"
    "float ffidl_longdouble_to_float(long double )\n"
    "float ffidl_pointer_to_float(void *)\n"

    "double ffidl_schar_to_double(signed char )\n"
    "double ffidl_uchar_to_double(unsigned char )\n"
    "double ffidl_sshort_to_double(signed short )\n"
    "double ffidl_ushort_to_double(unsigned short )\n"
    "double ffidl_sint_to_double(signed int )\n"
    "double ffidl_uint_to_double(unsigned int )\n"
    "double ffidl_slong_to_double(signed long )\n"
    "double ffidl_ulong_to_double(unsigned long )\n"
    "double ffidl_slonglong_to_double(signed long long )\n"
    "double ffidl_ulonglong_to_double(unsigned long long )\n"
    "double ffidl_float_to_double(float )\n"
    "double ffidl_double_to_double(double )\n"
    "double ffidl_longdouble_to_double(long double )\n"
    "double ffidl_pointer_to_double(void *)\n"

    "long double ffidl_schar_to_longdouble(signed char )\n"
    "long double ffidl_uchar_to_longdouble(unsigned char )\n"
    "long double ffidl_sshort_to_longdouble(signed short )\n"
    "long double ffidl_ushort_to_longdouble(unsigned short )\n"
    "long double ffidl_sint_to_longdouble(signed int )\n"
    "long double ffidl_uint_to_longdouble(unsigned int )\n"
    "long double ffidl_slong_to_longdouble(signed long )\n"
    "long double ffidl_ulong_to_longdouble(unsigned long )\n"
    "long double ffidl_slonglong_to_longdouble(signed long long )\n"
    "long double ffidl_ulonglong_to_longdouble(unsigned long long )\n"
    "long double ffidl_float_to_longdouble(float )\n"
    "long double ffidl_double_to_longdouble(double )\n"
    "long double ffidl_longdouble_to_longdouble(long double )\n"
    "long double ffidl_pointer_to_longdouble(void *)\n"

    "void *ffidl_schar_to_pointer(signed char )\n"
    "void *ffidl_uchar_to_pointer(unsigned char )\n"
    "void *ffidl_sshort_to_pointer(signed short )\n"
    "void *ffidl_ushort_to_pointer(unsigned short )\n"
    "void *ffidl_sint_to_pointer(signed int )\n"
    "void *ffidl_uint_to_pointer(unsigned int )\n"
    "void *ffidl_slong_to_pointer(signed long )\n"
    "void *ffidl_ulong_to_pointer(unsigned long )\n"
    "void *ffidl_slonglong_to_pointer(signed long long )\n"
    "void *ffidl_ulonglong_to_pointer(unsigned long long )\n"
    "void *ffidl_float_to_pointer(float )\n"
    "void *ffidl_double_to_pointer(double )\n"
    "void *ffidl_longdouble_to_pointer(long double )\n"
    "void *ffidl_pointer_to_pointer(void *)\n"

    "ffidl_test_struct ffidl_fill_struct()\n"
    "ffidl_test_struct ffidl_struct_to_struct(ffidl_test_struct )\n"
    ;
}
/*
 * callback tests
 */
EXTERN char ffidl_fchar(char (*f)(char a, char b), char a, char b) { return f(a,b); }
EXTERN short ffidl_fshort(short (*f)(short a, short b), short a, short b) { return f(a,b); }
EXTERN int ffidl_fint(int (*f)(int a, int b), int a, int b) { return f(a,b); }
EXTERN long ffidl_flong(long (*f)(long a, long b), long a, long b) { return f(a,b); }
EXTERN long long ffidl_flonglong(long long (*f)(long long a, long long b), long long a, long long b) { return f(a,b); }
EXTERN float ffidl_ffloat(float (*f)(float a, float b), float a, float b) { return f(a,b); }
EXTERN double ffidl_fdouble(double (*f)(double a, double b), double a, double b) { return f(a,b); }
EXTERN void ffidl_isort(int *base, int nmemb, int (*compar)(const int *,const int *))
{
  int i, j, t;
  for (i = 0; i < nmemb; i += 1) {
    for (j = i+1; j < nmemb;  j += 1) {
      if (compar(base+i, base+j) > 0) {
	t = base[i]; base[i] = base[j]; base[j] = t;
      }
    }
  }
}
