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
extern void  real_to_prot(void);
extern void  prot_to_real(void);
extern void  halt(void);
extern void  startprog(unsigned int address);

/* bios.s */
extern void  bios(biosBuf_t *bb);
extern void  get_memsize(biosBuf_t *bb);

/* biosfn.c */
#ifdef EISA_SUPPORT
extern BOOL eisa_present(void);
#endif
extern int  bgetc(void);
extern int  biosread(int dev, int cyl, int head, int sec, int num);
extern int  ebiosread(int dev, long sec, int count);
extern void putc(int ch);
extern int  getc(void);
extern int  readKeyboardStatus(void);
extern unsigned int  time18(void);
extern unsigned int  get_diskinfo(int dev);
extern int  APMPresent(void);
extern int  APMConnect32(void);
extern int  memsize(int i);
extern void video_mode(int mode);
extern void setCursorPosition(int x, int y);

/* bootstruct.c */
extern void initKernBootStruct(void);

/* console.c */
extern BOOL gVerboseMode;
extern void putchar(int ch);
extern int  getchar(void);
extern int  printf(const char *format, ...);
extern int  error(const char *format, ...);
extern int  verbose(const char *format, ...);

/* disk.c */
extern void devopen(char *name, struct iob *io);
extern int devread(struct iob *io);
extern void devflush(void);

/* gets.c */
extern int  gets(char *buf, int len);
extern int  Gets(
    char *buf,
    int len,
    int timeout,
    char *prompt,
    char *message
);

/* load.c */
extern int openfile(char *filename, int ignored);

extern int  loadmacho(
    struct mach_header *head,
    int dev,
    int io,
    entry_t *entry,
    char **addr,
    int *size,
    int file_offset
);
extern int  loadprog(
    int dev,
    int fd,
    struct mach_header *headOut,
    entry_t *entry,
    char **addr,
    int *size
);

/* misc.c */
extern void  sleep(int n);
extern void  enableA20(void);
extern void  turnOffFloppy(void);
extern char *newString(char *oldString);

/* stringTable.c */
extern char * newStringFromList(char **list, int *size);
extern int    stringLength(char *table, int compress);
extern BOOL   getValueForStringTableKey(char *table, char *key, char **val, int *size);
extern BOOL   removeKeyFromTable(const char *key, char *table);
extern char * newStringForStringTableKey(char *table, char *key);
extern char * newStringForKey(char *key);
extern BOOL   getValueForBootKey(char *line, char *match, char **matchval, int *len);
extern BOOL   getValueForKey(char *key, char **val, int *size);
extern BOOL   getBoolForKey(char *key);
extern BOOL   getIntForKey(char *key, int *val);
#if 0
extern char * loadLocalizableStrings(char *name, char *tableName);
extern char * bundleLongName(char *bundleName, char *tableName);
extern int    loadOtherConfigs(int useDefault);
#endif
extern int    loadConfigFile( char *configFile, char **table, BOOL allocTable);
extern int    loadConfigDir(char *bundleName, BOOL useDefault, char **table,
                            BOOL allocTable);
extern int    loadSystemConfig(char *which, int size);
extern void   addConfig(char *config);

/* sys.c */
extern void stop(char *message);
extern int  openmem(char * buf, int len);
extern int  open(char *str, int how);
extern int  close(int fdesc);
extern int  file_size(int fdesc);
extern int  read(int fdesc, char *buf, int count);
extern int  b_lseek(int fdesc, unsigned int addr, int ptr);
extern int  tell(int fdesc);
extern void  flushdev(void);
extern char *usrDevices(void);
extern struct dirstuff * opendir(char *path);
extern int  closedir(struct dirstuff *dirp);
extern struct direct * readdir(struct dirstuff *dirp);
extern int  currentdev(void);
extern int  switchdev(int dev);

/* ufs_byteorder.c */
extern void byte_swap_superblock(struct fs *sb);
extern void byte_swap_inode_in(struct dinode *dc, struct dinode *ic);
extern void byte_swap_dir_block_in(char *addr, int count);

/*
 * vbe.c
 */
extern int  set_linear_video_mode(unsigned short mode);

/*
 * vga.c
 */
extern void set_video_mode(unsigned int mode);

#endif /* !__LIBSAIO_SAIO_INTERNAL_H */
