/* APPLE LOCAL: A tdep file used by both the i386 and the amd64 (x86_64) 
   targets.  These two architectures are similar enough that we can share 
   the same algorithms between them easily.  */

#include "defs.h"
#include "gdb_assert.h"
#include "dis-asm.h"
#include "gdbcore.h"
#include "gdbarch.h"
#include "ui-out.h"
#include "target.h"
#include "gdbtypes.h"
#include "frame.h"
#include "objfiles.h"
#include "exceptions.h"
#include "inferior.h"
#include "objc-lang.h" /* for pc_in_objc_trampoline_p */
#include "regcache.h"  /* register_size */
#include "command.h"
#include "gdbcmd.h"

#include "x86-shared-tdep.h"
#include "i386-tdep.h"
#include "amd64-tdep.h"

int debug_x86bt;
static void
show_debug_x86bt (struct ui_file *file, int from_tty,
                  struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("x86 backtracer debugging is %s.\n"), value);
}


/* The REX.W prefix byte is used in ModR/M instructions - the 0th bit is
   added to the destination register number and the 2nd bit is added to
   the source register number.  */

#define REX_W_PREFIX_P(op) (((op) & (~0x5)) == 0x48)
#define REX_W_R(op) (((op) & 0x4) >> 2)
#define REX_W_B(op) ((op) & 0x1)

/* Return true if the i386/x86-64 instruction at MEMADDR is a
   push %esp, %ebp instruction that typically appears in a function prologue. */

static int
x86_mov_esp_ebp_pattern_p (CORE_ADDR memaddr)
{
  gdb_byte op = read_memory_unsigned_integer (memaddr, 1);

  /* Look for & consume the x86-64 REX.W prefix byte 0x48
     with no register bit additions */
  if (op == 0x48)
    {
      memaddr++;
      op = read_memory_unsigned_integer (memaddr, 1);
    }

  /* Two ways to write push %esp, %ebp: [0x8b 0xec] and [0x89 0xe5] */
  if (op == 0x8b)
    if (read_memory_unsigned_integer (memaddr + 1, 1) == 0xec)
      return 1;
  if (op == 0x89)
    if (read_memory_unsigned_integer (memaddr + 1, 1) == 0xe5)
      return 1;

  return 0;
}

/* Return true if the i386/x86-64 instruction at MEMADDR is a push %ebp [0x55]
   or push $0x0 [0x6a 0x00].  */

static int
x86_push_ebp_pattern_p (CORE_ADDR memaddr)
{
  gdb_byte op = read_memory_unsigned_integer (memaddr, 1);
  if (op == 0x55)
    return 1;
  if (op == 0x6a)
    if (read_memory_unsigned_integer (memaddr + 1, 1) == 0x00)
      return 1;

  return 0;
}

/* Return true if the i386/x86-64 instruction at MEMADDR is a ret/retq.  */

static int
x86_ret_pattern_p (CORE_ADDR memaddr)
{
  gdb_byte op = read_memory_unsigned_integer (memaddr, 1); 

  /* C2 and CA are 'RET imm16' instructions.  */
  if (op == 0xc3 || op == 0xcb || op == 0xc2 || op == 0xca)
    return 1;

  /* Let's recognize LEAVE as well.  */
  if (op == 0xc9)
    return 1;

  return 0;
}

/* Return 1 if MEMADDR is a call + pop instruction sequence to set up a
   register as the picbase.
   If REGNUM is non-NULL, it is filled in with the register # that the picbase
   address was popped into.  

   Looking for the sequence
     call L1        [ 0xe8 0x00 0x00 0x00 0x00 ]
   L1:
     pop %ebx       [ 0x5b ]

   The total length of this call + pop instruction sequence is 6 bytes.

   This sequence only occurs in i386 code.  */

static int
x86_picbase_setup_pattern_p (CORE_ADDR memaddr, int *regnum)
{
  gdb_byte op;
  if (length_of_this_instruction (memaddr) != 5
      || read_memory_unsigned_integer (memaddr, 1) != 0xe8
      || read_memory_unsigned_integer (memaddr + 1, 4) != 0)
    return 0;
  
  op = read_memory_unsigned_integer (memaddr + 5, 1);
  if ((op & 0xf8) != 0x58)
    return 0;

  if (regnum)
    *regnum = (int) op & 0x7;

  return 1;
}

int
i386_nonvolatile_regnum_p (int r)
{
  if (r == I386_EBX_REGNUM || r == I386_EBP_REGNUM
      || r == I386_ESI_REGNUM || r == I386_EDI_REGNUM
      || r == I386_ESP_REGNUM)
    return 1;
  else
    return 0;
}

int
i386_argument_regnum_p (int r)
{
  return 0;
}

int
x86_64_nonvolatile_regnum_p (int r)
{
  if (r == AMD64_RBX_REGNUM || r == AMD64_RSP_REGNUM
      || r == AMD64_RBP_REGNUM
      || r == AMD64_R8_REGNUM + 4 || r == AMD64_R8_REGNUM + 5
      || r == AMD64_R8_REGNUM + 6 || r == AMD64_R8_REGNUM + 7)
    return 1;
  else
    return 0;
}

int
x86_64_argument_regnum_p (int r)
{
  if (r == AMD64_RDI_REGNUM || r == AMD64_RSI_REGNUM
      || r == AMD64_RDX_REGNUM || r == AMD64_RCX_REGNUM
      || r == AMD64_R8_REGNUM || r == AMD64_R8_REGNUM + 1)
    return 1;
  else
    return 0;
}

int
i386_machine_regno_to_gdb_regno (int r)
{
  return r;
}

/* The register numbers we get from masking out bits in the x86_64
   instructions do not match the register numbers defined in gdb's
   enum amd64_regnum OR in the kernel's struct __darwin_x86_thread_state64.
   So we need to reorder a few of the first 8 registers when we're
   getting reg nums from assembly instructions.  */

