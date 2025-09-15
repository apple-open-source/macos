// workaround in case we accidentally grab this project's <sys/cdefs.h>
#define _LIBC_NO_FEATURE_VERIFICATION 1

#include <sys/cdefs.h>

#ifdef UNDEF_BOUNDS_SAFETY_ATTRIBUTES
#undef __LIBC_STAGED_BOUNDS_SAFETY_ATTRIBUTE
#endif

#ifdef _TEST_POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE _TEST_POSIX_C_SOURCE
#endif

#ifdef _TEST_DARWIN_C_LEVEL
#undef __DARWIN_C_LEVEL
#define __DARWIN_C_LEVEL _TEST_DARWIN_C_LEVEL
#endif

/* This file is preprocessed with the following feature combinations:
 POSIX_C_SOURCE  DARWIN_C_LEVEL	  UNIFDEF_DRIVERKIT  -fbounds-safety
 -               0                -                  -
 -               200112L          -                  -
 -               200809L          -                  -
 -               __DARWIN_C_FULL  -                  -
 0               __DARWIN_C_FULL  -                  -
 -               __DARWIN_C_FULL  1                  -
 -               __DARWIN_C_FULL  -                  1
 -               __DARWIN_C_FULL  1                  1
 */

#include <string.h>
#include <stdio.h>

extern void undefined;

// MARK: - <strings.h> safe functions
#ifndef bcopy
#define bcopy(...) undefined
#endif

#ifndef bzero
#define bzero(...) undefined
#endif

// MARK: - <stdio.h> safe functions
#ifndef sprintf
#define sprintf(...) undefined
#endif

#ifndef vsprintf
#define vsprintf(...) undefined
#endif

#ifndef snprintf
#define snprintf(...) undefined
#endif

#ifndef vsnprintf
#define vsnprintf(...) undefined
#endif

// MARK: - <string.h> safe functions
#ifndef memccpy
#define memccpy(...) undefined
#endif

#ifndef memcpy
#define memcpy(...) undefined
#endif

#ifndef memmove
#define memmove(...) undefined
#endif

#ifndef memset
#define memset(...) undefined
#endif

#ifndef strcpy
#define strcpy(...) undefined
#endif

#ifndef strcat
#define strcat(...) undefined
#endif

#ifndef strncpy
#define strncpy(...) undefined
#endif

#ifndef strncat
#define strncat(...) undefined
#endif

#ifndef stpcpy
#define stpcpy(...) undefined
#endif

#ifndef stpncpy
#define stpncpy(...) undefined
#endif

#ifndef strlcpy
#define strlcpy(...) undefined
#endif

#ifndef strlcat
#define strlcat(...) undefined
#endif

#define HELLO_SIZEOF_RESULT() "hello", sizeof(result)

void test_stdio(va_list ap) {
	char result[100];
	const char hello[] = "hello";
	const char world[] = " world";

test_bcopy:	bcopy(hello, result, 6);
test_bzero:	bzero(result, sizeof(result));
test_sprintf:	sprintf(result, "hello %s!", "Darwin");
test_vsprintf:	vsprintf(result, "hello %s!", ap);
test_snprintf:	snprintf(result, sizeof(result), "hello %s!", "Darwin");
test_vsnprintf:	vsnprintf(result, sizeof(result), "hello %s!", ap);
test_memccpy:	memccpy(result, hello, 0, 6);
test_memcpy:	memcpy(result, hello, 6);
test_memmove:	memmove(result, hello, 6);
test_memset:	memset(result, 0, sizeof(result));
test_strcpy:	strcpy(result, hello);
test_strcat:	strcat(result, world);
test_strncpy:	strncpy(result, HELLO_SIZEOF_RESULT());
test_strncat:	strncat(result, world, 10);
test_stpcpy:	stpcpy(result, hello);
test_stpncpy:	stpncpy(result, hello, 10);
test_strlcpy:	strlcpy(result, HELLO_SIZEOF_RESULT());
test_strlcat:	strlcat(result, HELLO_SIZEOF_RESULT());
}
