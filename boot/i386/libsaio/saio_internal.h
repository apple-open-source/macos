/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __LIBSAIO_SAIO_INTERNAL_H
#define __LIBSAIO_SAIO_INTERNAL_H

#include "saio_types.h"

/* asm.s */
extern void   real_to_prot(void);
extern void   prot_to_real(void);
extern void   halt(void);
extern void   startprog(unsigned int address, void *arg);
extern void   loader(UInt32 code, UInt32 cmdptr);

/* bios.s */
extern void   bios(biosBuf_t *bb);

/* biosfn.c */
#ifdef EISA_SUPPORT
extern BOOL   eisa_present(void);
#endif
extern int    bgetc(void);
extern int    biosread(int dev, int cyl, int head, int sec, int num);
extern int    ebiosread(int dev, long sec, int count);
extern int    get_drive_info(int drive, struct driveInfo *dp);
extern void   putc(int ch);
extern void   putca(int ch, int attr, int repeat);
extern int    getc(void);
extern int    readKeyboardStatus(void);
extern unsigned int time18(void);
extern void   delay(int ms);
extern unsigned int get_diskinfo(int dev);
extern int    APMPresent(void);
extern int    APMConnect32(void);
extern int    memsize(int i);
extern void   video_mode(int mode);
extern void   setCursorPosition(int x, int y, int page);
extern void   setCursorType(int type);
extern void   getCursorPositionAndType(int *x, int *y, int *type);
extern void   scollPage(int x1, int y1, int x2, int y2, int attr, int rows, int dir);
extern void   clearScreenRows(int y1, int y2);
extern void   setActiveDisplayPage( int page );
extern unsigned long getMemoryMap(struct MemoryRange * rangeArray, unsigned long maxRangeCount,
                                  unsigned long * conMemSizePtr, unsigned long * extMemSizePtr);
extern unsigned long getExtendedMemorySize();
extern unsigned long getConventionalMemorySize();
extern void   sleep(int n);

/* bootstruct.c */
extern void   initKernBootStruct(int biosdev);
extern void   reserveKernBootStruct(void);
extern void   copyKernBootStruct(void);

/* cache.c */
extern void   CacheInit(CICell ih, long blockSize);
extern long   CacheRead(CICell ih, char *buffer, long long offset,
                        long length, long cache);

/* console.c */
extern BOOL   gVerboseMode;
extern BOOL   gErrors;
extern void   putchar(int ch);
extern int    getchar(void);
extern int    printf(const char *format, ...);
extern int    error(const char *format, ...);
extern int    verbose(const char *format, ...);
extern void   stop(const char *message);

/* disk.c */
extern BVRef  diskScanBootVolumes(int biosdev, int *count);
extern void   diskSeek(BVRef bvr, long long position);
extern int    diskRead(BVRef bvr, long addr, long length);
extern int    readBootSector(int biosdev, unsigned int secno, void *buffer);
extern void   turnOffFloppy(void);

/* load.c */
extern char   gHaveKernelCache;
extern long ThinFatFile(void **binary, unsigned long *length);
extern long DecodeMachO(void *binary, entry_t *rentry, char **raddr, int *rsize);

/* memory.c */
long AllocateKernelMemory( long inSize );
long AllocateMemoryRange(char * rangeName, long start, long length, long type);

/* misc.c */
extern void   enableA20(void);

/* nbp.c */
extern UInt32 nbpUnloadBaseCode();
extern BVRef  nbpScanBootVolumes(int biosdev, int *count);

/* stringTable.c */
extern char * newStringFromList(char **list, int *size);
extern int    stringLength(const char *table, int compress);
extern BOOL   getValueForStringTableKey(const char *table, const char *key, const char **val, int *size);
extern BOOL   removeKeyFromTable(const char *key, char *table);
extern char * newStringForStringTableKey(char *table, char *key);
extern char * newStringForKey(char *key);
extern BOOL   getValueForBootKey(const char *line, const char *match, const char **matchval, int *len);
extern BOOL   getValueForKey(const char *key, const char **val, int *size);
extern BOOL   getBoolForKey(const char *key);
extern BOOL   getIntForKey(const char *key, int *val);
extern int    loadConfigFile(const char *configFile, const char **table, BOOL allocTable);
extern int    loadConfigDir(const char *bundleName, BOOL useDefault, const char **table,
                            BOOL allocTable);
extern int    loadSystemConfig(const char *which, int size);
extern void   addConfig(const char *config);
extern char * newString(const char *oldString);

/* sys.c */
extern long   LoadFile(const char *fileSpec);
extern long   GetDirEntry(const char *dirSpec, long *dirIndex, const char **name,
                          long *flags, long *time);
extern long   GetFileInfo(const char *dirSpec, const char *name,
                          long *flags, long *time);
extern int    openmem(char *buf, int len);
extern int    open(const char *str, int how);
extern int    close(int fdesc);
extern int    file_size(int fdesc);
extern int    read(int fdesc, char *buf, int count);
extern int    b_lseek(int fdesc, int addr, int ptr);
extern int    tell(int fdesc);
extern const char * usrDevices(void);
extern const char * systemConfigDir(void);
extern struct dirstuff * opendir(const char *path);
extern int    closedir(struct dirstuff *dirp);
extern int    readdir(struct dirstuff *dirp, const char **name, long *flags, long *time);
extern void   flushdev(void);
extern int    currentdev(void);
extern int    switchdev(int dev);
extern BVRef  scanBootVolumes(int biosdev, int *count);
extern BVRef  selectBootVolume(BVRef chain);
extern void   getBootVolumeDescription(BVRef bvr, char *str, long strMaxLen);

extern int gBIOSDev;
extern int gBootFileType;

#endif /* !__LIBSAIO_SAIO_INTERNAL_H */
