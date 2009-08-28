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

#if defined (__APPLE__) || defined (WIN32)
#include <libc.h>
#endif
#include <string.h>
#include <stdio.h>
#include <errno.h>
#ifdef WIN32
#include <direct.h>
#include <io.h>
#define F_OK 0
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#endif
#include <sys/param.h>

#define IS_LINK(st_mode)	(((st_mode) & S_IFMT) == S_IFLNK)

int main(int argc, char ** argv) {
   int i;
   char sourceDirBuf[MAXPATHLEN], *sourceDir = NULL, *destDir;
   char source[MAXPATHLEN], dest[MAXPATHLEN];

   if (argc < 3)
      exit(0);  // nothing to do

   destDir = argv[argc-1];
#ifdef __APPLE__
   if (!strncmp(destDir, "/private/Net", 12))
       destDir += 8;
#endif

   for (i = 1; i < argc-1; i++) {
#ifdef WIN32
      int j;
      for (j=0; argv[i][j]; j++)
         if (argv[i][j]=='\\')
            argv[i][j] = '/';
#endif

      if (*argv[i] == '/'
          || (isalpha(argv[i][0]) && argv[i][1] == ':' && argv[i][2] == '/')) {
         strcpy(source, argv[i]);
         sprintf(dest, "%s%s", destDir, strrchr(argv[i], '/'));
      } else {
         if (!sourceDir) {
#if defined(sun) || defined(hpux) || defined (WIN32)
            getcwd(sourceDirBuf, MAXPATHLEN);
#else
            getwd(sourceDirBuf);
#endif

#ifdef WIN32
            for (sourceDir = sourceDirBuf; *sourceDir; sourceDir++)
               if (*sourceDir == '\\')
                  *sourceDir = '/';
#endif
            sourceDir = sourceDirBuf;
#ifdef __APPLE__
            if (!strncmp(sourceDir,"/private", 8))
               sourceDir += 8;
#endif
         }
         sprintf(source, "%s/%s", sourceDir, argv[i]);
         sprintf(dest, "%s/%s", destDir, argv[i]);
      }
      if (access(source, F_OK) == 0) {
          FILE* f;
          char format[] = "#import \"%s\"\n";
          char oldDirective[MAXPATHLEN + sizeof (format)];
          char newDirective[MAXPATHLEN + sizeof (format)];
          sprintf (newDirective, format, source);
          f = fopen (dest, "r");
          if (f) {
              fgets (oldDirective, MAXPATHLEN + sizeof (format), f);
              fclose (f);
          }
          if (!f || 0!=strcmp (oldDirective, newDirective)) {
#ifndef WIN32
             struct stat buf;
             if (!lstat(dest,&buf) && IS_LINK(buf.st_mode))
                unlink(dest);
#endif             
              f = fopen (dest, "w");
              if (!f) {
                 perror(dest);
                 exit(1);
              }
              fputs (newDirective, f);
              fclose (f);
          }
      } else {
         perror(source);
         exit(1);
      }
   }
   exit(0);
}
