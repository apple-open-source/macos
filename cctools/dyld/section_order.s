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
/*
 * This file must be loaded first as it orders the section that appear in dyld.
 */
/* __TEXT segment */
	.text
	.const
	.cstring
	.literal8
	.picsymbol_stub

/* __DATA segment */
	/*
	 * The const_data and non_lazy_symbol_pointer sections should never be
	 * written on if dyld is not slid.  The sections lazy_symbol_pointer and
	 * picsymbol_stub (above in the TEXT segment) are zero sized as every
	 * thing staticly linked in.
	 */
	.const_data
	.non_lazy_symbol_pointer
	.lazy_symbol_pointer

	.data
	.zerofill __DATA, __bss
	.zerofill __DATA, __common, common, 0, 4
	/* The above data fits on one page. */

	/*
	 * The symbol symbol_blocks defined in symbols.c.  This is broken up
	 * into blocks and the end blocks are normally never touched.  And for
	 * launching prebound this should never be touched.
	 */
	.zerofill __DATA, __symbol_blocks, _symbol_blocks, 144360, 2
	.globl _symbol_blocks
	/*
	 * The symbol error_string and NSLinkEditError_fileName used in
	 * errors.c.  These are not used unless errors.  So they are last as it
	 * is * normally never touched.
	 */
	.zerofill __DATA, __error_buffers, _error_string, 1000, 2
	.globl _error_string
	.zerofill __DATA, __error_buffers, _NSLinkEditError_fileName, 1025, 2
	.globl _NSLinkEditError_fileName