int
x86_64_machine_regno_to_gdb_regno (int r)
{
  switch (r)
    {
      case 0: return AMD64_RAX_REGNUM;
      case 1: return AMD64_RCX_REGNUM;
      case 2: return AMD64_RDX_REGNUM;
      case 3: return AMD64_RBX_REGNUM;
      case 4: return AMD64_RSP_REGNUM;
      case 5: return AMD64_RBP_REGNUM;
      case 6: return AMD64_RSI_REGNUM;
      case 7: return AMD64_RDI_REGNUM;
      default: return r;
    }
}


/* Return 1 if MEMADDR is a push register instruction, regardless of whether
   it is a non-volatile reg or not.  (useful for tracking esp changes)
   If REGNUM is non-NULL, it is filled with the register # that was pushed.  */

static int
x86_push_reg_p (CORE_ADDR memaddr, int *regnum)
{
  gdb_byte op;
  int prefix_bit = 0;
  op = read_memory_unsigned_integer (memaddr, 1);

  /* If this is a REX.W prefix opcode with the B bit set to 1, we'll need
     to add a high bit to the register number in the next byte.  */
  if (op == 0x41)
    {
      prefix_bit = 1 << 3;
      op = read_memory_unsigned_integer (memaddr + 1, 1);
    }

  if (op >= 0x50 && op <= 0x57)
    {
      int r = (op - 0x50) | prefix_bit;
      if (regnum != NULL)
        *regnum = r;
      return 1;
    }
  return 0;
}

/* Return 1 if this is a POP insn.  */

static int
x86_pop_p (CORE_ADDR memaddr)
{
  gdb_byte op = read_memory_unsigned_integer (memaddr, 1);

  /* A REX.W prefix opcode indicating that the register being popped is
     in the second half of the reg set.  */
  if (op == 0x41)
    op = read_memory_unsigned_integer (memaddr + 1, 1);
    
  /* 58+ rd      POP r32 */
  if ((op & 0xf8) == 0x58)
    return 1;
  return 0;
}

/* Return 1 if MEMADDR is a mov register instruction of a register to
   the local stack frame.

   x86_64: mov    %rax,-0x10(%rbp)   [ 0x48 0x89 0x45 0xf0 ]
   i386:   mov    %eax,-0xc(%ebp)    [ 0x89 0x45 0xf4 ]

   If REGNUM is non-NULL, it is filled in with the register # that was saved.
   If OFFSET is non-NULL, it is filled in with the stack frame offset that the 
   reg was saved to.  
   e.g. for 'mov %ebx,-0xc(%ebp)', *OFFSET will be set to 12, indicating that 
   EBX was saved at EBP -12.  */

static int
x86_mov_reg_to_local_stack_frame_p (CORE_ADDR memaddr, int *regnum, int *offset)
{
  gdb_byte op;
  int source_reg_mod = 0;
  int target_reg_mod = 0;
  op = read_memory_unsigned_integer (memaddr, 1);

  if (REX_W_PREFIX_P (op))
    {
      source_reg_mod = REX_W_R (op) << 3;
      target_reg_mod = REX_W_B (op) << 3;
      /* The target register should be ebp/rbp which doesn't require an
         extra bit to specify.  */
      if (target_reg_mod == 1)
        return 0;
      op = read_memory_unsigned_integer (++memaddr, 1);
    }

  /* Detect a 'mov %ebx,-0xc(%ebp)' type instruction.
     aka 'MOV r/m32,r32' in Intel syntax terms.  */
  if (op == 0x89)
    {
      op = read_memory_unsigned_integer (memaddr + 1, 1);
      /* Mask off the 3-5 bits which indicate the destination register
         if this is a ModR/M byte.  */
      int op_sans_dest_reg = op & (~0x38);

      /* Look for a ModR/M byte with Mod bits 01 and R/M bits 101 
         ([EBP] disp8), i.e. 01xxx101
         I want to see a destination of ebp-disp8 or ebp-disp32.   */
      if (op_sans_dest_reg != 0x45 && op_sans_dest_reg != 0x85)
        return 0;

      int saved_reg = ((op >> 3) & 0x7) | source_reg_mod;
      int off;
      if (op_sans_dest_reg == 0x45)
        off = (int8_t) read_memory_unsigned_integer (memaddr + 2, 1);
      if (op_sans_dest_reg == 0x85)
        off = (uint32_t) read_memory_unsigned_integer (memaddr + 2, 4);

      if (off > 0)
        return 0;

      if (regnum != NULL)
        *regnum = saved_reg;
      if (offset != NULL)
        *offset = abs (off);
      return 1;
    }

  return 0;
}

/* Return 1 if MEMADDR is a mov register instruction of a function argument
   on the stack to a reg.  x86_64 passes most of its arguments in registers
   so this is mostly used in i386 code.

   x86_64:   mov    0x10(%rbp),%rax    [ 0x48 0x8b 0x45 0x10 ]
   i386:     mov    0x8(%ebp),%eax     [ 0x8b 0x45 0x08 ]

   If REGNUM is non-NULL, it is filled in with the register # is used.  */

static int
x86_mov_func_arg_to_reg_p (CORE_ADDR memaddr, int *regnum, int *offset)
{
  gdb_byte op;
  int source_reg_mod = 0;
  int target_reg_mod = 0;
  op = read_memory_unsigned_integer (memaddr, 1);

  if (REX_W_PREFIX_P (op))
    {
      source_reg_mod = REX_W_R (op) << 3;
      target_reg_mod = REX_W_B (op) << 3;
      /* The target register should be ebp/rbp which doesn't require an
         extra bit to specify.  */
      if (target_reg_mod == 1)
        return 0;
      op = read_memory_unsigned_integer (++memaddr, 1);
    }

  if (op == 0x8b)
    {
      op = read_memory_unsigned_integer (memaddr + 1, 1);
      /* Mask off the 3-5 bits which indicate the destination register
         if this is a ModR/M byte.  */
      int op_sans_dest_reg = op & (~0x38);

      /* Look for a ModR/M byte with Mod bits 01 and R/M bits 101 
         ([EBP] disp8), i.e. 01xxx101
         I want to see a destination of ebp-disp8 or ebp-disp32.   */
      if (op_sans_dest_reg != 0x45 && op_sans_dest_reg != 0x85)
        return 0;

      int saved_reg = ((op >> 3) & 0x7) | source_reg_mod;
      int off;
      if (op_sans_dest_reg == 0x45)
        off = (int8_t) read_memory_unsigned_integer (memaddr + 2, 1);
      if (op_sans_dest_reg == 0x85)
        off = (int32_t) read_memory_unsigned_integer (memaddr + 2, 4);

      if (offset != NULL)
        *offset = off;
      if (regnum != NULL)
        *regnum = saved_reg;
      return 1;
    }
  return 0;
}


