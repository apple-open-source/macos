/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import <stdio.h>
#import <stdlib.h>
#import <strings.h>

#define PROGRAM_NAME "frameworkFlags"

static const char * specialBuildTypes[] = {
   "debug",
   "profile",
   NULL
};

void usage(const char * program)
{
   fprintf(stderr,"Usage: %s -framework framework1 -framework framework2 ... <buildType>\n",program);
   fprintf(stderr,"       %s framework1 framework2 ... <buildType>\n",program); 
   exit(1);
}

int main(int argc, char *argv[]) 
{
   int i, numFrameworkFlags;
   const char * buildType;
   int specialBuildType = 0;

   
   if (argc == 1)
      usage(PROGRAM_NAME);

   buildType = argv[argc-1];
   for (i = 0; specialBuildTypes[i]; i++) {
      if (!strcmp(specialBuildTypes[i], buildType)) {
         specialBuildType = 1;
         break;
      }
   }
   numFrameworkFlags = argc - 2;
   for (i = 1; i <= numFrameworkFlags; i++) {
      if (!strcmp(argv[i],"-framework"))
         continue;
      if (specialBuildType)
         printf("-framework %s,_%s ", argv[i], buildType);
      else
         printf("-framework %s ", argv[i]);
   }
  
   putc('\n',stdout);      
   exit(0);
}
