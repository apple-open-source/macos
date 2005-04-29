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
#import <stdlib.h>
#import <sys/types.h>
#import <stdio.h>
#import <string.h>
#import <sys/stat.h>
#import <errno.h>
#ifdef __APPLE__
#import <fts.h>
#else
#import <sys/param.h>
#endif

#ifndef WIN32
#define IS_DIRECTORY(st_mode)  (((st_mode) & S_IFMT) == S_IFDIR)
#define IS_LINK(st_mode)	(((st_mode) & S_IFMT) == S_IFLNK)
#define IS_REGULAR(st_mode)	(((st_mode) & S_IFMT) == S_IFREG)
#ifdef __APPLE__
#import <sys/dir.h>
typedef struct direct DIRENT;
#else
#include <dirent.h>
typedef struct dirent DIRENT;
#endif
#else WIN32
#define IS_DIRECTORY(st_mode)	(((st_mode) & S_IFMT) == S_IFDIR)
#define IS_LINK(st_mode)	0
#define IS_REGULAR(st_mode)	(((st_mode) & S_IFMT) == S_IFREG)
#define lstat stat
#import <windows.h>
#import <objc/hashtable2.h>
#endif

#ifndef BOOL
#define BOOL char
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define PROGRAM_NAME "changes"


#ifdef WIN32
#define MAX_STRING_CHUNK 4096

typedef struct _StringTable {
   char strings[MAX_STRING_CHUNK];
   struct _StringTable * next;
} StringTable;
   
static BOOL searchForNewerFiles(char * dir, FILETIME refTime) {
    WIN32_FIND_DATA fileData;
    HANDLE cursor;
    char searchPattern[MAX_PATH];
    BOOL newerFiles = FALSE;
    StringTable allStrings;
    StringTable * stringTab = &allStrings;
    char *stringPtr = stringTab->strings;
#ifndef DIVE
    NXHashTable *table = NULL;
    NXHashState state;
#endif

    allStrings.next = NULL;
    *stringPtr = '\0';
    
    sprintf(searchPattern, "%s/*", dir);
    cursor = FindFirstFileA(searchPattern, &fileData);
    do {
       if (fileData.cFileName[0] == '.'  // skip . and ..
           && (fileData.cFileName[1] == '\0'
               || (fileData.cFileName[1] == '.' && fileData.cFileName[2] == '\0')))
          continue;
#ifdef DEBUG
    printf("Doing %s",fileData.cFileName);
#endif
       if (CompareFileTime(&refTime, &fileData.ftLastWriteTime) == -1) {
#ifdef DEBUG
    printf("-- NEWER!\n");
#endif
          newerFiles = TRUE;
          break;
       }
       if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
#ifdef DIVE
           char subdir[MAX_PATH];
           sprintf(subdir, "%s/%s", dir, fileData.cFileName);
 #ifdef DEBUG
     printf(" Diving: doing %s...\n", subdir);
 #endif
           if (searchForNewerFiles(subdir, refTime)) {
              newerFiles = TRUE;
              break;
           }
#else
          if (stringPtr + strlen(fileData.cFileName) + 2 >= &stringTab->strings[MAX_STRING_CHUNK]) {
             // append to string table
             stringTab->next = (StringTable *) malloc(sizeof(StringTable));  // just leak
             stringTab = stringTab->next;
             stringPtr = stringTab->strings;
             stringTab->next = NULL;
#ifdef DEBUG
    printf(" (reallocating)");
#endif
          }
          strcpy(stringPtr, fileData.cFileName);
          if (!table)
              table = NXCreateHashTable(NXStrPrototype, 0, NULL);
          NXHashInsert(table, stringPtr);
          stringPtr += strlen(fileData.cFileName) + 1;
#ifdef DEBUG
    printf(" (saving for later)");
#endif
#endif
       }
#ifdef DEBUG
    printf("\n");
#endif
    } while (!newerFiles && FindNextFileA(cursor, &fileData));
    FindClose(cursor);

#ifndef DIVE
    if (!newerFiles && table) {
       // check directories
       state = NXInitHashState(table);
       while (NXNextHashState(table, &state, (void **)&stringPtr)) {
          char subdir[MAX_PATH];
          sprintf(subdir, "%s/%s", dir, stringPtr);
#ifdef DEBUG
    printf(" Later: doing %s...\n", subdir);
#endif
          if (searchForNewerFiles(subdir, refTime)) {
             newerFiles = TRUE;
             break;
          }
       }
    }