/* gcc 4.2 seems to like to copy its arguments from their passed-in
   location (at -O0) to inside the local stack frame as a part of its
   prologue sequence.  If we stop before these instructions are executed,
   the DWARF for these args will point to the uninitialized memory in the
   current func's stack frame.  So try to detect the pattern of things like

0x1b7c90 <normalize+6>:     mov    0x8(%ebp),%eax
0x1b7c93 <normalize+9>:     mov    %eax,-0x10(%ebp)
0x1b7c96 <normalize+12>:    mov    0xc(%ebp),%eax
0x1b7c99 <normalize+15>:    mov    %eax,-0xc(%ebp)

   and stop after them.

   This function returns 0 if this memory address is NOT these pair of
   instructions.  It returns the number of bytes to skip if it is.  

   This sequence only occurs in i386 code.  */

static int
x86_unopt_arg_copy_to_local_stack_p (CORE_ADDR memaddr)
{
  int reg1, off1, reg2, off2;

  if (x86_mov_func_arg_to_reg_p (memaddr, &reg1, &off1) == 0)
    return 0;

  memaddr += length_of_this_instruction (memaddr);

  if (x86_mov_reg_to_local_stack_frame_p (memaddr, &reg2, &off2) == 0)
    return 0;

  if (reg1 != reg2)
    return 0;

  /* The offset from ebp of the first insn should be positive; 
     the offset from ebp of the second insn should be negative.  */
  if (off1 < 0 || off2 > 0)
    return 0;

  return 1;
}

/* Return non-zero if MEMADDR is an instruction that subtracts a value from the
   stack pointer (esp) - typically seen in the middle of a function prologue.
   In an -fomit-frame-pointer program this instruction may be the only 
   identifiable prologue instruction outside register saves.  

   The return value is the esp displacement.  0 indicates that this is not
   a sub $esp instruction.  A positive value, e.g. 0x1c, indicates that 12 is
   to be subtracted from $esp.

   There are several patterns for this instruction; the disassembler should be
   used to iterate over it.  */

static int32_t
x86_sub_esp_pattern_p (CORE_ADDR memaddr)
{
  gdb_byte op, next_op;

  op = read_memory_unsigned_integer (memaddr, 1);
  /* Consume x86_64 REX_W prefix with no register bit additions. */
  if (op == 0x48)
    {
      op = read_memory_unsigned_integer (++memaddr, 1);
    }
  next_op = read_memory_unsigned_integer (memaddr + 1, 1);

  /* sub with 8-bit immediate operand.  */
  if (op == 0x83 && next_op == 0xec)
    {
      return (int8_t) read_memory_integer (memaddr + 2, 1);
    }

  /* sub with 32-bit immediate operand.  */
  if (op == 0x81 && next_op == 0xec)
    {
      return (int32_t) read_memory_integer (memaddr + 2, 4);
    }

  /* Handle [0x83, 0xc4] for imm8 with neg values?  
     Or [0x81, 0xc4] for imm32 with neg values?  Does gcc ever emit these?  
     The old i386_find_esp_adjustments func used to detect them.  */

  return 0;
}

/* Returns true if MEMADDR is a JMP or one of the Jcc (Jump if Condition) 
   instructions.  */

static int
x86_jump_insn_p (CORE_ADDR memaddr)
{
  gdb_byte op;
  op = read_memory_unsigned_integer (memaddr, 1);
  
  /* Jcc */
  if (op >= 0x70 && op <= 0x7f)
    return 1;
  if (op == 0xe3)
    return 1;

  /* JMP */
  if (op == 0xe9 || op == 0xea || op == 0xeb)
    return 1;

  /* Jcc */
  if (op == 0x0f)
    {
      op = read_memory_unsigned_integer (memaddr + 1, 1);
      if (op >= 0x80 && op <= 0x8f)
        return 1;
    }

  return 0;
}

/* Returns true if MEMADDR is a 3-instruction sequence that copies variables 
   which are a part of a block's context into the local stack frame from the 
   first argument ("_self") in the block's prologue setup.
   On x86_64 this looks like
        <__helper_1+15>:     mov    -0x18(%rbp),%rax   [ 0x48 0x8b 0x45 0xe8 ]
        <__helper_1+19>:     mov    0x30(%rax),%rax    [ 0x48 0x8b 0x40 0x30 ]
        <__helper_1+23>:     mov    %rax,-0x10(%rbp)   [ 0x48 0x89 0x45 0xf0 ]
   On i386 this looks like
        <__helper_1+13>:     mov    0x8(%ebp),%eax     [ 0x8b 0x45 0x08 ]
        <__helper_1+16>:     mov    0x1c(%eax),%eax    [ 0x8b 0x40 0x1c ]
        <__helper_1+19>:     mov    %eax,-0xc(%ebp)    [ 0x89 0x45 0xf4 ]

   To recognize this sequence, I expect the same register to be used to hold
   the pointer to _self and then the value being retrieved from _self, and
   I expect a positive initial offset from the frame pointer in the beginning
   and a negative offset from the frame pointer at the end.  This will not
   work correctly for an -fomit-frame-pointer style function which refers to
   everything via the stack pointer instead of the frame pointer.  */

