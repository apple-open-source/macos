/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* test standalone library */
#include <stdarg.h>

#define TEST_RESULT "The quick brown #5 fox did fine.\n"
#define TEST_FMT "The quick %s #%d fox did fine.\n"
#define TEST_ARGS "brown", 5
 
char error_buf[1024];
char *sa_rld_error_buf_addr = error_buf;
int sa_rld_error_buf_size = 1024;
char progname[] = "satest";

 /*
 * All printing of all messages goes through this function.
 */
void
vprint(
const char *format,
va_list ap)
{
	sa_rld_error_buf_size =- slvprintf(sa_rld_error_buf_addr,
					   sa_rld_error_buf_size,
					   format, ap);
}
/*
 * Print the error message and set the 'error' indication.
 */
void
error(
const char *format,
...)
{
    va_list ap;

//	if(arch_multiple)
//	    print_architecture_banner();
	va_start(ap, format);
//        print("%s: ", progname);
	vprint(format, ap);
//        print("\n");
	va_end(ap);
//	errors = 1;
}

main()
{
    char buf[1024];
    int args[4], ret;
    
    printf("Testing standalone library\n");
    printf("The following two lines should be identical:\n");
    sprintf(buf, TEST_FMT, TEST_ARGS);
    printf(buf);
    printf(TEST_RESULT);
    printf("\nThe following two lines should be identical:\n");
    error(TEST_FMT, TEST_ARGS);
    printf(error_buf);
    printf(TEST_RESULT); 
    
    printf("Comparing two strings:\n");
    ret = strcmp("abra","cadabra");
    printf("%d should not be 0.\n",ret);
    
    printf("Comparing two strings:\n");
    ret = strcmp("abra","abra");
    printf("%d should be 0.\n",ret);
    
    printf("Comparing two strings:\n");
    ret = strncmp("abcdefghij","abcdef",6);
    printf("%d should be 0.\n",ret);
    
    printf("printf(\"Test: %% 12s\\n\",\"abb\");\n");
    printf("Test:          abb\n");
    printf("Test: % 12s\n","abb");
    
    printf("printf(\"Test: %%04d\\n\",77);\n");
    printf("Test: 0077\n");
    printf("Test: %04d\n",77);
    
    printf("printf(\"Test: %% 2d\\n\",9);\n");
    printf("Test:  9\n");
    printf("Test: % 2d\n",9);

    printf("printf(\"Test: %% 8d\\n\",-9);\n");
    printf("Test:       -9\n");
    printf("Test: % 8d\n",-9);
}
