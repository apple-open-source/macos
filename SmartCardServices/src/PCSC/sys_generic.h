/*
 *  Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  sys_generic.h
 *  SmartCardServices
 */

/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id: sys_generic.h 123 2010-03-27 10:50:42Z ludovic.rousseau@gmail.com $
 */

/**
 * @file
 * @brief This handles abstract system level calls.
 */

#ifndef __sys_generic_h__
#define __sys_generic_h__

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/stat.h>

	int SYS_Initialize(void);

	int SYS_Mkdir(const char *, int);

	int SYS_GetPID(void);

	int SYS_Sleep(int);

	int SYS_USleep(int);

	int SYS_OpenFile(const char *, int, int);

	int SYS_CloseFile(int);

	int SYS_RemoveFile(const char *);

	int SYS_Chmod(const char *, int);

	int SYS_Chdir(const char *);

	int SYS_GetUID(void);

	int SYS_GetGID(void);

	int SYS_ChangePermissions(const char *, int);

	int SYS_SeekFile(int, int);

	int SYS_ReadFile(int, char *, int);

	int SYS_WriteFile(int, const char *, int);

	int SYS_GetPageSize(void);

	void *SYS_MemoryMap(int, int, int);

	void *SYS_PublicMemoryMap(int, int, int);

	void SYS_PublicMemoryUnmap(void *, int);

	int SYS_MMapSynchronize(void *, int);

	int SYS_Fork(void);

	int SYS_Daemon(int, int);

	int SYS_Stat(const char *pcFile, struct stat *psStatus);

	int SYS_Fstat(int);

	int SYS_Random(int, float, float);

	int SYS_GetSeed();

	void SYS_Exit(int);

	int SYS_Unlink(const char *pcFile);

#ifdef __cplusplus
}
#endif

#endif							/* __sys_generic_h__ */