static int
x86_blocks_context_var_copy_pattern_p (CORE_ADDR memaddr, int wordsize)
{
  int reg1, off1, reg2, reg3, off3;
  if (x86_mov_func_arg_to_reg_p (memaddr, &reg1, &off1) == 0)
    return 0;
  
  memaddr += length_of_this_instruction (memaddr);

  /* Now look for a 'mov 0x30(%rax),%rax' type instruction  */

  gdb_byte op;
  int off = 0;
  op = read_memory_unsigned_integer (memaddr, 1);
    
  if (REX_W_PREFIX_P (op))
    {
      int source_reg_mod = REX_W_R (op) << 3;
      int target_reg_mod = REX_W_B (op) << 3;
      if (source_reg_mod != target_reg_mod)
        return 0;
      off++;
      op = read_memory_unsigned_integer (memaddr + off, 1);
    }
  if (op != 0x8b)
    return 0;

  off++;
  op = read_memory_unsigned_integer (memaddr + off, 1);
  /* See if this ModR/M has a Mod of 01 -- (reg)+disp8  */
  if ((op & ~0x3f) != 0x40)
    return 0;
  /* Extract the source & target regs from the ModR/M byte */
  int sreg = (op & 0x38) >> 3;
  int treg = (op & 0x7);
  if (sreg != treg)
    return 0;
  reg2 = sreg;

  memaddr += length_of_this_instruction (memaddr);

  if (x86_mov_reg_to_local_stack_frame_p (memaddr, &reg3, &off3) == 0)
    return 0;

  if (reg1 != reg2 || reg2 != reg3)
    return 0;
    
  /* The offset from ebp of the first insn should be positive;
     the offset from ebp of the second insn should be negative.  */
  if (wordsize == 4 && off1 < 0)
    return 0;
    
  /* The offset from rbp of the first insn should be negative;
     the offset from rbp of the second insn should be negative.  */
  if (wordsize == 8 && off1 > 0)
    return 0;
  
  return 1;
}



static struct type *
init_vector_type (struct type *elt_type, int n)
{
  struct type *array_type;
 
  array_type = create_array_type (0, elt_type,
                                  create_range_type (0, builtin_type_int,
                                                     0, n-1));
  TYPE_FLAGS (array_type) |= TYPE_FLAG_VECTOR;
  return array_type;
}

struct type *
build_builtin_type_vec128i_big (void)
{
  /* 128-bit Intel SIMD registers */
  struct type *t;

  struct type *int16_big;
  struct type *int32_big;
  struct type *int64_big;
  struct type *uint128_big;

  struct type *v4_float_big;
  struct type *v2_double_big;

  struct type *v16_int8_big;
  struct type *v8_int16_big;
  struct type *v4_int32_big;
  struct type *v2_int64_big;

  int16_big = init_type (TYPE_CODE_INT, 16 / 8, 0, "int16_t", (struct objfile *) NULL);
  TYPE_BYTE_ORDER (int16_big) = BFD_ENDIAN_BIG;
  int32_big = init_type (TYPE_CODE_INT, 32 / 8, 0, "int32_t", (struct objfile *) NULL);
  TYPE_BYTE_ORDER (int32_big) = BFD_ENDIAN_BIG;
  int64_big = init_type (TYPE_CODE_INT, 64 / 8, 0, "int64_t", (struct objfile *) NULL);
  TYPE_BYTE_ORDER (int64_big) = BFD_ENDIAN_BIG;
  uint128_big = init_type (TYPE_CODE_INT, 128 / 8, TYPE_FLAG_UNSIGNED, "uint128_t", (struct objfile *) NULL);
  TYPE_BYTE_ORDER (uint128_big) = BFD_ENDIAN_BIG;

  v4_float_big = init_vector_type (builtin_type_ieee_single_big, 4);
  v2_double_big = init_vector_type (builtin_type_ieee_double_big, 2);

  v2_int64_big = init_vector_type (int64_big, 2);
  v4_int32_big = init_vector_type (int32_big, 4);
  v8_int16_big = init_vector_type (int16_big, 8);
  v16_int8_big = init_vector_type (builtin_type_int8, 16);

  t = init_composite_type ("__gdb_builtin_type_vec128i_big", TYPE_CODE_UNION);
  append_composite_type_field (t, "v4_float", v4_float_big);
  append_composite_type_field (t, "v2_double", v2_double_big);
  append_composite_type_field (t, "v16_int8", v16_int8_big);
  append_composite_type_field (t, "v8_int16", v8_int16_big);
  append_composite_type_field (t, "v4_int32", v4_int32_big);
  append_composite_type_field (t, "v2_int64", v2_int64_big);
  append_composite_type_field (t, "uint128", uint128_big);

  TYPE_FLAGS (t) |= TYPE_FLAG_VECTOR;
  TYPE_NAME (t) = "builtin_type_vec128i_big";
  return t;
}

void
x86_initialize_frame_cache (struct x86_frame_cache *cache, int wordsize)
{
  int i;

  if (wordsize == 4)
    {
      cache->eip_regnum = I386_EIP_REGNUM;
      cache->ebp_regnum = I386_EBP_REGNUM;
      cache->esp_regnum = I386_ESP_REGNUM;
      cache->num_savedregs = I386_NUM_GREGS;
      cache->volatile_reg_p = i386_nonvolatile_regnum_p;
      cache->argument_reg_p = i386_argument_regnum_p;
      cache->machine_regno_to_gdb_regno = i386_machine_regno_to_gdb_regno;
    }
  else
    {
      cache->eip_regnum = AMD64_RIP_REGNUM;
      cache->ebp_regnum = AMD64_RBP_REGNUM;
      cache->esp_regnum = AMD64_RSP_REGNUM;
      cache->num_savedregs = AMD64_NUM_GREGS;
      cache->volatile_reg_p = x86_64_nonvolatile_regnum_p;
      cache->argument_reg_p = x86_64_argument_regnum_p;
      cache->machine_regno_to_gdb_regno = x86_64_machine_regno_to_gdb_regno;
    }

  cache->frame_base = INVALID_ADDRESS;
  cache->sp_offset = 0;
  cache->func_start_addr = INVALID_ADDRESS;
  cache->scanned_limit = INVALID_ADDRESS;
  cache->pc = INVALID_ADDRESS;
  cache->saved_regs = FRAME_OBSTACK_CALLOC (cache->num_savedregs, CORE_ADDR);
  cache->wordsize = wordsize;

  /* Saved registers.  We initialize these to INVALID_ADDRESS since zero 
     is a valid offset (that's where eip is).  */
  for (i = 0; i < cache->num_savedregs; i++)
    cache->saved_regs[i] = INVALID_ADDRESS;
  cache->saved_sp = INVALID_ADDRESS;
  cache->saved_regs_are_absolute = 0;
  cache->ebp_is_frame_pointer = 0;
  cache->prologue_scan_status = no_scan_done;

  cache->saved_regs[cache->eip_regnum] = 0;
}

