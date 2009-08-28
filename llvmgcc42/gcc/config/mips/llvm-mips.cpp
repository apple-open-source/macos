/* LLVM LOCAL begin (ENTIRE FILE!)  */
/* High-level LLVM backend interface 
Copyright (C) 2008 Free Software Foundation, Inc.
Contributed by Bruno Cardoso Lopes (bruno.cardoso@gmail.com)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

//===----------------------------------------------------------------------===//
// This is a C++ source file that implements specific llvm mips ABI.
//===----------------------------------------------------------------------===//

#include "llvm-abi.h"
#include "llvm-mips-target.h"

/* Target hook for llvm-abi.h. It returns true if an aggregate of the
   specified type should be passed in memory. In mips EABI this is 
   true for aggregates with size > 32-bits. */
bool llvm_mips_should_pass_aggregate_in_memory(tree TreeType, const Type *Ty) {
  if (mips_abi == ABI_EABI)
  {
    enum machine_mode mode = TYPE_MODE(TreeType);
    int size;

    if (mode == DImode || mode == DFmode)
      return false;

    size = TreeType ? int_size_in_bytes (TreeType) : GET_MODE_SIZE (mode);
    return size == -1 || size > UNITS_PER_WORD;
  }
  return false; // TODO: support o32 ABI
}

/* LLVM LOCAL end (ENTIRE FILE!)  */
