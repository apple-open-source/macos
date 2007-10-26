#include "defs.h"
#include "gdb_assert.h"
#include "i386-amd64-shared-tdep.h"
#include "dis-asm.h"
#include "gdbcore.h"
#include "ui-out.h"
#include "target.h"
#include "gdbtypes.h"

static struct disassemble_info gdb_disassemble_info_noprint 
                              (struct gdbarch *gdbarch, struct ui_file *file);
int length_of_this_instruction (CORE_ADDR memaddr);

/* Like target_read_memory, but slightly different parameters.  */
static int
dis_asm_read_memory_noprint (bfd_vma memaddr, gdb_byte *myaddr, 
                             unsigned int len, struct disassemble_info *info)
{
  return target_read_memory (memaddr, myaddr, len);
}

/* Like memory_error with slightly different parameters.  */
static void
dis_asm_memory_error_noprint (int status, bfd_vma memaddr,
                              struct disassemble_info *info)
{
  memory_error (status, memaddr);
}

/* Like print_address with slightly different parameters.  */
static void
dis_asm_print_address_noprint (bfd_vma addr, struct disassemble_info *info)
{
}

static int ATTR_FORMAT (printf, 2, 3)
fprintf_disasm_noprint (void *stream, const char *format, ...)
{
  va_list args;
  va_start (args, format);
  va_end (args);
  return 0;
}

static struct disassemble_info
gdb_disassemble_info_noprint (struct gdbarch *gdbarch, struct ui_file *file)
{
  struct disassemble_info di;
  init_disassemble_info (&di, file, fprintf_disasm_noprint);
  di.flavour = bfd_target_unknown_flavour;
  di.memory_error_func = dis_asm_memory_error_noprint;
  di.print_address_func = dis_asm_print_address_noprint;
  di.read_memory_func = dis_asm_read_memory_noprint;
  di.arch = gdbarch_bfd_arch_info (gdbarch)->arch;
  di.mach = gdbarch_bfd_arch_info (gdbarch)->mach;
  di.endian = gdbarch_byte_order (gdbarch);
  disassemble_init_for_target (&di);
  return di;
}

int 
i386_length_of_this_instruction (CORE_ADDR memaddr)
{
  static struct ui_stream *stb = NULL;
  if (stb == NULL)
    stb = ui_out_stream_new (uiout); 
  struct disassemble_info di = gdb_disassemble_info_noprint (current_gdbarch, 
                                                            stb->stream);
  return TARGET_PRINT_INSN (memaddr, &di);
}

/* Return true if the i386/x86-64 instruction at MEMADDR is a
   push %esp, %ebp instruction that typically appears in a function prologue. */

int
i386_mov_esp_ebp_pattern_p (CORE_ADDR memaddr)
{
  gdb_byte op = read_memory_unsigned_integer (memaddr, 1);

  /* Look for & consume the x86-64 prefix byte 0x48 */
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
int
i386_push_ebp_pattern_p (CORE_ADDR memaddr)
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
int
i386_ret_pattern_p (CORE_ADDR memaddr)
{
  gdb_byte op = read_memory_unsigned_integer (memaddr, 1); 
  if (op == 0xc3)
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

   The total length of this call + pop instruction sequence is 6 bytes.  */

int
i386_picbase_setup_pattern_p (CORE_ADDR memaddr, enum i386_regnum *regnum)
{
  gdb_byte op;
  if (i386_length_of_this_instruction (memaddr) != 5
      || read_memory_unsigned_integer (memaddr, 1) != 0xe8
      || read_memory_unsigned_integer (memaddr + 1, 4) != 0)
    return 0;
  
  op = read_memory_unsigned_integer (memaddr + 5, 1);
  if ((op & 0xf8) != 0x58)
    return 0;

  if (regnum)
    *regnum = (enum i386_regnum) op & 0x7;

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