struct x86_frame_cache *
x86_alloc_frame_cache (int wordsize)
{
  struct x86_frame_cache *cache;

  cache = FRAME_OBSTACK_ZALLOC (struct x86_frame_cache);

  x86_initialize_frame_cache (cache, wordsize);
  
  return cache;
}


/* Check whether FUNC_START_ADDR points at instructions that set up a new 
   stack frame.  If so, it updates CACHE and returns the address of the first
   instruction after the sequence that sets up the frame or LIMIT,
   whichever is smaller.  If we don't recognize the instruction sequence, 
   return FUNC_START_ADDR.  */

CORE_ADDR
x86_analyze_prologue (CORE_ADDR func_start_addr, CORE_ADDR limit,
                      struct x86_frame_cache *cache)
{
  int insn_count = 0;

  int saw_push_ebp = 0;
  int saw_mov_esp_ebp = 0;
  int saw_sub_esp_insn = 0;

  int non_prologue_insns = 0;

  /* If we iterate over 50 instructions max. 
     We can get in this state when backtracing through a dylib/etc
     with no symbols: we have a pc in the middle of it and we
     consider the start of the function to be the last symbol before
     the dylib.  We step over all the instructions between those
     two points, which can be quite large, and take a long time
     doing it.  */
  const int insn_limit = 50;

  /* APPLE LOCAL: This function returns a CORE_ADDR which is the instruction
     following the last frame-setup instruction we saw such that "frame-setup
     instruction" is one of push %ebp, push $0x0, mov %esp, %ebp, 
     sub $<size>, $esp, enter, etc.. */
 
  CORE_ADDR end_of_frame_setup = func_start_addr;
  CORE_ADDR pc = func_start_addr;

  cache->scanned_limit = limit;
  cache->prologue_scan_status = full_scan_succeeded;

  if (debug_x86bt > 2)
    printf_filtered ("X86BT: Analyzing prologue func start addr 0x%s, "
                     "pc addr 0x%s\n",
                     paddr_nz (func_start_addr), paddr_nz (limit));

  /* Let's assume the typical calling convention is used and pretend we
     saw a prologue.  */
  if (func_start_addr == INVALID_ADDRESS)
    {
      cache->ebp_is_frame_pointer = 1;
      cache->saved_regs[cache->ebp_regnum] = cache->wordsize;
      return limit;
    }

  while (non_prologue_insns < 10 && pc < limit && insn_count++ < insn_limit)
    {
      int r;
      CORE_ADDR next_insn = pc + length_of_this_instruction (pc);
      int prologue_insn = 0;

      if (x86_ret_pattern_p (pc))
        break;

      if (x86_push_reg_p (pc, &r))
        {
          r = cache->machine_regno_to_gdb_regno (r);
          cache->sp_offset += cache->wordsize;
          if (cache->volatile_reg_p (r) || cache->argument_reg_p (r))
            {
              if (cache->saved_regs[r] == INVALID_ADDRESS)
                {
                  cache->saved_regs[r] = cache->sp_offset;
                  if (r == cache->ebp_regnum)
                    saw_push_ebp = 1;
                  prologue_insn = 1;
                }
            }
          goto loopend;
        }

      if (x86_sub_esp_pattern_p (pc))
        {
          cache->sp_offset += x86_sub_esp_pattern_p (pc);
          saw_sub_esp_insn = 1;
          prologue_insn = 1;
          goto loopend;
        }

      if (x86_mov_esp_ebp_pattern_p (pc))
        {
          saw_mov_esp_ebp = 1;
          prologue_insn = 1;
          goto loopend;
        }

      if (x86_picbase_setup_pattern_p (pc, NULL))
        {
          /* This is a two-instruction sequence so skip the pair of them */
          next_insn = next_insn + length_of_this_instruction (next_insn);
          prologue_insn = 1;
          goto loopend;
        }

      /* Detect a 'mov %ebx, -0xc(%ebp)' type store.  */
      int off;
      if (x86_mov_reg_to_local_stack_frame_p (pc, &r, &off))
        {
          r = cache->machine_regno_to_gdb_regno (r);
          if ((cache->volatile_reg_p (r) || cache->argument_reg_p (r))
              && cache->saved_regs[r] == INVALID_ADDRESS)
            {
              cache->saved_regs[r] = off + cache->wordsize;
              prologue_insn = 1;
            }
          goto loopend;
        }
  
      if (x86_unopt_arg_copy_to_local_stack_p (pc))
        {
          /* This is a two-instruction sequence so skip the pair of them */
          next_insn = next_insn + length_of_this_instruction (next_insn);
          prologue_insn = 1;
          goto loopend;
        }

      /* Any kind of jump instruction implies flow control -- we're past
         the prologue now.  */
      if (x86_jump_insn_p (pc))
        break;

      if (x86_pop_p (pc))
        {
          cache->sp_offset -= cache->wordsize;
          goto loopend;
        }

      if (x86_blocks_context_var_copy_pattern_p (pc, cache->wordsize))
        {
          /* This is a three-instruction sequence so skip the lot of them */
          next_insn = next_insn + length_of_this_instruction (next_insn);
          next_insn = next_insn + length_of_this_instruction (next_insn);
          prologue_insn = 1;
          goto loopend;
        }

      /* The plain old i386_push_reg_p() would detect the standard 'push %ebp'
         but an alternate insn that this function checks for is 'push $0x0'
         which is done by the 'start' func.  */
      if (x86_push_ebp_pattern_p (pc))
        {
          if (saw_push_ebp)
            break;
          saw_push_ebp = 1;
          cache->sp_offset += cache->wordsize;
          cache->saved_regs[cache->ebp_regnum] = cache->sp_offset;
          prologue_insn = 1;
          goto loopend;
        }

      /* A call instruction (opcode 0xe8) which ISN'T a picbase setup
         instruction -- we're calling another function and it's safe to
         say that we're done with prologue instructions.  */
      if (read_memory_unsigned_integer (pc, 1) == 0xe8)
        break;

      /* Why use a goto?  So we don't evaluate all the other instruction
         patterns after we've found a match -- we're just doing pointless
         memory reads after we've got a match and they're not necessarily
         cheap.  */
loopend:
      if (prologue_insn)
        end_of_frame_setup = next_insn;
      else
        non_prologue_insns++;

      pc = next_insn;
    }

