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

#import <stdio.h>
#import <string.h>

#define PROGRAM_NAME "arch_tool"

#define MAX_ARCH_LIST_SIZE 16 


typedef int compareFunc(const void *, const void *);

static int compare_strings(const char **a, const char **b) {
  return strcmp(*a, *b);
}

void usage(const char * program)
{
   fprintf(stderr,"Usage: %s -archify_list arch1 arch2 ...\n",program); 
   fprintf(stderr,"       %s -dearchify flags ...\n",program); 
   fprintf(stderr,"       %s -choose_obj_dir arch1 arch2 ...\n",program); 
   exit(1);
}

void main(int argc, char *argv[]) 
{
   char ** archNames;
   int i, numArchs;

   if (argc <= 1) {
      usage(PROGRAM_NAME);
   }

   numArchs = argc - 2;
   if (!strcmp(argv[1],"-archify_list")) {
      for (i = 2 ; i < argc; i++) {
         printf("-arch %s ",argv[i]);
      }
   } else if (!strcmp(argv[1],"-dearchify")) {
      for (i = 2 ; i < argc; i++) {
         if (!strcmp(argv[i],"-arch"))
            i++;
         else
            printf("%s ",argv[i]);
      }
   } else if (!strcmp(argv[1],"-choose_obj_dir")) {
      archNames = (char**) malloc((numArchs)*sizeof(char*));
      for (i = 2 ; i < argc; i++) {
         archNames[i-2] = argv[i];
      }
      qsort((void*) archNames, argc-2, sizeof(char*),
	    (compareFunc*)compare_strings);
      for (i = 0; i < numArchs; i++) {
         printf("%s",archNames[i]);
         if (i+1 < numArchs) putc('_',stdout);
      }
   } else {
      usage(PROGRAM_NAME);
   }
   putc('\n',stdout);      
   exit(0);
}
