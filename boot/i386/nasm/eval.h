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
/* eval.h   header file for eval.c
 *
 * The Netwide Assembler is copyright (C) 1996 Simon Tatham and
 * Julian Hall. All rights reserved. The software is
 * redistributable under the licence given in the file "Licence"
 * distributed in the NASM archive.
 */

#ifndef NASM_EVAL_H
#define NASM_EVAL_H

/*
 * Called once to tell the evaluator what output format is
 * providing segment-base details, and what function can be used to
 * look labels up.
 */
void eval_global_info (struct ofmt *output, lfunc lookup_label);

/*
 * Called to set the information the evaluator needs: the value of
 * $ is set from `segment' and `offset' if `labelname' is NULL, and
 * otherwise the name of the current line's label is set from
 * `labelname' instead.
 */
void eval_info (char *labelname, long segment, long offset);

/*
 * The evaluator itself.
 */
expr *evaluate (scanner sc, void *scprivate, struct tokenval *tv,
		int *fwref, int critical, efunc report_error,
		struct eval_hints *hints);

#endif