  if (saw_push_ebp && saw_mov_esp_ebp)
    {
      cache->ebp_is_frame_pointer = 1;
      cache->saved_regs[cache->ebp_regnum] = cache->wordsize;
      return end_of_frame_setup;
    }

  /* If we haven't seen the standard push ebp; mov esp, ebp sequence
     then this is a function that doesn't set up a frame pointer.  */
  if (saw_push_ebp == 0 && saw_sub_esp_insn)
    {
      cache->saved_regs[cache->ebp_regnum] = INVALID_ADDRESS;
      cache->ebp_is_frame_pointer = 0;
      return end_of_frame_setup;
    }

  /* We're probably mid-prologue, having just executed the push ebp but
     not yet executing the mov esp, ebp.  */
  if (saw_push_ebp && saw_mov_esp_ebp == 0)
    {
      cache->ebp_is_frame_pointer = 0;
      return end_of_frame_setup;
    }

  /* We didn't detect any prologue instructions at all.  */
  cache->ebp_is_frame_pointer = 0;
  return end_of_frame_setup;
}

void
x86_finalize_saved_reg_locations (struct frame_info *next_frame, 
                                  struct x86_frame_cache *cache)
{
  gdb_byte buf[8];

  if (cache->saved_regs_are_absolute == 1)
    return;

  if (cache->ebp_is_frame_pointer)
    {
      frame_unwind_register (next_frame, cache->ebp_regnum, buf);
      cache->frame_base = extract_unsigned_integer (buf, cache->wordsize) 
                          + cache->wordsize;
    }
  else
    {
      frame_unwind_register (next_frame, cache->esp_regnum, buf);
      cache->frame_base = extract_unsigned_integer (buf, cache->wordsize) 
                          + cache->sp_offset;
    }
  int i;
  for (i = 0; i < cache->num_savedregs; i++)
    if (cache->saved_regs[i] != INVALID_ADDRESS)
      cache->saved_regs[i] = cache->frame_base - cache->saved_regs[i];

  cache->saved_sp = cache->frame_base + cache->wordsize;
  cache->saved_regs_are_absolute = 1;
}


/* A quick & dirty look at the function prologue to see if it follows
   the standard convention of push %ebp; mov %esp, %ebp.  If so, and we
   are only crawling the stack, that's all we need to know -- we don't
   need to disassemble all of the instrucitons looking for other prologue
   insns.
   If gdb wants to know any other registers, or this is an unusual 
   prologue, we can do the full prologue instruction analysis - but it
   is costly to do for no good reason.  */

CORE_ADDR
x86_quickie_analyze_prologue (CORE_ADDR func_start_addr, CORE_ADDR limit,
                              struct x86_frame_cache *cache,
                              int potentially_frameless)
{
  gdb_byte buf[4];

  /* Should we look for Fix & Continue nop paddings?  */

  gdb_byte i386_pat[3] = { 0x55, 0x89, 0xe5 };
  gdb_byte x86_64_pat[4] = { 0x55, 0x48, 0x89, 0xe5 };

  cache->func_start_addr = func_start_addr;
  cache->pc = limit;

  /* If we were unable to find the start address of the function we're
     currently in, try using the current RIP value as a function start addr
     and look for the typical prologue instructions.  Maybe we just 
     instruction-stepped into a function w/o syms.  

     If we don't see typical prologue instructions we can't make
     an accurate assumption here.  Maybe it would be best to just
     assume that we instruction stepped into something... */

  if (func_start_addr == INVALID_ADDRESS && potentially_frameless == 1)
    {
      read_memory (limit, buf, 4);
      if (memcmp (buf, i386_pat, sizeof (i386_pat)) == 0
          || memcmp (buf, x86_64_pat, sizeof (x86_64_pat)) == 0)
        {
          cache->prologue_scan_status = quick_scan_succeeded;
          cache->ebp_is_frame_pointer = 0;
          return limit;
        }
    }

  if (func_start_addr == INVALID_ADDRESS)
    {
      /* Set some reasonable defaults assuming the normal calling 
         convention and a frame was set up already */
      cache->prologue_scan_status = quick_scan_succeeded;
      cache->saved_regs[cache->ebp_regnum] = cache->wordsize;
      cache->ebp_is_frame_pointer = 1;
      return limit;
    }

  read_memory (func_start_addr, buf, 4);
  
  int i386_matched = memcmp (buf, i386_pat, sizeof (i386_pat)) == 0;
  int x86_64_matched = memcmp (buf, x86_64_pat, sizeof (x86_64_pat)) == 0;
  if (i386_matched || x86_64_matched)
    {
      if (limit == func_start_addr)
        {
          cache->prologue_scan_status = quick_scan_succeeded;
          cache->ebp_is_frame_pointer = 0;
          return func_start_addr;
        }
      if (limit == func_start_addr + 1)
        {
          cache->prologue_scan_status = quick_scan_succeeded;
          cache->ebp_is_frame_pointer = 0;
          cache->sp_offset += cache->wordsize;
          return limit;
        }
      cache->prologue_scan_status = quick_scan_succeeded;
      cache->ebp_is_frame_pointer = 1;
      cache->sp_offset += cache->wordsize;
      cache->saved_regs[cache->ebp_regnum] = cache->wordsize;
      if (i386_matched)
        return func_start_addr + 3;
      else
        return func_start_addr + 4;
    }

  cache->prologue_scan_status = quick_scan_failed;
  cache->ebp_is_frame_pointer = 0;
  return func_start_addr;
}