#endif          
    return newerFiles;
}
#else
static BOOL searchForNewerFiles(char * dir, time_t refTime) {
    DIRENT	*dp;
    DIR		*dirp;
    dirp = opendir(dir);
    if (!dirp) 
       return TRUE;
    dp = readdir(dirp); /* Skip . */
    dp = readdir(dirp); /* Skip .. */
    for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	char   dirEntry[FILENAME_MAX+1];
	struct stat statbuf;
        strcpy(dirEntry, dir); strcat(dirEntry, "/"); strcat(dirEntry, dp->d_name);
	if (lstat(dirEntry, &statbuf) || (refTime < statbuf.st_mtime)) {
           closedir(dirp);
           return TRUE;  /* If error, just assume something changed.  Make will
                            investigate the problem for the user. */
	}
        if ((IS_DIRECTORY(statbuf.st_mode)) &&            
	    searchForNewerFiles(dirEntry, refTime)) {
           /* If the directory wasn't newer, it's still possible some file in 
	      it is, so recurse into it. */
          closedir(dirp);
          return TRUE;
        }	  
    }
    closedir(dirp);
    return FALSE;
}
#endif

#ifdef BSD44
static BOOL searchForNewerFiles(char * dir, time_t refTime) {
    const char * paths[2];
    register FTSENT *entry;
    FTS * tree;
    paths[0] = dir;
    paths[1] = NULL;
    if ((tree = fts_open(paths, FTS_PHYSICAL | FTS_COMFOLLOW, (int (*)())NULL)) == NULL) {
       perror("fts_open");
       return TRUE;
    }
    while ((entry = fts_read(tree)) != NULL) {
       switch (entry->fts_info) {
         case FTS_DP:
         case FTS_ERR:
         case FTS_DNR:
         case FTS_DOT:
         case FTS_NS:
          continue;
       }
       if (refTime < entry->fts_statp->st_mtime)
          return TRUE;
    }
    if (errno)
       perror("fts_read");
    fts_close(tree);
    return FALSE;
}
#endif BSD44

static void signifyUnknownChanges(char *filelist[], int numFiles) {
   int i;
   
   for (i = 0; filelist[i] && (i < numFiles); i++)
     printf("%s ",filelist[i]);
   printf("\n");
   exit(0);  /* Though this is the general error case, 
                we don't want to upset make by returning non-zero. 
		Rather, the conservative "they've all changed" message is
		sent to the makefiles. */
}


void main(int argc, char *argv[]) {
    if (argc < 2) {
       fprintf(stderr,"Usage: %s <referenceFile> <buildtype> [file1 ... fileN]\n",argv[0]);
       fprintf(stderr,"          (note: <referenceFile> muist contain <buildtype> for no changes to be identified)\n");
       exit(1);
    } else {
       char ** filelist;
       int j;
       FILE * ref;
       char refContents[128];
       char * refFile = argv[1];
       char * newBuildType = (argc > 2) ? argv[2] : NULL;
       int numFiles = argc - 3;
#ifndef WIN32
       struct stat refstatbuf, statbuf;
#else
       WIN32_FIND_DATA refFileData, fileData;
       HANDLE cursor;
#endif
       
       if (numFiles <= 0) {
          printf("\n");
          exit(0);   /* No files on input, so none to report. */
       }
       filelist = &argv[3];
       ref = fopen(refFile,"r");
       if (!ref)
          signifyUnknownChanges(filelist, numFiles);  /* No reference file */
       if (fgets(refContents,127,ref))
          refContents[strlen(refContents)-1] = '\0';  // remove newline
       if (!newBuildType || strcmp(refContents, newBuildType))
          signifyUnknownChanges(filelist, numFiles); /* target archs changed */
#ifndef WIN32
       if (lstat(refFile,&refstatbuf) != 0)
          signifyUnknownChanges(filelist, numFiles);  /* No reference file */
#else // WIN32
       cursor = FindFirstFileA(refFile,&refFileData);
       if (cursor == INVALID_HANDLE_VALUE)
          signifyUnknownChanges(filelist, numFiles);  /* No reference file */
       FindClose(cursor);
#endif       
       for (j = 0; filelist[j] && (j < numFiles); j++) {
#ifndef WIN32
          if ((lstat(filelist[j],&statbuf) != 0) ||
              (refstatbuf.st_mtime < statbuf.st_mtime) ) {
             /* Not there or newer, so report it as something to "make" */
             printf("%s ",filelist[j]);
             continue;
          }
          if ((IS_DIRECTORY(statbuf.st_mode)) &&
              searchForNewerFiles(filelist[j], refstatbuf.st_mtime)) {
             printf("%s ",filelist[j]);
          }
#else // WIN32
          cursor = FindFirstFileA(filelist[j],&fileData);
          FindClose(cursor);
          if ((cursor == INVALID_HANDLE_VALUE)
              || (CompareFileTime(&refFileData.ftLastWriteTime,
                                  &fileData.ftLastWriteTime) == -1)) {
             /* Not there or newer, so report it as something to "make" */
             printf("%s ",filelist[j]);
             continue;
          }
          if ((fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
              && searchForNewerFiles(filelist[j], refFileData.ftLastWriteTime)) {
             printf("%s ",filelist[j]);
          }
#endif
       }
       printf("\n");
       exit(0);
    }
}
