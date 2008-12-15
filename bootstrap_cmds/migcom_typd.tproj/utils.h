/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef	_UTILS_H
#define	_UTILS_H

/* stuff used by more than one of header.c, user.c, server.c */

typedef void	write_list_fn_t(FILE *file, const argument_t *arg);

extern void WriteImport(FILE *file, const_string_t filename);
extern void WriteRCSDecl(FILE *file, identifier_t name, const_string_t rcs);
extern void WriteBogusDefines(FILE *file);

extern void WriteList(FILE *file, const argument_t *args, write_list_fn_t *func,
		      u_int mask, const char *between, const char *after);

extern void WriteReverseList(FILE *file, const argument_t *args,
			     write_list_fn_t *func, u_int mask,
			     const char *between, const char *after);

/* good as arguments to WriteList */
extern write_list_fn_t WriteNameDecl;
extern write_list_fn_t WriteUserVarDecl;
extern write_list_fn_t WriteServerVarDecl;
extern write_list_fn_t WriteTypeDeclIn;
extern write_list_fn_t WriteTypeDeclOut;
extern write_list_fn_t WriteCheckDecl;

extern const char *ReturnTypeStr(const routine_t *rt);

extern const char *FetchUserType(const ipc_type_t *it);
extern const char *FetchServerType(const ipc_type_t *it);
extern void WriteFieldDeclPrim(FILE *file, const argument_t *arg,
			       const char *(*tfunc)(const ipc_type_t *it));

extern void WriteStructDecl(FILE *file, const argument_t *args,
			    write_list_fn_t *func, u_int mask,
			    const char *name);

extern void WriteStaticDecl(FILE *file, const ipc_type_t *it,
			    dealloc_t dealloc, boolean_t longform,
			    boolean_t inname, identifier_t name);

extern void WriteCopyType(FILE *file, const ipc_type_t *it,
			  const char *left, const char *right, ...);

extern void WritePackMsgType(FILE *file, const ipc_type_t *it,
			     dealloc_t dealloc, boolean_t longform,
			     boolean_t inname, const char *left,
			     const char *right, ...);

#endif	/* _UTILS_H */