struct x86_frame_cache *
x86_frame_cache (struct frame_info *next_frame, void **this_cache, int wordsize)
{
  struct x86_frame_cache *cache;
  int potentially_frameless;
  CORE_ADDR prologue_parsed_to = 0;
  CORE_ADDR current_pc;

  if (*this_cache)
    return *this_cache;

  potentially_frameless = frame_relative_level (next_frame) == -1 
                       || get_frame_type (next_frame) == SIGTRAMP_FRAME
                       || get_frame_type (next_frame) == DUMMY_FRAME;

  cache = x86_alloc_frame_cache (wordsize);
  *this_cache = cache;

  /* We want to make sure we get the function beginning right or
     analyze_prologue will be reading off into the weeds.  So make sure
     the load level is raised before we get the function pc.  */
  current_pc = frame_pc_unwind (next_frame);
  pc_set_load_state (current_pc, OBJF_SYM_ALL, 0);

  cache->func_start_addr = frame_func_unwind (next_frame);
  cache->pc = current_pc;

  /* The nuggets of code that constitute the ObjC trampolines confuse us.
     However, we know that they are in fact little frameless jumps, so we
     will detect whether we are in one, and if so just manually set the
     results of the prologue scanning appropriately.  */

  if (pc_in_objc_trampoline_p (current_pc, NULL))
    {
      cache->prologue_scan_status = full_scan_succeeded;
      cache->ebp_is_frame_pointer = 0;
      return cache;
    }

  /* Sanity check: If the start of the function is more than 65kbytes away
     from the current pc value, I'm going to assume that the "start of the
     function" address is some random symbol we found, not really the 
     start of this function, and that we're currently stopped in some code 
     without symbols.  128k was chosen with great scientific consideration... 
     ok, no it wasn't, I pulled a number out of my hat.  Our defaults won't 
     behave too badly if someone is actually stopped in a larger-than-65k
     function. */

  if (cache->func_start_addr == 0)
    {
      cache->func_start_addr = INVALID_ADDRESS;
    }
  else
    {
      if (current_pc < cache->func_start_addr
          || (current_pc - cache->func_start_addr) > 65000)
        prologue_parsed_to = cache->func_start_addr = INVALID_ADDRESS;
    }

  /* Someone jumped through a null function pointer.  */
  if (cache->func_start_addr == INVALID_ADDRESS 
      && current_pc == 0 && potentially_frameless)
    {
      cache->prologue_scan_status = full_scan_succeeded;
      cache->ebp_is_frame_pointer = 0;
      return cache;
    }

  prologue_parsed_to = x86_quickie_analyze_prologue (cache->func_start_addr, 
                                     current_pc, cache, potentially_frameless);

  if (cache->prologue_scan_status == quick_scan_failed)
    {
      x86_initialize_frame_cache (cache, wordsize);
      cache->func_start_addr = frame_func_unwind (next_frame);
      cache->pc = frame_pc_unwind (next_frame);
      prologue_parsed_to = x86_analyze_prologue (cache->func_start_addr,
                                                 current_pc, cache);
    }

  /* If this can't be a frameless function but the i386_analyze_prologue
     claims that it is, then we obviously have a problem.  Either this
     function was built -fomit-frame-pointer (in which case we can't back
     out of it without CFI/EH info) or the prologue analyzer got it wrong.

     We'll assume that the prologue analyzer got it wrong and set up some
     probably-good defaults.  */

  if (potentially_frameless == 0 && cache->ebp_is_frame_pointer == 0)
    {
      cache->saved_regs[cache->ebp_regnum] = cache->wordsize;
      cache->ebp_is_frame_pointer = 1;
    }

  /* It is frameless or we haven't executed the frame setup insns yet.  */
  return cache;
}

void
x86_frame_this_id (struct frame_info *next_frame, void **this_cache,
                   struct frame_id *this_id)
{
  int wordsize = TARGET_PTR_BIT / 8;
  struct x86_frame_cache *cache = x86_frame_cache (next_frame, this_cache, 
                                                   wordsize);
  CORE_ADDR startaddr;
  CORE_ADDR prev_frame_pc = INVALID_ADDRESS;
  x86_finalize_saved_reg_locations (next_frame, cache); 

  /* If this is the sentinel frame, make sure the frame base we get
     from it is readable, otherwise we aren't going to get anywhere,
     and we should just stop now... */
  if (get_frame_type (next_frame) == SENTINEL_FRAME)
    {
      struct gdb_exception e;
      TRY_CATCH (e, RETURN_MASK_ERROR)
	{
	  gdb_byte buf[8];
	  read_memory (cache->frame_base, buf, wordsize);
	}
      if (e.reason != NO_ERROR)
	{
	  if (debug_x86bt)
	    printf_filtered ("X86BT: Terminating backtrace of thread port#: "
                    "0x%lx with unreadible "
		    "initial stack address at 0x%s, at frame level %d.\n", 
		    inferior_ptid.tid, paddr_nz (cache->frame_base),
                     frame_relative_level (next_frame) + 1);
	  *this_id = null_frame_id;
	  return;
	}
    }

  /* This marks the outermost frame.  
     APPLE LOCAL: Don't do this if NEXT_FRAME is the sentinal
     frame, because then the id we are building should just
     come from the registers.  */

