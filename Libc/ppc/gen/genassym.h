/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * genassym.h -- macros of use with genassym.c and assymdefs.c
 */

#import	<architecture/ppc/reg_help.h>
#import	<architecture/ppc/macro_help.h>

#define	PRINT_OFFSET(ptr_type, field)					\
MACRO_BEGIN								\
	print_define("", #ptr_type, #field);				\
	print_hex((unsigned) &(((ptr_type)0)->field));			\
MACRO_END

#define	PRINT_BIT_FIELD(reg_type, field)				\
MACRO_BEGIN								\
	reg_type __reg;							\
	CONTENTS(__reg) = 0;						\
	__reg.field = (typeof (__reg.field)) -1;			\
	print_define("", #reg_type, #field);				\
	print_hex(CONTENTS(__reg));					\
MACRO_END

#define	PRINT_ENUM(item)						\
MACRO_BEGIN								\
	print_define("", "", #item);					\
	print_hex((unsigned)item);					\
MACRO_END

#define	PRINT_DEFINE(macro)						\
MACRO_BEGIN								\
	print_define("", "", #macro);					\
	print_str(STRINGIFY(macro));					\
MACRO_END

#define	PRINT_CONSTANT(macro)						\
MACRO_BEGIN								\
	print_define("", "", #macro);					\
	print_hex((unsigned)macro);					\
MACRO_END

#define	PRINT_REGADDR(macro)						\
MACRO_BEGIN								\
	print_define("", "", #macro);					\
	print_hex((unsigned) &macro);					\
MACRO_END

#define	PRINT_REG_PAIR(struct_ptr, name0, name1)			\
MACRO_BEGIN								\
	print_define("", #struct_ptr, #name0 "_" #name1);		\
	print_hex((unsigned) &(((struct_ptr)0)->U_##name0##_##name1));	\
MACRO_END

#define	PRINT_BIT_POS(reg_type, field)					\
MACRO_BEGIN								\
	reg_type __reg;							\
	CONTENTS(__reg) = 0;						\
	__reg.field = 1;						\
	print_define("", #reg_type, #field "_BIT");			\
	print_dec((int) bit_num(#reg_type, #field, CONTENTS(__reg)));	\
MACRO_END

#define	PRINT_FIELD_INFO(reg_type, field)				\
MACRO_BEGIN								\
	reg_type __reg;							\
	CONTENTS(__reg) = 0;						\
	__reg.field = -1;						\
	print_define("", #reg_type, #field "_OFF");			\
	print_dec((int) bit_num(#reg_type, #field, CONTENTS(__reg)));	\
	print_define("", #reg_type, #field "_WIDTH");			\
	print_dec((int) field_width(#reg_type, #field, CONTENTS(__reg)));\
MACRO_END

#define	PRINT_L2_SIZE(type)						\
MACRO_BEGIN								\
	print_define("L2_SIZEOF", #type, "");				\
	print_dec((int) log2(sizeof(type), #type));			\
MACRO_END

#define	PRINT_SIZEOF(type)						\
MACRO_BEGIN								\
	print_define("SIZEOF", #type, "");				\
	print_dec((int) sizeof(type));					\
MACRO_END

#define	PRINT_L2_CONSTANT(macro)					\
MACRO_BEGIN								\
	print_define("L2", "", #macro);					\
	print_dec((int) log2(macro, #macro));				\
MACRO_END

typedef enum {
	MAJOR, MINOR
} cmt_level_t;

extern void comment(cmt_level_t level, const char *cmt);
extern void print_define(const char *prefix, const char *type_name,
 const char *field);
extern void print_dec(int val);
extern void print_hex(unsigned val);
extern void print_str(const char *str);
extern unsigned bit_num(char *reg_type, char *field, unsigned bits);
extern unsigned field_width(char *reg_type, char *field, unsigned bits);
extern unsigned log2(unsigned val, char *type);
extern void assymdefs(void);

