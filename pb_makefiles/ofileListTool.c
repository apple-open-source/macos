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
#import <string.h>
#import <stdlib.h>
#import <errno.h>
#import <sys/param.h>
#ifndef WIN32
#import <unistd.h>
#endif

#define PROGRAM_NAME "ofileListTool"

static void usage(const char * program)
{
   fprintf(stderr,"Usage: %s file1.o file2.o ... list1.ofileList list2.ofileList ... [-o ofileList] [-removePrefix <prefix>] [-inDirectory <ofileDir>]\n",program);
   exit(1);
}


int main(int argc, const char *argv[])
{
   const char * ofiles[argc];
   const char * listFiles[argc];
   const char * prefixes[argc];
   const char * str;
   char ofileDirBuf[MAXPATHLEN];
   int prefixLen[argc], ofileDirLen = 0;
   int i, j, numOfiles = 0, numLists = 0, numPrefixes = 0;
   FILE * outs = stdout;

   if (argc <= 1)
      usage(PROGRAM_NAME);

   for (i = 1 ; i < argc; i++) {
      if (argv[i][0] == '-') {
         if (!strncmp(argv[i],"-o", 2)) {
            if (++i < argc) {
#ifndef WIN32
               unlink(argv[i]);    /* In case this is a hard-link a la SGS */
#endif               
               outs = fopen(argv[i], "w");
               if (!outs) {
                  fprintf(stderr,"ofileListTool: Couldn't open output file %s (%s)",
                          argv[i],strerror(errno));
                  exit(1);
               }
            }
         } else if (!strncmp(argv[i],"-removePrefix", 2)) {
            if (++i < argc) {
               int len = prefixLen[numPrefixes] = strlen(argv[i]);
               prefixes[numPrefixes++] = argv[i];
               // make ./path equivalent to path
               if (len > 2 && !strncmp(argv[i], "./", 2)) {
                  prefixes[numPrefixes] = &argv[i][2];
                  prefixLen[numPrefixes++] = len - 2;
               }
            }
         } else if (!strncmp(argv[i],"-inDirectory", 2)) {
            if (++i < argc) {
               sprintf(ofileDirBuf, "%s/", argv[i]);
               ofileDirLen = strlen(argv[i])+1;
            }
         } else {
            usage(PROGRAM_NAME);
         }
      } else {
         const char * extension = strrchr(argv[i],'.');
         int good = 0;
         if (extension && !strcmp(extension,".o")) {
            ofiles[numOfiles++] = argv[i];
            good = 1;
         } else if (extension && !strcmp(extension, ".ofileList")) {
            listFiles[numLists++] = argv[i];
            good = 1;            
         }
         if (!good)
            fprintf(stderr,"%s: %s is not a .o or .ofileList.  Ignoring....\n",
                    PROGRAM_NAME, argv[i]);
      }
   }
   for (i = 0; i < numOfiles; i++) {
      if (ofileDirLen) {
         strcpy((char *)(ofileDirBuf+ofileDirLen), ofiles[i]);
         str = ofileDirBuf;
      } else {
         str = ofiles[i];
      }
      for (j = 0; j < numPrefixes; j++)
         while (!strncmp(str, prefixes[j], prefixLen[j]))
            str = (const char*) (str + prefixLen[j]);
      fprintf(outs, "%s\n", str);
   }
   for (i = 0; i < numLists; i++) {
      FILE * ins = fopen(listFiles[i], "r");
      if (!ins) {
         fprintf(stderr,"%s: %s cannot be read.  Ignoring....\n",
                 PROGRAM_NAME, listFiles[i]);
      } else {
         char buf[MAXPATHLEN+1];
         while (fgets(buf, MAXPATHLEN, ins)) {
            str = buf;
            for (j = 0; j < numPrefixes; j++)
               while (!strncmp(str, prefixes[j], prefixLen[j]))
                  str = (const char*) (buf + prefixLen[j]);
            if (str)
               fputs(str, outs);
         }
      }
   }
   fclose(outs);
   exit(0);
}