  if (get_frame_type (next_frame) != SENTINEL_FRAME)
    {
      /* Try to read the saved PC in a try-catch block so we don't cease
         a backtrace display just because the last frame got an invalid
         saved pc value. */
      if (get_prev_frame (next_frame) != NULL)
        {
          struct gdb_exception e;
          TRY_CATCH (e, RETURN_MASK_ALL)
            {                   
              prev_frame_pc = frame_pc_unwind (get_prev_frame (next_frame));
            }
          if (e.reason != NO_ERROR)
            {
              if (debug_x86bt)
                printf_filtered ("X86BT: Could not retrieve saved pc value at frame "
                                 "level %d in backtrace of thread port# 0x%lx, "
                                 "tried to read it from stack addr 0x%s.\n",
                                 frame_relative_level (next_frame) + 1,
                                 inferior_ptid.tid, 
                                 paddr_nz (cache->frame_base));
              prev_frame_pc = 0;
            }
        }

      /* We have to handle the trap_from_kernel() function specially.  It
         will have a saved pc value of 0.  Normally a non-leaf frame with a
         saved pc value of 0 means that we've screwed up our stack walk but 
         in this one case it is expected.  */
      if (prev_frame_pc == 0)
        {
          struct minimal_symbol *minsym;
          minsym = lookup_minimal_symbol_by_pc (frame_pc_unwind (next_frame));
          if (!minsym || strcmp (SYMBOL_LINKAGE_NAME (minsym), 
                                "trap_from_kernel") != 0)
            {
              *this_id = null_frame_id;
              return;
            }
        }

      if (cache->frame_base == 0)
        {
          if (debug_x86bt)
            printf_filtered ("X86BT: Frame level %d's frame base is 0, done "
                             "backtracing thread port# 0x%lx\n", 
                             frame_relative_level (next_frame) + 1,
                             inferior_ptid.tid);
          *this_id = null_frame_id;
          return;
        }
      else
        {
          ULONGEST prev_frame_addr = 0;
          if (safe_read_memory_unsigned_integer
              (cache->frame_base - wordsize, wordsize, &prev_frame_addr))
            {
              if (prev_frame_addr == 0)
                {
                  if (debug_x86bt)
                    printf_filtered ("X86BT: Frame level %d's frame base is 0, "
                                     "done backtracing thread port# 0x%lx\n", 
                                     frame_relative_level (next_frame) + 1,
                                     inferior_ptid.tid);
                  *this_id = null_frame_id;
                  return;
                }
            }
          else
            {
                if (debug_x86bt)
                  printf_filtered ("X86BT: Could not read frame level %d's frame "
                                   "pointer value in thread port# 0x%lx.\n",
                                   frame_relative_level (next_frame) + 1,
                                   inferior_ptid.tid);
            }
        }
    }

  /* Most of gdb uses 0 to represent an unknown address here.  */
  if (cache->func_start_addr == INVALID_ADDRESS)
    startaddr = 0;
  else
    startaddr = cache->func_start_addr;

  if (debug_x86bt > 2)
    printf_filtered ("X86BT: Creating frame %d id pc 0x%s, "
                     "func-start-pc 0x%s, frame ptr 0x%s\n",
                     frame_relative_level (next_frame) + 1,
                     paddr_nz (cache->pc),
                     paddr_nz (startaddr), 
                     paddr_nz (cache->frame_base + wordsize));

  /* See the end of i386_push_dummy_call.  */
  (*this_id) = frame_id_build (cache->frame_base + wordsize, startaddr);
}

void
x86_frame_prev_register (struct frame_info *next_frame, void **this_cache,
                         int regnum, enum opt_state *optimizedp,
                         enum lval_type *lvalp, CORE_ADDR *addrp,
                         int *realnump, gdb_byte *valuep)
{
  int wordsize = TARGET_PTR_BIT / 8;
  struct x86_frame_cache *cache = x86_frame_cache (next_frame, this_cache, 
                                                   wordsize);

  /* If we're retrieving the eip, ebp or esp, we can do that with just the
     "quickie" scan.  If we want any other regs, we need to do the full
     prologue scan.  Don't try to do a scan (and throw away our quickie scan
     results) if we don't have a function start address.  */
  if (cache->prologue_scan_status != full_scan_succeeded
      && regnum != cache->eip_regnum 
      && regnum != cache->ebp_regnum
      && regnum != cache->esp_regnum
      && cache->func_start_addr != 0
      && cache->func_start_addr != INVALID_ADDRESS)
    {
      x86_initialize_frame_cache (cache, wordsize);
      cache->func_start_addr = frame_func_unwind (next_frame);
      cache->pc = frame_pc_unwind (next_frame);
      x86_analyze_prologue (cache->func_start_addr, cache->pc, cache);
    }

  if (cache->saved_regs_are_absolute == 0)
    x86_finalize_saved_reg_locations (next_frame, cache);

  gdb_assert (regnum >= 0);

  if (regnum == cache->esp_regnum && cache->saved_sp != INVALID_ADDRESS)
    {
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      if (valuep)
        {
          /* Store the value.  */
          store_unsigned_integer (valuep, wordsize, cache->saved_sp);
        }
      return;
    }

  if (regnum < cache->num_savedregs && cache->saved_regs[regnum] != -1)
    {
      /* APPLE LOCAL variable opt states.  */
      *optimizedp = opt_okay;
      *lvalp = lval_memory;
      *addrp = cache->saved_regs[regnum];
      *realnump = -1;
      if (valuep)
        {
          /* Read the value in from memory.  */
          read_memory (*addrp, valuep,
                       register_size (current_gdbarch, regnum));
          if (debug_x86bt > 8)
            printf_filtered ("X86BT: Retrieving reg #%d for frame %d, "
                             "is saved at 0x%s, value 0x%s\n",
                             regnum,
                             frame_relative_level (next_frame) + 2,
                             paddr_nz (cache->saved_regs[regnum]),
                             paddr_nz (read_memory_unsigned_integer (cache->saved_regs[regnum], cache->wordsize)));
        }
      return;
    }

  /* APPLE LOCAL variable opt states.  */
  *optimizedp = opt_okay;
  *lvalp = lval_register;
  *addrp = 0;
  *realnump = regnum;
  if (valuep)
    {
      if (debug_x86bt > 8)
        printf_filtered ("X86BT: Could not find save location for regnum %d in frame %d, going down stack.\n",
                         regnum, frame_relative_level (next_frame) + 1);
      frame_unwind_register (next_frame, (*realnump), valuep);
    }
}


void
_initialize_x86_shared_tdep (void)
{
  /* Debug this file's internals. */
  add_setshow_zinteger_cmd ("x86bt", class_maintenance, &debug_x86bt, _("\
Set x86 backtrace debugging."), _("\
Show x86 backtrace debugging."), _("\
When non-zero, x86 backtrace specific debugging is enabled."),
                            NULL,
                            show_debug_x86bt,
                            &setdebuglist, &showdebuglist);
}
