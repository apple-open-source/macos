/* Macro definitions for i386 running under the win32 API Unix.
   Copyright 1995, 1996 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#ifndef TM_WIN32_H
#define TM_WIN32_H

#include "i386/tm-i386v.h"

#undef MAX_REGISTER_RAW_SIZE
#undef MAX_REGISTER_VIRTUAL_SIZE
#undef NUM_REGS
#undef REGISTER_BYTE
#undef REGISTER_BYTES
#undef REGISTER_CONVERTIBLE
#undef REGISTER_CONVERT_TO_RAW
#undef REGISTER_CONVERT_TO_VIRTUAL
#undef REGISTER_NAMES
#undef REGISTER_RAW_SIZE
#undef REGISTER_VIRTUAL_SIZE
#undef REGISTER_VIRTUAL_TYPE

/* Number of machine registers */

#define NUM_REGS 24

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */

/* the order of the first 8 registers must match the compiler's 
 * numbering scheme (which is the same as the 386 scheme)
 * also, this table must match regmap in i386-pinsn.c.
 */

#define REGISTER_NAMES { "eax",  "ecx",  "edx",  "ebx",  \
			 "esp",  "ebp",  "esi",  "edi",  \
			 "eip",  "ps",   "cs",   "ss",   \
			 "ds",   "es",   "fs",   "gs",   \
			 "st",   "st(1)","st(2)","st(3)",\
                         "st(4)","st(5)","st(6)","st(7)",}

#define FP0_REGNUM 16

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  */

#define REGISTER_BYTES (16 * 4 + 8 * 10)

/* Index within `registers' of the first byte of the space for
   register N.  */

#define REGISTER_BYTE(N) (((N) < 16) ? (N) * 4 : (((N) - 16) * 10) + (16 * 4))

/* Number of bytes of storage in the actual machine representation
   for register N.  */

#define REGISTER_RAW_SIZE(N) (((N) < 16) ? 4 : 10)

/* Number of bytes of storage in the program's representation
   for register N. */

#define REGISTER_VIRTUAL_SIZE(N) (((N) < 16) ? 4 : 10)

/* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE 10

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#define MAX_REGISTER_VIRTUAL_SIZE 10

/* Nonzero if register N requires conversion
   from raw format to virtual format.  */

#define REGISTER_CONVERTIBLE(N) \
  ((N < FP0_REGNUM) ? 0 : 1)

/* Convert data from raw format for register REGNUM in buffer FROM
   to virtual format with type TYPE in buffer TO.  */
extern void i387_to_double (char *, char *);


#define REGISTER_CONVERT_TO_VIRTUAL(REGNUM,TYPE,FROM,TO) \
{ \
  double VAL; \
  i387_to_double ((FROM), (char *)&VAL); \
  store_floating ((TO), TYPE_LENGTH (TYPE), VAL); \
}

extern void double_to_i387 (char *, char *);

#define REGISTER_CONVERT_TO_RAW(TYPE,REGNUM,FROM,TO) \
{ \
  double VAL = extract_floating ((FROM), TYPE_LENGTH (TYPE)); \
  double_to_i387((char *)&VAL, (TO)); \
}

/* Should we use EXTRACT_STRUCT_VALUE_ADDRESS instead of
   EXTRACT_RETURN_VALUE?  GCC_P is true if compiled with gcc
   and TYPE is the type (which is known to be struct, union or array).

   On most machines, the struct convention is used unless we are
   using gcc and the type is of a special size.

   On Win32 we do not successfully detect the fact that we are using 
   GCC, so don't use that test -- just go with the size of the struct */

#undef  USE_STRUCT_CONVENTION
#define USE_STRUCT_CONVENTION(gcc_p, type)      \
  (!((TYPE_LENGTH (value_type) == 1             \
   || TYPE_LENGTH (value_type) == 2             \
   || TYPE_LENGTH (value_type) == 4             \
   || TYPE_LENGTH (value_type) == 8             \
   )                                            \
 ))

#undef  EXTRACT_RETURN_VALUE	/* MVS: let's get this right */
#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF)        \
  if (TYPE_CODE(TYPE) == TYPE_CODE_FLT)                 \
  {                                                     \
    REGISTER_CONVERT_TO_VIRTUAL (FP0_REGNUM, (TYPE),    \
      &(REGBUF)[REGISTER_BYTE (FP0_REGNUM)], (VALBUF)); \
  }                                                     \
  else if (TYPE_LENGTH(TYPE) == 8) { /* 8-byte struct or long long int */\
    memcpy((VALBUF),     (REGBUF) + REGISTER_BYTE(0), 4);	/* AX */ \
    memcpy((VALBUF) + 4, (REGBUF) + REGISTER_BYTE(2), 4);	/* DX */ \
  }                                                     \
  else                                                  \
    memcpy ((VALBUF), (REGBUF), TYPE_LENGTH (TYPE))

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#define REGISTER_VIRTUAL_TYPE(N) \
  ((N < FP0_REGNUM) ? builtin_type_int : \
   builtin_type_double)

#define NAMES_HAVE_UNDERSCORE


#define IN_SOLIB_CALL_TRAMPOLINE(pc, name) skip_trampoline_code (pc, name)
#define SKIP_TRAMPOLINE_CODE(pc)           skip_trampoline_code (pc, 0)
extern CORE_ADDR skip_trampoline_code (CORE_ADDR pc, char *name);


#ifdef NeXT_PDO
/* Compiler's internal register numbering scheme */

/* Note that in the stab, gcc has already mapped the register number from
 * its internal scheme to our numbering scheme.
 * We only need this map, therefore, when trying to figure out
 * in what registers the compiler has allocated a value which spans
 * multiple registers.
 */
#define GCC_REMAPPED_REGNUMS(n)	((n)==0 ? 0 : (			\
                                 (n)==1 ? 2 : (			\
                                 (n)==2 ? 1 : (			\
                                 (n)==3 ? 3 : (			\
                                 (n)==4 ? 6 : (			\
                                 (n)==5 ? 7 : (			\
                                 (n)==6 ? 4 : (			\
                                 (n)==7 ? 5 : (			\
                                 (n)+4)))))))))
#define NUM_GCC_REMAPPED_REGNUMS (14)

#define CREATE_DUMMY_THREAD	create_call_dummy_thread
#define DESTROY_DUMMY_THREAD	destroy_call_dummy_thread

/* On Windows95, the Kernel can and does write to the user's stack when an exception
 * occurs.  We want to leave enough room on the stack when gdb writes the call dummy
 * so that the kernel doesn't blow away everything the call dummy needs on the stack
 * to execute.
 * Empirically, this area seems to be 512 bytes large.  If the kernel is using the
 * user's stack to execute some exception code on, however, this area could be
 * arbitrarily large.  I set this to be 1024 bytes for now until more information
 * comes to light.
 */
#define CALL_DUMMY_STACK_PADDING	(isWin95() ? 1024 : 0)

/* Some better stopping point checks */
#undef FRAME_CHAIN_VALID
#define FRAME_CHAIN_VALID(chain, this)  frame_chain_valid(chain, this)

#endif	/* NeXT_PDO */

#endif /* TM_WIN32_H */
