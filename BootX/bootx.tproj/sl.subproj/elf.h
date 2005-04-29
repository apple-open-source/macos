/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*
 *  elf.h - Header for Elf stuctures.
 *
 *  Copyright (c) 2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

// Elf Signature is a 0x7f + ELF
#define kElfSignature (0x7f454C46)

#define kElfProgramTypeLoad (1)

#define kElfAddressMask (0x0FFFFFFF)

struct ElfHeader {
  long  signature;
  char  class;
  char  ei_data;
  char  ei_version;
  char  pad1;
  long  pad2;
  long  pad3;
  short type;
  short machine;
  long  version;
  long  entry;
  long  phoff;
  long  shoff;
  long  flags;
  short ehsize;
  short phentsize;
  short phnum;
  short shentsize;
  short shnum;
  short shstrndx;
};
typedef struct ElfHeader ElfHeader, *ElfHeaderPtr;

struct ProgramHeader {
  long  type;
  long  offset;
  long  vaddr;
  long  paddr;
  long  filesz;
  long  memsz;
  long  flags;
  long  align;
};
typedef struct ProgramHeader ProgramHeader, *ProgramHeaderPtr;

struct SectionHeader {
  long  name;
  long  type;
  long  flags;
  long  addr;
  long  offset;
  long  size;
  long  link;
  long  info;
  long  align;
  long  entsize;
};
typedef struct SectionHeader SectionHeader, *SectionHeaderPtr;
