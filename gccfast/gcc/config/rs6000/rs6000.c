/* Subroutines used for code generation on IBM RS/6000.
   Copyright (C) 1991, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 
   2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Richard Kenner (kenner@vlsi1.ultra.nyu.edu)

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "insn-attr.h"
#include "flags.h"
#include "recog.h"
#include "obstack.h"
#include "tree.h"
#include "expr.h"
#include "optabs.h"
#include "except.h"
#include "function.h"
#include "output.h"
#include "basic-block.h"
#include "integrate.h"
#include "toplev.h"
#include "ggc.h"
#include "hashtab.h"
#include "tm_p.h"
#include "target.h"
#include "target-def.h"
#include "langhooks.h"
#include "reload.h"
#include "sched-int.h" /* DN */
/* APPLE LOCAL why is this needed? */
#include "insn-addr.h"
/* APPLE LOCAL begin inline floor */
#include "cfglayout.h"
/* APPLE LOCAL end inline floor */

/* APPLE LOCAL begin Macintosh alignment */
#ifndef TARGET_ALIGN_MAC68K
#define TARGET_ALIGN_MAC68K 0
#endif
/* APPLE LOCAL end Macintosh alignment */

#ifndef TARGET_NO_PROTOTYPE
#define TARGET_NO_PROTOTYPE 0
#endif

#define min(A,B)	((A) < (B) ? (A) : (B))
#define max(A,B)	((A) > (B) ? (A) : (B))

/* Target cpu type */

enum processor_type rs6000_cpu;
struct rs6000_cpu_select rs6000_select[3] =
{
  /* switch		name,			tune	arch */
  { (const char *)0,	"--with-cpu=",		1,	1 },
  { (const char *)0,	"-mcpu=",		1,	1 },
  { (const char *)0,	"-mtune=",		1,	0 },
};

/* DN begin */
#if 1
#define MFCR_TYPE_IS_MICROCODED 1
#else
#undef MFCR_TYPE_IS_MICROCODED
#endif

#define ISSUE_RATE_INCLUDES_BU_SLOT 1
#define DN_SCHED_FINISH 1

#if 0
#define DN_DEBUG 1
#else
#undef DN_DEBUG
#endif

#ifdef DN_SCHED_FINISH
extern int sched_costly_dep;
extern int insert_sched_nops;
extern int sched_load_balance;
extern int sched_load_balance_scale;
extern int insert_sched_nops_for_ldb;
extern int sched_load_balance;
extern int flag_sched_ldb_global;
#define GROUP_SIZE 4

#if 1
#undef RELY_ON_GROUPING
#else
#define RELY_ON_GROUPING 1
#endif

struct permute{
  int from_slot;
  int to_slot;
};

#ifdef RELY_ON_GROUPING

struct group_usage_per_unit{
  int iu[2];
  int lsu[2];
  int fpu[2];
  rtx insn_at_slot[4];
};
#define N_PERMUTATIONS 4
struct permute alt_permute[N_PERMUTATIONS] = {{0,1},{0,2},{3,2},{3,1}};

#else

struct group_usage_per_unit{
  int iu[GROUP_SIZE];
  int lsu[GROUP_SIZE];
  int fpu[GROUP_SIZE];
  rtx insn_at_slot[4];
};
#define N_PERMUTATIONS 3
struct permute alt_permute[N_PERMUTATIONS] = {{0,1},{1,2},{2,3}};

#endif /* RELY_ON_GROUPING */

#endif /* DN_SCHED_FINISH */
/* DN end */


/* Size of long double */
const char *rs6000_long_double_size_string;
int rs6000_long_double_type_size;

/* Whether -mabi=altivec has appeared */
int rs6000_altivec_abi;

/* Whether VRSAVE instructions should be generated.  */
int rs6000_altivec_vrsave;

/* String from -mvrsave= option.  */
const char *rs6000_altivec_vrsave_string;

/* Nonzero if we want SPE ABI extensions.  */
int rs6000_spe_abi;

/* Whether isel instructions should be generated.  */
int rs6000_isel;

/* Nonzero if we have FPRs.  */
int rs6000_fprs = 1;

/* String from -misel=.  */
const char *rs6000_isel_string;

/* Set to nonzero once AIX common-mode calls have been defined.  */
static int common_mode_defined;

/* APPLE LOCAL begin forbidding over-speculative motion */
/* -mcond-exec-insns= support */
const char *rs6000_condexec_insns_str;                 /* -mcond-exec-insns= option */
int rs6000_condexec_insns = DEFAULT_CONDEXEC_INSNS; /* value of -mcond-exec-insns*/
/* APPLE LOCAL end forbidding over-speculative motion */

/* Private copy of original value of flag_pic for ABI_AIX.  */
static int rs6000_flag_pic;

/* Save information from a "cmpxx" operation until the branch or scc is
   emitted.  */
rtx rs6000_compare_op0, rs6000_compare_op1;
int rs6000_compare_fp_p;

/* Label number of label created for -mrelocatable, to call to so we can
   get the address of the GOT section */
int rs6000_pic_labelno;

#ifdef USING_ELFOS_H
/* Which abi to adhere to */
const char *rs6000_abi_name = RS6000_ABI_NAME;

/* Semantics of the small data area */
enum rs6000_sdata_type rs6000_sdata = SDATA_DATA;

/* Which small data model to use */
const char *rs6000_sdata_name = (char *)0;

/* Counter for labels which are to be placed in .fixup.  */
int fixuplabelno = 0;
#endif

/* ABI enumeration available for subtarget to use.  */
enum rs6000_abi rs6000_current_abi;

/* ABI string from -mabi= option.  */
const char *rs6000_abi_string;

/* Debug flags */
const char *rs6000_debug_name;
int rs6000_debug_stack;		/* debug stack applications */
int rs6000_debug_arg;		/* debug argument handling */

const char *rs6000_traceback_name;
static enum {
  traceback_default = 0,
  traceback_none,
  traceback_part,
  traceback_full
} rs6000_traceback;

/* Flag to say the TOC is initialized */
int toc_initialized;
char toc_label_name[10];

/* Alias set for saves and restores from the rs6000 stack.  */
static int rs6000_sr_alias_set;

/* Call distance, overridden by -mlongcall and #pragma longcall(1).
   The only place that looks at this is rs6000_set_default_type_attributes;
   everywhere else should rely on the presence or absence of a longcall
   attribute on the function declaration.  */
int rs6000_default_long_calls;
const char *rs6000_longcall_switch;

struct builtin_description
{
  /* mask is not const because we're going to alter it below.  This
     nonsense will go away when we rewrite the -march infrastructure
     to give us more target flag bits.  */
  unsigned int mask;
  const enum insn_code icode;
  const char *const name;
  const enum rs6000_builtins code;
};

static int num_insns_constant_wide PARAMS ((HOST_WIDE_INT));
/* APPLE LOCAL MEM_OFFSET setting */
static rtx expand_block_move_mem PARAMS ((enum machine_mode, rtx, rtx, int));
static void validate_condition_mode 
  PARAMS ((enum rtx_code, enum machine_mode));
static rtx rs6000_generate_compare PARAMS ((enum rtx_code));
static void rs6000_maybe_dead PARAMS ((rtx));
static void rs6000_emit_stack_tie PARAMS ((void));
static void rs6000_frame_related PARAMS ((rtx, rtx, HOST_WIDE_INT, rtx, rtx));
static void emit_frame_save PARAMS ((rtx, rtx, enum machine_mode,
				     unsigned int, int, int));
static rtx gen_frame_mem_offset PARAMS ((enum machine_mode, rtx, int));
static void rs6000_emit_allocate_stack PARAMS ((HOST_WIDE_INT, int));
static unsigned rs6000_hash_constant PARAMS ((rtx));
static unsigned toc_hash_function PARAMS ((const void *));
static int toc_hash_eq PARAMS ((const void *, const void *));
static int constant_pool_expr_1 PARAMS ((rtx, int *, int *));
static struct machine_function * rs6000_init_machine_status PARAMS ((void));
static bool rs6000_assemble_integer PARAMS ((rtx, unsigned int, int));
#ifdef HAVE_GAS_HIDDEN
static void rs6000_assemble_visibility PARAMS ((tree, int));
#endif
static int rs6000_ra_ever_killed PARAMS ((void));
static tree rs6000_handle_longcall_attribute PARAMS ((tree *, tree, tree, int, bool *));
const struct attribute_spec rs6000_attribute_table[];
static void rs6000_set_default_type_attributes PARAMS ((tree));
static void rs6000_output_function_prologue PARAMS ((FILE *, HOST_WIDE_INT));
static void rs6000_output_function_epilogue PARAMS ((FILE *, HOST_WIDE_INT));
static void rs6000_output_mi_thunk PARAMS ((FILE *, tree, HOST_WIDE_INT,
					    HOST_WIDE_INT, tree));
static rtx rs6000_emit_set_long_const PARAMS ((rtx,
  HOST_WIDE_INT, HOST_WIDE_INT));
#if TARGET_ELF
static unsigned int rs6000_elf_section_type_flags PARAMS ((tree, const char *,
							   int));
static void rs6000_elf_asm_out_constructor PARAMS ((rtx, int));
static void rs6000_elf_asm_out_destructor PARAMS ((rtx, int));
static void rs6000_elf_select_section PARAMS ((tree, int,
						 unsigned HOST_WIDE_INT));
static void rs6000_elf_unique_section PARAMS ((tree, int));
static void rs6000_elf_select_rtx_section PARAMS ((enum machine_mode, rtx,
						   unsigned HOST_WIDE_INT));
static void rs6000_elf_encode_section_info PARAMS ((tree, int))
     ATTRIBUTE_UNUSED;
static const char *rs6000_elf_strip_name_encoding PARAMS ((const char *));
static bool rs6000_elf_in_small_data_p PARAMS ((tree));
#endif
#if TARGET_XCOFF
static void rs6000_xcoff_asm_globalize_label PARAMS ((FILE *, const char *));
static void rs6000_xcoff_asm_named_section PARAMS ((const char *, unsigned int));
static void rs6000_xcoff_select_section PARAMS ((tree, int,
						 unsigned HOST_WIDE_INT));
static void rs6000_xcoff_unique_section PARAMS ((tree, int));
static void rs6000_xcoff_select_rtx_section PARAMS ((enum machine_mode, rtx,
						     unsigned HOST_WIDE_INT));
static const char * rs6000_xcoff_strip_name_encoding PARAMS ((const char *));
static unsigned int rs6000_xcoff_section_type_flags PARAMS ((tree, const char *, int));
#endif
static void rs6000_xcoff_encode_section_info PARAMS ((tree, int))
     ATTRIBUTE_UNUSED;

/* APPLE LOCAL begin AltiVec */
/* AltiVec Programming Model.  */

/* Machine intrinsics */
#define BUILT_IN_FIRST_TARGET_OVERLOADED_INTRINSIC 500
#define BUILT_IN_LAST_TARGET_OVERLOADED_INTRINSIC 800

#define BUILT_IN_FIRST_TARGET_INTRINSIC 1000
#define BUILT_IN_LAST_TARGET_INTRINSIC 3000

/* #pragma altivec_vrsave stuff.  */
int current_vrsave_save_type = VRSAVE_NORMAL;	/* for the current function  */
int standard_vrsave_save_type = VRSAVE_NORMAL;	/* "global" setting  */

struct builtin {
  /* A pointer to the type of the parameters for the builtin function.
     There are at most three parameters.  */
  tree *args[3];
  /* A string giving the constraint letters for each parameter.  */
  const char *constraints;
  /* A pointer to the type of the result of the builtin function.  */
  tree *result;
  /* The number of parameters for the builtin function.  */
  const int n_args : 8;
  /* 1 if any pointer parameter may point to a const qualified version
     of the same type.  */
  const unsigned const_ptr_ok : 1;
  /* 1 if any pointer parameter may point to a volatile qualified version
     of the same type.  */
  const unsigned volatile_ptr_ok : 1;
  /* A nonzero value listed in enum builtin_optimize indicating the
     equivalences for the given builtin function.  */
  const unsigned optimize : 4;
  /* A unique mangled name for the builtin function.  (The mangling is
     of the form <function>:<index>.)  */
  const char *name;
  /* The spelling of the instruction corresponding to the builtin function.  */
  const char *insn_name;
  /* The insn code for the builtin function.  */
  const int icode;
  /* The assigned built-in function code for the builtin function.  */
  const enum built_in_function fcode;
};

#define BUILTIN_arg(b,i)		(b)->args[i]
#define BUILTIN_constraint(b,i)		(b)->constraints[i]
#define BUILTIN_result(b)		(b)->result
#define BUILTIN_name(b)			(b)->name
#define BUILTIN_insn_name(b)		(b)->insn_name
#define BUILTIN_n_args(b)		(b)->n_args
#define BUILTIN_const_ptr_ok(b)		(b)->const_ptr_ok
#define BUILTIN_volatile_ptr_ok(b)	(b)->volatile_ptr_ok
#define BUILTIN_optimize(b)		((enum builtin_optimize)(b)->optimize)
#define BUILTIN_icode(b)		(b)->icode
#define BUILTIN_fcode(b)		(b)->fcode

#define BUILTIN_to_DECL(b) \
  decl_for_builtin[(b)->fcode - BUILT_IN_FIRST_TARGET_INTRINSIC]

/* These values are encoded by ops-to-gp into the vec.h table.  */
enum builtin_optimize {
  BUILTIN_zero_if_same = 1,
  BUILTIN_copy_if_same = 2,
  BUILTIN_vsldoi = 3,
  BUILTIN_vspltisb = 4,
  BUILTIN_vspltish = 5,
  BUILTIN_vspltisw = 6,
  BUILTIN_ones_if_same = 7,
  BUILTIN_lvsl = 8,
  BUILTIN_lvsr = 9,
  BUILTIN_cmp_reverse = 10,
  BUILTIN_abs = 11
};

extern const struct builtin * const Builtin[];
#define DECL_to_BUILTIN(d)		 \
  Builtin[DECL_FUNCTION_CODE(d) - BUILT_IN_FIRST_TARGET_INTRINSIC]

struct overloadx {
  /* The name of the overloaded builtin function.  */
  const char *name;
  /* The number of overloads for this name.  1 if the function has a
     unique signature.  */
  const int n_fcns;
  /* The number of arguments to each overload.  ops-to-gp validates that
     this is the same for each overload.  */
  const int n_args;
  /* An array of builtin function descriptors.  */
  const struct builtin *const *functions;
  /* The assigned built-in function code for the overloaded builtin
     function.  */
  enum built_in_function fcode;
};

#define OVERLOAD_name(o)		(o)->name
#define OVERLOAD_n_fcns(o)		(o)->n_fcns
#define OVERLOAD_n_args(o)		(o)->n_args
#define OVERLOAD_functions(o)		(o)->functions
#define OVERLOAD_fcode(o)		(o)->fcode

extern const struct overloadx Overload[];
#define DECL_to_TARGET_OVERLOADED_INTRINSIC(d)		 \
  Overload[DECL_FUNCTION_CODE(d) - BUILT_IN_FIRST_TARGET_OVERLOADED_INTRINSIC]

/* Return 1 if TYPE1 and TYPE2 are compatible types for assignment  
   or various other operations.  Note that this prototype corresponds
   to the C++ version of this function - other front ends will ignore
   the third parameter.  */
extern int comptypes PARAMS ((tree, tree, int)); 

/* Add qualifiers to a type, in the fashion for C.  */
extern tree lang_build_type_variant PARAMS ((tree, int, int));

/* Regard two pointer types T1 and T2 as the same by ignoring const
   and volatile qualifiers as specified by SELF.  */

static int
funny_pointer_check (self, t1, t2)
     const struct builtin *self;
     tree t1;
     tree t2;
{
  if (!BUILTIN_const_ptr_ok(self)
      && TYPE_READONLY (t2))
    return 0;
  if (!BUILTIN_volatile_ptr_ok(self)
      && TYPE_VOLATILE (t2))
    return 0;
  /* APPLE LOCAL AltiVec */
  return comptypes (lang_build_type_variant (t1, 0, 0), 
		    lang_build_type_variant (t2, 0, 0), 0);
}

/* Return whether parameter types FORMAL and ACTUAL are compatible.
   Same as comptypes except it removes any const-ness from ACTUAL.
   This allows

    x = vec_mergeh((const vector unsigned long) zero, blah);

   to work on C++ (where the above CONST would cause our matching to fail.)
   Play it very safe by furtling with the TYPE_READONLY only if required.  */

static int
comptypes_ignoring_const (const tree formal, tree actual)
{
  int compatible = comptypes (formal, actual, 0);

  if (!compatible && VECTOR_MODE_P (TYPE_MODE (actual))
      && TYPE_READONLY (actual))
    {
      TYPE_READONLY (actual) = 0;
      compatible = comptypes (formal, actual, 0);
      TYPE_READONLY (actual) = 1;
    }

  return compatible;
}

rtx
old_expand_builtin (exp, target)
     tree exp;
     rtx target;
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  enum machine_mode value_mode = TYPE_MODE (TREE_TYPE (exp));

  return expand_target_intrinsic (fndecl, target, value_mode, arglist);
}

/* Expand the call to the FUNCTION_DECL FNDECL, is known to be a
   target-specific intrinsic, with arguments ARGLIST.  Put the result
   in TARGET if that's convenient (and in mode MODE if that's
   convenient).  */

extern rtx expand_expr ();

rtx
expand_target_intrinsic (fndecl, target, mode, arglist)
     tree fndecl;
     rtx target;
     enum machine_mode mode;
     tree arglist;
{
  const struct builtin *b = DECL_to_BUILTIN (fndecl);
  rtx ops[5], insns;
  tree t;
  int i, n, c;
  int icode;

  /* Expand each operand and check the constraints should the builtin function
     use them.  */
  for (t = arglist, n = 0; t; t = TREE_CHAIN (t), n++)
    {
      ops[n] = expand_expr (TREE_VALUE (t), NULL_RTX, VOIDmode, 0);
      switch (c = BUILTIN_constraint(b, n))
	{
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'I':
	case 'J':
	case 'K':
	case 'L':
	case 'M':
	case 'N':
	case 'O':
	case 'P':
	  if (!(GET_CODE (ops[n]) == CONST_INT
		&& CONST_OK_FOR_LETTER_P (INTVAL (ops[n]), c)))
	    {
	      error ("argument %d to built-in function `%s' does not satisfy constraint %c",
		     n, BUILTIN_name(b), c);
	      ops[n] = const0_rtx;
	    }
	  break;
	case 'x':
	  if (TREE_CODE (TREE_VALUE (t)) == ERROR_MARK)
	    ops[n] = immed_vector_const (build_vector 
		(vector_unsigned_long_type_node, 
		 tree_cons (0, integer_zero_node, 
		    tree_cons (0, integer_zero_node, 
			tree_cons (0, integer_zero_node, 
			    tree_cons (0, integer_zero_node, 0))))));
	  break;
	}
    }

  if (BUILTIN_optimize (b) == BUILTIN_cmp_reverse) {
    /* This is a reversed compare operation.  Just swap the two operands.  */
    rtx temp = ops[0];
    ops[0] = ops[1];
    ops[1] = temp;

  } else if (BUILTIN_optimize (b) == BUILTIN_abs) {
    /* This is an abs operation.  */
    const char *op1, *op2;
    n = i = 0;
    switch (BUILTIN_insn_name (b)[0] - '0') {
    case 1: /* vec_abs(s8) */
      op2 = "*vmaxsb";
      op1 = "*vsububm";
      break;
    case 2: /* vec_abs(s16) */
      op2 = "*vmaxsh";
      op1 = "*vsubuhm";
      break;
    case 3: /* vec_abs(s32) */
      op2 = "*vmaxsw";
      op1 = "*vsubuwm";
      break;
    case 4: /* vec_abs(f32) */
      op2 = "*vandc";
      op1 = "*vslw";
      i = -1;
      n = 1;
      break;
    case 5: /* vec_abss(s8) */
      op2 = "*vmaxsb";
      op1 = "*vsubsbs";
      break;
    case 6: /* vec_abss(s16) */
      op2 = "*vmaxsh";
      op1 = "*vsubshs";
      break;
    case 7: /* vec_abss(s32) */
      op2 = "*vmaxsw";
      op1 = "*vsubsws";
      break;
    default:
      abort ();
    }
    t = build_int_2 (i, 0);
    ops[1] = immed_vector_const (build_vector 
	(vector_unsigned_long_type_node, 
	    tree_cons (0, t, 
	      tree_cons (0, t, 
		tree_cons (0, t, 
		  tree_cons (0, t, 0))))));
    icode = CODE_FOR_xfxx_simple;
    
    /* Check the predicates for the insn associated with the builtin.  */
    for (i = 0; i < 2; i++)
      if (! (*insn_data[icode].operand[1+i].predicate) (ops[i], 
				    insn_data[icode].operand[1+i].mode))
	ops[i] = copy_to_mode_reg (
	    GET_MODE (ops[i]) != VOIDmode 
	    ? GET_MODE (ops[i])
	    : insn_data[icode].operand[1+i].mode, ops[i]);
    ops[2] = ops[n];

    /* Generate the insn that computes the builtin.  */
    if (!target
	|| ! (*insn_data[icode].operand[0].predicate) (target, mode))
      target = gen_reg_rtx (mode);

    ops[3] = gen_reg_rtx(mode);
    emit_insn (gen_xfxx_simple (ops[3], ops[1], ops[2],
				gen_rtx (SYMBOL_REF, Pmode, op1)));
    emit_insn (gen_xfxx_simple (target, ops[0], ops[3],
				gen_rtx (SYMBOL_REF, Pmode, op2)));
    return target;
  }

  /* Add an additional operand that gives the spelling of the actual
     instruction to be used.  */
  ops[n++] = gen_rtx (SYMBOL_REF, Pmode, BUILTIN_insn_name (b));

  emit_queue ();
  start_sequence ();
  {
    rtx pat;
    int has_result = (TREE_CODE(*BUILTIN_result(b)) != VOID_TYPE);

    icode = BUILTIN_icode (b);

    /* Check the predicates for the insn associated with the builtin.  */
    for (i = 0; i < n; i++)
      if (! (*insn_data[icode].operand[has_result+i].predicate) (ops[i],
				insn_data[icode].operand[has_result+i].mode))
	ops[i] = copy_to_mode_reg (
	    GET_MODE (ops[i]) != VOIDmode 
	    ? GET_MODE (ops[i])
	    : insn_data[icode].operand[has_result+i].mode, ops[i]);

    /* Generate the insn that computes the builtin.  */
    if (has_result)
      {
	if (!target
	    || ! (*insn_data[icode].operand[0].predicate) (target, mode))
	  target = gen_reg_rtx (mode);
	if (n == 1)
	  pat = GEN_FCN (icode) (target, ops[0]);
	else if (n == 2)
	  pat = GEN_FCN (icode) (target, ops[0], ops[1]);
	else if (n == 3)
	  pat = GEN_FCN (icode) (target, ops[0], ops[1], ops[2]);
	else if (n == 4)
	  pat = GEN_FCN (icode) (target, ops[0], ops[1], ops[2], ops[3]);
	else
	  abort ();
      }
    else
      {
	target = NULL_RTX;
	if (n == 1)
	  pat = GEN_FCN (icode) (ops[0]);
	else if (n == 2)
	  pat = GEN_FCN (icode) (ops[0], ops[1]);
	else if (n == 3)
	  pat = GEN_FCN (icode) (ops[0], ops[1], ops[2]);
	else if (n == 4)
	  pat = GEN_FCN (icode) (ops[0], ops[1], ops[2], ops[3]);
	else
	  abort ();
      }
    emit_insn (pat);
  }
  insns = get_insns ();
  end_sequence ();
  emit_insn (insns);

  return target;
}

/* Returns a nonzero value if any insns were emitted.  */

int
mov_generic_vector_mode (operands)
     rtx operands[];
{
  if (! no_new_pseudos && GET_CODE (operands[0]) != REG)
    operands[1] = force_reg (GET_MODE (operands[1]), operands[1]);

  /* Handle the case where reload calls us with an invalid address;
     and the case of CONSTANT_P_RTX.  */
  if (! general_operand (operands[1], GET_MODE (operands[1]))
      || ! nonimmediate_operand (operands[0], GET_MODE (operands[0]))
      || GET_CODE (operands[1]) == CONSTANT_P_RTX)
    {
      emit_insn (gen_rtx_SET (VOIDmode, operands[0], operands[1]));
      return 1;
    }
  
  /* The calling convention can move a vector value into one of
     two sets of GPRs: r5/r6/r7/r8 or r9/r10.  Complete that here.  */
  if (GET_CODE (operands[0]) == REG && INT_REGNO_P (REGNO (operands[0])))
    {
      int i;
      int regno = REGNO (operands[0]);
      rtx from;
      if (regno != 5 && regno != 9)
	abort ();
      if (GET_CODE (operands[1]) != MEM)
	{
	  rtx stack_slot = assign_stack_temp (GET_MODE (operands[1]), 16, 0);
	  emit_move_insn (stack_slot, operands[1]);
	  operands[1] = stack_slot;
	}
      from = force_reg (SImode, XEXP (operands[1], 0));
      for (i = 0; i < (regno == 5 ? 4 : 2); i++)
	emit_move_insn (gen_rtx (REG, SImode, regno + i),
			change_address (operands[1], SImode,
					plus_constant (from, i * 4)));
    }
  else if (GET_CODE (operands[1]) == CONST_VECTOR)
    {
      HOST_WIDE_INT immed;
      switch (easy_vector_constant (operands[1]))
	{
	  case 0:
	    operands[1] = force_const_mem (GET_MODE (operands[1]),
					   operands[1]);
	    if (! reload_in_progress
		&& ! memory_address_p (GET_MODE (operands[1]),
				       XEXP (operands[1], 0)))
	      operands[1] = change_address (operands[1],
					    GET_MODE (operands[1]),
					    XEXP (operands[1], 0));
	    return 0;
	  case 1: /* vzero */
	    emit_insn (gen_vzero (operands[0]));
	    break;
	  case 2: /* vones */
	    emit_insn (gen_vones (operands[0]));
	    break;
	  case 3: /* vone */
	    emit_insn (gen_vone (operands[0]));
	    break;
	  case 4: /* vspltisw */
	    immed = INTVAL (CONST_VECTOR_ELT (operands[1], 0));
	    immed = (immed & 0x1f) | ((-((immed>>4)&1))<<5);
	    emit_insn (gen_xfA_perm (operands[0],
				     gen_rtx (CONST_INT, VOIDmode, immed),
				     gen_rtx (SYMBOL_REF, Pmode,
					      "*vspltisw")));
	    break;
	  case 5: /* vspltish */
	    immed = INTVAL (CONST_VECTOR_ELT (operands[1], 0));
	    immed &= 0xffff;
	    immed = (immed & 0x1f) | ((-((immed>>4)&1))<<5);
	    emit_insn (gen_xfA_perm (operands[0],
				     gen_rtx (CONST_INT, VOIDmode, immed),
				     gen_rtx (SYMBOL_REF, Pmode,
					      "*vspltish")));
	    break;
	  case 6: /* vspltisb */
	    immed = INTVAL (CONST_VECTOR_ELT (operands[1], 0));
	    immed &= 0xff;
	    immed = (immed & 0x1f) | ((-((immed>>4)&1))<<5);
	    emit_insn (gen_xfA_perm (operands[0],
				     gen_rtx (CONST_INT, VOIDmode, immed),
				     gen_rtx (SYMBOL_REF, Pmode,
					      "*vspltisb")));
	    break;
	  case 7: /* lvsr 0,0 */
	    emit_insn (gen_xfii_load (operands[0],
				      gen_rtx (CONST_INT, VOIDmode, (HOST_WIDE_INT)0),
				      copy_to_mode_reg (SImode,
							gen_rtx (CONST_INT,
								 VOIDmode, (HOST_WIDE_INT)0)),
				      gen_rtx (SYMBOL_REF, Pmode, "*lvsr")));
	    break;
	  case 8: /* lvsl 0,immed */
	    immed = INTVAL (CONST_VECTOR_ELT (operands[1], 0));
	    immed >>= 24;
	    emit_insn (gen_xfii_load (operands[0],
				      gen_rtx (CONST_INT, VOIDmode, (HOST_WIDE_INT)0),
				      copy_to_mode_reg (SImode,
							gen_rtx (CONST_INT,
								 VOIDmode,
								 immed)),
				      gen_rtx (SYMBOL_REF, Pmode, "*lvsl")));
	    break;
	  default:
	    abort ();
	}
    }
  else if (GET_CODE (operands[0]) == REG && GET_CODE (operands[1]) == REG)
    emit_insn (gen_mov_vreg (operands[0], operands[1]));
  else
    {
      /* Above, we may have called force_const_mem which may have returned
	 an invalid address.  If we can, fix this up; otherwise, reload will
	 have to deal with it.  */
      if (GET_CODE (operands[1]) == MEM
	  && ! memory_address_p (GET_MODE (operands[1]), XEXP (operands[1], 0))
	  && ! reload_in_progress)
	operands[1] = adjust_address (operands[1], GET_MODE (operands[1]), 0);
      emit_insn (gen_rtx_SET (VOIDmode, operands[0], operands[1]));
    }
  return 1;
}

/* Type nodes specific to the set of AltiVec intrinsics.  */
static GTY(()) tree float_ptr_type_node;
static GTY(()) tree integer_ptr_type_node;
static GTY(()) tree long_integer_ptr_type_node;
static GTY(()) tree short_integer_ptr_type_node;
static GTY(()) tree signed_char_ptr_type_node;
static GTY(()) tree short_unsigned_ptr_type_node;
static GTY(()) tree long_unsigned_ptr_type_node;
static GTY(()) tree unsigned_char_ptr_type_node;
static GTY(()) tree unsigned_ptr_type_node;
static GTY(()) tree vector_boolean_char_ptr_type_node;
static GTY(()) tree vector_boolean_long_ptr_type_node;
static GTY(()) tree vector_boolean_short_ptr_type_node;
static GTY(()) tree vector_float_ptr_type_node;
static GTY(()) tree vector_pixel_ptr_type_node;
static GTY(()) tree vector_signed_char_ptr_type_node;
static GTY(()) tree vector_signed_long_ptr_type_node;
static GTY(()) tree vector_signed_short_ptr_type_node;
static GTY(()) tree vector_unsigned_char_ptr_type_node;
static GTY(()) tree vector_unsigned_long_ptr_type_node;
static GTY(()) tree vector_unsigned_short_ptr_type_node;

/* APPLE LOCAL begin AltiVec */
/* We need const- and const-volatile- qualified flavors of
   some of the above.  */
static GTY(()) tree const_float_ptr_type_node;
static GTY(()) tree const_integer_ptr_type_node;
static GTY(()) tree const_long_integer_ptr_type_node;
static GTY(()) tree const_short_integer_ptr_type_node;
static GTY(()) tree const_signed_char_ptr_type_node;
static GTY(()) tree const_short_unsigned_ptr_type_node;
static GTY(()) tree const_long_unsigned_ptr_type_node;
static GTY(()) tree const_unsigned_char_ptr_type_node;
static GTY(()) tree const_unsigned_ptr_type_node;
static GTY(()) tree const_vector_boolean_char_ptr_type_node;
static GTY(()) tree const_vector_boolean_long_ptr_type_node;
static GTY(()) tree const_vector_boolean_short_ptr_type_node;
static GTY(()) tree const_vector_float_ptr_type_node;
static GTY(()) tree const_vector_pixel_ptr_type_node;
static GTY(()) tree const_vector_signed_char_ptr_type_node;
static GTY(()) tree const_vector_signed_long_ptr_type_node;
static GTY(()) tree const_vector_signed_short_ptr_type_node;
static GTY(()) tree const_vector_unsigned_char_ptr_type_node;
static GTY(()) tree const_vector_unsigned_long_ptr_type_node;
static GTY(()) tree const_vector_unsigned_short_ptr_type_node;

static GTY(()) tree const_volatile_float_ptr_type_node;
static GTY(()) tree const_volatile_integer_ptr_type_node;
static GTY(()) tree const_volatile_long_integer_ptr_type_node;
static GTY(()) tree const_volatile_short_integer_ptr_type_node;
static GTY(()) tree const_volatile_signed_char_ptr_type_node;
static GTY(()) tree const_volatile_short_unsigned_ptr_type_node;
static GTY(()) tree const_volatile_long_unsigned_ptr_type_node;
static GTY(()) tree const_volatile_unsigned_char_ptr_type_node;
static GTY(()) tree const_volatile_unsigned_ptr_type_node;
/* APPLE LOCAL end AltiVec */

/* Macros to map the names used in the intrinsic table.  */
#define B_UID(X) \
  ((enum built_in_function)(BUILT_IN_FIRST_TARGET_INTRINSIC+(X)))
#define O_UID(X) \
  ((enum built_in_function)(BUILT_IN_FIRST_TARGET_OVERLOADED_INTRINSIC+(X)))

#define T_char_ptr		char_ptr_type_node
#define T_float_ptr		float_ptr_type_node
#define T_int			integer_type_node
#define T_int_ptr		integer_ptr_type_node
#define T_long_ptr		long_integer_ptr_type_node
#define T_short_ptr		short_integer_ptr_type_node
#define T_signed_char_ptr	signed_char_ptr_type_node
#define T_unsigned_char_ptr	unsigned_char_ptr_type_node
#define T_unsigned_int_ptr	unsigned_ptr_type_node
#define T_unsigned_long_ptr	long_unsigned_ptr_type_node
#define T_unsigned_short_ptr	short_unsigned_ptr_type_node
#define T_vec_b16		vector_boolean_short_type_node
#define T_vec_b16_ptr		vector_boolean_short_ptr_type_node
#define T_vec_b32		vector_boolean_long_type_node
#define T_vec_b32_ptr		vector_boolean_long_ptr_type_node
#define T_vec_b8		vector_boolean_char_type_node
#define T_vec_b8_ptr		vector_boolean_char_ptr_type_node
#define T_vec_f32		vector_float_type_node
#define T_vec_f32_ptr		vector_float_ptr_type_node
#define T_vec_p16		vector_pixel_type_node
#define T_vec_p16_ptr		vector_pixel_ptr_type_node
#define T_vec_s16		vector_signed_short_type_node
#define T_vec_s16_ptr		vector_signed_short_ptr_type_node
#define T_vec_s32		vector_signed_long_type_node
#define T_vec_s32_ptr		vector_signed_long_ptr_type_node
#define T_vec_s8		vector_signed_char_type_node
#define T_vec_s8_ptr		vector_signed_char_ptr_type_node
#define T_vec_u16		vector_unsigned_short_type_node
#define T_vec_u16_ptr		vector_unsigned_short_ptr_type_node
#define T_vec_u32		vector_unsigned_long_type_node
#define T_vec_u32_ptr		vector_unsigned_long_ptr_type_node
#define T_vec_u8		vector_unsigned_char_type_node
#define T_vec_u8_ptr		vector_unsigned_char_ptr_type_node
#define T_void			void_type_node
#define T_volatile_vec_u16	T_vec_u16
#define T_cc24f			T_int
#define T_cc24fd		T_int
#define T_cc24fr		T_int
#define T_cc24t			T_int
#define T_cc24td		T_int
#define T_cc24tr		T_int
#define T_cc26f			T_int
#define T_cc26fd		T_int
#define T_cc26fr		T_int
#define T_cc26t			T_int
#define T_cc26td		T_int
#define T_cc26tr		T_int
#define T_immed_s5		T_int
#define T_immed_u2		T_int
#define T_immed_u4		T_int
#define T_immed_u5		T_int
#define T_volatile_void		T_void

/* APPLE LOCAL begin AltiVec */
/* We need const- and const-volatile- qualified flavors of
   some of the above.  */
#define T_const_char_ptr		const_char_ptr_type_node
#define T_const_float_ptr		const_float_ptr_type_node
#define T_const_int_ptr			const_integer_ptr_type_node
#define T_const_long_ptr		const_long_integer_ptr_type_node
#define T_const_short_ptr		const_short_integer_ptr_type_node
#define T_const_signed_char_ptr		const_signed_char_ptr_type_node
#define T_const_unsigned_char_ptr	const_unsigned_char_ptr_type_node
#define T_const_unsigned_int_ptr	const_unsigned_ptr_type_node
#define T_const_unsigned_long_ptr	const_long_unsigned_ptr_type_node
#define T_const_unsigned_short_ptr	const_short_unsigned_ptr_type_node
#define T_const_vec_b16_ptr		const_vector_boolean_short_ptr_type_node
#define T_const_vec_b32_ptr		const_vector_boolean_long_ptr_type_node
#define T_const_vec_b8_ptr		const_vector_boolean_char_ptr_type_node
#define T_const_vec_f32_ptr		const_vector_float_ptr_type_node
#define T_const_vec_p16_ptr		const_vector_pixel_ptr_type_node
#define T_const_vec_s16_ptr		const_vector_signed_short_ptr_type_node
#define T_const_vec_s32_ptr		const_vector_signed_long_ptr_type_node
#define T_const_vec_s8_ptr		const_vector_signed_char_ptr_type_node
#define T_const_vec_u16_ptr		const_vector_unsigned_short_ptr_type_node
#define T_const_vec_u32_ptr		const_vector_unsigned_long_ptr_type_node
#define T_const_vec_u8_ptr		const_vector_unsigned_char_ptr_type_node

#define T_const_volatile_char_ptr		const_volatile_char_ptr_type_node
#define T_const_volatile_float_ptr		const_volatile_float_ptr_type_node
#define T_const_volatile_int_ptr		const_volatile_integer_ptr_type_node
#define T_const_volatile_long_ptr		const_volatile_long_integer_ptr_type_node
#define T_const_volatile_short_ptr		const_volatile_short_integer_ptr_type_node
#define T_const_volatile_signed_char_ptr	const_volatile_signed_char_ptr_type_node
#define T_const_volatile_unsigned_char_ptr	const_volatile_unsigned_char_ptr_type_node
#define T_const_volatile_unsigned_int_ptr	const_volatile_unsigned_ptr_type_node
#define T_const_volatile_unsigned_long_ptr	const_volatile_long_unsigned_ptr_type_node
#define T_const_volatile_unsigned_short_ptr	const_volatile_short_unsigned_ptr_type_node
   
#include "vec.h"

static GTY(()) tree decl_for_builtin
  [LAST_B_UID - BUILT_IN_FIRST_TARGET_INTRINSIC];

/* A call to the FUNCTION_DECL FUNCTION with actual arguments PARAMS
   is known to be a target-specific overloaded intrinsic.  Validate
   the call and return the tree representing the selected overload
   or NULL if the call is invalid.  */

tree
select_target_overloaded_intrinsic (function, params)
     tree function;
     tree params;
{
  extern tree default_conversion (tree);
  tree parm;
  const struct overloadx *o = &DECL_to_TARGET_OVERLOADED_INTRINSIC (function);
  int idx, i, match = -1;

  if (list_length (params) == OVERLOAD_n_args (o))
    {
      /* Search for a parameter list that matches.  */
      for (idx = 0; idx < OVERLOAD_n_fcns (o); idx++)
	{
	  const struct builtin *self = OVERLOAD_functions (o)[idx];
	  for (parm = params, i = 0; parm; parm = TREE_CHAIN (parm), i++)
	    {
	      tree t1 = *BUILTIN_arg (self, i);
	      tree t2;
	      tree val = TREE_VALUE (parm);
	      if (TREE_CODE (val) == NON_LVALUE_EXPR)
		val = TREE_OPERAND (val, 0);
	      if (TREE_CODE (TREE_TYPE (val)) == ARRAY_TYPE
		  || TREE_CODE (TREE_TYPE (val)) == FUNCTION_TYPE)
		val = default_conversion (val);
	      t2 = TREE_TYPE (val);
	      if (TREE_CODE (val) == ERROR_MARK)
		return NULL_TREE;
              /* Parameters match if they are identical or if they are both
		 integral, or if both are similar enough pointers for the
		 specific overloaded function (i.e. some allow pointers to
		 const and/or volatile qualified types.  */
	      if (!(comptypes_ignoring_const (t1, t2)
		    || (POINTER_TYPE_P (t1)
			&& POINTER_TYPE_P (t2)
			&& funny_pointer_check(self,
					       TREE_TYPE (t1),
					       TREE_TYPE (t2)))
		    || (INTEGRAL_TYPE_P (t1)
			&& INTEGRAL_TYPE_P (t2))))
		goto fail;
	    }
	  /* If there is more than one match, the tables are incorrect.  There
	     shouldn't be any ambiguous overloaded builtin functions.  */
	  if (match >= 0)
	    abort();
	  match = idx;
#ifdef VECTOR_PIXEL_AND_BOOL_NOT_DISTINCT
	  break;
#endif
	fail:;
	}
    }
  if (match < 0) {
    error("no instance of overloaded builtin function `%s' matches the parameter list",
	  OVERLOAD_name(o));
    return NULL_TREE;
  }
  /* Substitute the specific overloaded builtin function that we selected.  */
  return BUILTIN_to_DECL (OVERLOAD_functions (o)[match]);
}
/* APPLE LOCAL end AltiVec */

void
old_init_builtins ()
{
  tree endlink = void_list_node;

  init_target_intrinsic (endlink, flag_altivec);
}

/* Return the parameter list for builtin function B using ENDLINK as the end
   of the prototype.  */

static tree
altivec_ftype (b, endlink)
     const struct builtin *b;
     tree endlink;
{
  tree parms = endlink;
  int i;

  for (i = BUILTIN_n_args(b) - 1; i >= 0; i--)
    parms = tree_cons (NULL_TREE, *BUILTIN_arg(b, i), parms);

  return build_function_type (*BUILTIN_result(b), parms);
}

/* Initialize the AltiVec builtin functions using ENDLINK as the end of the
   prototypes created.  */

void
init_target_intrinsic (endlink, flag_altivec)
     tree endlink;
     int flag_altivec;
{
  tree decl = NULL_TREE;
  const struct overloadx *o;
  const struct builtin *b = NULL;
  int i;
  tree void_ftype_any;

  /* These only apply if -faltivec is specified.  */
  if (!flag_altivec)
    return;

  current_vrsave_save_type = (TARGET_VRSAVE) ? VRSAVE_NORMAL : VRSAVE_OFF;
  standard_vrsave_save_type = current_vrsave_save_type;

  /* Additional types needed for the set of intrinsics.  */
  void_ftype_any = build_function_type (void_type_node, NULL_TREE);
  float_ptr_type_node = build_pointer_type (float_type_node);
  integer_ptr_type_node = build_pointer_type (integer_type_node);
  long_integer_ptr_type_node = build_pointer_type (long_integer_type_node);
  short_integer_ptr_type_node = build_pointer_type (short_integer_type_node);
  signed_char_ptr_type_node = build_pointer_type (signed_char_type_node);
  short_unsigned_ptr_type_node = build_pointer_type (short_unsigned_type_node);
  long_unsigned_ptr_type_node = build_pointer_type (long_unsigned_type_node);
  unsigned_char_ptr_type_node = build_pointer_type (unsigned_char_type_node);
  unsigned_ptr_type_node = build_pointer_type (unsigned_type_node);
  vector_boolean_char_ptr_type_node = build_pointer_type (vector_boolean_char_type_node);
  vector_boolean_long_ptr_type_node = build_pointer_type (vector_boolean_long_type_node);
  vector_boolean_short_ptr_type_node = build_pointer_type (vector_boolean_short_type_node);
  vector_float_ptr_type_node = build_pointer_type (vector_float_type_node);
  vector_pixel_ptr_type_node = build_pointer_type (vector_pixel_type_node);
  vector_signed_char_ptr_type_node = build_pointer_type (vector_signed_char_type_node);
  vector_signed_long_ptr_type_node = build_pointer_type (vector_signed_long_type_node);
  vector_signed_short_ptr_type_node = build_pointer_type (vector_signed_short_type_node);
  vector_unsigned_char_ptr_type_node = build_pointer_type (vector_unsigned_char_type_node);
  vector_unsigned_long_ptr_type_node = build_pointer_type (vector_unsigned_long_type_node);
  vector_unsigned_short_ptr_type_node = build_pointer_type (vector_unsigned_short_type_node);

/* APPLE LOCAL begin AltiVec */
/* We need const- and const-volatile- qualified flavors of
   some of the above.  */
  const_float_ptr_type_node 
    = build_pointer_type (build_qualified_type (float_type_node, TYPE_QUAL_CONST));
  const_integer_ptr_type_node 
    = build_pointer_type (build_qualified_type (integer_type_node, TYPE_QUAL_CONST));
  const_long_integer_ptr_type_node 
    = build_pointer_type (build_qualified_type (long_integer_type_node, TYPE_QUAL_CONST));
  const_short_integer_ptr_type_node 
    = build_pointer_type (build_qualified_type (short_integer_type_node, TYPE_QUAL_CONST));
  const_signed_char_ptr_type_node 
    = build_pointer_type (build_qualified_type (signed_char_type_node, TYPE_QUAL_CONST));
  const_short_unsigned_ptr_type_node 
    = build_pointer_type (build_qualified_type (short_unsigned_type_node, TYPE_QUAL_CONST));
  const_long_unsigned_ptr_type_node 
    = build_pointer_type (build_qualified_type (long_unsigned_type_node, TYPE_QUAL_CONST));
  const_unsigned_char_ptr_type_node 
    = build_pointer_type (build_qualified_type (unsigned_char_type_node, TYPE_QUAL_CONST));
  const_unsigned_ptr_type_node 
    = build_pointer_type (build_qualified_type (unsigned_type_node, TYPE_QUAL_CONST));
  const_vector_boolean_char_ptr_type_node 
    = build_pointer_type (build_qualified_type (vector_boolean_char_type_node, TYPE_QUAL_CONST));
  const_vector_boolean_long_ptr_type_node 
    = build_pointer_type (build_qualified_type (vector_boolean_long_type_node, TYPE_QUAL_CONST));
  const_vector_boolean_short_ptr_type_node 
    = build_pointer_type (build_qualified_type (vector_boolean_short_type_node, TYPE_QUAL_CONST));
  const_vector_float_ptr_type_node 
    = build_pointer_type (build_qualified_type (vector_float_type_node, TYPE_QUAL_CONST));
  const_vector_pixel_ptr_type_node 
    = build_pointer_type (build_qualified_type (vector_pixel_type_node, TYPE_QUAL_CONST));
  const_vector_signed_char_ptr_type_node 
    = build_pointer_type (build_qualified_type (vector_signed_char_type_node, TYPE_QUAL_CONST));
  const_vector_signed_long_ptr_type_node 
    = build_pointer_type (build_qualified_type (vector_signed_long_type_node, TYPE_QUAL_CONST));
  const_vector_signed_short_ptr_type_node 
    = build_pointer_type (build_qualified_type (vector_signed_short_type_node, TYPE_QUAL_CONST));
  const_vector_unsigned_char_ptr_type_node 
    = build_pointer_type (build_qualified_type (vector_unsigned_char_type_node, TYPE_QUAL_CONST));
  const_vector_unsigned_long_ptr_type_node 
    = build_pointer_type (build_qualified_type (vector_unsigned_long_type_node, TYPE_QUAL_CONST));
  const_vector_unsigned_short_ptr_type_node 
    = build_pointer_type (build_qualified_type (vector_unsigned_short_type_node, TYPE_QUAL_CONST));

  const_volatile_float_ptr_type_node 
    = build_pointer_type (build_qualified_type (float_type_node, TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE));
  const_volatile_integer_ptr_type_node 
    = build_pointer_type (build_qualified_type (integer_type_node, TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE));
  const_volatile_long_integer_ptr_type_node 
    = build_pointer_type (build_qualified_type (long_integer_type_node, TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE));
  const_volatile_short_integer_ptr_type_node 
    = build_pointer_type (build_qualified_type (short_integer_type_node, TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE));
  const_volatile_signed_char_ptr_type_node 
    = build_pointer_type (build_qualified_type (signed_char_type_node, TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE));
  const_volatile_short_unsigned_ptr_type_node 
    = build_pointer_type (build_qualified_type (short_unsigned_type_node, TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE));
  const_volatile_long_unsigned_ptr_type_node 
    = build_pointer_type (build_qualified_type (long_unsigned_type_node, TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE));
  const_volatile_unsigned_char_ptr_type_node 
    = build_pointer_type (build_qualified_type (unsigned_char_type_node, TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE));
  const_volatile_unsigned_ptr_type_node 
    = build_pointer_type (build_qualified_type (unsigned_type_node, TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE));
/* APPLE LOCAL end AltiVec */

  /* Walk the Overload table.  */
  for (o = Overload; OVERLOAD_name (o); o++)
    {
      /* Walk each builtin for the overload.  */
      for (i = 0; i < OVERLOAD_n_fcns(o); i++)
	{
	  /* Some overloads map to the same builtin.  Only declare the
	     function once.  */
	  b = OVERLOAD_functions (o)[i];
	  if ((decl = BUILTIN_to_DECL (b)) == NULL_TREE)
	    {
	      /* Declare the function and record it's declaration.  */
	      decl = builtin_function (BUILTIN_name (b),
				       altivec_ftype (b, endlink),
				       BUILTIN_fcode (b), BUILT_IN_MD,
				       BUILTIN_insn_name (b), NULL);
	      BUILTIN_to_DECL (b) = decl;
	    }
	}
      /* If the function is overloaded or has a single overload with a
	 different name, record the overload name as an overloaded
	 builtin.  Give it a prototype that will match any set of
	 parameters.  */
      if (OVERLOAD_n_fcns (o) > 1
	  || strcmp(BUILTIN_name (b), OVERLOAD_name (o)) != 0)
	decl = builtin_function (OVERLOAD_name (o), void_ftype_any,
				 OVERLOAD_fcode (o), BUILT_IN_MD, NULL, NULL);
    }
}
/* APPLE LOCAL end AltiVec */

static bool rs6000_binds_local_p PARAMS ((tree));
static int rs6000_use_dfa_pipeline_interface PARAMS ((void));
static int rs6000_variable_issue PARAMS ((FILE *, int, rtx, int));
static int rs6000_adjust_cost PARAMS ((rtx, rtx, rtx, int));
static int rs6000_adjust_priority PARAMS ((rtx, int));
static int rs6000_issue_rate PARAMS ((void));

/* DN begin */
#ifdef DN_SCHED_FINISH
extern rtx sched_emit_insn_after (rtx, rtx, rtx);
static void rs6000_sched_finish PARAMS ((FILE *, int));
static int pad_groups PARAMS ((FILE *, int, rtx, rtx));
static int redefine_groups PARAMS ((FILE *, int, rtx, rtx));
static rtx get_next_active_insn PARAMS ((rtx *, rtx, rtx));
static rtx get_prev_active_insn PARAMS ((rtx *, rtx, rtx));
static int is_dispatch_slot_restricted PARAMS((rtx insn));
static int is_cracked_insn PARAMS((rtx insn));
static int is_microcoded_insn PARAMS((rtx insn));
static bool insn_uses_multiple_units PARAMS((rtx));
static bool try_permute PARAMS ((FILE *,int,struct group_usage_per_unit *, int, int, struct group_usage_per_unit *));
static int apply_permute PARAMS ((FILE *,int,struct group_usage_per_unit *, int, struct group_usage_per_unit, int, int));
static void inter_group_load_balance PARAMS ((FILE *,int,struct group_usage_per_unit *, int,int,int));
static void record_data1 PARAMS ((FILE *, int, struct group_usage_per_unit *, rtx, int));
static void record_data PARAMS ((FILE *, int, struct group_usage_per_unit *, int, rtx, int));
static void init_group_data PARAMS ((FILE *,int,struct group_usage_per_unit *,int,rtx,rtx));
static int calc_total_score PARAMS ((struct group_usage_per_unit *,int,int,int,int));
static int calc_alt_total_score PARAMS((struct group_usage_per_unit *,int,int,int, struct group_usage_per_unit,int,int));
static enum FU_TYPE get_fu_for_insn PARAMS((rtx, int));
static int group_cost PARAMS ((struct group_usage_per_unit *, int, int));
static int group_cost1 PARAMS ((struct group_usage_per_unit, int));
static void zero_group_info PARAMS ((struct group_usage_per_unit *, int));
static void zero_group_info1 PARAMS ((struct group_usage_per_unit *));
#endif

static int rs6000_prioritize PARAMS((rtx, rtx));
static int rs6000_is_costly_dependence PARAMS((rtx, rtx, rtx, int));
static int is_dispatch_slot_restricted PARAMS((rtx insn));
static int is_cracked_insn PARAMS((rtx insn));
static int is_microcoded_insn PARAMS((rtx insn));
/* DN end */

static void rs6000_init_builtins PARAMS ((void));
static rtx rs6000_expand_unop_builtin PARAMS ((enum insn_code, tree, rtx));
static rtx rs6000_expand_binop_builtin PARAMS ((enum insn_code, tree, rtx));
static rtx rs6000_expand_ternop_builtin PARAMS ((enum insn_code, tree, rtx));
static rtx rs6000_expand_builtin PARAMS ((tree, rtx, rtx, enum machine_mode, int));
static void altivec_init_builtins PARAMS ((void));
static void rs6000_common_init_builtins PARAMS ((void));

static void enable_mask_for_builtins PARAMS ((struct builtin_description *,
					      int, enum rs6000_builtins,
					      enum rs6000_builtins));
static void spe_init_builtins PARAMS ((void));
static rtx spe_expand_builtin PARAMS ((tree, rtx, bool *));
static rtx spe_expand_predicate_builtin PARAMS ((enum insn_code, tree, rtx));
static rtx spe_expand_evsel_builtin PARAMS ((enum insn_code, tree, rtx));
static int rs6000_emit_int_cmove PARAMS ((rtx, rtx, rtx, rtx));

static rtx altivec_expand_builtin PARAMS ((tree, rtx, bool *));
static rtx altivec_expand_ld_builtin PARAMS ((tree, rtx, bool *));
static rtx altivec_expand_st_builtin PARAMS ((tree, rtx, bool *));
static rtx altivec_expand_dst_builtin PARAMS ((tree, rtx, bool *));
static rtx altivec_expand_abs_builtin PARAMS ((enum insn_code, tree, rtx));
static rtx altivec_expand_predicate_builtin PARAMS ((enum insn_code, const char *, tree, rtx));
static rtx altivec_expand_stv_builtin PARAMS ((enum insn_code, tree));
static void rs6000_parse_abi_options PARAMS ((void));
static void rs6000_parse_vrsave_option PARAMS ((void));
static void rs6000_parse_isel_option PARAMS ((void));
static int first_altivec_reg_to_save PARAMS ((void));
static unsigned int compute_vrsave_mask PARAMS ((void));
static void is_altivec_return_reg PARAMS ((rtx, void *));
static rtx generate_set_vrsave PARAMS ((rtx, rs6000_stack_t *, int));
static void altivec_frame_fixup PARAMS ((rtx, rtx, HOST_WIDE_INT));
/* APPLE LOCAL make easy_vector_constant globally visible (rs6000-protos.h) */

/* Hash table stuff for keeping track of TOC entries.  */

struct toc_hash_struct GTY(())
{
  /* `key' will satisfy CONSTANT_P; in fact, it will satisfy
     ASM_OUTPUT_SPECIAL_POOL_ENTRY_P.  */
  rtx key;
  enum machine_mode key_mode;
  int labelno;
};

static GTY ((param_is (struct toc_hash_struct))) htab_t toc_hash_table;

/* Default register names.  */
char rs6000_reg_names[][8] =
{
      "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
      "8",  "9", "10", "11", "12", "13", "14", "15",
     "16", "17", "18", "19", "20", "21", "22", "23",
     "24", "25", "26", "27", "28", "29", "30", "31",
      "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
      "8",  "9", "10", "11", "12", "13", "14", "15",
     "16", "17", "18", "19", "20", "21", "22", "23",
     "24", "25", "26", "27", "28", "29", "30", "31",
     "mq", "lr", "ctr","ap",
      "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
      "xer",
      /* AltiVec registers.  */
      "0",  "1",  "2",  "3",  "4",  "5",  "6", "7",
      "8",  "9",  "10", "11", "12", "13", "14", "15",
      "16", "17", "18", "19", "20", "21", "22", "23",
      "24", "25", "26", "27", "28", "29", "30", "31",
      "vrsave", "vscr",
      /* SPE registers.  */
      "spe_acc", "spefscr"
};

#ifdef TARGET_REGNAMES
static const char alt_reg_names[][8] =
{
   "%r0",   "%r1",  "%r2",  "%r3",  "%r4",  "%r5",  "%r6",  "%r7",
   "%r8",   "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15",
  "%r16",  "%r17", "%r18", "%r19", "%r20", "%r21", "%r22", "%r23",
  "%r24",  "%r25", "%r26", "%r27", "%r28", "%r29", "%r30", "%r31",
   "%f0",   "%f1",  "%f2",  "%f3",  "%f4",  "%f5",  "%f6",  "%f7",
   "%f8",   "%f9", "%f10", "%f11", "%f12", "%f13", "%f14", "%f15",
  "%f16",  "%f17", "%f18", "%f19", "%f20", "%f21", "%f22", "%f23",
  "%f24",  "%f25", "%f26", "%f27", "%f28", "%f29", "%f30", "%f31",
    "mq",    "lr",  "ctr",   "ap",
  "%cr0",  "%cr1", "%cr2", "%cr3", "%cr4", "%cr5", "%cr6", "%cr7",
   "xer",
  /* AltiVec registers.  */
   "%v0",  "%v1",  "%v2",  "%v3",  "%v4",  "%v5",  "%v6", "%v7",
   "%v8",  "%v9", "%v10", "%v11", "%v12", "%v13", "%v14", "%v15",
  "%v16", "%v17", "%v18", "%v19", "%v20", "%v21", "%v22", "%v23",
  "%v24", "%v25", "%v26", "%v27", "%v28", "%v29", "%v30", "%v31",
  "vrsave", "vscr",
  /* SPE registers.  */
  "spe_acc", "spefscr"
};
#endif

#ifndef MASK_STRICT_ALIGN
#define MASK_STRICT_ALIGN 0
#endif

/* The VRSAVE bitmask puts bit %v0 as the most significant bit.  */
#define ALTIVEC_REG_BIT(REGNO) (0x80000000 >> ((REGNO) - FIRST_ALTIVEC_REGNO))

/* Initialize the GCC target structure.  */
#undef TARGET_ATTRIBUTE_TABLE
#define TARGET_ATTRIBUTE_TABLE rs6000_attribute_table
#undef TARGET_SET_DEFAULT_TYPE_ATTRIBUTES
#define TARGET_SET_DEFAULT_TYPE_ATTRIBUTES rs6000_set_default_type_attributes

#undef TARGET_ASM_ALIGNED_DI_OP
#define TARGET_ASM_ALIGNED_DI_OP DOUBLE_INT_ASM_OP

/* Default unaligned ops are only provided for ELF.  Find the ops needed
   for non-ELF systems.  */
#ifndef OBJECT_FORMAT_ELF
#if TARGET_XCOFF
/* For XCOFF.  rs6000_assemble_integer will handle unaligned DIs on
   64-bit targets.  */
#undef TARGET_ASM_UNALIGNED_HI_OP
#define TARGET_ASM_UNALIGNED_HI_OP "\t.vbyte\t2,"
#undef TARGET_ASM_UNALIGNED_SI_OP
#define TARGET_ASM_UNALIGNED_SI_OP "\t.vbyte\t4,"
#undef TARGET_ASM_UNALIGNED_DI_OP
#define TARGET_ASM_UNALIGNED_DI_OP "\t.vbyte\t8,"
#else
/* For Darwin.  */
#undef TARGET_ASM_UNALIGNED_HI_OP
#define TARGET_ASM_UNALIGNED_HI_OP "\t.short\t"
#undef TARGET_ASM_UNALIGNED_SI_OP
#define TARGET_ASM_UNALIGNED_SI_OP "\t.long\t"
#endif
#endif

/* This hook deals with fixups for relocatable code and DI-mode objects
   in 64-bit code.  */
#undef TARGET_ASM_INTEGER
#define TARGET_ASM_INTEGER rs6000_assemble_integer

#ifdef HAVE_GAS_HIDDEN
#undef TARGET_ASM_ASSEMBLE_VISIBILITY
#define TARGET_ASM_ASSEMBLE_VISIBILITY rs6000_assemble_visibility
#endif

#undef TARGET_ASM_FUNCTION_PROLOGUE
#define TARGET_ASM_FUNCTION_PROLOGUE rs6000_output_function_prologue
#undef TARGET_ASM_FUNCTION_EPILOGUE
#define TARGET_ASM_FUNCTION_EPILOGUE rs6000_output_function_epilogue

#undef  TARGET_SCHED_USE_DFA_PIPELINE_INTERFACE 
#define TARGET_SCHED_USE_DFA_PIPELINE_INTERFACE rs6000_use_dfa_pipeline_interface
#undef  TARGET_SCHED_VARIABLE_ISSUE
#define TARGET_SCHED_VARIABLE_ISSUE rs6000_variable_issue

#undef TARGET_SCHED_ISSUE_RATE
#define TARGET_SCHED_ISSUE_RATE rs6000_issue_rate
#undef TARGET_SCHED_ADJUST_COST
#define TARGET_SCHED_ADJUST_COST rs6000_adjust_cost
#undef TARGET_SCHED_ADJUST_PRIORITY
#define TARGET_SCHED_ADJUST_PRIORITY rs6000_adjust_priority

/* DN begin */
#ifdef DN_SCHED_FINISH
#undef TARGET_SCHED_FINISH
#define TARGET_SCHED_FINISH rs6000_sched_finish
#endif /* DN_SCHED_FINISH */

#undef TARGET_SCHED_PRIORITY_EVALUATION_HOOK
#define TARGET_SCHED_PRIORITY_EVALUATION_HOOK rs6000_prioritize

#undef TARGET_SCHED_IS_COSTLY_DEPENDENCE
#define TARGET_SCHED_IS_COSTLY_DEPENDENCE rs6000_is_costly_dependence
/* DN end */

#undef TARGET_INIT_BUILTINS
#define TARGET_INIT_BUILTINS rs6000_init_builtins

#undef TARGET_EXPAND_BUILTIN
#define TARGET_EXPAND_BUILTIN rs6000_expand_builtin

#undef TARGET_BINDS_LOCAL_P
#define TARGET_BINDS_LOCAL_P rs6000_binds_local_p

#undef TARGET_ASM_OUTPUT_MI_THUNK
#define TARGET_ASM_OUTPUT_MI_THUNK rs6000_output_mi_thunk

/* ??? Should work everywhere, but ask dje@watson.ibm.com before
   enabling for AIX.  */
#if TARGET_OBJECT_FORMAT != OBJECT_XCOFF
#undef TARGET_ASM_CAN_OUTPUT_MI_THUNK
#define TARGET_ASM_CAN_OUTPUT_MI_THUNK default_can_output_mi_thunk_no_vcall
#endif

struct gcc_target targetm = TARGET_INITIALIZER;

/* APPLE LOCAL begin -fast */
extern void set_target_switch PARAMS ((const char *));
extern void set_fast_math_flags PARAMS ((int set));
/* APPLE LOCAL end -fast */

/* Override command line options.  Mostly we process the processor
   type and sometimes adjust other TARGET_ options.  */

void
rs6000_override_options (default_cpu)
     const char *default_cpu;
{
  size_t i, j;
  struct rs6000_cpu_select *ptr;


  /* Simplify the entries below by making a mask for any POWER
     variant and any PowerPC variant.  */

#define POWER_MASKS (MASK_POWER | MASK_POWER2 | MASK_MULTIPLE | MASK_STRING)
#define POWERPC_MASKS (MASK_POWERPC | MASK_PPC_GPOPT \
		       | MASK_PPC_GFXOPT | MASK_POWERPC64)
#define POWERPC_OPT_MASKS (MASK_PPC_GPOPT | MASK_PPC_GFXOPT)

  static struct ptt
    {
      const char *const name;		/* Canonical processor name.  */
      const enum processor_type processor; /* Processor type enum value.  */
      const int target_enable;	/* Target flags to enable.  */
      const int target_disable;	/* Target flags to disable.  */
    } const processor_target_table[]
      = {{"common", PROCESSOR_COMMON, MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_MASKS},
	 {"power", PROCESSOR_POWER,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"power2", PROCESSOR_POWER,
	    MASK_POWER | MASK_POWER2 | MASK_MULTIPLE | MASK_STRING,
	    POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"power3", PROCESSOR_PPC630,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS},
	 {"power4", PROCESSOR_POWER4,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS},
	 {"powerpc", PROCESSOR_POWERPC,
	    MASK_POWERPC | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"powerpc64", PROCESSOR_POWERPC64,
	    MASK_POWERPC | MASK_POWERPC64 | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS},
	 {"rios", PROCESSOR_RIOS1,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"rios1", PROCESSOR_RIOS1,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"rsc", PROCESSOR_PPC601,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"rsc1", PROCESSOR_PPC601,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"rios2", PROCESSOR_RIOS2,
	    MASK_POWER | MASK_MULTIPLE | MASK_STRING | MASK_POWER2,
	    POWERPC_MASKS | MASK_NEW_MNEMONICS},
	 {"rs64a", PROCESSOR_RS64A,
	    MASK_POWERPC | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS},
	 {"401", PROCESSOR_PPC403,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"403", PROCESSOR_PPC403,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS | MASK_STRICT_ALIGN,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"405", PROCESSOR_PPC405,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"505", PROCESSOR_MPCCORE,
	    MASK_POWERPC | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"601", PROCESSOR_PPC601,
	    MASK_POWER | MASK_POWERPC | MASK_NEW_MNEMONICS | MASK_MULTIPLE | MASK_STRING,
	    MASK_POWER2 | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"602", PROCESSOR_PPC603,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"603", PROCESSOR_PPC603,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"603e", PROCESSOR_PPC603,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"ec603e", PROCESSOR_PPC603,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"604", PROCESSOR_PPC604,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"604e", PROCESSOR_PPC604e,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"620", PROCESSOR_PPC620,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS},
	 {"630", PROCESSOR_PPC630,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS},
	 {"740", PROCESSOR_PPC750,
 	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
 	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"750", PROCESSOR_PPC750,
 	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
 	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"G3", PROCESSOR_PPC750,
 	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
 	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"7400", PROCESSOR_PPC7400,
            MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
            POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"7450", PROCESSOR_PPC7450,
            MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
            POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"G4", PROCESSOR_PPC7450,
            MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
            POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"8540", PROCESSOR_PPC8540,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | MASK_PPC_GPOPT | MASK_POWERPC64},
	 {"801", PROCESSOR_MPCCORE,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"821", PROCESSOR_MPCCORE,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"823", PROCESSOR_MPCCORE,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"860", PROCESSOR_MPCCORE,
	    MASK_POWERPC | MASK_SOFT_FLOAT | MASK_NEW_MNEMONICS,
	    POWER_MASKS | POWERPC_OPT_MASKS | MASK_POWERPC64},
	 {"970", PROCESSOR_POWER4,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS},
	 {"G5", PROCESSOR_POWER4,
	    MASK_POWERPC | MASK_PPC_GFXOPT | MASK_NEW_MNEMONICS,
	    POWER_MASKS}};

  const size_t ptt_size = ARRAY_SIZE (processor_target_table);

  /* Save current -mmultiple/-mno-multiple status.  */
  int multiple = TARGET_MULTIPLE;
  /* Save current -mstring/-mno-string status.  */
  int string = TARGET_STRING;
  int mtune = 0;
  enum processor_type mcpu_cpu;

  /* Identify the processor type.  */
  rs6000_select[0].string = default_cpu;
  rs6000_cpu = TARGET_POWERPC64 ? PROCESSOR_DEFAULT64 : PROCESSOR_DEFAULT;
  /* APPLE LOCAL begin -fast */
  if (flag_fast || flag_fastf)
  {
    mcpu_cpu = PROCESSOR_POWER4;
    if (rs6000_select[1].string == (char *)0 && rs6000_select[2].string == (char *)0)
    {
      /* -mcpu and -mtune unspecified. Assume both are G5 */
      set_target_switch ("tune=G5");
      set_target_switch ("cpu=G5");
    }
  }
  /* APPLE LOCAL end -fast */

  for (i = 0; i < ARRAY_SIZE (rs6000_select); i++)
    {
      ptr = &rs6000_select[i];
      if (ptr->string != (char *)0 && ptr->string[0] != '\0')
	{
	  for (j = 0; j < ptt_size; j++)
	    if (! strcmp (ptr->string, processor_target_table[j].name))
	      {
		if (ptr->set_tune_p)
		{
		  rs6000_cpu = processor_target_table[j].processor;
		  mtune = 1;
                }  
		if (ptr->set_arch_p)
		  {
		    target_flags |= processor_target_table[j].target_enable;
		    target_flags &= ~processor_target_table[j].target_disable;
		    mcpu_cpu = processor_target_table[j].processor;
		  }
		break;
	      }

	  if (j == ptt_size)
	    error ("bad value (%s) for %s switch", ptr->string, ptr->name);
	}
    }

  /* APPLE LOCAL begin -fast */
  if (flag_fast || flag_fastf)   
    {
      flag_loop_transpose = 1;
      flag_loop_to_memset = 1;
      flag_relax_aliasing = 1;
      flag_load_after_store = 1;
      flag_gcse_mem_alias = 1;
      flag_gcse_sm = 0;
      flag_gcse_loop_depth = 1;
      insert_sched_nops = 2;
      use_sched_prioritize_hook=1;
      flag_unroll_loops = 1;
      flag_strict_aliasing = 1;
      flag_schedule_interblock = 1;
      align_jumps_max_skip = 15;
      align_loops_max_skip = 15;
      align_functions = 16;
      align_loops = 16;
      align_jumps = 16;
      set_fast_math_flags (1);


      if (!flag_pic)
        set_target_switch ("dynamic-no-pic");

      if (mcpu_cpu == PROCESSOR_POWER4)
      {
        flag_inline_floor = 1;
        flag_mpowerpc64fix = 1;
        set_target_switch ("powerpc-gpopt");
        set_target_switch ("powerpc64");
      }

      if (flag_fast)
	set_target_switch ("align-natural");
      set_param_value ("max-gcse-passes", 3);
      /* IMI flags */
      disable_typechecking_for_spec_flag = 1;
      flag_unit_at_a_time = 1;
      flag_callgraph_inlining = 1;

      /* Haifa's latest fcse-after-ra flags */
      flag_gcse_after_ra = 1; 
      flag_gcse_after_ra_case1 = flag_gcse_after_ra_case2 = 0;
      flag_gcse_after_ra_parial = 1;
      flag_gcse_after_ra_hot_fraction = 20;
      sched_costly_dep = 2;
      flag_gcse_after_ra_partial_fraction = 3;
    }
  if (flag_use_feedback || flag_reorder_blocks_and_partition)
    {
      flag_reorder_blocks_and_partition = 1;
      flag_branch_probabilities = 1;
      flag_branch_predictions = 1;
      branch_predictions_threshold = 98;
    }
  /* APPLE LOCAL end -fast */

  /* APPLE LOCAL begin extra settings for -mtune=G5|970 */
  if (mtune && !flag_fast && optimize >= 3 && rs6000_cpu == PROCESSOR_POWER4)
  {   
    if (!align_functions)
    align_functions = 16;
    if (!align_loops)
    {
      align_loops = 16;
      align_loops_max_skip = 15;
    }
    if (!align_jumps)
    {
      align_jumps = 16;
      align_jumps_max_skip = 15;
    }
  }
  /* APPLE LOCAL end extra settings for -mtune=G5|970 */

  if (rs6000_cpu == PROCESSOR_PPC8540)
    rs6000_isel = 1;

  /* If we are optimizing big endian systems for space, use the load/store
     multiple and string instructions.  */
  if (BYTES_BIG_ENDIAN && optimize_size)
    target_flags |= MASK_MULTIPLE | MASK_STRING;

  /* If -mmultiple or -mno-multiple was explicitly used, don't
     override with the processor default */
  if (TARGET_MULTIPLE_SET)
    target_flags = (target_flags & ~MASK_MULTIPLE) | multiple;

  /* If -mstring or -mno-string was explicitly used, don't override
     with the processor default.  */
  if (TARGET_STRING_SET)
    target_flags = (target_flags & ~MASK_STRING) | string;

  /* Don't allow -mmultiple or -mstring on little endian systems
     unless the cpu is a 750, because the hardware doesn't support the
     instructions used in little endian mode, and causes an alignment
     trap.  The 750 does not cause an alignment trap (except when the
     target is unaligned).  */

  if (! BYTES_BIG_ENDIAN && rs6000_cpu != PROCESSOR_PPC750)
    {
      if (TARGET_MULTIPLE)
	{
	  target_flags &= ~MASK_MULTIPLE;
	  if (TARGET_MULTIPLE_SET)
	    warning ("-mmultiple is not supported on little endian systems");
	}

      if (TARGET_STRING)
	{
	  target_flags &= ~MASK_STRING;
	  if (TARGET_STRING_SET)
	    warning ("-mstring is not supported on little endian systems");
	}
    }

  /* APPLE LOCAL begin dynamic-no-pic  */
  if (DEFAULT_ABI == ABI_DARWIN)
    {
#if TARGET_MACHO
      if (MACHO_DYNAMIC_NO_PIC_P ())
	{
	  if (flag_pic)
	    warning ("-mdynamic-no-pic overrides -fpic or -fPIC");
	  flag_pic = 0;
	}
      else
#endif
      if (flag_pic == 1)
	{
	  /* Darwin doesn't support -fpic.  */
	  warning ("-fpic is not supported; -fPIC assumed");  
	  flag_pic = 2;
	}
    }
  else
  /* APPLE LOCAL end dynamic-no-pic  */
  if (flag_pic != 0 && DEFAULT_ABI == ABI_AIX)
    {
      rs6000_flag_pic = flag_pic;
      flag_pic = 0;
    }

  /* For Darwin, always silently make -fpic and -fPIC identical.  */
  if (flag_pic == 1 && DEFAULT_ABI == ABI_DARWIN)
    flag_pic = 2;

  /* Set debug flags */
  if (rs6000_debug_name)
    {
      if (! strcmp (rs6000_debug_name, "all"))
	rs6000_debug_stack = rs6000_debug_arg = 1;
      else if (! strcmp (rs6000_debug_name, "stack"))
	rs6000_debug_stack = 1;
      else if (! strcmp (rs6000_debug_name, "arg"))
	rs6000_debug_arg = 1;
      else
	error ("unknown -mdebug-%s switch", rs6000_debug_name);
    }

  if (rs6000_traceback_name)
    {
      if (! strncmp (rs6000_traceback_name, "full", 4))
	rs6000_traceback = traceback_full;
      else if (! strncmp (rs6000_traceback_name, "part", 4))
	rs6000_traceback = traceback_part;
      else if (! strncmp (rs6000_traceback_name, "no", 2))
	rs6000_traceback = traceback_none;
      else
	error ("unknown -mtraceback arg `%s'; expecting `full', `partial' or `none'",
	       rs6000_traceback_name);
    }

  /* Set size of long double */
  rs6000_long_double_type_size = 64;
  if (rs6000_long_double_size_string)
    {
      char *tail;
      int size = strtol (rs6000_long_double_size_string, &tail, 10);
      if (*tail != '\0' || (size != 64 && size != 128))
	error ("Unknown switch -mlong-double-%s",
	       rs6000_long_double_size_string);
      else
	rs6000_long_double_type_size = size;
    }

  /* Handle -mabi= options.  */
  rs6000_parse_abi_options ();

  /* Handle -mvrsave= option.  */
  rs6000_parse_vrsave_option ();

  /* APPLE LOCAL begin AltiVec */
  /* -faltivec implies -maltivec.  */
  if (flag_altivec)
    target_flags |= MASK_ALTIVEC;
  /* APPLE LOCAL end AltiVec */
    
  /* Handle -misel= option.  */
  rs6000_parse_isel_option ();

  /* APPLE LOCAL begin forbidding over-speculative motion */
  /* Change the # of insns to be converted to conditional execution */
  if (rs6000_condexec_insns_str)
    rs6000_condexec_insns = atoi (rs6000_condexec_insns_str);
  /* APPLE LOCAL end forbidding over-speculative motion */

#ifdef SUBTARGET_OVERRIDE_OPTIONS
  SUBTARGET_OVERRIDE_OPTIONS;
#endif
#ifdef SUBSUBTARGET_OVERRIDE_OPTIONS
  SUBSUBTARGET_OVERRIDE_OPTIONS;
#endif

  /* Handle -m(no-)longcall option.  This is a bit of a cheap hack,
     using TARGET_OPTIONS to handle a toggle switch, but we're out of
     bits in target_flags so TARGET_SWITCHES cannot be used.
     Assumption here is that rs6000_longcall_switch points into the
     text of the complete option, rather than being a copy, so we can
     scan back for the presence or absence of the no- modifier.  */
  if (rs6000_longcall_switch)
    {
      const char *base = rs6000_longcall_switch;
      while (base[-1] != 'm') base--;

      if (*rs6000_longcall_switch != '\0')
	error ("invalid option `%s'", base);
      rs6000_default_long_calls = (base[0] != 'n');
    }

#ifdef TARGET_REGNAMES
  /* If the user desires alternate register names, copy in the
     alternate names now.  */
  if (TARGET_REGNAMES)
    memcpy (rs6000_reg_names, alt_reg_names, sizeof (rs6000_reg_names));
#endif

  /* Set TARGET_AIX_STRUCT_RET last, after the ABI is determined.
     If -maix-struct-return or -msvr4-struct-return was explicitly
     used, don't override with the ABI default.  */
  if (!(target_flags & MASK_AIX_STRUCT_RET_SET))
    {
      if (DEFAULT_ABI == ABI_V4 && !DRAFT_V4_STRUCT_RET)
	target_flags = (target_flags & ~MASK_AIX_STRUCT_RET);
      else
	target_flags |= MASK_AIX_STRUCT_RET;
    }

  if (TARGET_LONG_DOUBLE_128
      && (DEFAULT_ABI == ABI_AIX || DEFAULT_ABI == ABI_DARWIN))
    real_format_for_mode[TFmode - QFmode] = &ibm_extended_format;

  /* Allocate an alias set for register saves & restores from stack.  */
  rs6000_sr_alias_set = new_alias_set ();

  if (TARGET_TOC) 
    ASM_GENERATE_INTERNAL_LABEL (toc_label_name, "LCTOC", 1);

  /* We can only guarantee the availability of DI pseudo-ops when
     assembling for 64-bit targets.  */
  if (!TARGET_64BIT)
    {
      targetm.asm_out.aligned_op.di = NULL;
      targetm.asm_out.unaligned_op.di = NULL;
    }

  /* Set maximum branch target alignment at two instructions, eight bytes.  */
/* APPLE LOCAL begin -f[align-loops|align-jumps]-max-skip */
  if (align_jumps_max_skip <= 0)
    align_jumps_max_skip = 8;
  if (align_loops_max_skip <= 0)
    align_loops_max_skip = 8;
/* APPLE LOCAL end -f[align-loops|align-jumps]-max-skip */

  /* Arrange to save and restore machine status around nested functions.  */
  init_machine_status = rs6000_init_machine_status;
}

/* Handle -misel= option.  */
static void
rs6000_parse_isel_option ()
{
  if (rs6000_isel_string == 0)
    return;
  else if (! strcmp (rs6000_isel_string, "yes"))
    rs6000_isel = 1;
  else if (! strcmp (rs6000_isel_string, "no"))
    rs6000_isel = 0;
  else
    error ("unknown -misel= option specified: '%s'",
         rs6000_isel_string);
}

/* Handle -mvrsave= options.  */
static void
rs6000_parse_vrsave_option ()
{
  /* Generate VRSAVE instructions by default.  */
  if (rs6000_altivec_vrsave_string == 0
      || ! strcmp (rs6000_altivec_vrsave_string, "yes"))
    rs6000_altivec_vrsave = 1;
  else if (! strcmp (rs6000_altivec_vrsave_string, "no"))
    rs6000_altivec_vrsave = 0;
  else
    error ("unknown -mvrsave= option specified: '%s'",
	   rs6000_altivec_vrsave_string);
}

/* Handle -mabi= options.  */
static void
rs6000_parse_abi_options ()
{
  if (rs6000_abi_string == 0)
    return;
  else if (! strcmp (rs6000_abi_string, "altivec"))
    rs6000_altivec_abi = 1;
  else if (! strcmp (rs6000_abi_string, "no-altivec"))
    rs6000_altivec_abi = 0;
  else if (! strcmp (rs6000_abi_string, "spe"))
    rs6000_spe_abi = 1;
  else if (! strcmp (rs6000_abi_string, "no-spe"))
    rs6000_spe_abi = 0;
  else
    error ("unknown ABI specified: '%s'", rs6000_abi_string);
}

void
optimization_options (level, size)
     int level ATTRIBUTE_UNUSED;
     int size ATTRIBUTE_UNUSED;
{
  /* APPLE LOCAL begin tweak default optimizations */
  if (DEFAULT_ABI == ABI_DARWIN)
    {
      /* Turn these on only if specifically requested, not with -O* */
      /* Strict aliasing breaks too much existing code */
      flag_strict_aliasing = 0;
      /* Block reordering causes code bloat, and very little speedup */
      flag_reorder_blocks = 0;
      /* Multi-basic-block scheduling loses badly when the compiler
         misguesses which blocks are going to be executed, more than
	 it gains when it guesses correctly.  Its guesses for cases
	 where interblock scheduling occurs (if-then-else's) are
	 little better than random, so disable this unless requested. */
      flag_schedule_interblock = 0;
    }
  /* APPLE LOCAL end tweak default optimizations */
}

/* Do anything needed at the start of the asm file.  */

void
rs6000_file_start (file, default_cpu)
     FILE *file;
     const char *default_cpu;
{
  size_t i;
  char buffer[80];
  const char *start = buffer;
  struct rs6000_cpu_select *ptr;

  if (flag_verbose_asm)
    {
      sprintf (buffer, "\n%s rs6000/powerpc options:", ASM_COMMENT_START);
      rs6000_select[0].string = default_cpu;

      for (i = 0; i < ARRAY_SIZE (rs6000_select); i++)
	{
	  ptr = &rs6000_select[i];
	  if (ptr->string != (char *)0 && ptr->string[0] != '\0')
	    {
	      fprintf (file, "%s %s%s", start, ptr->name, ptr->string);
	      start = "";
	    }
	}

#ifdef USING_ELFOS_H
      switch (rs6000_sdata)
	{
	case SDATA_NONE: fprintf (file, "%s -msdata=none", start); start = ""; break;
	case SDATA_DATA: fprintf (file, "%s -msdata=data", start); start = ""; break;
	case SDATA_SYSV: fprintf (file, "%s -msdata=sysv", start); start = ""; break;
	case SDATA_EABI: fprintf (file, "%s -msdata=eabi", start); start = ""; break;
	}

      if (rs6000_sdata && g_switch_value)
	{
	  fprintf (file, "%s -G %d", start, g_switch_value);
	  start = "";
	}
#endif

      if (*start == '\0')
	putc ('\n', file);
    }
}

/* APPLE LOCAL begin AltiVec */
/* Return either PERMUTE or SIMPLE based on the best scheduling of INSN.  */

const char *
choose_vec_easy (insn, permute, simple)
     rtx insn;
     const char *permute;
     const char *simple;
{
  /* Remember our last choice.  */
  static enum attr_type last_easy = TYPE_INTEGER;
  enum attr_type type;
  /* First look at the previous instruction.  */
  rtx prev = prev_active_insn (insn);
  /* Jump tables don't have a type; prevent get_attr_type from crashing.  */
  if (prev && GET_CODE (prev) == JUMP_INSN &&
	(GET_CODE (PATTERN (prev)) == ADDR_VEC 
	  || GET_CODE (PATTERN (prev)) == ADDR_DIFF_VEC))
    prev = 0;
  type = prev ? get_attr_type (prev) : TYPE_INTEGER;

  /* If the previous instruction was VEC_EASY,
     its chosen type is in last_easy.  */
  if (type == TYPE_VEC_EASY)
    type = last_easy;

  /* Attempt to alternate VEC_PERM and VEC_SIMPLE.  */
  if (type == TYPE_VECPERM)
    last_easy = TYPE_VECSIMPLE;
  else if (type == TYPE_VECSIMPLE || type == TYPE_VECCOMPLEX || type == TYPE_VECFLOAT)
    last_easy = TYPE_VECPERM;
  else
    {
      /* Look at the next instruction to decide.  */
      rtx next = next_active_insn (insn);
      /* Jump tables don't have a type; prevent get_attr_type from crashing.  */
      if (next && GET_CODE (next) == JUMP_INSN
	   && (GET_CODE (PATTERN (next)) == ADDR_VEC 
	       || GET_CODE (PATTERN (next)) == ADDR_DIFF_VEC))
	next = 0;
      type = next ? get_attr_type (next) : TYPE_INTEGER;
      /* Prefer VEC_PERM unless the next instruction conflicts.  */
      last_easy = (type == TYPE_VECPERM ? TYPE_VECSIMPLE : TYPE_VECPERM);
    }
  extract_insn (insn);
  return (last_easy == TYPE_VECSIMPLE ? simple : permute);
}

/* APPLE LOCAL end AltiVec */

/* Return nonzero if this function is known to have a null epilogue.  */

int
direct_return ()
{
  if (reload_completed)
    {
      rs6000_stack_t *info = rs6000_stack_info ();

      if (info->first_gp_reg_save == 32
	  && info->first_fp_reg_save == 64
	  && info->first_altivec_reg_save == LAST_ALTIVEC_REGNO + 1
	  /* APPLE LOCAL AltiVec */
	  && ! info->vrsave_save_p
	  && ! info->lr_save_p
	  && ! info->cr_save_p
	  && info->vrsave_mask == 0
	  && ! info->push_p)
	return 1;
    }

  return 0;
}

/* Returns 1 always.  */

int
any_operand (op, mode)
     rtx op ATTRIBUTE_UNUSED;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return 1;
}

/* Returns 1 if op is the count register.  */
int
count_register_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  if (GET_CODE (op) != REG)
    return 0;

  if (REGNO (op) == COUNT_REGISTER_REGNUM)
    return 1;

  if (REGNO (op) > FIRST_PSEUDO_REGISTER)
    return 1;

  return 0;
}

/* Returns 1 if op is an altivec register.  */
int
altivec_register_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  
  return (register_operand (op, mode)
	  && (GET_CODE (op) != REG
	      || REGNO (op) > FIRST_PSEUDO_REGISTER
	      || ALTIVEC_REGNO_P (REGNO (op))));
}

int
xer_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  if (GET_CODE (op) != REG)
    return 0;

  if (XER_REGNO_P (REGNO (op)))
    return 1;

  return 0;
}

/* Return 1 if OP is a signed 8-bit constant.  Int multiplication
   by such constants completes more quickly.  */

int
s8bit_cint_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return ( GET_CODE (op) == CONST_INT
	  && (INTVAL (op) >= -128 && INTVAL (op) <= 127));
}

/* Return 1 if OP is a constant that can fit in a D field.  */

int
short_cint_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == CONST_INT
	  && CONST_OK_FOR_LETTER_P (INTVAL (op), 'I'));
}

/* Similar for an unsigned D field.  */

int
u_short_cint_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == CONST_INT
	  && CONST_OK_FOR_LETTER_P (INTVAL (op) & GET_MODE_MASK (mode), 'K'));
}

/* Return 1 if OP is a CONST_INT that cannot fit in a signed D field.  */

int
non_short_cint_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == CONST_INT
	  && (unsigned HOST_WIDE_INT) (INTVAL (op) + 0x8000) >= 0x10000);
}

/* Returns 1 if OP is a CONST_INT that is a positive value
   and an exact power of 2.  */

int
exact_log2_cint_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == CONST_INT
	  && INTVAL (op) > 0
	  && exact_log2 (INTVAL (op)) >= 0);
}

/* Returns 1 if OP is a register that is not special (i.e., not MQ,
   ctr, or lr).  */

int
gpc_reg_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode)
	  && (GET_CODE (op) != REG
	      || (REGNO (op) >= ARG_POINTER_REGNUM 
		  /* APPLE LOCAL begin AltiVec */
		  && !XER_REGNO_P (REGNO (op))
		  && !ALTIVEC_REGNO_P (REGNO (op)))
		  /* APPLE LOCAL end AltiVec */
	      || REGNO (op) < MQ_REGNO));
}

/* Returns 1 if OP is either a pseudo-register or a register denoting a
   CR field.  */

int
cc_reg_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode)
	  && (GET_CODE (op) != REG
	      || REGNO (op) >= FIRST_PSEUDO_REGISTER
	      || CR_REGNO_P (REGNO (op))));
}

/* Returns 1 if OP is either a pseudo-register or a register denoting a
   CR field that isn't CR0.  */

int
cc_reg_not_cr0_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (register_operand (op, mode)
	  && (GET_CODE (op) != REG
	      || REGNO (op) >= FIRST_PSEUDO_REGISTER
	      || CR_REGNO_NOT_CR0_P (REGNO (op))));
}

/* Returns 1 if OP is either a constant integer valid for a D-field or
   a non-special register.  If a register, it must be in the proper
   mode unless MODE is VOIDmode.  */

int
reg_or_short_operand (op, mode)
      rtx op;
      enum machine_mode mode;
{
  return short_cint_operand (op, mode) || gpc_reg_operand (op, mode);
}

/* Similar, except check if the negation of the constant would be
   valid for a D-field.  */

int
reg_or_neg_short_operand (op, mode)
      rtx op;
      enum machine_mode mode;
{
  if (GET_CODE (op) == CONST_INT)
    return CONST_OK_FOR_LETTER_P (INTVAL (op), 'P');

  return gpc_reg_operand (op, mode);
}

/* Returns 1 if OP is either a constant integer valid for a DS-field or
   a non-special register.  If a register, it must be in the proper
   mode unless MODE is VOIDmode.  */

int
reg_or_aligned_short_operand (op, mode)
      rtx op;
      enum machine_mode mode;
{
  if (gpc_reg_operand (op, mode))
    return 1;
  else if (short_cint_operand (op, mode) && !(INTVAL (op) & 3))
    return 1;

  return 0;
}


/* Return 1 if the operand is either a register or an integer whose
   high-order 16 bits are zero.  */

int
reg_or_u_short_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return u_short_cint_operand (op, mode) || gpc_reg_operand (op, mode);
}

/* Return 1 is the operand is either a non-special register or ANY
   constant integer.  */

int
reg_or_cint_operand (op, mode)
    rtx op;
    enum machine_mode mode;
{
  return (GET_CODE (op) == CONST_INT || gpc_reg_operand (op, mode));
}

/* Return 1 is the operand is either a non-special register or ANY
   32-bit signed constant integer.  */

int
reg_or_arith_cint_operand (op, mode)
    rtx op;
    enum machine_mode mode;
{
  return (gpc_reg_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT
#if HOST_BITS_PER_WIDE_INT != 32
	      && ((unsigned HOST_WIDE_INT) (INTVAL (op) + 0x80000000)
		  < (unsigned HOST_WIDE_INT) 0x100000000ll)
#endif
	      ));
}

/* Return 1 is the operand is either a non-special register or a 32-bit
   signed constant integer valid for 64-bit addition.  */

int
reg_or_add_cint64_operand (op, mode)
    rtx op;
    enum machine_mode mode;
{
  return (gpc_reg_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT
#if HOST_BITS_PER_WIDE_INT == 32
	      && INTVAL (op) < 0x7fff8000
#else
	      && ((unsigned HOST_WIDE_INT) (INTVAL (op) + 0x80008000)
		  < 0x100000000ll)
#endif
	      ));
}

/* Return 1 is the operand is either a non-special register or a 32-bit
   signed constant integer valid for 64-bit subtraction.  */

int
reg_or_sub_cint64_operand (op, mode)
    rtx op;
    enum machine_mode mode;
{
  return (gpc_reg_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT
#if HOST_BITS_PER_WIDE_INT == 32
	      && (- INTVAL (op)) < 0x7fff8000
#else
	      && ((unsigned HOST_WIDE_INT) ((- INTVAL (op)) + 0x80008000)
		  < 0x100000000ll)
#endif
	      ));
}

/* Return 1 is the operand is either a non-special register or ANY
   32-bit unsigned constant integer.  */

int
reg_or_logical_cint_operand (op, mode)
    rtx op;
    enum machine_mode mode;
{
  if (GET_CODE (op) == CONST_INT)
    {
      if (GET_MODE_BITSIZE (mode) > HOST_BITS_PER_WIDE_INT)
	{
	  if (GET_MODE_BITSIZE (mode) <= 32)
	    abort ();

	  if (INTVAL (op) < 0)
	    return 0;
	}

      return ((INTVAL (op) & GET_MODE_MASK (mode)
	       & (~ (unsigned HOST_WIDE_INT) 0xffffffff)) == 0);
    }
  else if (GET_CODE (op) == CONST_DOUBLE)
    {
      if (GET_MODE_BITSIZE (mode) <= HOST_BITS_PER_WIDE_INT
	  || mode != DImode)
	abort ();

      return CONST_DOUBLE_HIGH (op) == 0;
    }
  else 
    return gpc_reg_operand (op, mode);
}

/* Return 1 if the operand is an operand that can be loaded via the GOT.  */

int
got_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == SYMBOL_REF
	  || GET_CODE (op) == CONST
	  || GET_CODE (op) == LABEL_REF);
}

/* Return 1 if the operand is a simple references that can be loaded via
   the GOT (labels involving addition aren't allowed).  */

int
got_no_const_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == SYMBOL_REF || GET_CODE (op) == LABEL_REF);
}

/* Return the number of instructions it takes to form a constant in an
   integer register.  */

static int
num_insns_constant_wide (value)
     HOST_WIDE_INT value;
{
  /* signed constant loadable with {cal|addi} */
  if (CONST_OK_FOR_LETTER_P (value, 'I'))
    return 1;

  /* constant loadable with {cau|addis} */
  else if (CONST_OK_FOR_LETTER_P (value, 'L'))
    return 1;

#if HOST_BITS_PER_WIDE_INT == 64
  else if (TARGET_POWERPC64)
    {
      HOST_WIDE_INT low  = ((value & 0xffffffff) ^ 0x80000000) - 0x80000000;
      HOST_WIDE_INT high = value >> 31;

      if (high == 0 || high == -1)
	return 2;

      high >>= 1;

      if (low == 0)
	return num_insns_constant_wide (high) + 1;
      else
	return (num_insns_constant_wide (high)
		+ num_insns_constant_wide (low) + 1);
    }
#endif

  else
    return 2;
}

int
num_insns_constant (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) == CONST_INT)
    {
#if HOST_BITS_PER_WIDE_INT == 64
      if ((INTVAL (op) >> 31) != 0 && (INTVAL (op) >> 31) != -1
	  && mask64_operand (op, mode))
	    return 2;
      else
#endif
	return num_insns_constant_wide (INTVAL (op));
    }

  else if (GET_CODE (op) == CONST_DOUBLE && mode == SFmode)
    {
      long l;
      REAL_VALUE_TYPE rv;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
      REAL_VALUE_TO_TARGET_SINGLE (rv, l);
      return num_insns_constant_wide ((HOST_WIDE_INT) l);
    }

  else if (GET_CODE (op) == CONST_DOUBLE)
    {
      HOST_WIDE_INT low;
      HOST_WIDE_INT high;
      long l[2];
      REAL_VALUE_TYPE rv;
      int endian = (WORDS_BIG_ENDIAN == 0);

      if (mode == VOIDmode || mode == DImode)
	{
	  high = CONST_DOUBLE_HIGH (op);
	  low  = CONST_DOUBLE_LOW (op);
	}
      else
	{
	  REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
	  REAL_VALUE_TO_TARGET_DOUBLE (rv, l);
	  high = l[endian];
	  low  = l[1 - endian];
	}

      if (TARGET_32BIT)
	return (num_insns_constant_wide (low)
		+ num_insns_constant_wide (high));

      else
	{
	  if (high == 0 && low >= 0)
	    return num_insns_constant_wide (low);

	  else if (high == -1 && low < 0)
	    return num_insns_constant_wide (low);

	  else if (mask64_operand (op, mode))
	    return 2;

	  else if (low == 0)
	    return num_insns_constant_wide (high) + 1;

	  else
	    return (num_insns_constant_wide (high)
		    + num_insns_constant_wide (low) + 1);
	}
    }

  else
    abort ();
}

/* Return 1 if the operand is a CONST_DOUBLE and it can be put into a
   register with one instruction per word.  We only do this if we can
   safely read CONST_DOUBLE_{LOW,HIGH}.  */

int
easy_fp_constant (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) != CONST_DOUBLE
      || GET_MODE (op) != mode
      || (GET_MODE_CLASS (mode) != MODE_FLOAT && mode != DImode))
    return 0;

  /* Consider all constants with -msoft-float to be easy.  */
  if ((TARGET_SOFT_FLOAT || !TARGET_FPRS)
      && mode != DImode)
    return 1;

  /* If we are using V.4 style PIC, consider all constants to be hard.  */
  if (flag_pic && DEFAULT_ABI == ABI_V4)
    return 0;

#ifdef TARGET_RELOCATABLE
  /* Similarly if we are using -mrelocatable, consider all constants
     to be hard.  */
  if (TARGET_RELOCATABLE)
    return 0;
#endif

  if (mode == TFmode)
    {
      long k[4];
      REAL_VALUE_TYPE rv;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
      REAL_VALUE_TO_TARGET_LONG_DOUBLE (rv, k);

      return (num_insns_constant_wide ((HOST_WIDE_INT) k[0]) == 1
	      && num_insns_constant_wide ((HOST_WIDE_INT) k[1]) == 1
	      && num_insns_constant_wide ((HOST_WIDE_INT) k[2]) == 1
	      && num_insns_constant_wide ((HOST_WIDE_INT) k[3]) == 1);
    }

  else if (mode == DFmode)
    {
      long k[2];
      REAL_VALUE_TYPE rv;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
      REAL_VALUE_TO_TARGET_DOUBLE (rv, k);

      return (num_insns_constant_wide ((HOST_WIDE_INT) k[0]) == 1
	      && num_insns_constant_wide ((HOST_WIDE_INT) k[1]) == 1);
    }

  else if (mode == SFmode)
    {
      long l;
      REAL_VALUE_TYPE rv;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, op);
      REAL_VALUE_TO_TARGET_SINGLE (rv, l);

      return num_insns_constant_wide (l) == 1;
    }

  else if (mode == DImode)
    return ((TARGET_POWERPC64
	     && GET_CODE (op) == CONST_DOUBLE && CONST_DOUBLE_LOW (op) == 0)
	    || (num_insns_constant (op, DImode) <= 2));

  else if (mode == SImode)
    return 1;
  else
    abort ();
}

/* APPLE LOCAL begin AltiVec */
/* Return 1..8 indicating how to compute the value if the operand is a
   CONST_VECTOR and it can be put into a register with one instruction.  */
/* This version replaces Aldy's version; it is far more functional. */

int
easy_vector_constant (op)
     rtx op;
{
  unsigned HOST_WIDE_INT elt[4];
  int i;
  if (GET_CODE (op) != CONST_VECTOR || !VECTOR_MODE_P (GET_MODE (op)))
    return 0;

  if (CONST_VECTOR_NUNITS (op) != 4 )
    abort ();

  for (i = 0; i < 4; i++ )
    {
      if ( GET_CODE (CONST_VECTOR_ELT (op, i)) != CONST_INT )
	return 0;
      elt[i] = INTVAL (CONST_VECTOR_ELT (op, i));
    }

  /* If the four 32-bit words aren't the same, it can't be done unless it
     matches an lvsl or lvsr value.  */
  if (elt[0] != elt[1] || elt[0] != elt[2] || elt[0] != elt[3])
    {
      if (elt[0] + 0x04040404 == elt[1]
	  && elt[1] + 0x04040404 == elt[2]
	  && elt[2] + 0x04040404 == elt[3]
	  && (elt[0] >> 16) + 0x0202 == (elt[0] & 0xffff)
	  && (elt[0] >> 24) + 1 == ((elt[0] >> 16) & 0xff)
	  && (elt[0] >>= 24) <= 0x10)
	{
	  if (elt[0] == 0x10)
	    /* Use lvsr 0,0.  */
	    return 7;
	  else
	    /* Use lvsl 0,elt[0].  */
	    return 8;
	}
      else
	return 0;
    }

  /* vxor v,v,v and vspltisw v,0 will work.  */
  else if (elt[0] == 0)
    return 1;

  /* vcmpequw v,v,v and vspltisw v,-1 will work.  */
  else if (elt[0] + 1 == 0)
    return 2;

  /* vsubcuw v,v,v and vspltisw v,1 will work.  */
  else if (elt[0] == 1)
    return 3;

  /* vspltisw will work.  */
  else if (elt[0] + 16 < 32)
    return 4;

  /* The two 16-bit halves aren't the same.  */
  else if (elt[0] >> 16 != (elt[0] & 0xffff))
    return 0;

  /* vspltish will work.  */
  else if (((elt[0] + 16) & 0xffff) < 32)
    return 5;

  /* The two 8-bit halves aren't the same.  */
  else if (elt[0] >> 24 != (elt[0] & 0xff))
    return 0;

  /* vspltisb will work.  */
  else if (((elt[0] + 16) & 0xff) < 32)
    return 6;

  return 0;
}

/* Return 1 is the operand is either a non-special register or
   constant zero.  For Altivec load/store instructions.  */
int
reg_or_zero_operand (op, mode)
    rtx op;
    enum machine_mode mode;
{
  return (gpc_reg_operand (op, mode)
	  || (GET_CODE (op) == CONST_INT
	      && CONST_OK_FOR_LETTER_P (INTVAL (op), 'O')));
}

/* Return 1 if the operand is a CONST_INT and can be put into a
   register with one instruction.  */
/* APPLE LOCAL AltiVec */
#if 0
static int
easy_vector_constant (op)
     rtx op;
{
  rtx elt;
  int units, i;

  if (GET_CODE (op) != CONST_VECTOR)
    return 0;

  units = CONST_VECTOR_NUNITS (op);

  /* We can generate 0 easily.  Look for that.  */
  for (i = 0; i < units; ++i)
    {
      elt = CONST_VECTOR_ELT (op, i);

      /* We could probably simplify this by just checking for equality
	 with CONST0_RTX for the current mode, but let's be safe
	 instead.  */

      switch (GET_CODE (elt))
	{
	case CONST_INT:
	  if (INTVAL (elt) != 0)
	    return 0;
	  break;
	case CONST_DOUBLE:
	  if (CONST_DOUBLE_LOW (elt) != 0 || CONST_DOUBLE_HIGH (elt) != 0)
	    return 0;
	  break;
	default:
	  return 0;
	}
    }

  /* We could probably generate a few other constants trivially, but
     gcc doesn't generate them yet.  FIXME later.  */
  return 1;
}
/* APPLE LOCAL AltiVec */
#endif

/* Return 1 if the operand is the constant 0.  This works for scalars
   as well as vectors.  */
int
zero_constant (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return op == CONST0_RTX (mode);
}

/* Return 1 if the operand is 0.0.  */
int
zero_fp_constant (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return GET_MODE_CLASS (mode) == MODE_FLOAT && op == CONST0_RTX (mode);
}

/* Return 1 if the operand is in volatile memory.  Note that during
   the RTL generation phase, memory_operand does not return TRUE for
   volatile memory references.  So this function allows us to
   recognize volatile references where its safe.  */

int
volatile_mem_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (GET_CODE (op) != MEM)
    return 0;

  if (!MEM_VOLATILE_P (op))
    return 0;

  if (mode != GET_MODE (op))
    return 0;

  if (reload_completed)
    return memory_operand (op, mode);

  if (reload_in_progress)
    return strict_memory_address_p (mode, XEXP (op, 0));

  return memory_address_p (mode, XEXP (op, 0));
}

/* Return 1 if the operand is an offsettable memory operand.  */

int
offsettable_mem_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return ((GET_CODE (op) == MEM)
	  && offsettable_address_p (reload_completed || reload_in_progress,
				    mode, XEXP (op, 0)));
}

/* Return 1 if the operand is either an easy FP constant (see above) or
   memory.  */

int
mem_or_easy_const_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return memory_operand (op, mode) || easy_fp_constant (op, mode);
}

/* Return 1 if the operand is either a non-special register or an item
   that can be used as the operand of a `mode' add insn.  */

int
add_operand (op, mode)
    rtx op;
    enum machine_mode mode;
{
  if (GET_CODE (op) == CONST_INT)
    return (CONST_OK_FOR_LETTER_P (INTVAL (op), 'I')
	    || CONST_OK_FOR_LETTER_P (INTVAL (op), 'L'));

  return gpc_reg_operand (op, mode);
}

/* APPLE LOCAL begin AltiVec */
/* Return 1 if the operand is either 0 or -1.  */

int
zero_m1_operand (op, mode)
    rtx op;
    enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == CONST_INT
	  && (unsigned HOST_WIDE_INT) (INTVAL (op) + 1) < 2);
}
/* APPLE LOCAL end AltiVec */

/* Return 1 if OP is a constant but not a valid add_operand.  */

int
non_add_cint_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == CONST_INT
	  && !CONST_OK_FOR_LETTER_P (INTVAL (op), 'I')
	  && !CONST_OK_FOR_LETTER_P (INTVAL (op), 'L'));
}

/* Return 1 if the operand is a non-special register or a constant that
   can be used as the operand of an OR or XOR insn on the RS/6000.  */

int
logical_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  HOST_WIDE_INT opl, oph;

  if (gpc_reg_operand (op, mode))
    return 1;

  if (GET_CODE (op) == CONST_INT)
    {
      opl = INTVAL (op) & GET_MODE_MASK (mode);

#if HOST_BITS_PER_WIDE_INT <= 32
      if (GET_MODE_BITSIZE (mode) > HOST_BITS_PER_WIDE_INT && opl < 0)
	return 0;
#endif
    }
  else if (GET_CODE (op) == CONST_DOUBLE)
    {
      if (GET_MODE_BITSIZE (mode) <= HOST_BITS_PER_WIDE_INT)
	abort ();

      opl = CONST_DOUBLE_LOW (op);
      oph = CONST_DOUBLE_HIGH (op);
      if (oph != 0)
	return 0;
    }
  else
    return 0;

  return ((opl & ~ (unsigned HOST_WIDE_INT) 0xffff) == 0
	  || (opl & ~ (unsigned HOST_WIDE_INT) 0xffff0000) == 0);
}

/* Return 1 if C is a constant that is not a logical operand (as
   above), but could be split into one.  */

int
non_logical_cint_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return ((GET_CODE (op) == CONST_INT || GET_CODE (op) == CONST_DOUBLE)
	  && ! logical_operand (op, mode)
	  && reg_or_logical_cint_operand (op, mode));
}

/* Return 1 if C is a constant that can be encoded in a 32-bit mask on the
   RS/6000.  It is if there are no more than two 1->0 or 0->1 transitions.
   Reject all ones and all zeros, since these should have been optimized
   away and confuse the making of MB and ME.  */

int
mask_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  HOST_WIDE_INT c, lsb;

  if (GET_CODE (op) != CONST_INT)
    return 0;

  c = INTVAL (op);

  /* Fail in 64-bit mode if the mask wraps around because the upper
     32-bits of the mask will all be 1s, contrary to GCC's internal view.  */
  if (TARGET_POWERPC64 && (c & 0x80000001) == 0x80000001)
    return 0;

  /* We don't change the number of transitions by inverting,
     so make sure we start with the LS bit zero.  */
  if (c & 1)
    c = ~c;

  /* Reject all zeros or all ones.  */
  if (c == 0)
    return 0;

  /* Find the first transition.  */
  lsb = c & -c;

  /* Invert to look for a second transition.  */
  c = ~c;

  /* Erase first transition.  */
  c &= -lsb;

  /* Find the second transition (if any).  */
  lsb = c & -c;

  /* Match if all the bits above are 1's (or c is zero).  */
  return c == -lsb;
}

/* Return 1 for the PowerPC64 rlwinm corner case.  */

int
mask_operand_wrap (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  HOST_WIDE_INT c, lsb;

  if (GET_CODE (op) != CONST_INT)
    return 0;

  c = INTVAL (op);

  if ((c & 0x80000001) != 0x80000001)
    return 0;

  c = ~c;
  if (c == 0)
    return 0;

  lsb = c & -c;
  c = ~c;
  c &= -lsb;
  lsb = c & -c;
  return c == -lsb;
}

/* Return 1 if the operand is a constant that is a PowerPC64 mask.
   It is if there are no more than one 1->0 or 0->1 transitions.
   Reject all zeros, since zero should have been optimized away and
   confuses the making of MB and ME.  */

int
mask64_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  if (GET_CODE (op) == CONST_INT)
    {
      HOST_WIDE_INT c, lsb;

      c = INTVAL (op);

      /* Reject all zeros.  */
      if (c == 0)
	return 0;

      /* We don't change the number of transitions by inverting,
	 so make sure we start with the LS bit zero.  */
      if (c & 1)
	c = ~c;

      /* Find the transition, and check that all bits above are 1's.  */
      lsb = c & -c;
      return c == -lsb;
    }
  return 0;
}

/* Like mask64_operand, but allow up to three transitions.  This
   predicate is used by insn patterns that generate two rldicl or
   rldicr machine insns.  */

int
mask64_2_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  if (GET_CODE (op) == CONST_INT)
    {
      HOST_WIDE_INT c, lsb;

      c = INTVAL (op);

      /* Disallow all zeros.  */
      if (c == 0)
	return 0;

      /* We don't change the number of transitions by inverting,
	 so make sure we start with the LS bit zero.  */
      if (c & 1)
	c = ~c;

      /* Find the first transition.  */
      lsb = c & -c;

      /* Invert to look for a second transition.  */
      c = ~c;

      /* Erase first transition.  */
      c &= -lsb;

      /* Find the second transition.  */
      lsb = c & -c;

      /* Invert to look for a third transition.  */
      c = ~c;

      /* Erase second transition.  */
      c &= -lsb;

      /* Find the third transition (if any).  */
      lsb = c & -c;

      /* Match if all the bits above are 1's (or c is zero).  */
      return c == -lsb;
    }
  return 0;
}

/* Generates shifts and masks for a pair of rldicl or rldicr insns to
   implement ANDing by the mask IN.  */
void
build_mask64_2_operands (in, out)
     rtx in;
     rtx *out;
{
#if HOST_BITS_PER_WIDE_INT >= 32
#if HOST_BITS_PER_WIDE_INT >= 64
  unsigned HOST_WIDE_INT c, lsb, m1, m2;
#else
  unsigned HOST_WIDEST_INT c, lsb, m1, m2;
#endif
  int shift;

  if (GET_CODE (in) != CONST_INT)
    abort ();

  c = INTVAL (in);
  if (c & 1)
    {
      /* Assume c initially something like 0x00fff000000fffff.  The idea
	 is to rotate the word so that the middle ^^^^^^ group of zeros
	 is at the MS end and can be cleared with an rldicl mask.  We then
	 rotate back and clear off the MS    ^^ group of zeros with a
	 second rldicl.  */
      c = ~c;			/*   c == 0xff000ffffff00000 */
      lsb = c & -c;		/* lsb == 0x0000000000100000 */
      m1 = -lsb;		/*  m1 == 0xfffffffffff00000 */
      c = ~c;			/*   c == 0x00fff000000fffff */
      c &= -lsb;		/*   c == 0x00fff00000000000 */
      lsb = c & -c;		/* lsb == 0x0000100000000000 */
      c = ~c;			/*   c == 0xff000fffffffffff */
      c &= -lsb;		/*   c == 0xff00000000000000 */
      shift = 0;
      while ((lsb >>= 1) != 0)
	shift++;		/* shift == 44 on exit from loop */
      m1 <<= 64 - shift;	/*  m1 == 0xffffff0000000000 */
      m1 = ~m1;			/*  m1 == 0x000000ffffffffff */
      m2 = ~c;			/*  m2 == 0x00ffffffffffffff */
    }
  else
    {
      /* Assume c initially something like 0xff000f0000000000.  The idea
	 is to rotate the word so that the     ^^^  middle group of zeros
	 is at the LS end and can be cleared with an rldicr mask.  We then
	 rotate back and clear off the LS group of ^^^^^^^^^^ zeros with
	 a second rldicr.  */
      lsb = c & -c;		/* lsb == 0x0000010000000000 */
      m2 = -lsb;		/*  m2 == 0xffffff0000000000 */
      c = ~c;			/*   c == 0x00fff0ffffffffff */
      c &= -lsb;		/*   c == 0x00fff00000000000 */
      lsb = c & -c;		/* lsb == 0x0000100000000000 */
      c = ~c;			/*   c == 0xff000fffffffffff */
      c &= -lsb;		/*   c == 0xff00000000000000 */
      shift = 0;
      while ((lsb >>= 1) != 0)
	shift++;		/* shift == 44 on exit from loop */
      m1 = ~c;			/*  m1 == 0x00ffffffffffffff */
      m1 >>= shift;		/*  m1 == 0x0000000000000fff */
      m1 = ~m1;			/*  m1 == 0xfffffffffffff000 */
    }

  /* Note that when we only have two 0->1 and 1->0 transitions, one of the
     masks will be all 1's.  We are guaranteed more than one transition.  */
  out[0] = GEN_INT (64 - shift);
  out[1] = GEN_INT (m1);
  out[2] = GEN_INT (shift);
  out[3] = GEN_INT (m2);
#else
  (void)in;
  (void)out;
  abort ();
#endif
}

/* Return 1 if the operand is either a non-special register or a constant
   that can be used as the operand of a PowerPC64 logical AND insn.  */

int
and64_operand (op, mode)
    rtx op;
    enum machine_mode mode;
{
  if (fixed_regs[CR0_REGNO])	/* CR0 not available, don't do andi./andis.  */
    return (gpc_reg_operand (op, mode) || mask64_operand (op, mode));

  return (logical_operand (op, mode) || mask64_operand (op, mode));
}

/* Like the above, but also match constants that can be implemented
   with two rldicl or rldicr insns.  */

int
and64_2_operand (op, mode)
    rtx op;
    enum machine_mode mode;
{
  if (fixed_regs[CR0_REGNO])	/* CR0 not available, don't do andi./andis. */
    return gpc_reg_operand (op, mode) || mask64_2_operand (op, mode);

  return logical_operand (op, mode) || mask64_2_operand (op, mode);
}

/* Return 1 if the operand is either a non-special register or a
   constant that can be used as the operand of an RS/6000 logical AND insn.  */

int
and_operand (op, mode)
    rtx op;
    enum machine_mode mode;
{
  if (fixed_regs[CR0_REGNO])	/* CR0 not available, don't do andi./andis.  */
    return (gpc_reg_operand (op, mode) || mask_operand (op, mode));

  return (logical_operand (op, mode) || mask_operand (op, mode));
}

/* Return 1 if the operand is a general register or memory operand.  */

int
reg_or_mem_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  return (gpc_reg_operand (op, mode)
	  || memory_operand (op, mode)
	  || volatile_mem_operand (op, mode));
}

/* As above, but accept memory operands only after reload.  (For
   the decrement-ctr insns.)  */

int
reg_or_postreload_mem_operand (op, mode)
    rtx op;
    enum machine_mode mode;
{
  return (register_operand (op, mode)
	  || ((reload_completed || reload_in_progress) 
	      && memory_operand (op, mode)));
}

/* Return 1 if the operand is a general register or memory operand without
   pre_inc or pre_dec which produces invalid form of PowerPC lwa
   instruction.  */

int
lwa_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  rtx inner = op;

  if (reload_completed && GET_CODE (inner) == SUBREG)
    inner = SUBREG_REG (inner);
    
  return gpc_reg_operand (inner, mode)
    || (memory_operand (inner, mode)
	&& GET_CODE (XEXP (inner, 0)) != PRE_INC
	&& GET_CODE (XEXP (inner, 0)) != PRE_DEC
	&& (GET_CODE (XEXP (inner, 0)) != PLUS
	    || GET_CODE (XEXP (XEXP (inner, 0), 1)) != CONST_INT
	    || INTVAL (XEXP (XEXP (inner, 0), 1)) % 4 == 0));
}

/* Return 1 if the operand, used inside a MEM, is a SYMBOL_REF.  */

int
symbol_ref_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (mode != VOIDmode && GET_MODE (op) != mode)
    return 0;

  return (GET_CODE (op) == SYMBOL_REF);
}

/* Return 1 if the operand, used inside a MEM, is a valid first argument
   to CALL.  This is a SYMBOL_REF, a pseudo-register, LR or CTR.  */

int
call_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  if (mode != VOIDmode && GET_MODE (op) != mode)
    return 0;

  return (GET_CODE (op) == SYMBOL_REF
	  /* APPLE LOCAL begin accept hard R12 as target reg */
#ifdef MAGIC_INDIRECT_CALL_REG
	  || (GET_CODE (op) == REG && REGNO (op) == MAGIC_INDIRECT_CALL_REG)
#endif
	  /* APPLE LOCAL end accept hard R12 as target reg */
	  || (GET_CODE (op) == REG
	      && (REGNO (op) == LINK_REGISTER_REGNUM
		  || REGNO (op) == COUNT_REGISTER_REGNUM
		  || REGNO (op) >= FIRST_PSEUDO_REGISTER)));
}

/* Return 1 if the operand is a SYMBOL_REF for a function known to be in
   this file and the function is not weakly defined.  */

int
current_file_function_operand (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  return (GET_CODE (op) == SYMBOL_REF
	  && (SYMBOL_REF_FLAG (op)
	      || (op == XEXP (DECL_RTL (current_function_decl), 0)
	          && ! DECL_WEAK (current_function_decl))));
}

/* Return 1 if this operand is a valid input for a move insn.  */

int
input_operand (op, mode)
     rtx op;
     enum machine_mode mode;
{
  /* Memory is always valid.  */
  if (memory_operand (op, mode))
    return 1;

  /* Only a tiny bit of handling for CONSTANT_P_RTX is necessary.  */
  if (GET_CODE (op) == CONSTANT_P_RTX)
    return 1;

  /* For floating-point, easy constants are valid.  */
  if (GET_MODE_CLASS (mode) == MODE_FLOAT
      && CONSTANT_P (op)
      && easy_fp_constant (op, mode))
    return 1;

  /* Allow any integer constant.  */
  if (GET_MODE_CLASS (mode) == MODE_INT
      && (GET_CODE (op) == CONST_INT
	  || GET_CODE (op) == CONST_DOUBLE))
    return 1;

  /* For floating-point or multi-word mode, the only remaining valid type
     is a register.  */
  if (GET_MODE_CLASS (mode) == MODE_FLOAT
      || GET_MODE_SIZE (mode) > ABI_UNITS_PER_WORD)
    return register_operand (op, mode);

  /* The only cases left are integral modes one word or smaller (we
     do not get called for MODE_CC values).  These can be in any
     register.  */
  if (register_operand (op, mode))
    return 1;

  /* A SYMBOL_REF referring to the TOC is valid.  */
  if (LEGITIMATE_CONSTANT_POOL_ADDRESS_P (op))
    return 1;

  /* A constant pool expression (relative to the TOC) is valid */
  if (TOC_RELATIVE_EXPR_P (op))
    return 1;

  /* V.4 allows SYMBOL_REFs and CONSTs that are in the small data region
     to be valid.  */
  if (DEFAULT_ABI == ABI_V4
      && (GET_CODE (op) == SYMBOL_REF || GET_CODE (op) == CONST)
      && small_data_operand (op, Pmode))
    return 1;

  return 0;
}

/* Return 1 for an operand in small memory on V.4/eabi.  */

int
small_data_operand (op, mode)
     rtx op ATTRIBUTE_UNUSED;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
#if TARGET_ELF
  rtx sym_ref;

  if (rs6000_sdata == SDATA_NONE || rs6000_sdata == SDATA_DATA)
    return 0;

  if (DEFAULT_ABI != ABI_V4)
    return 0;

  if (GET_CODE (op) == SYMBOL_REF)
    sym_ref = op;

  else if (GET_CODE (op) != CONST
	   || GET_CODE (XEXP (op, 0)) != PLUS
	   || GET_CODE (XEXP (XEXP (op, 0), 0)) != SYMBOL_REF
	   || GET_CODE (XEXP (XEXP (op, 0), 1)) != CONST_INT)
    return 0;

  else
    {
      rtx sum = XEXP (op, 0);
      HOST_WIDE_INT summand;

      /* We have to be careful here, because it is the referenced address
        that must be 32k from _SDA_BASE_, not just the symbol.  */
      summand = INTVAL (XEXP (sum, 1));
      if (summand < 0 || summand > g_switch_value)
       return 0;

      sym_ref = XEXP (sum, 0);
    }

  if (*XSTR (sym_ref, 0) != '@')
    return 0;

  return 1;

#else
  return 0;
#endif
}

static int 
constant_pool_expr_1 (op, have_sym, have_toc) 
    rtx op;
    int *have_sym;
    int *have_toc;
{
  switch (GET_CODE(op)) 
    {
    case SYMBOL_REF:
      if (CONSTANT_POOL_ADDRESS_P (op))
	{
	  if (ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (get_pool_constant (op), Pmode))
	    {
	      *have_sym = 1;
	      return 1;
	    }
	  else
	    return 0;
	}
      else if (! strcmp (XSTR (op, 0), toc_label_name))
	{
	  *have_toc = 1;
	  return 1;
	}
      else
	return 0;
    case PLUS:
    case MINUS:
      return (constant_pool_expr_1 (XEXP (op, 0), have_sym, have_toc)
	      && constant_pool_expr_1 (XEXP (op, 1), have_sym, have_toc));
    case CONST:
      return constant_pool_expr_1 (XEXP (op, 0), have_sym, have_toc);
    case CONST_INT:
      return 1;
    default:
      return 0;
    }
}

int
constant_pool_expr_p (op)
    rtx op;
{
  int have_sym = 0;
  int have_toc = 0;
  return constant_pool_expr_1 (op, &have_sym, &have_toc) && have_sym;
}

int
toc_relative_expr_p (op)
    rtx op;
{
    int have_sym = 0;
    int have_toc = 0;
    return constant_pool_expr_1 (op, &have_sym, &have_toc) && have_toc;
}

/* Try machine-dependent ways of modifying an illegitimate address
   to be legitimate.  If we find one, return the new, valid address.
   This is used from only one place: `memory_address' in explow.c.

   OLDX is the address as it was before break_out_memory_refs was
   called.  In some cases it is useful to look at this to decide what
   needs to be done.

   MODE is passed so that this function can use GO_IF_LEGITIMATE_ADDRESS.

   It is always safe for this function to do nothing.  It exists to
   recognize opportunities to optimize the output.

   On RS/6000, first check for the sum of a register with a constant
   integer that is out of range.  If so, generate code to add the
   constant with the low-order 16 bits masked to the register and force
   this result into another register (this can be done with `cau').
   Then generate an address of REG+(CONST&0xffff), allowing for the
   possibility of bit 16 being a one.

   Then check for the sum of a register and something not constant, try to
   load the other things into a register and return the sum.  */
rtx
rs6000_legitimize_address (x, oldx, mode)
     rtx x;
     rtx oldx ATTRIBUTE_UNUSED;
     enum machine_mode mode;
{
  if (GET_CODE (x) == PLUS 
      && GET_CODE (XEXP (x, 0)) == REG
      && GET_CODE (XEXP (x, 1)) == CONST_INT
      && (unsigned HOST_WIDE_INT) (INTVAL (XEXP (x, 1)) + 0x8000) >= 0x10000)
    { 
      HOST_WIDE_INT high_int, low_int;
      rtx sum;
      low_int = ((INTVAL (XEXP (x, 1)) & 0xffff) ^ 0x8000) - 0x8000;
      high_int = INTVAL (XEXP (x, 1)) - low_int;
      sum = force_operand (gen_rtx_PLUS (Pmode, XEXP (x, 0),
					 GEN_INT (high_int)), 0);
      return gen_rtx_PLUS (Pmode, sum, GEN_INT (low_int));
    }
  else if (GET_CODE (x) == PLUS 
	   && GET_CODE (XEXP (x, 0)) == REG
	   && GET_CODE (XEXP (x, 1)) != CONST_INT
	   && GET_MODE_NUNITS (mode) == 1
	   && ((TARGET_HARD_FLOAT && TARGET_FPRS)
	       || TARGET_POWERPC64
	       || (mode != DFmode && mode != TFmode))
	   && (TARGET_POWERPC64 || mode != DImode)
	   && mode != TImode)
    {
      return gen_rtx_PLUS (Pmode, XEXP (x, 0),
			   force_reg (Pmode, force_operand (XEXP (x, 1), 0)));
    }
  /* APPLE LOCAL darwin native, AltiVec */
  /* this might now be subsumed in the next if-block */
  /* well, it might, but I think the force_operand's were important,
     although I can't recall why --dj */
  else if (GET_CODE (x) == PLUS && VECTOR_MODE_P (mode))
    {
      /* Express this as reg<-const, [reg+reg] rather than reg<-reg+const,
	 [reg], so interference checking works better */
      return gen_rtx_PLUS (Pmode, force_reg (Pmode, force_operand (XEXP (x, 0), 0)),
			   force_reg (Pmode, force_operand (XEXP (x, 1), 0)));
    }
  else if (ALTIVEC_VECTOR_MODE (mode))
    {
      rtx reg;

      /* Make sure both operands are registers.  */
      if (GET_CODE (x) == PLUS)
	return gen_rtx_PLUS (Pmode, force_reg (Pmode, XEXP (x, 0)),
			     force_reg (Pmode, XEXP (x, 1)));

      reg = force_reg (Pmode, x);
      return reg;
    }
  else if (SPE_VECTOR_MODE (mode))
    {
      /* We accept [reg + reg] and [reg + OFFSET].  */

      if (GET_CODE (x) == PLUS)
      {
        rtx op1 = XEXP (x, 0);
        rtx op2 = XEXP (x, 1);

        op1 = force_reg (Pmode, op1);

        if (GET_CODE (op2) != REG
            && (GET_CODE (op2) != CONST_INT
                || !SPE_CONST_OFFSET_OK (INTVAL (op2))))
          op2 = force_reg (Pmode, op2);

        return gen_rtx_PLUS (Pmode, op1, op2);
      }

      return force_reg (Pmode, x);
    }
  else if (TARGET_ELF && TARGET_32BIT && TARGET_NO_TOC && ! flag_pic
	   && GET_CODE (x) != CONST_INT
	   && GET_CODE (x) != CONST_DOUBLE 
	   && CONSTANT_P (x)
	   && GET_MODE_NUNITS (mode) == 1
	   && (GET_MODE_BITSIZE (mode) <= 32
	       || ((TARGET_HARD_FLOAT && TARGET_FPRS) && mode == DFmode)))
    {
      rtx reg = gen_reg_rtx (Pmode);
      emit_insn (gen_elf_high (reg, (x)));
      return gen_rtx_LO_SUM (Pmode, reg, (x));
    }
  else if (TARGET_MACHO && TARGET_32BIT && TARGET_NO_TOC
	   && ! flag_pic
	   /* APPLE LOCAL  dynamic-no-pic  */
#if TARGET_MACHO
	   && ! MACHO_DYNAMIC_NO_PIC_P ()
#endif
	   && GET_CODE (x) != CONST_INT
	   && GET_CODE (x) != CONST_DOUBLE 
	   && CONSTANT_P (x)
	   && ((TARGET_HARD_FLOAT && TARGET_FPRS) || mode != DFmode)
	   && mode != DImode 
	   && mode != TImode)
    {
      rtx reg = gen_reg_rtx (Pmode);
      /* APPLE LOCAL begin AltiVec */
      rtx newx;
      if (TARGET_ELF)
        emit_insn (gen_elf_high (reg, x));
      else if (TARGET_MACHO)
        emit_insn (gen_macho_high (reg, x));
      newx = gen_rtx_LO_SUM (Pmode, reg, x);
      if (VECTOR_MODE_P (mode))
	newx = force_reg (Pmode, x);
      return newx;
      /* APPLE LOCAL end AltiVec */
    }
  else if (TARGET_TOC 
	   && CONSTANT_POOL_EXPR_P (x)
	   && ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (get_pool_constant (x), Pmode))
    {
      return create_TOC_reference (x);
    }
  else
    return NULL_RTX;
}

/* The convention appears to be to define this wherever it is used.
   With legitimize_reload_address now defined here, REG_MODE_OK_FOR_BASE_P
   is now used here.  */
#ifndef REG_MODE_OK_FOR_BASE_P
#define REG_MODE_OK_FOR_BASE_P(REGNO, MODE) REG_OK_FOR_BASE_P (REGNO)
#endif

/* Our implementation of LEGITIMIZE_RELOAD_ADDRESS.  Returns a value to
   replace the input X, or the original X if no replacement is called for.
   The output parameter *WIN is 1 if the calling macro should goto WIN,
   0 if it should not.

   For RS/6000, we wish to handle large displacements off a base
   register by splitting the addend across an addiu/addis and the mem insn.
   This cuts number of extra insns needed from 3 to 1.

   On Darwin, we use this to generate code for floating point constants.
   A movsf_low is generated so we wind up with 2 instructions rather than 3.
   The Darwin code is inside #if TARGET_MACHO because only then is
   machopic_function_base_name() defined.  */
rtx
/* APPLE LOCAL pass reload addr by address */
rs6000_legitimize_reload_address (addr_x, mode, opnum, type, ind_levels, win)
    rtx *addr_x;
    enum machine_mode mode;
    int opnum;
    int type;
    int ind_levels ATTRIBUTE_UNUSED;
    int *win;
{
  /* APPLE LOCAL pass reload addr by address */
  rtx x = *addr_x;
  /* We must recognize output that we have already generated ourselves.  */ 
  if (GET_CODE (x) == PLUS
      && GET_CODE (XEXP (x, 0)) == PLUS
      && GET_CODE (XEXP (XEXP (x, 0), 0)) == REG
      && GET_CODE (XEXP (XEXP (x, 0), 1)) == CONST_INT
      && GET_CODE (XEXP (x, 1)) == CONST_INT)
    {
      push_reload (XEXP (x, 0), NULL_RTX, &XEXP (x, 0), NULL,
                   BASE_REG_CLASS, GET_MODE (x), VOIDmode, 0, 0,
                   opnum, (enum reload_type)type);
      *win = 1;
      return x;
    }

#if TARGET_MACHO
  if (DEFAULT_ABI == ABI_DARWIN && flag_pic
      && GET_CODE (x) == LO_SUM
      && GET_CODE (XEXP (x, 0)) == PLUS
      && XEXP (XEXP (x, 0), 0) == pic_offset_table_rtx
      && GET_CODE (XEXP (XEXP (x, 0), 1)) == HIGH
      && GET_CODE (XEXP (XEXP (XEXP (x, 0), 1), 0)) == CONST
      && XEXP (XEXP (XEXP (x, 0), 1), 0) == XEXP (x, 1)
      && GET_CODE (XEXP (XEXP (x, 1), 0)) == MINUS
      && GET_CODE (XEXP (XEXP (XEXP (x, 1), 0), 0)) == SYMBOL_REF
      && GET_CODE (XEXP (XEXP (XEXP (x, 1), 0), 1)) == SYMBOL_REF)
    {
      /* Result of previous invocation of this function on Darwin
	 floating point constant.  */
      push_reload (XEXP (x, 0), NULL_RTX, &XEXP (x, 0), NULL,
		BASE_REG_CLASS, Pmode, VOIDmode, 0, 0,
		opnum, (enum reload_type)type);
      *win = 1;
      return x;
    }
#endif
  /* APPLE LOCAL begin AltiVec */
  /* X is passed by address so this push_reload works */
  if (GET_CODE (x) == PLUS && VECTOR_MODE_P (mode))
    {
      rtx orig_x = x;
      *addr_x = copy_rtx (x);
      push_reload (orig_x, NULL_RTX, addr_x, NULL,
		BASE_REG_CLASS, GET_MODE (*addr_x), VOIDmode, 0, 0,
		opnum, (enum reload_type)type);
      *win = 1;
      return *addr_x;
    }
  /* APPLE LOCAL end AltiVec */
  if (GET_CODE (x) == PLUS
      && GET_CODE (XEXP (x, 0)) == REG
      && REGNO (XEXP (x, 0)) < FIRST_PSEUDO_REGISTER
      && REG_MODE_OK_FOR_BASE_P (XEXP (x, 0), mode)
      && GET_CODE (XEXP (x, 1)) == CONST_INT
      && !SPE_VECTOR_MODE (mode)
      && !ALTIVEC_VECTOR_MODE (mode))
    {
      HOST_WIDE_INT val = INTVAL (XEXP (x, 1));
      HOST_WIDE_INT low = ((val & 0xffff) ^ 0x8000) - 0x8000;
      HOST_WIDE_INT high
        = (((val - low) & 0xffffffff) ^ 0x80000000) - 0x80000000;

      /* Check for 32-bit overflow.  */
      if (high + low != val)
        {
	  *win = 0;
	  return x;
	}

      /* Reload the high part into a base reg; leave the low part
         in the mem directly.  */

      x = gen_rtx_PLUS (GET_MODE (x),
                        gen_rtx_PLUS (GET_MODE (x), XEXP (x, 0),
                                      GEN_INT (high)),
                        GEN_INT (low));

      push_reload (XEXP (x, 0), NULL_RTX, &XEXP (x, 0), NULL,
                   BASE_REG_CLASS, GET_MODE (x), VOIDmode, 0, 0,
                   opnum, (enum reload_type)type);
      *win = 1;
      return x;
    }
#if TARGET_MACHO
  if (GET_CODE (x) == SYMBOL_REF
      && DEFAULT_ABI == ABI_DARWIN
      && !ALTIVEC_VECTOR_MODE (mode)
      && flag_pic)
    {
      /* Darwin load of floating point constant.  */
      rtx offset = gen_rtx (CONST, Pmode,
		    gen_rtx (MINUS, Pmode, x,
		    gen_rtx (SYMBOL_REF, Pmode,
			machopic_function_base_name ())));
      x = gen_rtx (LO_SUM, GET_MODE (x),
	    gen_rtx (PLUS, Pmode, pic_offset_table_rtx,
		gen_rtx (HIGH, Pmode, offset)), offset);
      push_reload (XEXP (x, 0), NULL_RTX, &XEXP (x, 0), NULL,
		BASE_REG_CLASS, Pmode, VOIDmode, 0, 0,
		opnum, (enum reload_type)type);
      *win = 1;
      return x;
    }
  /* APPLE LOCAL begin dynamic-no-pic */
  if (GET_CODE (x) == SYMBOL_REF
      && DEFAULT_ABI == ABI_DARWIN
      /* APPLE LOCAL AltiVec */
      && !ALTIVEC_VECTOR_MODE (mode)
      && MACHO_DYNAMIC_NO_PIC_P ())
    {
      /* Darwin load of floating point constant.  */
      x = gen_rtx (LO_SUM, GET_MODE (x),
		gen_rtx (HIGH, Pmode, x), x);
      push_reload (XEXP (x, 0), NULL_RTX, &XEXP (x, 0), NULL,
		BASE_REG_CLASS, Pmode, VOIDmode, 0, 0,
		opnum, (enum reload_type)type);
      *win = 1;
      return x;
    }
  /* APPLE LOCAL end dynamic-no-pic */
#endif
  if (TARGET_TOC
      && CONSTANT_POOL_EXPR_P (x)
      && ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (get_pool_constant (x), mode))
    {
      (x) = create_TOC_reference (x);
      *win = 1;
      return x;
    }
  *win = 0;
  return x;
}    

/* GO_IF_LEGITIMATE_ADDRESS recognizes an RTL expression
   that is a valid memory address for an instruction.
   The MODE argument is the machine mode for the MEM expression
   that wants to use this address.

   On the RS/6000, there are four valid address: a SYMBOL_REF that
   refers to a constant pool entry of an address (or the sum of it
   plus a constant), a short (16-bit signed) constant plus a register,
   the sum of two registers, or a register indirect, possibly with an
   auto-increment.  For DFmode and DImode with an constant plus register,
   we must ensure that both words are addressable or PowerPC64 with offset
   word aligned.

   For modes spanning multiple registers (DFmode in 32-bit GPRs,
   32-bit DImode, TImode), indexed addressing cannot be used because
   adjacent memory cells are accessed by adding word-sized offsets
   during assembly output.  */
int
rs6000_legitimate_address (mode, x, reg_ok_strict)
    enum machine_mode mode;
    rtx x;
    int reg_ok_strict;
{
  if (LEGITIMATE_INDIRECT_ADDRESS_P (x, reg_ok_strict))
    return 1;
  if ((GET_CODE (x) == PRE_INC || GET_CODE (x) == PRE_DEC)
      && !ALTIVEC_VECTOR_MODE (mode)
      && !SPE_VECTOR_MODE (mode)
      && TARGET_UPDATE
      && LEGITIMATE_INDIRECT_ADDRESS_P (XEXP (x, 0), reg_ok_strict))
    return 1;
  if (LEGITIMATE_SMALL_DATA_P (mode, x))
    return 1;
  if (LEGITIMATE_CONSTANT_POOL_ADDRESS_P (x))
    return 1;
  /* If not REG_OK_STRICT (before reload) let pass any stack offset.  */
  if (! reg_ok_strict
      && GET_CODE (x) == PLUS
      && GET_CODE (XEXP (x, 0)) == REG
      && XEXP (x, 0) == virtual_stack_vars_rtx
      && GET_CODE (XEXP (x, 1)) == CONST_INT)
    return 1;
  if (LEGITIMATE_OFFSET_ADDRESS_P (mode, x, reg_ok_strict))
    return 1;
  if (mode != TImode
      && ((TARGET_HARD_FLOAT && TARGET_FPRS)
	  || TARGET_POWERPC64
	  || (mode != DFmode && mode != TFmode))
      && (TARGET_POWERPC64 || mode != DImode)
      && LEGITIMATE_INDEXED_ADDRESS_P (x, reg_ok_strict))
    return 1;
  if (LEGITIMATE_LO_SUM_ADDRESS_P (mode, x, reg_ok_strict))
    return 1;
  return 0;
}

/* Try to output insns to set TARGET equal to the constant C if it can
   be done in less than N insns.  Do all computations in MODE.
   Returns the place where the output has been placed if it can be
   done and the insns have been emitted.  If it would take more than N
   insns, zero is returned and no insns and emitted.  */

rtx
rs6000_emit_set_const (dest, mode, source, n)
     rtx dest, source;
     enum machine_mode mode;
     int n ATTRIBUTE_UNUSED;
{
  rtx result, insn, set;
  HOST_WIDE_INT c0, c1;

  if (mode == QImode || mode == HImode)
    {
      if (dest == NULL)
        dest = gen_reg_rtx (mode);
      emit_insn (gen_rtx_SET (VOIDmode, dest, source));
      return dest;
    }
  else if (mode == SImode)
    {
      result = no_new_pseudos ? dest : gen_reg_rtx (SImode);

      emit_insn (gen_rtx_SET (VOIDmode, result,
			      GEN_INT (INTVAL (source)
				       & (~ (HOST_WIDE_INT) 0xffff))));
      emit_insn (gen_rtx_SET (VOIDmode, dest,
			      gen_rtx_IOR (SImode, result,
					   GEN_INT (INTVAL (source) & 0xffff))));
      result = dest;
    }
  else if (mode == DImode)
    {
      if (GET_CODE (source) == CONST_INT)
	{
	  c0 = INTVAL (source);
	  c1 = -(c0 < 0);
	}
      else if (GET_CODE (source) == CONST_DOUBLE)
	{
#if HOST_BITS_PER_WIDE_INT >= 64
	  c0 = CONST_DOUBLE_LOW (source);
	  c1 = -(c0 < 0);
#else
	  c0 = CONST_DOUBLE_LOW (source);
	  c1 = CONST_DOUBLE_HIGH (source);
#endif
	}
      else
	abort ();

      result = rs6000_emit_set_long_const (dest, c0, c1);
    }
  else
    abort ();

  insn = get_last_insn ();
  set = single_set (insn);
  if (! CONSTANT_P (SET_SRC (set)))
    set_unique_reg_note (insn, REG_EQUAL, source);

  return result;
}

/* Having failed to find a 3 insn sequence in rs6000_emit_set_const,
   fall back to a straight forward decomposition.  We do this to avoid
   exponential run times encountered when looking for longer sequences
   with rs6000_emit_set_const.  */
static rtx
rs6000_emit_set_long_const (dest, c1, c2)
     rtx dest;
     HOST_WIDE_INT c1, c2;
{
  if (!TARGET_POWERPC64)
    {
      rtx operand1, operand2;

      operand1 = operand_subword_force (dest, WORDS_BIG_ENDIAN == 0,
					DImode);
      operand2 = operand_subword_force (dest, WORDS_BIG_ENDIAN != 0,
					DImode);
      emit_move_insn (operand1, GEN_INT (c1));
      emit_move_insn (operand2, GEN_INT (c2));
    }
  else
    {
      HOST_WIDE_INT ud1, ud2, ud3, ud4;

      ud1 = c1 & 0xffff;
      ud2 = (unsigned HOST_WIDE_INT)(c1 & 0xffff0000) >> 16;
#if HOST_BITS_PER_WIDE_INT >= 64
      c2 = c1 >> 32;
#endif
      ud3 = c2 & 0xffff;
      ud4 = (unsigned HOST_WIDE_INT)(c2 & 0xffff0000) >> 16;

      if ((ud4 == 0xffff && ud3 == 0xffff && ud2 == 0xffff && (ud1 & 0x8000)) 
	  || (ud4 == 0 && ud3 == 0 && ud2 == 0 && ! (ud1 & 0x8000)))
	{
	  if (ud1 & 0x8000)
	    emit_move_insn (dest, GEN_INT (((ud1  ^ 0x8000) -  0x8000)));
	  else
	    emit_move_insn (dest, GEN_INT (ud1));
	}

      else if ((ud4 == 0xffff && ud3 == 0xffff && (ud2 & 0x8000)) 
	       || (ud4 == 0 && ud3 == 0 && ! (ud2 & 0x8000)))
	{
	  if (ud2 & 0x8000)
	    emit_move_insn (dest, GEN_INT (((ud2 << 16) ^ 0x80000000) 
					   - 0x80000000));
	  else
	    emit_move_insn (dest, GEN_INT (ud2 << 16));
	  if (ud1 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, GEN_INT (ud1)));
	}
      else if ((ud4 == 0xffff && (ud3 & 0x8000)) 
	       || (ud4 == 0 && ! (ud3 & 0x8000)))
	{
	  if (ud3 & 0x8000)
	    emit_move_insn (dest, GEN_INT (((ud3 << 16) ^ 0x80000000) 
					   - 0x80000000));
	  else
	    emit_move_insn (dest, GEN_INT (ud3 << 16));

	  if (ud2 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, GEN_INT (ud2)));
	  emit_move_insn (dest, gen_rtx_ASHIFT (DImode, dest, GEN_INT (16)));
	  if (ud1 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, GEN_INT (ud1)));
	}
      else 
	{
	  if (ud4 & 0x8000)
	    emit_move_insn (dest, GEN_INT (((ud4 << 16) ^ 0x80000000) 
					   - 0x80000000));
	  else
	    emit_move_insn (dest, GEN_INT (ud4 << 16));

	  if (ud3 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, GEN_INT (ud3)));

	  emit_move_insn (dest, gen_rtx_ASHIFT (DImode, dest, GEN_INT (32)));
	  if (ud2 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, 
					       immed_double_const(ud2 << 16, 0, DImode)));	
	  if (ud1 != 0)
	    emit_move_insn (dest, gen_rtx_IOR (DImode, dest, GEN_INT (ud1)));
	}
    }
  return dest;
}

/* APPLE LOCAL begin RTX_COST for multiply */
int 
rs6000_rtx_mult_cost (x)
    rtx x;
{
    switch (rs6000_cpu)
      {
      case PROCESSOR_RIOS1:
      case PROCESSOR_PPC405:
        return (GET_CODE (XEXP (x, 1)) != CONST_INT
		? COSTS_N_INSNS (5)
		: (INTVAL (XEXP (x, 1)) >= -256
		   && INTVAL (XEXP (x, 1)) <= 255)
		? COSTS_N_INSNS (3) : COSTS_N_INSNS (4));
      case PROCESSOR_RS64A:
        return (GET_CODE (XEXP (x, 1)) != CONST_INT
		? GET_MODE (XEXP (x, 1)) != DImode
		? COSTS_N_INSNS (20) : COSTS_N_INSNS (34)
		: (INTVAL (XEXP (x, 1)) >= -256
		   && INTVAL (XEXP (x, 1)) <= 255)
		? COSTS_N_INSNS (8) : COSTS_N_INSNS (12));
      case PROCESSOR_RIOS2:
      case PROCESSOR_MPCCORE:
      case PROCESSOR_PPC440:
      case PROCESSOR_PPC604e:
        return COSTS_N_INSNS (2);
      case PROCESSOR_PPC601:
        return COSTS_N_INSNS (5);
      case PROCESSOR_PPC603:
      case PROCESSOR_PPC7400:
      case PROCESSOR_PPC750:
        return (GET_CODE (XEXP (x, 1)) != CONST_INT
		? COSTS_N_INSNS (5)
		: INTVAL (XEXP (x, 1)) >= -256
		&& INTVAL (XEXP (x, 1)) <= 255
		? COSTS_N_INSNS (2) : COSTS_N_INSNS (3));
      case PROCESSOR_PPC7450:
        return (GET_CODE (XEXP (x, 1)) != CONST_INT
		? COSTS_N_INSNS (4)
		: COSTS_N_INSNS (3));
      case PROCESSOR_PPC403:
      case PROCESSOR_PPC604:
      case PROCESSOR_PPC8540:
        return COSTS_N_INSNS (4);
      case PROCESSOR_PPC620:
      case PROCESSOR_PPC630:
        return (GET_CODE (XEXP (x, 1)) != CONST_INT
		? GET_MODE (XEXP (x, 1)) != DImode
		? COSTS_N_INSNS (5) : COSTS_N_INSNS (7)
		: (INTVAL (XEXP (x, 1)) >= -256
		   && INTVAL (XEXP (x, 1)) <= 255)
		? COSTS_N_INSNS (3) : COSTS_N_INSNS (4));
      case PROCESSOR_POWER4:
	return (GET_CODE (XEXP (x, 1)) != CONST_INT
 	        ? GET_MODE (XEXP (x, 1)) != DImode
	        ? COSTS_N_INSNS (5) : COSTS_N_INSNS (7)
	        : COSTS_N_INSNS (4));
      default:
	abort ();
      }
}
/* APPLE LOCAL end RTX_COST for multiply */

/* Emit a move from SOURCE to DEST in mode MODE.  */
void
rs6000_emit_move (dest, source, mode)
     rtx dest;
     rtx source;
     enum machine_mode mode;
{
  rtx operands[2];
  operands[0] = dest;
  operands[1] = source;
  
  /* Sanity checks.  Check that we get CONST_DOUBLE only when we should.  */
  if (GET_CODE (operands[1]) == CONST_DOUBLE
      && ! FLOAT_MODE_P (mode)
      && GET_MODE_BITSIZE (mode) <= HOST_BITS_PER_WIDE_INT)
    {
      /* FIXME.  This should never happen.  */
      /* Since it seems that it does, do the safe thing and convert
	 to a CONST_INT.  */
      operands[1] = gen_int_mode (CONST_DOUBLE_LOW (operands[1]), mode);
    }
  if (GET_CODE (operands[1]) == CONST_DOUBLE
      && ! FLOAT_MODE_P (mode)
      && ((CONST_DOUBLE_HIGH (operands[1]) == 0
	   && CONST_DOUBLE_LOW (operands[1]) >= 0)
	  || (CONST_DOUBLE_HIGH (operands[1]) == -1
	      && CONST_DOUBLE_LOW (operands[1]) < 0)))
    abort ();

  /* Check if GCC is setting up a block move that will end up using FP
     registers as temporaries.  We must make sure this is acceptable.  */
  if (GET_CODE (operands[0]) == MEM
      && GET_CODE (operands[1]) == MEM
      && mode == DImode
      && (SLOW_UNALIGNED_ACCESS (DImode, MEM_ALIGN (operands[0]))
	  || SLOW_UNALIGNED_ACCESS (DImode, MEM_ALIGN (operands[1])))
      && ! (SLOW_UNALIGNED_ACCESS (SImode, (MEM_ALIGN (operands[0]) > 32
					    ? 32 : MEM_ALIGN (operands[0])))
	    || SLOW_UNALIGNED_ACCESS (SImode, (MEM_ALIGN (operands[1]) > 32
					       ? 32 
					       : MEM_ALIGN (operands[1]))))
      && ! MEM_VOLATILE_P (operands [0])
      && ! MEM_VOLATILE_P (operands [1]))
    {
      emit_move_insn (adjust_address (operands[0], SImode, 0),
		      adjust_address (operands[1], SImode, 0));
      emit_move_insn (adjust_address (operands[0], SImode, 4),
		      adjust_address (operands[1], SImode, 4));
      return;
    }
  
  if (!no_new_pseudos)
    {
      if (GET_CODE (operands[1]) == MEM && optimize > 0
	  && (mode == QImode || mode == HImode || mode == SImode)
	  && GET_MODE_SIZE (mode) < GET_MODE_SIZE (ABI_WORD_MODE))
	{
	  rtx reg = gen_reg_rtx (ABI_WORD_MODE);

	  emit_insn (gen_rtx_SET (ABI_WORD_MODE, reg,
				  gen_rtx_ZERO_EXTEND (ABI_WORD_MODE,
						       operands[1])));
	  operands[1] = gen_lowpart (mode, reg);
	}
      if (GET_CODE (operands[0]) != REG)
	operands[1] = force_reg (mode, operands[1]);
    }

  if (mode == SFmode && ! TARGET_POWERPC
      && TARGET_HARD_FLOAT && TARGET_FPRS
      && GET_CODE (operands[0]) == MEM)
    {
      int regnum;

      if (reload_in_progress || reload_completed)
	regnum = true_regnum (operands[1]);
      else if (GET_CODE (operands[1]) == REG)
	regnum = REGNO (operands[1]);
      else
	regnum = -1;
      
      /* If operands[1] is a register, on POWER it may have
	 double-precision data in it, so truncate it to single
	 precision.  */
      if (FP_REGNO_P (regnum) || regnum >= FIRST_PSEUDO_REGISTER)
	{
	  rtx newreg;
	  newreg = (no_new_pseudos ? operands[1] : gen_reg_rtx (mode));
	  emit_insn (gen_aux_truncdfsf2 (newreg, operands[1]));
	  operands[1] = newreg;
	}
    }

  /* Handle the case where reload calls us with an invalid address;
     and the case of CONSTANT_P_RTX.  */
  if (!ALTIVEC_VECTOR_MODE (mode)
      && (! general_operand (operands[1], mode)
	  || ! nonimmediate_operand (operands[0], mode)
	  || GET_CODE (operands[1]) == CONSTANT_P_RTX))
    {
      emit_insn (gen_rtx_SET (VOIDmode, operands[0], operands[1]));
      return;
    }
  
  /* FIXME:  In the long term, this switch statement should go away
     and be replaced by a sequence of tests based on things like
     mode == Pmode.  */
  switch (mode)
    {
    case HImode:
    case QImode:
      if (CONSTANT_P (operands[1])
	  && GET_CODE (operands[1]) != CONST_INT)
	operands[1] = force_const_mem (mode, operands[1]);
      break;

    case TFmode:
    case DFmode:
    case SFmode:
      if (CONSTANT_P (operands[1]) 
	  && ! easy_fp_constant (operands[1], mode))
	operands[1] = force_const_mem (mode, operands[1]);
      break;
      
    case V16QImode:
    case V8HImode:
    case V4SFmode:
    case V4SImode:
    case V4HImode:
    case V2SFmode:
    case V2SImode:
    case V1DImode:
      if (CONSTANT_P (operands[1])
	  && !easy_vector_constant (operands[1]))
	operands[1] = force_const_mem (mode, operands[1]);
      break;
      
    case SImode:
    case DImode:
      /* Use default pattern for address of ELF small data */
      if (TARGET_ELF
	  && mode == Pmode
	  && DEFAULT_ABI == ABI_V4
	  && (GET_CODE (operands[1]) == SYMBOL_REF 
	      || GET_CODE (operands[1]) == CONST)
	  && small_data_operand (operands[1], mode))
	{
	  emit_insn (gen_rtx_SET (VOIDmode, operands[0], operands[1]));
	  return;
	}

      if (DEFAULT_ABI == ABI_V4
	  && mode == Pmode && mode == SImode
	  && flag_pic == 1 && got_operand (operands[1], mode))
	{
	  emit_insn (gen_movsi_got (operands[0], operands[1]));
	  return;
	}

      if ((TARGET_ELF || DEFAULT_ABI == ABI_DARWIN)
	  && TARGET_NO_TOC && ! flag_pic
	  && mode == Pmode
	  && CONSTANT_P (operands[1])
	  && GET_CODE (operands[1]) != HIGH
	  && GET_CODE (operands[1]) != CONST_INT)
	{
	  rtx target = (no_new_pseudos ? operands[0] : gen_reg_rtx (mode));

	  /* If this is a function address on -mcall-aixdesc,
	     convert it to the address of the descriptor.  */
	  if (DEFAULT_ABI == ABI_AIX
	      && GET_CODE (operands[1]) == SYMBOL_REF
	      && XSTR (operands[1], 0)[0] == '.')
	    {
	      const char *name = XSTR (operands[1], 0);
	      rtx new_ref;
	      while (*name == '.')
		name++;
	      new_ref = gen_rtx_SYMBOL_REF (Pmode, name);
	      CONSTANT_POOL_ADDRESS_P (new_ref)
		= CONSTANT_POOL_ADDRESS_P (operands[1]);
	      SYMBOL_REF_FLAG (new_ref) = SYMBOL_REF_FLAG (operands[1]);
	      SYMBOL_REF_USED (new_ref) = SYMBOL_REF_USED (operands[1]);
	      operands[1] = new_ref;
	    }

/* APPLE LOCAL  dynamic-no-pic */
#if TARGET_MACHO
	  if (DEFAULT_ABI == ABI_DARWIN)
	    {
	      /* APPLE LOCAL begin dynamic-no-pic */
	      if (MACHO_DYNAMIC_NO_PIC_P ())
		{
		  /* Take care of any required data indirection.  */
		  operands[1] = rs6000_machopic_legitimize_pic_address (
					operands[1], mode, operands[0]);
		  if (operands[0] != operands[1])
		    emit_insn (gen_rtx_SET (VOIDmode,
					    operands[0], operands[1]));
	        }
	      else
		{
		  emit_insn (gen_macho_high (target, operands[1]));
		  emit_insn (gen_macho_low (operands[0], target, operands[1]));
		}
	    }
	  else
#endif
	    {
	      emit_insn (gen_elf_high (target, operands[1]));
	      emit_insn (gen_elf_low (operands[0], target, operands[1]));
	      /* APPLE LOCAL end dynamic-no-pic */
	    }
	  return;
	}

      /* If this is a SYMBOL_REF that refers to a constant pool entry,
	 and we have put it in the TOC, we just need to make a TOC-relative
	 reference to it.  */
      if (TARGET_TOC
	  && GET_CODE (operands[1]) == SYMBOL_REF
	  && CONSTANT_POOL_EXPR_P (operands[1])
	  && ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (get_pool_constant (operands[1]),
					      get_pool_mode (operands[1])))
	{
	  operands[1] = create_TOC_reference (operands[1]);
	}
      else if (mode == Pmode
	       && CONSTANT_P (operands[1])
	       && ((GET_CODE (operands[1]) != CONST_INT
		    && ! easy_fp_constant (operands[1], mode))
		   || (GET_CODE (operands[1]) == CONST_INT
		       && num_insns_constant (operands[1], mode) > 2)
		   || (GET_CODE (operands[0]) == REG
		       && FP_REGNO_P (REGNO (operands[0]))))
	       && GET_CODE (operands[1]) != HIGH
	       && ! LEGITIMATE_CONSTANT_POOL_ADDRESS_P (operands[1])
	       && ! TOC_RELATIVE_EXPR_P (operands[1]))
	{
	  /* Emit a USE operation so that the constant isn't deleted if
	     expensive optimizations are turned on because nobody
	     references it.  This should only be done for operands that
	     contain SYMBOL_REFs with CONSTANT_POOL_ADDRESS_P set.
	     This should not be done for operands that contain LABEL_REFs.
	     For now, we just handle the obvious case.  */
	  if (GET_CODE (operands[1]) != LABEL_REF)
	    emit_insn (gen_rtx_USE (VOIDmode, operands[1]));

#if TARGET_MACHO
	  /* Darwin uses a special PIC legitimizer.  */
	  /* APPLE LOCAL dynamic-no-pic */
	  if (DEFAULT_ABI == ABI_DARWIN && MACHOPIC_INDIRECT)
	    {
	      operands[1] =
		rs6000_machopic_legitimize_pic_address (operands[1], mode,
							operands[0]);
	      if (operands[0] != operands[1])
		emit_insn (gen_rtx_SET (VOIDmode, operands[0], operands[1]));
	      return;
	    }
#endif

	  /* If we are to limit the number of things we put in the TOC and
	     this is a symbol plus a constant we can add in one insn,
	     just put the symbol in the TOC and add the constant.  Don't do
	     this if reload is in progress.  */
	  if (GET_CODE (operands[1]) == CONST
	      && TARGET_NO_SUM_IN_TOC && ! reload_in_progress
	      && GET_CODE (XEXP (operands[1], 0)) == PLUS
	      && add_operand (XEXP (XEXP (operands[1], 0), 1), mode)
	      && (GET_CODE (XEXP (XEXP (operands[1], 0), 0)) == LABEL_REF
		  || GET_CODE (XEXP (XEXP (operands[1], 0), 0)) == SYMBOL_REF)
	      && ! side_effects_p (operands[0]))
	    {
	      rtx sym =
		force_const_mem (mode, XEXP (XEXP (operands[1], 0), 0));
	      rtx other = XEXP (XEXP (operands[1], 0), 1);

	      sym = force_reg (mode, sym);
	      if (mode == SImode)
		emit_insn (gen_addsi3 (operands[0], sym, other));
	      else
		emit_insn (gen_adddi3 (operands[0], sym, other));
	      return;
	    }

	  operands[1] = force_const_mem (mode, operands[1]);

	  if (TARGET_TOC 
	      && CONSTANT_POOL_EXPR_P (XEXP (operands[1], 0))
	      && ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (
			get_pool_constant (XEXP (operands[1], 0)),
			get_pool_mode (XEXP (operands[1], 0))))
	    {
	      operands[1]
		= gen_rtx_MEM (mode,
			       create_TOC_reference (XEXP (operands[1], 0)));
	      set_mem_alias_set (operands[1], get_TOC_alias_set ());
	      RTX_UNCHANGING_P (operands[1]) = 1;
	    }
	}
      break;

    case TImode:
      if (GET_CODE (operands[0]) == MEM
	  && GET_CODE (XEXP (operands[0], 0)) != REG
	  && ! reload_in_progress)
	operands[0]
	  = replace_equiv_address (operands[0],
				   copy_addr_to_reg (XEXP (operands[0], 0)));

      if (GET_CODE (operands[1]) == MEM
	  && GET_CODE (XEXP (operands[1], 0)) != REG
	  && ! reload_in_progress)
	operands[1]
	  = replace_equiv_address (operands[1],
				   copy_addr_to_reg (XEXP (operands[1], 0)));
      if (TARGET_POWER)
        {
	  emit_insn (gen_rtx_PARALLEL (VOIDmode,
		       gen_rtvec (2,
				  gen_rtx_SET (VOIDmode,
					       operands[0], operands[1]),
				  gen_rtx_CLOBBER (VOIDmode,
						   gen_rtx_SCRATCH (SImode)))));
	  return;
	}
      break;

    default:
      abort ();
    }

  /* Above, we may have called force_const_mem which may have returned
     an invalid address.  If we can, fix this up; otherwise, reload will
     have to deal with it.  */
  if (GET_CODE (operands[1]) == MEM
      && ! memory_address_p (mode, XEXP (operands[1], 0))
      && ! reload_in_progress)
    operands[1] = adjust_address (operands[1], mode, 0);

  emit_insn (gen_rtx_SET (VOIDmode, operands[0], operands[1]));
  return;
}

/* Initialize a variable CUM of type CUMULATIVE_ARGS
   for a call to a function whose data type is FNTYPE.
   For a library call, FNTYPE is 0.

   For incoming args we set the number of arguments in the prototype large
   so we never return a PARALLEL.  */

void
init_cumulative_args (cum, fntype, libname, incoming)
     CUMULATIVE_ARGS *cum;
     tree fntype;
     rtx libname ATTRIBUTE_UNUSED;
     int incoming;
{
  static CUMULATIVE_ARGS zero_cumulative;

  *cum = zero_cumulative;
  cum->words = 0;
  cum->fregno = FP_ARG_MIN_REG;
  cum->vregno = ALTIVEC_ARG_MIN_REG;
  cum->prototype = (fntype && TYPE_ARG_TYPES (fntype));
  cum->call_cookie = CALL_NORMAL;
  cum->sysv_gregno = GP_ARG_MIN_REG;
  /* APPLE LOCAL begin AltiVec */
  cum->num_vector = 0;
  cum->is_incoming = incoming;
  /* APPLE LOCAL end AltiVec */

  if (incoming)
    /* APPLE LOCAL begin AltiVec */
    {
      cum->is_varargs = (current_function_stdarg);
      cum->nargs_prototype = 1000;		/* don't return a PARALLEL */
    }
    /* APPLE LOCAL end AltiVec */

  else if (cum->prototype)
    /* APPLE LOCAL begin AltiVec */
    {
      tree last = tree_last (TYPE_ARG_TYPES (fntype));
      cum->is_varargs = !(last && TREE_VALUE (last) == void_type_node);
      cum->nargs_prototype = (list_length (TYPE_ARG_TYPES (fntype)) - 1
			      + (TYPE_MODE (TREE_TYPE (fntype)) == BLKmode
				 || RETURN_IN_MEMORY (TREE_TYPE (fntype))));
    }
    /* APPLE LOCAL end AltiVec */

  else
    cum->nargs_prototype = 0;

  cum->orig_nargs = cum->nargs_prototype;

  /* Check for a longcall attribute.  */
  if (fntype
      /* APPLE LOCAL long-branch */
      && TARGET_LONG_BRANCH
      && lookup_attribute ("longcall", TYPE_ATTRIBUTES (fntype))
      && !lookup_attribute ("shortcall", TYPE_ATTRIBUTES (fntype)))
    cum->call_cookie = CALL_LONG;

  if (TARGET_DEBUG_ARG)
    {
      fprintf (stderr, "\ninit_cumulative_args:");
      if (fntype)
	{
	  tree ret_type = TREE_TYPE (fntype);
	  fprintf (stderr, " ret code = %s,",
		   tree_code_name[ (int)TREE_CODE (ret_type) ]);
	}

      if (cum->call_cookie & CALL_LONG)
	fprintf (stderr, " longcall,");

      fprintf (stderr, " proto = %d, nargs = %d\n",
	       cum->prototype, cum->nargs_prototype);
    }
}

/* APPLE LOCAL begin AltiVec */
/* Rearrange the argument or parameter list just before scanning it to
   place locate the arguments.

   For the Apple and AIX AltiVec Programming Model, non-vector
   parameters are passed in the same registers and stack locations as
   they would be if the vector parameters were not present.
   Accomplish this by moving the vector parameters to the end of the
   argument list.  */

tree
rearrange_arg_list (cum, args)
     CUMULATIVE_ARGS *cum;
     tree args;
{
  tree arg, value, next, orig_arg_vec;
  tree prev = NULL_TREE;
  tree vector_args = NULL_TREE;
  tree last_vector_arg = NULL_TREE;
  tree vector_reg_args = NULL_TREE;
  int i = 0;
  /* lenght of argument list.  */
  int len = 0;
  int arg_count = 0;
  bool vector_arg_found = false;

  if (!flag_altivec)
    return args;

  len = list_length (args);
  orig_arg_vec = make_tree_vec (len);
  
  /* Don't rearrange for SVR4 or varargs and stdarg.  */
  if (DEFAULT_ABI == ABI_V4 || cum->is_varargs)
    return args;

  /* Remove all vector args.  */
  for (arg = args; arg; arg = next)
    {
      next = TREE_CHAIN (arg);
      TREE_VEC_ELT (orig_arg_vec, arg_count) = arg;
      arg_count++;
      value = (TREE_CODE (arg) == TREE_LIST ? TREE_VALUE (arg) : arg);
      if (TREE_CODE (TREE_TYPE (value)) == VECTOR_TYPE)
	{
	  vector_arg_found = true;
	  if (prev)
	    TREE_CHAIN (prev) = next;
	  else
	    args = next;

	  if (i++ < ALTIVEC_ARG_NUM_REG)
	    {
	      TREE_CHAIN (arg) = vector_reg_args;
	      vector_reg_args = arg;
	    }
	  else
	    {
	      TREE_CHAIN (arg) = vector_args;
	      last_vector_arg = vector_args = arg;
	    }
	}
      else
	prev = arg;
    }

  vector_reg_args = nreverse (vector_reg_args);
  vector_args = nreverse (vector_args);

  cum->num_vector = i;

  if (last_vector_arg)
    TREE_CHAIN (last_vector_arg) = vector_reg_args;
  else
    vector_args = vector_reg_args;

  if (prev)
    TREE_CHAIN (prev) = vector_args;
  else
    args = vector_args;


  /* If args is a TREE_LIST, do  not attach orig_arg_vec.  */
  if (vector_arg_found && (TREE_CODE (args) == PARM_DECL))
    DECL_ORIGINAL_ARGUMENTS (args) = orig_arg_vec;
  else
    orig_arg_vec = NULL;


  return args;
}
/* APPLE LOCAL end AltiVec */

/* If defined, a C expression which determines whether, and in which
   direction, to pad out an argument with extra space.  The value
   should be of type `enum direction': either `upward' to pad above
   the argument, `downward' to pad below, or `none' to inhibit
   padding.

   For the AIX ABI structs are always stored left shifted in their
   argument slot.  */

enum direction
function_arg_padding (mode, type)
     enum machine_mode mode;
     tree type;
{
  if (type != 0 && AGGREGATE_TYPE_P (type))
    return upward;

  /* This is the default definition.  */
  return (! BYTES_BIG_ENDIAN
          ? upward
          : ((mode == BLKmode
              ? (type && TREE_CODE (TYPE_SIZE (type)) == INTEGER_CST
                 && int_size_in_bytes (type) < (PARM_BOUNDARY / BITS_PER_UNIT))
              : GET_MODE_BITSIZE (mode) < PARM_BOUNDARY)
             ? downward : upward));
}

/* If defined, a C expression that gives the alignment boundary, in bits,
   of an argument with the specified mode and type.  If it is not defined, 
   PARM_BOUNDARY is used for all arguments.
   
   V.4 wants long longs to be double word aligned.  */

int
function_arg_boundary (mode, type)
     enum machine_mode mode;
     tree type ATTRIBUTE_UNUSED;
{
  /* APPLE LOCAL begin AltiVec */
  if (VECTOR_MODE_P (mode)
      || (mode == BLKmode
	  && DEFAULT_ABI != ABI_V4
	  && TYPE_ALIGN (type) == 128))
    /* Vector parameters must be 16-byte aligned.  This places them at
       64 mod 128 from the arg pointer.  */
    return 128;
  /* APPLE LOCAL end AltiVec */

  if (DEFAULT_ABI == ABI_V4 && (mode == DImode || mode == DFmode))
    return 64;
   else if (SPE_VECTOR_MODE (mode))
     return 64;
  else if (TARGET_ALTIVEC_ABI && ALTIVEC_VECTOR_MODE (mode))
    return 128;
  else
    return PARM_BOUNDARY;
}

/* APPLE LOCAL begin AltiVec */
int
function_arg_mod_boundary (mode, type)
     enum machine_mode mode;
     tree type;
{
  if (VECTOR_MODE_P (mode)
      || (mode == BLKmode 
	  && DEFAULT_ABI != ABI_V4
	  && TYPE_ALIGN (type) == 128))
    /* Vector parameters must be 16-byte aligned.  This places them at
       2 mod 4 in terms of words.  */
    return 64;
  return 0;
}

static int
function_arg_skip (mode, type, words)
     enum machine_mode mode;
     tree type;
     int words;
{
  if (VECTOR_MODE_P (mode)
      || (mode == BLKmode 
	  && DEFAULT_ABI != ABI_V4
	  && TYPE_ALIGN (type) == 128))
    /* Vector parameters must be 16-byte aligned.  This places them at
       2 mod 4 in terms of words.  */
    return ((6 - (words & 3)) & 3);
  if (TARGET_32BIT && function_arg_boundary (mode, type) == 64)
    return (words & 1);
  return 0;
}

int
no_reg_parm_stack_space (cum, entry)
     CUMULATIVE_ARGS *cum;
     rtx entry;
{
  return (!cum->is_varargs
	  && entry
	  && GET_CODE (entry) == REG
	  && VECTOR_MODE_P (GET_MODE (entry)));
}
/* APPLE LOCAL end AltiVec */

/* APPLE LOCAL begin 64bit registers, ABI32bit */
struct rtx_def * 
rs6000_function_value (valtype, func)
     tree valtype;     
     tree func ATTRIBUTE_UNUSED;
{
  enum machine_mode mode;
  unsigned int regno;

  if (TYPE_MODE (valtype) == DImode && TARGET_POWERPC64 && TARGET_32BIT)
    {
      /* Long longs need to be split here. */
      return gen_rtx (PARALLEL, DImode,
                      gen_rtvec (2,
                                 gen_rtx_EXPR_LIST (VOIDmode,
                                         gen_rtx_REG (SImode,
                                                      GP_ARG_RETURN),
                                                      const0_rtx),
                                 gen_rtx_EXPR_LIST (VOIDmode,
                                         gen_rtx_REG (SImode,
                                                      GP_ARG_RETURN + 1),
                                                      gen_rtx_CONST_INT
                                                        (SImode, 4))));
    }
  if (GET_MODE_CLASS (TYPE_MODE (valtype)) == MODE_COMPLEX_FLOAT
      && TARGET_POWERPC64 && TARGET_32BIT)
    {
      /* complex types need be split in multiple registers. */
      mode = TYPE_MODE (valtype);
      int n_units = RS6000_ARG_SIZE (mode, valtype);
      rtx rvec[GET_MODE_SIZE(TCmode) + 1];
      int k = 0;
      int i;
      for (i=0; i < n_units; i++)
	{
	  rtx r = gen_rtx_REG (SImode, GP_ARG_RETURN + i);
	  rtx off = GEN_INT (i * 4);
	  rvec[k++] = gen_rtx_EXPR_LIST (VOIDmode, r, off);
	}
      return gen_rtx_PARALLEL (mode, gen_rtvec_v (k, rvec));
    }

  if ((INTEGRAL_TYPE_P (valtype) && TYPE_PRECISION (valtype) < BITS_PER_WORD)
        || POINTER_TYPE_P (valtype))
    mode = ABI_WORD_MODE;
  else
    mode = TYPE_MODE (valtype);  

  if (TREE_CODE (valtype) == VECTOR_TYPE && TARGET_ALTIVEC)
    regno = ALTIVEC_ARG_RETURN;
  else if (TREE_CODE (valtype) == REAL_TYPE && TARGET_SPE_ABI && !TARGET_FPRS)
    regno = GP_ARG_RETURN;
  else if (TREE_CODE (valtype) == REAL_TYPE && TARGET_HARD_FLOAT
           && TARGET_FPRS)
    regno = FP_ARG_RETURN;
  else
    regno = GP_ARG_RETURN;
  return gen_rtx_REG (mode, regno);
}
/* APPLE LOCAL end 64bit registers, ABI32bit */

/* Update the data in CUM to advance over an argument
   of mode MODE and data type TYPE.
   (TYPE is null for libcalls where that information may not be available.)  */

void
function_arg_advance (cum, mode, type, named)
     CUMULATIVE_ARGS *cum;
     enum machine_mode mode;
     tree type;
     int named;
{
  cum->nargs_prototype--;

  /* APPLE LOCAL begin AltiVec */
  if (cum->is_incoming && VECTOR_MODE_P (mode))
    named = 1;
  /* APPLE LOCAL end AltiVec */

  if (TARGET_ALTIVEC_ABI && ALTIVEC_VECTOR_MODE (mode) && !cum->is_varargs)
    {
      if (cum->vregno <= ALTIVEC_ARG_MAX_REG && cum->nargs_prototype >= 0)
	cum->vregno++;
      else
	cum->words += RS6000_ARG_SIZE (mode, type);
    }
  else if (TARGET_SPE_ABI && TARGET_SPE && SPE_VECTOR_MODE (mode)
	   && named && cum->sysv_gregno <= GP_ARG_MAX_REG)
    cum->sysv_gregno++;
  else if (DEFAULT_ABI == ABI_V4)
    {
      if (TARGET_HARD_FLOAT && TARGET_FPRS
	  && (mode == SFmode || mode == DFmode))
	{
	  if (cum->fregno <= FP_ARG_V4_MAX_REG)
	    cum->fregno++;
	  else
	    {
	      if (mode == DFmode)
	        cum->words += cum->words & 1;
	      cum->words += RS6000_ARG_SIZE (mode, type);
	    }
	}
      /* APPLE LOCAL begin AltiVec */
      else if (VECTOR_MODE_P (mode))
	{
	  /* Vectors go in registers and don't occupy space in the GPRs.  */
	  if (cum->vregno <= ALTIVEC_ARG_MAX_REG
	      && cum->nargs_prototype >= -1)
	    cum->vregno++;
	  else
	    cum->words += RS6000_ARG_SIZE (mode, type);
	}
      /* APPLE LOCAL end AltiVec */
      else
	{
	  int n_words;
	  int gregno = cum->sysv_gregno;

	  /* Aggregates and IEEE quad get passed by reference.  */
	  if ((type && AGGREGATE_TYPE_P (type))
	      || mode == TFmode)
	    n_words = 1;
	  else 
	    n_words = RS6000_ARG_SIZE (mode, type);

	  /* Long long and SPE vectors are put in odd registers.  */
	  if (n_words == 2 && (gregno & 1) == 0)
	    gregno += 1;

	  /* Long long and SPE vectors are not split between registers
	     and stack.  */
	  if (gregno + n_words - 1 > GP_ARG_MAX_REG)
	    {
	      /* Long long is aligned on the stack.  */
	      if (n_words == 2)
		cum->words += cum->words & 1;
	      cum->words += n_words;
	    }

	  /* Note: continuing to accumulate gregno past when we've started
	     spilling to the stack indicates the fact that we've started
	     spilling to the stack to expand_builtin_saveregs.  */
	  cum->sysv_gregno = gregno + n_words;
	}

      if (TARGET_DEBUG_ARG)
	{
	  fprintf (stderr, "function_adv: words = %2d, fregno = %2d, ",
		   cum->words, cum->fregno);
	  fprintf (stderr, "gregno = %2d, nargs = %4d, proto = %d, ",
		   cum->sysv_gregno, cum->nargs_prototype, cum->prototype);
	  fprintf (stderr, "mode = %4s, named = %d\n",
		   GET_MODE_NAME (mode), named);
	}
    }
  else
    {
      /* APPLE LOCAL AltiVec */
      int align = function_arg_skip (mode, type, cum->words);
      cum->words += align + RS6000_ARG_SIZE (mode, type);

      /* APPLE LOCAL begin AltiVec */
      /* Vectors go in registers and don't occupy space in the GPRs.  */
      if (named && VECTOR_MODE_P (mode) && cum->nargs_prototype >= -1)
	cum->vregno++;
      /* APPLE LOCAL end AltiVec */

      if (GET_MODE_CLASS (mode) == MODE_FLOAT
	  && TARGET_HARD_FLOAT && TARGET_FPRS)
	cum->fregno += (mode == TFmode ? 2 : 1);

      if (TARGET_DEBUG_ARG)
	{
	  fprintf (stderr, "function_adv: words = %2d, fregno = %2d, ",
		   cum->words, cum->fregno);
	  /* APPLE LOCAL begin AltiVec */
	  fprintf (stderr, "vregno = %2d, num_vector = %2d, ",
		   cum->vregno, cum->num_vector);
	  /* APPLE LOCAL end AltiVec */
	  fprintf (stderr, "nargs = %4d, proto = %d, mode = %4s, ",
		   cum->nargs_prototype, cum->prototype, GET_MODE_NAME (mode));
	  fprintf (stderr, "named = %d, align = %d\n", named, align);
	}
    }
}

/* Determine where to put an argument to a function.
   Value is zero to push the argument on the stack,
   or a hard register in which to store the argument.

   MODE is the argument's machine mode.
   TYPE is the data type of the argument (as a tree).
    This is null for libcalls where that information may
    not be available.
   CUM is a variable of type CUMULATIVE_ARGS which gives info about
    the preceding args and about the function being called.
   NAMED is nonzero if this argument is a named parameter
    (otherwise it is an extra parameter matching an ellipsis).

   On RS/6000 the first eight words of non-FP are normally in registers
   and the rest are pushed.  Under AIX, the first 13 FP args are in registers.
   Under V.4, the first 8 FP args are in registers.

   If this is floating-point and no prototype is specified, we use
   both an FP and integer register (or possibly FP reg and stack).  Library
   functions (when TYPE is zero) always have the proper types for args,
   so we can pass the FP value just in one register.  emit_library_function
   doesn't support PARALLEL anyway.  */

struct rtx_def *
function_arg (cum, mode, type, named)
     CUMULATIVE_ARGS *cum;
     enum machine_mode mode;
     tree type;
     int named;
{
  enum rs6000_abi abi = DEFAULT_ABI;

  /* Return a marker to indicate whether CR1 needs to set or clear the
     bit that V.4 uses to say fp args were passed in registers.
     Assume that we don't need the marker for software floating point,
     or compiler generated library calls.  */
  if (mode == VOIDmode)
    {
      if (abi == ABI_V4
	  && cum->nargs_prototype < 0
	  && type && (cum->prototype || TARGET_NO_PROTOTYPE))
	{
	  /* For the SPE, we need to crxor CR6 always.  */
	  if (TARGET_SPE_ABI)
	    return GEN_INT (cum->call_cookie | CALL_V4_SET_FP_ARGS);
	  else if (TARGET_HARD_FLOAT && TARGET_FPRS)
	    return GEN_INT (cum->call_cookie
			    | ((cum->fregno == FP_ARG_MIN_REG)
			       ? CALL_V4_SET_FP_ARGS
			       : CALL_V4_CLEAR_FP_ARGS));
	}

      return GEN_INT (cum->call_cookie);
    }

  /* APPLE LOCAL AltiVec */
  if (TARGET_ALTIVEC_ABI && ALTIVEC_VECTOR_MODE (mode) && !cum->is_varargs)
    {
      if (named && cum->vregno <= ALTIVEC_ARG_MAX_REG)
	return gen_rtx_REG (mode, cum->vregno);
      else
	return NULL;
    }
  else if (TARGET_SPE_ABI && TARGET_SPE && SPE_VECTOR_MODE (mode) && named)
    {
      if (cum->sysv_gregno <= GP_ARG_MAX_REG)
	return gen_rtx_REG (mode, cum->sysv_gregno);
      else
	return NULL;
    }
  else if (abi == ABI_V4)
    {
      if (TARGET_HARD_FLOAT && TARGET_FPRS
	  && (mode == SFmode || mode == DFmode))
	{
	  if (cum->fregno <= FP_ARG_V4_MAX_REG)
	    return gen_rtx_REG (mode, cum->fregno);
	  else
	    return NULL;
	}
      /* APPLE LOCAL begin AltiVec */
      else if (VECTOR_MODE_P (mode))
	{
	  if (cum->nargs_prototype >= 0)
	    {
	      int vregno = cum->vregno;
	      if (cum->num_vector > ALTIVEC_ARG_NUM_REG)
		vregno -= cum->num_vector - ALTIVEC_ARG_NUM_REG;
	      if ((unsigned) vregno - ALTIVEC_ARG_MIN_REG
		  < (unsigned) ALTIVEC_ARG_NUM_REG)
		return gen_rtx (REG, mode, vregno);
	    }
	  return NULL;
	}
      /* APPLE LOCAL end AltiVec */
      else
	{
	  int n_words;
	  int gregno = cum->sysv_gregno;

	  /* Aggregates and IEEE quad get passed by reference.  */
	  if ((type && AGGREGATE_TYPE_P (type))
	      || mode == TFmode)
	    n_words = 1;
	  else 
	    n_words = RS6000_ARG_SIZE (mode, type);

	  /* Long long and SPE vectors are put in odd registers.  */
	  if (n_words == 2 && (gregno & 1) == 0)
	    gregno += 1;

	  /* Long long and SPE vectors are not split between registers
	     and stack.  */
	  if (gregno + n_words - 1 <= GP_ARG_MAX_REG)
	    {
	      /* SPE vectors in ... get split into 2 registers.  */
	      if (TARGET_SPE && TARGET_SPE_ABI
		  && SPE_VECTOR_MODE (mode) && !named)
		{
		  rtx r1, r2;
		  enum machine_mode m = SImode;

		  r1 = gen_rtx_REG (m, gregno);
		  r1 = gen_rtx_EXPR_LIST (m, r1, const0_rtx);
		  r2 = gen_rtx_REG (m, gregno + 1);
		  r2 = gen_rtx_EXPR_LIST (m, r2, GEN_INT (4));
		  return gen_rtx_PARALLEL (mode, gen_rtvec (2, r1, r2));
		}
	      return gen_rtx_REG (mode, gregno);
	    }
	  else
	    return NULL;
	}
    }
  else
    {
      /* APPLE LOCAL AltiVec */
      int align = function_arg_skip (mode, type, cum->words);
      int align_words = cum->words + align;

      if (type && TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST)
        return NULL_RTX;

      if (USE_FP_FOR_ARG_P (*cum, mode, type))
	{
	  if (! type
	      || ((cum->nargs_prototype > 0)
	          /* IBM AIX extended its linkage convention definition always
		     to require FP args after register save area hole on the
		     stack.  */
	          && (DEFAULT_ABI != ABI_AIX
		      || ! TARGET_XL_CALL
		      || (align_words < GP_ARG_NUM_REG))))
	    return gen_rtx_REG (mode, cum->fregno);

	  /* APPLE LOCAL begin 64bit registers, ABI32bit */
	  if (TARGET_32BIT && TARGET_POWERPC64 && cum->is_varargs && mode == DFmode)
	    {
	    	if (align_words >= GP_ARG_NUM_REG)
          	  return gen_rtx_PARALLEL (mode,
	    	    gen_rtvec (2,
		       	       gen_rtx_EXPR_LIST (VOIDmode, NULL_RTX, const0_rtx),
		       	       gen_rtx_EXPR_LIST (VOIDmode, gen_rtx_REG (mode, cum->fregno), const0_rtx)));
		else if (align_words + RS6000_ARG_SIZE (mode, type) > GP_ARG_NUM_REG)
		  /* If this is partially on the stack, then we only include the portion actually in registers here. */ 
          	  return gen_rtx_PARALLEL (mode,
	    	    gen_rtvec (2,
		       	       gen_rtx_EXPR_LIST (VOIDmode, 
						  gen_rtx_REG (SImode, GP_ARG_MIN_REG + align_words), 
						  const0_rtx),
		       	       gen_rtx_EXPR_LIST (VOIDmode, gen_rtx_REG (mode, cum->fregno), const0_rtx)));

		/* split a DFmode arg into two GPRs */
		return gen_rtx_PARALLEL (mode,
		  gen_rtvec (3,
			     gen_rtx_EXPR_LIST (VOIDmode, 
						gen_rtx_REG (SImode, GP_ARG_MIN_REG + align_words), 
						const0_rtx),
			     gen_rtx_EXPR_LIST (VOIDmode, 
						gen_rtx_REG (SImode, GP_ARG_MIN_REG + align_words+1), 
						gen_rtx_CONST_INT (SImode, 4)),
			     gen_rtx_EXPR_LIST (VOIDmode, gen_rtx_REG (mode, cum->fregno), const0_rtx)));
	    }
	    /* APPLE LOCAL end 64bit registers, ABI32bit */
          return gen_rtx_PARALLEL (mode,
	    gen_rtvec (2,
		       gen_rtx_EXPR_LIST (VOIDmode,
				((align_words >= GP_ARG_NUM_REG)
				 ? NULL_RTX
				 : (align_words
				    + RS6000_ARG_SIZE (mode, type)
				    > GP_ARG_NUM_REG
				    /* If this is partially on the stack, then
				       we only include the portion actually
				       in registers here.  */
				    ? gen_rtx_REG (SImode,
					       GP_ARG_MIN_REG + align_words)
				    : gen_rtx_REG (mode,
					       GP_ARG_MIN_REG + align_words))),
				const0_rtx),
		       gen_rtx_EXPR_LIST (VOIDmode,
				gen_rtx_REG (mode, cum->fregno),
				const0_rtx)));
	}
      /* APPLE LOCAL begin AltiVec */
      else if (VECTOR_MODE_P (mode))
	{
	  if (cum->nargs_prototype >= 0)
	    {
	      int vregno = cum->vregno;
	      if (cum->num_vector > ALTIVEC_ARG_NUM_REG)
		vregno -= cum->num_vector - ALTIVEC_ARG_NUM_REG;
	      if ((unsigned)vregno - ALTIVEC_ARG_MIN_REG < (unsigned)ALTIVEC_ARG_NUM_REG)
		return gen_rtx_REG (mode, vregno);
	      return NULL_RTX;
	    }
	  else if (align_words < GP_ARG_NUM_REG)
	    {
	      /* Claim that the vector value goes in both memory and GPRs.
		 See gen_movvxxx for how the GPR copy gets interpreted.
		 Varargs vector regs must be saved in R5-R8 or R9-R10.  */
	      int regno = (align_words < 3) ? 5 : 9;
	      rtx reg = gen_rtx_REG (mode, regno);
	      return gen_rtx (PARALLEL, mode,
			      gen_rtvec (2,
					 gen_rtx (EXPR_LIST, VOIDmode,
						  NULL_RTX, const0_rtx),
					 gen_rtx (EXPR_LIST, VOIDmode,
						  reg, const0_rtx)));
	    }
	  else
	    {
	      /* This is for a vector arg to a varargs function which
		 WILL NOT appear in the GPR area.  Just use memory.  */
	    }

	  return NULL_RTX;
	}
      /* APPLE LOCAL end AltiVec */
      /* APPLE LOCAL begin 64bit registers, ABI32bit */
      else if (TARGET_32BIT && TARGET_POWERPC64 && mode==DImode
               && align_words < GP_ARG_NUM_REG - 1)
        {
          return gen_rtx (PARALLEL, mode,  
                          gen_rtvec (2,
                                     gen_rtx_EXPR_LIST (VOIDmode, 
                                             gen_rtx_REG (SImode,
                                                          GP_ARG_MIN_REG      
                                                          + align_words),  
                                                          const0_rtx),
                                     gen_rtx_EXPR_LIST (VOIDmode,
                                             gen_rtx_REG (SImode,
                                                          GP_ARG_MIN_REG
                                                          + align_words + 1),  
                                                          gen_rtx_CONST_INT
                                                            (SImode, 4))));
        }
      else if (TARGET_32BIT && TARGET_POWERPC64 && mode==DImode
               && align_words == GP_ARG_NUM_REG - 1)
        {
          return gen_rtx (PARALLEL, mode,
                          gen_rtvec (2,
				     gen_rtx (EXPR_LIST, VOIDmode,
                                                  NULL_RTX, const0_rtx),
                                     gen_rtx_EXPR_LIST (VOIDmode,
                                             gen_rtx_REG (SImode,
                                                          GP_ARG_MIN_REG
                                                          + align_words),
                                                          const0_rtx)));
        }
      else if (GET_MODE_CLASS (mode) == MODE_COMPLEX_FLOAT 
	       && TARGET_POWERPC64 && TARGET_32BIT
	       && align_words < GP_ARG_NUM_REG)
	{
	  rtx rvec[GP_ARG_NUM_REG + 1];
	  int i,k;
	  int n_units = RS6000_ARG_SIZE (mode, type);
	  k=0;
	  if (align_words + n_units > GP_ARG_NUM_REG)
	    rvec[k++] = gen_rtx_EXPR_LIST (VOIDmode, NULL_RTX, const0_rtx);
	  i = 0;
  	  do
    	    {
      	      rtx r = gen_rtx_REG (SImode, GP_ARG_MIN_REG + align_words);
      	      rtx off = GEN_INT (i++ * 4);
      	      rvec[k++] = gen_rtx_EXPR_LIST (VOIDmode, r, off);
    	    }
  	  while (++align_words < GP_ARG_NUM_REG && --n_units != 0);
  	  return gen_rtx_PARALLEL (mode, gen_rtvec_v (k, rvec));
	}
      /* APPLE LOCAL end 64bit registers, ABI32bit */
      else if (align_words < GP_ARG_NUM_REG)
	return gen_rtx_REG (mode, GP_ARG_MIN_REG + align_words);
      else
	return NULL_RTX;
    }
}

/* For an arg passed partly in registers and partly in memory,
   this is the number of registers used.
   For args passed entirely in registers or entirely in memory, zero.  */

int
function_arg_partial_nregs (cum, mode, type, named)
     CUMULATIVE_ARGS *cum;
     enum machine_mode mode;
     tree type;
     int named ATTRIBUTE_UNUSED;
{
  /* APPLE LOCAL AltiVec */
  int words;

  if (DEFAULT_ABI == ABI_V4)
    return 0;

  if (USE_FP_FOR_ARG_P (*cum, mode, type)
      || USE_ALTIVEC_FOR_ARG_P (*cum, mode, type))
    {
      if (cum->nargs_prototype >= 0)
	return 0;
    }

  /* APPLE LOCAL begin AltiVec */
  if (type && TREE_CODE (type) == VECTOR_TYPE)
    return 0;
  words = cum->words;
  words += function_arg_skip (mode, type, words);
  if (words < GP_ARG_NUM_REG
      && GP_ARG_NUM_REG < (words + RS6000_ARG_SIZE (mode, type)))
    /* APPLE LOCAL end AltiVec */
    {
      /* APPLE LOCAL AltiVec */
      int ret = GP_ARG_NUM_REG - words;
      if (ret && TARGET_DEBUG_ARG)
	fprintf (stderr, "function_arg_partial_nregs: %d\n", ret);

      return ret;
    }

  return 0;
}

/* A C expression that indicates when an argument must be passed by
   reference.  If nonzero for an argument, a copy of that argument is
   made in memory and a pointer to the argument is passed instead of
   the argument itself.  The pointer is passed in whatever way is
   appropriate for passing a pointer to that type.

   Under V.4, structures and unions are passed by reference.  */

int
function_arg_pass_by_reference (cum, mode, type, named)
     CUMULATIVE_ARGS *cum ATTRIBUTE_UNUSED;
     enum machine_mode mode ATTRIBUTE_UNUSED;
     tree type;
     int named ATTRIBUTE_UNUSED;
{
  if (DEFAULT_ABI == ABI_V4
      && ((type && AGGREGATE_TYPE_P (type))
	  || mode == TFmode))
    {
      if (TARGET_DEBUG_ARG)
	fprintf (stderr, "function_arg_pass_by_reference: aggregate\n");

      return 1;
    }

  return 0;
}

/* Perform any needed actions needed for a function that is receiving a
   variable number of arguments. 

   CUM is as above.

   MODE and TYPE are the mode and type of the current parameter.

   PRETEND_SIZE is a variable that should be set to the amount of stack
   that must be pushed by the prolog to pretend that our caller pushed
   it.

   Normally, this macro will push all remaining incoming registers on the
   stack and set PRETEND_SIZE to the length of the registers pushed.  */

void
setup_incoming_varargs (cum, mode, type, pretend_size, no_rtl)
     CUMULATIVE_ARGS *cum;
     enum machine_mode mode;
     tree type;
     int *pretend_size ATTRIBUTE_UNUSED;
     int no_rtl;

{
  CUMULATIVE_ARGS next_cum;
  int reg_size = TARGET_32BIT ? 4 : 8;
  rtx save_area = NULL_RTX, mem;
  int first_reg_offset, set;
  tree fntype;
  int stdarg_p;

  fntype = TREE_TYPE (current_function_decl);
  stdarg_p = (TYPE_ARG_TYPES (fntype) != 0
	      && (TREE_VALUE (tree_last (TYPE_ARG_TYPES (fntype)))
		  != void_type_node));

  /* For varargs, we do not want to skip the dummy va_dcl argument.
     For stdargs, we do want to skip the last named argument.  */
  next_cum = *cum;
  if (stdarg_p)
    function_arg_advance (&next_cum, mode, type, 1);

  if (DEFAULT_ABI == ABI_V4)
    {
      /* Indicate to allocate space on the stack for varargs save area.  */
      cfun->machine->sysv_varargs_p = 1;
      if (! no_rtl)
	save_area = plus_constant (virtual_stack_vars_rtx,
				   - RS6000_VARARGS_SIZE);

      first_reg_offset = next_cum.sysv_gregno - GP_ARG_MIN_REG;
    }
  else
    {
      first_reg_offset = next_cum.words;
      save_area = virtual_incoming_args_rtx;
      cfun->machine->sysv_varargs_p = 0;

      /* APPLE LOCAL begin AltiVec */
      /* For varargs, vector values occupy memory locations, so count them.  */
      if (VECTOR_MODE_P (mode))
	{
	  first_reg_offset += function_arg_skip (mode, type, cum->words);
	  first_reg_offset += RS6000_ARG_SIZE (mode, type);
	}
      /* APPLE LOCAL end AltiVec */

      if (MUST_PASS_IN_STACK (mode, type))
	first_reg_offset += RS6000_ARG_SIZE (TYPE_MODE (type), type);
    }

  set = get_varargs_alias_set ();
  if (! no_rtl && first_reg_offset < GP_ARG_NUM_REG)
    {
      mem = gen_rtx_MEM (BLKmode,
		         plus_constant (save_area,
					first_reg_offset * reg_size)),
      set_mem_alias_set (mem, set);
      set_mem_align (mem, BITS_PER_WORD);

      move_block_from_reg
	(GP_ARG_MIN_REG + first_reg_offset, mem,
	 GP_ARG_NUM_REG - first_reg_offset,
	 (GP_ARG_NUM_REG - first_reg_offset) * ABI_UNITS_PER_WORD);
    }

  /* Save FP registers if needed.  */
  if (DEFAULT_ABI == ABI_V4
      && TARGET_HARD_FLOAT && TARGET_FPRS
      && ! no_rtl
      && next_cum.fregno <= FP_ARG_V4_MAX_REG)
    {
      int fregno = next_cum.fregno;
      rtx cr1 = gen_rtx_REG (CCmode, CR1_REGNO);
      rtx lab = gen_label_rtx ();
      int off = (GP_ARG_NUM_REG * reg_size) + ((fregno - FP_ARG_MIN_REG) * 8);

      emit_jump_insn (gen_rtx_SET (VOIDmode,
				   pc_rtx,
				   gen_rtx_IF_THEN_ELSE (VOIDmode,
					    gen_rtx_NE (VOIDmode, cr1,
						        const0_rtx),
					    gen_rtx_LABEL_REF (VOIDmode, lab),
					    pc_rtx)));

      while (fregno <= FP_ARG_V4_MAX_REG)
	{
	  mem = gen_rtx_MEM (DFmode, plus_constant (save_area, off));
          set_mem_alias_set (mem, set);
	  emit_move_insn (mem, gen_rtx_REG (DFmode, fregno));
	  fregno++;
	  off += 8;
	}

      emit_label (lab);
    }
}

/* Create the va_list data type.  */

tree
rs6000_build_va_list ()
{
  tree f_gpr, f_fpr, f_ovf, f_sav, record, type_decl;

  /* For AIX, prefer 'char *' because that's what the system
     header files like.  */
  if (DEFAULT_ABI != ABI_V4)
    return build_pointer_type (char_type_node);

  record = (*lang_hooks.types.make_type) (RECORD_TYPE);
  type_decl = build_decl (TYPE_DECL, get_identifier ("__va_list_tag"), record);

  f_gpr = build_decl (FIELD_DECL, get_identifier ("gpr"), 
		      unsigned_char_type_node);
  f_fpr = build_decl (FIELD_DECL, get_identifier ("fpr"), 
		      unsigned_char_type_node);
  f_ovf = build_decl (FIELD_DECL, get_identifier ("overflow_arg_area"),
		      ptr_type_node);
  f_sav = build_decl (FIELD_DECL, get_identifier ("reg_save_area"),
		      ptr_type_node);

  DECL_FIELD_CONTEXT (f_gpr) = record;
  DECL_FIELD_CONTEXT (f_fpr) = record;
  DECL_FIELD_CONTEXT (f_ovf) = record;
  DECL_FIELD_CONTEXT (f_sav) = record;

  TREE_CHAIN (record) = type_decl;
  TYPE_NAME (record) = type_decl;
  TYPE_FIELDS (record) = f_gpr;
  TREE_CHAIN (f_gpr) = f_fpr;
  TREE_CHAIN (f_fpr) = f_ovf;
  TREE_CHAIN (f_ovf) = f_sav;

  layout_type (record);

  /* The correct type is an array type of one element.  */
  return build_array_type (record, build_index_type (size_zero_node));
}

/* Implement va_start.  */

void
rs6000_va_start (valist, nextarg)
     tree valist;
     rtx nextarg;
{
  HOST_WIDE_INT words, n_gpr, n_fpr;
  tree f_gpr, f_fpr, f_ovf, f_sav;
  tree gpr, fpr, ovf, sav, t;

  /* Only SVR4 needs something special.  */
  if (DEFAULT_ABI != ABI_V4)
    {
      std_expand_builtin_va_start (valist, nextarg);
      return;
    }

  f_gpr = TYPE_FIELDS (TREE_TYPE (va_list_type_node));
  f_fpr = TREE_CHAIN (f_gpr);
  f_ovf = TREE_CHAIN (f_fpr);
  f_sav = TREE_CHAIN (f_ovf);

  valist = build1 (INDIRECT_REF, TREE_TYPE (TREE_TYPE (valist)), valist);
  gpr = build (COMPONENT_REF, TREE_TYPE (f_gpr), valist, f_gpr);
  fpr = build (COMPONENT_REF, TREE_TYPE (f_fpr), valist, f_fpr);
  ovf = build (COMPONENT_REF, TREE_TYPE (f_ovf), valist, f_ovf);
  sav = build (COMPONENT_REF, TREE_TYPE (f_sav), valist, f_sav);

  /* Count number of gp and fp argument registers used.  */
  words = current_function_args_info.words;
  n_gpr = current_function_args_info.sysv_gregno - GP_ARG_MIN_REG;
  n_fpr = current_function_args_info.fregno - FP_ARG_MIN_REG;

  if (TARGET_DEBUG_ARG)
    {
      fputs ("va_start: words = ", stderr);
      fprintf (stderr, HOST_WIDE_INT_PRINT_DEC, words);
      fputs (", n_gpr = ", stderr);
      fprintf (stderr, HOST_WIDE_INT_PRINT_DEC, n_gpr);
      fputs (", n_fpr = ", stderr);
      fprintf (stderr, HOST_WIDE_INT_PRINT_DEC, n_fpr);
      putc ('\n', stderr);
    }

  t = build (MODIFY_EXPR, TREE_TYPE (gpr), gpr, build_int_2 (n_gpr, 0));
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  t = build (MODIFY_EXPR, TREE_TYPE (fpr), fpr, build_int_2 (n_fpr, 0));
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  /* Find the overflow area.  */
  t = make_tree (TREE_TYPE (ovf), virtual_incoming_args_rtx);
  if (words != 0)
    t = build (PLUS_EXPR, TREE_TYPE (ovf), t,
	       build_int_2 (words * ABI_UNITS_PER_WORD, 0));
  t = build (MODIFY_EXPR, TREE_TYPE (ovf), ovf, t);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  /* Find the register save area.  */
  t = make_tree (TREE_TYPE (sav), virtual_stack_vars_rtx);
  t = build (PLUS_EXPR, TREE_TYPE (sav), t,
	     build_int_2 (-RS6000_VARARGS_SIZE, -1));
  t = build (MODIFY_EXPR, TREE_TYPE (sav), sav, t);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);
}

/* Implement va_arg.  */

rtx
rs6000_va_arg (valist, type)
     tree valist, type;
{
  tree f_gpr, f_fpr, f_ovf, f_sav;
  tree gpr, fpr, ovf, sav, reg, t, u;
  int indirect_p, size, rsize, n_reg, sav_ofs, sav_scale;
  rtx lab_false, lab_over, addr_rtx, r;

  /* APPLE LOCAL begin AltiVec */
  if (DEFAULT_ABI == ABI_DARWIN)
    {
      HOST_WIDE_INT align, rounded_size;
      enum machine_mode mode;
      tree addr_tree, valist_src;
      /* Compute the rounded size of the type.  */
      align = PARM_BOUNDARY / BITS_PER_UNIT;
      rounded_size = (((int_size_in_bytes (type) + align - 1) / align)
		      * align);

      addr_tree = valist_src = valist;

      mode = TYPE_MODE (type);
      if (mode != BLKmode)
	{
	  HOST_WIDE_INT adj;
	  adj = TREE_INT_CST_LOW (TYPE_SIZE (type)) / BITS_PER_UNIT;
	  if (rounded_size > align)
	    adj = rounded_size;
	  
	  addr_tree = build (PLUS_EXPR, TREE_TYPE (addr_tree), addr_tree,
			     build_int_2 (rounded_size - adj, 0));
	}

      if ( ALTIVEC_VECTOR_MODE (mode) 
	   || (mode == BLKmode && TYPE_ALIGN (type) == 128))
	{
	  /* Round address up to multiple of 16.  Computes (addr+15)&~0xf */
	  addr_tree = fold (build (BIT_AND_EXPR, TREE_TYPE (addr_tree), 
				fold (build (PLUS_EXPR, TREE_TYPE (addr_tree),
					addr_tree, build_int_2 (15, 0))),
				build_int_2 (~15, 0)));
	  valist_src = addr_tree;
	}

      addr_rtx = expand_expr (addr_tree, NULL_RTX, Pmode, EXPAND_NORMAL);
      addr_rtx = copy_to_reg (addr_rtx);
      
      /* Compute new value for AP.  */
      t = build (MODIFY_EXPR, TREE_TYPE (valist), valist,
		 build (PLUS_EXPR, TREE_TYPE (valist_src), valist_src,
			build_int_2 (rounded_size, 0)));
      TREE_SIDE_EFFECTS (t) = 1;
      expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);
      
      return addr_rtx;
    }
  /* APPLE LOCAL end AltiVec */

  if (DEFAULT_ABI != ABI_V4)
    return std_expand_builtin_va_arg (valist, type);

  f_gpr = TYPE_FIELDS (TREE_TYPE (va_list_type_node));
  f_fpr = TREE_CHAIN (f_gpr);
  f_ovf = TREE_CHAIN (f_fpr);
  f_sav = TREE_CHAIN (f_ovf);

  valist = build1 (INDIRECT_REF, TREE_TYPE (TREE_TYPE (valist)), valist);
  gpr = build (COMPONENT_REF, TREE_TYPE (f_gpr), valist, f_gpr);
  fpr = build (COMPONENT_REF, TREE_TYPE (f_fpr), valist, f_fpr);
  ovf = build (COMPONENT_REF, TREE_TYPE (f_ovf), valist, f_ovf);
  sav = build (COMPONENT_REF, TREE_TYPE (f_sav), valist, f_sav);

  size = int_size_in_bytes (type);
  rsize = (size + ABI_UNITS_PER_WORD - 1) / ABI_UNITS_PER_WORD;

  if (AGGREGATE_TYPE_P (type) || TYPE_MODE (type) == TFmode)
    {
      /* Aggregates and long doubles are passed by reference.  */
      indirect_p = 1;
      reg = gpr;
      n_reg = 1;
      sav_ofs = 0;
      sav_scale = 4;
      size = ABI_UNITS_PER_WORD;
      rsize = 1;
    }
  else if (FLOAT_TYPE_P (type) && TARGET_HARD_FLOAT && TARGET_FPRS)
    {
      /* FP args go in FP registers, if present.  */
      indirect_p = 0;
      reg = fpr;
      n_reg = 1;
      sav_ofs = 8*4;
      sav_scale = 8;
    }
  else
    {
      /* Otherwise into GP registers.  */
      indirect_p = 0;
      reg = gpr;
      n_reg = rsize;
      sav_ofs = 0;
      sav_scale = 4;
    }

  /* Pull the value out of the saved registers ...  */

  lab_false = gen_label_rtx ();
  lab_over = gen_label_rtx ();
  addr_rtx = gen_reg_rtx (Pmode);

  /*  AltiVec vectors never go in registers.  */
  if (!TARGET_ALTIVEC || TREE_CODE (type) != VECTOR_TYPE)
    {
      TREE_THIS_VOLATILE (reg) = 1;
      emit_cmp_and_jump_insns
	(expand_expr (reg, NULL_RTX, QImode, EXPAND_NORMAL),
	 GEN_INT (8 - n_reg + 1), GE, const1_rtx, QImode, 1,
	 lab_false);

      /* Long long is aligned in the registers.  */
      if (n_reg > 1)
	{
	  u = build (BIT_AND_EXPR, TREE_TYPE (reg), reg,
		     build_int_2 (n_reg - 1, 0));
	  u = build (PLUS_EXPR, TREE_TYPE (reg), reg, u);
	  u = build (MODIFY_EXPR, TREE_TYPE (reg), reg, u);
	  TREE_SIDE_EFFECTS (u) = 1;
	  expand_expr (u, const0_rtx, VOIDmode, EXPAND_NORMAL);
	}

      if (sav_ofs)
	t = build (PLUS_EXPR, ptr_type_node, sav, build_int_2 (sav_ofs, 0));
      else
	t = sav;

      u = build (POSTINCREMENT_EXPR, TREE_TYPE (reg), reg,
		 build_int_2 (n_reg, 0));
      TREE_SIDE_EFFECTS (u) = 1;

      u = build1 (CONVERT_EXPR, integer_type_node, u);
      TREE_SIDE_EFFECTS (u) = 1;

      u = build (MULT_EXPR, integer_type_node, u, build_int_2 (sav_scale, 0));
      TREE_SIDE_EFFECTS (u) = 1;

      t = build (PLUS_EXPR, ptr_type_node, t, u);
      TREE_SIDE_EFFECTS (t) = 1;

      r = expand_expr (t, addr_rtx, Pmode, EXPAND_NORMAL);
      if (r != addr_rtx)
	emit_move_insn (addr_rtx, r);

      emit_jump_insn (gen_jump (lab_over));
      emit_barrier ();
    }

  emit_label (lab_false);

  /* ... otherwise out of the overflow area.  */

  /* Make sure we don't find reg 7 for the next int arg.

     All AltiVec vectors go in the overflow area.  So in the AltiVec
     case we need to get the vectors from the overflow area, but
     remember where the GPRs and FPRs are.  */
  if (n_reg > 1 && (TREE_CODE (type) != VECTOR_TYPE
		    || !TARGET_ALTIVEC))
    {
      t = build (MODIFY_EXPR, TREE_TYPE (reg), reg, build_int_2 (8, 0));
      TREE_SIDE_EFFECTS (t) = 1;
      expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);
    }

  /* Care for on-stack alignment if needed.  */
  if (rsize <= 1)
    t = ovf;
  else
    {
      int align;

      /* AltiVec vectors are 16 byte aligned.  */
      if (TARGET_ALTIVEC && TREE_CODE (type) == VECTOR_TYPE)
	align = 15;
      else
	align = 7;

      t = build (PLUS_EXPR, TREE_TYPE (ovf), ovf, build_int_2 (align, 0));
      t = build (BIT_AND_EXPR, TREE_TYPE (t), t, build_int_2 (-align-1, -1));
    }
  t = save_expr (t);

  r = expand_expr (t, addr_rtx, Pmode, EXPAND_NORMAL);
  if (r != addr_rtx)
    emit_move_insn (addr_rtx, r);

  t = build (PLUS_EXPR, TREE_TYPE (t), t, build_int_2 (size, 0));
  t = build (MODIFY_EXPR, TREE_TYPE (ovf), ovf, t);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  emit_label (lab_over);

  if (indirect_p)
    {
      r = gen_rtx_MEM (Pmode, addr_rtx);
      set_mem_alias_set (r, get_varargs_alias_set ());
      emit_move_insn (addr_rtx, r);
    }

  return addr_rtx;
}

/* Builtins.  */

#define def_builtin(MASK, NAME, TYPE, CODE)			\
do {								\
  if ((MASK) & target_flags)					\
    builtin_function ((NAME), (TYPE), (CODE), BUILT_IN_MD,	\
		      NULL, NULL_TREE);				\
} while (0)

/* Simple ternary operations: VECd = foo (VECa, VECb, VECc).  */

static const struct builtin_description bdesc_3arg[] =
{
  { MASK_ALTIVEC, CODE_FOR_altivec_vmaddfp, "__builtin_altivec_vmaddfp", ALTIVEC_BUILTIN_VMADDFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmhaddshs, "__builtin_altivec_vmhaddshs", ALTIVEC_BUILTIN_VMHADDSHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmhraddshs, "__builtin_altivec_vmhraddshs", ALTIVEC_BUILTIN_VMHRADDSHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmladduhm, "__builtin_altivec_vmladduhm", ALTIVEC_BUILTIN_VMLADDUHM},
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsumubm, "__builtin_altivec_vmsumubm", ALTIVEC_BUILTIN_VMSUMUBM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsummbm, "__builtin_altivec_vmsummbm", ALTIVEC_BUILTIN_VMSUMMBM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsumuhm, "__builtin_altivec_vmsumuhm", ALTIVEC_BUILTIN_VMSUMUHM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsumshm, "__builtin_altivec_vmsumshm", ALTIVEC_BUILTIN_VMSUMSHM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsumuhs, "__builtin_altivec_vmsumuhs", ALTIVEC_BUILTIN_VMSUMUHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmsumshs, "__builtin_altivec_vmsumshs", ALTIVEC_BUILTIN_VMSUMSHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vnmsubfp, "__builtin_altivec_vnmsubfp", ALTIVEC_BUILTIN_VNMSUBFP }, 
  { MASK_ALTIVEC, CODE_FOR_altivec_vperm_4sf, "__builtin_altivec_vperm_4sf", ALTIVEC_BUILTIN_VPERM_4SF },
  { MASK_ALTIVEC, CODE_FOR_altivec_vperm_4si, "__builtin_altivec_vperm_4si", ALTIVEC_BUILTIN_VPERM_4SI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vperm_8hi, "__builtin_altivec_vperm_8hi", ALTIVEC_BUILTIN_VPERM_8HI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vperm_16qi, "__builtin_altivec_vperm_16qi", ALTIVEC_BUILTIN_VPERM_16QI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsel_4sf, "__builtin_altivec_vsel_4sf", ALTIVEC_BUILTIN_VSEL_4SF },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsel_4si, "__builtin_altivec_vsel_4si", ALTIVEC_BUILTIN_VSEL_4SI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsel_8hi, "__builtin_altivec_vsel_8hi", ALTIVEC_BUILTIN_VSEL_8HI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsel_16qi, "__builtin_altivec_vsel_16qi", ALTIVEC_BUILTIN_VSEL_16QI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsldoi_16qi, "__builtin_altivec_vsldoi_16qi", ALTIVEC_BUILTIN_VSLDOI_16QI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsldoi_8hi, "__builtin_altivec_vsldoi_8hi", ALTIVEC_BUILTIN_VSLDOI_8HI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsldoi_4si, "__builtin_altivec_vsldoi_4si", ALTIVEC_BUILTIN_VSLDOI_4SI },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsldoi_4sf, "__builtin_altivec_vsldoi_4sf", ALTIVEC_BUILTIN_VSLDOI_4SF },
};

/* DST operations: void foo (void *, const int, const char).  */

static const struct builtin_description bdesc_dst[] =
{
  { MASK_ALTIVEC, CODE_FOR_altivec_dst, "__builtin_altivec_dst", ALTIVEC_BUILTIN_DST },
  { MASK_ALTIVEC, CODE_FOR_altivec_dstt, "__builtin_altivec_dstt", ALTIVEC_BUILTIN_DSTT },
  { MASK_ALTIVEC, CODE_FOR_altivec_dstst, "__builtin_altivec_dstst", ALTIVEC_BUILTIN_DSTST },
  { MASK_ALTIVEC, CODE_FOR_altivec_dststt, "__builtin_altivec_dststt", ALTIVEC_BUILTIN_DSTSTT }
};

/* Simple binary operations: VECc = foo (VECa, VECb).  */

static struct builtin_description bdesc_2arg[] =
{
  { MASK_ALTIVEC, CODE_FOR_addv16qi3, "__builtin_altivec_vaddubm", ALTIVEC_BUILTIN_VADDUBM },
  { MASK_ALTIVEC, CODE_FOR_addv8hi3, "__builtin_altivec_vadduhm", ALTIVEC_BUILTIN_VADDUHM },
  { MASK_ALTIVEC, CODE_FOR_addv4si3, "__builtin_altivec_vadduwm", ALTIVEC_BUILTIN_VADDUWM },
  { MASK_ALTIVEC, CODE_FOR_addv4sf3, "__builtin_altivec_vaddfp", ALTIVEC_BUILTIN_VADDFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vaddcuw, "__builtin_altivec_vaddcuw", ALTIVEC_BUILTIN_VADDCUW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vaddubs, "__builtin_altivec_vaddubs", ALTIVEC_BUILTIN_VADDUBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vaddsbs, "__builtin_altivec_vaddsbs", ALTIVEC_BUILTIN_VADDSBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vadduhs, "__builtin_altivec_vadduhs", ALTIVEC_BUILTIN_VADDUHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vaddshs, "__builtin_altivec_vaddshs", ALTIVEC_BUILTIN_VADDSHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vadduws, "__builtin_altivec_vadduws", ALTIVEC_BUILTIN_VADDUWS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vaddsws, "__builtin_altivec_vaddsws", ALTIVEC_BUILTIN_VADDSWS },
  { MASK_ALTIVEC, CODE_FOR_andv4si3, "__builtin_altivec_vand", ALTIVEC_BUILTIN_VAND },
  { MASK_ALTIVEC, CODE_FOR_altivec_vandc, "__builtin_altivec_vandc", ALTIVEC_BUILTIN_VANDC },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavgub, "__builtin_altivec_vavgub", ALTIVEC_BUILTIN_VAVGUB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavgsb, "__builtin_altivec_vavgsb", ALTIVEC_BUILTIN_VAVGSB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavguh, "__builtin_altivec_vavguh", ALTIVEC_BUILTIN_VAVGUH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavgsh, "__builtin_altivec_vavgsh", ALTIVEC_BUILTIN_VAVGSH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavguw, "__builtin_altivec_vavguw", ALTIVEC_BUILTIN_VAVGUW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vavgsw, "__builtin_altivec_vavgsw", ALTIVEC_BUILTIN_VAVGSW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcfux, "__builtin_altivec_vcfux", ALTIVEC_BUILTIN_VCFUX },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcfsx, "__builtin_altivec_vcfsx", ALTIVEC_BUILTIN_VCFSX },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpbfp, "__builtin_altivec_vcmpbfp", ALTIVEC_BUILTIN_VCMPBFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpequb, "__builtin_altivec_vcmpequb", ALTIVEC_BUILTIN_VCMPEQUB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpequh, "__builtin_altivec_vcmpequh", ALTIVEC_BUILTIN_VCMPEQUH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpequw, "__builtin_altivec_vcmpequw", ALTIVEC_BUILTIN_VCMPEQUW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpeqfp, "__builtin_altivec_vcmpeqfp", ALTIVEC_BUILTIN_VCMPEQFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgefp, "__builtin_altivec_vcmpgefp", ALTIVEC_BUILTIN_VCMPGEFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtub, "__builtin_altivec_vcmpgtub", ALTIVEC_BUILTIN_VCMPGTUB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtsb, "__builtin_altivec_vcmpgtsb", ALTIVEC_BUILTIN_VCMPGTSB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtuh, "__builtin_altivec_vcmpgtuh", ALTIVEC_BUILTIN_VCMPGTUH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtsh, "__builtin_altivec_vcmpgtsh", ALTIVEC_BUILTIN_VCMPGTSH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtuw, "__builtin_altivec_vcmpgtuw", ALTIVEC_BUILTIN_VCMPGTUW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtsw, "__builtin_altivec_vcmpgtsw", ALTIVEC_BUILTIN_VCMPGTSW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vcmpgtfp, "__builtin_altivec_vcmpgtfp", ALTIVEC_BUILTIN_VCMPGTFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vctsxs, "__builtin_altivec_vctsxs", ALTIVEC_BUILTIN_VCTSXS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vctuxs, "__builtin_altivec_vctuxs", ALTIVEC_BUILTIN_VCTUXS },
  { MASK_ALTIVEC, CODE_FOR_umaxv16qi3, "__builtin_altivec_vmaxub", ALTIVEC_BUILTIN_VMAXUB },
  { MASK_ALTIVEC, CODE_FOR_smaxv16qi3, "__builtin_altivec_vmaxsb", ALTIVEC_BUILTIN_VMAXSB },
  { MASK_ALTIVEC, CODE_FOR_umaxv8hi3, "__builtin_altivec_vmaxuh", ALTIVEC_BUILTIN_VMAXUH },
  { MASK_ALTIVEC, CODE_FOR_smaxv8hi3, "__builtin_altivec_vmaxsh", ALTIVEC_BUILTIN_VMAXSH },
  { MASK_ALTIVEC, CODE_FOR_umaxv4si3, "__builtin_altivec_vmaxuw", ALTIVEC_BUILTIN_VMAXUW },
  { MASK_ALTIVEC, CODE_FOR_smaxv4si3, "__builtin_altivec_vmaxsw", ALTIVEC_BUILTIN_VMAXSW },
  { MASK_ALTIVEC, CODE_FOR_smaxv4sf3, "__builtin_altivec_vmaxfp", ALTIVEC_BUILTIN_VMAXFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrghb, "__builtin_altivec_vmrghb", ALTIVEC_BUILTIN_VMRGHB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrghh, "__builtin_altivec_vmrghh", ALTIVEC_BUILTIN_VMRGHH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrghw, "__builtin_altivec_vmrghw", ALTIVEC_BUILTIN_VMRGHW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrglb, "__builtin_altivec_vmrglb", ALTIVEC_BUILTIN_VMRGLB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrglh, "__builtin_altivec_vmrglh", ALTIVEC_BUILTIN_VMRGLH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmrglw, "__builtin_altivec_vmrglw", ALTIVEC_BUILTIN_VMRGLW },
  { MASK_ALTIVEC, CODE_FOR_uminv16qi3, "__builtin_altivec_vminub", ALTIVEC_BUILTIN_VMINUB },
  { MASK_ALTIVEC, CODE_FOR_sminv16qi3, "__builtin_altivec_vminsb", ALTIVEC_BUILTIN_VMINSB },
  { MASK_ALTIVEC, CODE_FOR_uminv8hi3, "__builtin_altivec_vminuh", ALTIVEC_BUILTIN_VMINUH },
  { MASK_ALTIVEC, CODE_FOR_sminv8hi3, "__builtin_altivec_vminsh", ALTIVEC_BUILTIN_VMINSH },
  { MASK_ALTIVEC, CODE_FOR_uminv4si3, "__builtin_altivec_vminuw", ALTIVEC_BUILTIN_VMINUW },
  { MASK_ALTIVEC, CODE_FOR_sminv4si3, "__builtin_altivec_vminsw", ALTIVEC_BUILTIN_VMINSW },
  { MASK_ALTIVEC, CODE_FOR_sminv4sf3, "__builtin_altivec_vminfp", ALTIVEC_BUILTIN_VMINFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmuleub, "__builtin_altivec_vmuleub", ALTIVEC_BUILTIN_VMULEUB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmulesb, "__builtin_altivec_vmulesb", ALTIVEC_BUILTIN_VMULESB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmuleuh, "__builtin_altivec_vmuleuh", ALTIVEC_BUILTIN_VMULEUH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmulesh, "__builtin_altivec_vmulesh", ALTIVEC_BUILTIN_VMULESH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmuloub, "__builtin_altivec_vmuloub", ALTIVEC_BUILTIN_VMULOUB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmulosb, "__builtin_altivec_vmulosb", ALTIVEC_BUILTIN_VMULOSB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmulouh, "__builtin_altivec_vmulouh", ALTIVEC_BUILTIN_VMULOUH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vmulosh, "__builtin_altivec_vmulosh", ALTIVEC_BUILTIN_VMULOSH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vnor, "__builtin_altivec_vnor", ALTIVEC_BUILTIN_VNOR },
  { MASK_ALTIVEC, CODE_FOR_iorv4si3, "__builtin_altivec_vor", ALTIVEC_BUILTIN_VOR },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuhum, "__builtin_altivec_vpkuhum", ALTIVEC_BUILTIN_VPKUHUM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuwum, "__builtin_altivec_vpkuwum", ALTIVEC_BUILTIN_VPKUWUM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkpx, "__builtin_altivec_vpkpx", ALTIVEC_BUILTIN_VPKPX },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuhss, "__builtin_altivec_vpkuhss", ALTIVEC_BUILTIN_VPKUHSS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkshss, "__builtin_altivec_vpkshss", ALTIVEC_BUILTIN_VPKSHSS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuwss, "__builtin_altivec_vpkuwss", ALTIVEC_BUILTIN_VPKUWSS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkswss, "__builtin_altivec_vpkswss", ALTIVEC_BUILTIN_VPKSWSS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuhus, "__builtin_altivec_vpkuhus", ALTIVEC_BUILTIN_VPKUHUS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkshus, "__builtin_altivec_vpkshus", ALTIVEC_BUILTIN_VPKSHUS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkuwus, "__builtin_altivec_vpkuwus", ALTIVEC_BUILTIN_VPKUWUS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vpkswus, "__builtin_altivec_vpkswus", ALTIVEC_BUILTIN_VPKSWUS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrlb, "__builtin_altivec_vrlb", ALTIVEC_BUILTIN_VRLB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrlh, "__builtin_altivec_vrlh", ALTIVEC_BUILTIN_VRLH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrlw, "__builtin_altivec_vrlw", ALTIVEC_BUILTIN_VRLW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vslb, "__builtin_altivec_vslb", ALTIVEC_BUILTIN_VSLB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vslh, "__builtin_altivec_vslh", ALTIVEC_BUILTIN_VSLH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vslw, "__builtin_altivec_vslw", ALTIVEC_BUILTIN_VSLW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsl, "__builtin_altivec_vsl", ALTIVEC_BUILTIN_VSL },
  { MASK_ALTIVEC, CODE_FOR_altivec_vslo, "__builtin_altivec_vslo", ALTIVEC_BUILTIN_VSLO },
  { MASK_ALTIVEC, CODE_FOR_altivec_vspltb, "__builtin_altivec_vspltb", ALTIVEC_BUILTIN_VSPLTB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsplth, "__builtin_altivec_vsplth", ALTIVEC_BUILTIN_VSPLTH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vspltw, "__builtin_altivec_vspltw", ALTIVEC_BUILTIN_VSPLTW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsrb, "__builtin_altivec_vsrb", ALTIVEC_BUILTIN_VSRB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsrh, "__builtin_altivec_vsrh", ALTIVEC_BUILTIN_VSRH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsrw, "__builtin_altivec_vsrw", ALTIVEC_BUILTIN_VSRW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsrab, "__builtin_altivec_vsrab", ALTIVEC_BUILTIN_VSRAB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsrah, "__builtin_altivec_vsrah", ALTIVEC_BUILTIN_VSRAH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsraw, "__builtin_altivec_vsraw", ALTIVEC_BUILTIN_VSRAW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsr, "__builtin_altivec_vsr", ALTIVEC_BUILTIN_VSR },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsro, "__builtin_altivec_vsro", ALTIVEC_BUILTIN_VSRO },
  { MASK_ALTIVEC, CODE_FOR_subv16qi3, "__builtin_altivec_vsububm", ALTIVEC_BUILTIN_VSUBUBM },
  { MASK_ALTIVEC, CODE_FOR_subv8hi3, "__builtin_altivec_vsubuhm", ALTIVEC_BUILTIN_VSUBUHM },
  { MASK_ALTIVEC, CODE_FOR_subv4si3, "__builtin_altivec_vsubuwm", ALTIVEC_BUILTIN_VSUBUWM },
  { MASK_ALTIVEC, CODE_FOR_subv4sf3, "__builtin_altivec_vsubfp", ALTIVEC_BUILTIN_VSUBFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubcuw, "__builtin_altivec_vsubcuw", ALTIVEC_BUILTIN_VSUBCUW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsububs, "__builtin_altivec_vsububs", ALTIVEC_BUILTIN_VSUBUBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubsbs, "__builtin_altivec_vsubsbs", ALTIVEC_BUILTIN_VSUBSBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubuhs, "__builtin_altivec_vsubuhs", ALTIVEC_BUILTIN_VSUBUHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubshs, "__builtin_altivec_vsubshs", ALTIVEC_BUILTIN_VSUBSHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubuws, "__builtin_altivec_vsubuws", ALTIVEC_BUILTIN_VSUBUWS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsubsws, "__builtin_altivec_vsubsws", ALTIVEC_BUILTIN_VSUBSWS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsum4ubs, "__builtin_altivec_vsum4ubs", ALTIVEC_BUILTIN_VSUM4UBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsum4sbs, "__builtin_altivec_vsum4sbs", ALTIVEC_BUILTIN_VSUM4SBS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsum4shs, "__builtin_altivec_vsum4shs", ALTIVEC_BUILTIN_VSUM4SHS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsum2sws, "__builtin_altivec_vsum2sws", ALTIVEC_BUILTIN_VSUM2SWS },
  { MASK_ALTIVEC, CODE_FOR_altivec_vsumsws, "__builtin_altivec_vsumsws", ALTIVEC_BUILTIN_VSUMSWS },
  { MASK_ALTIVEC, CODE_FOR_xorv4si3, "__builtin_altivec_vxor", ALTIVEC_BUILTIN_VXOR },

  /* Place holder, leave as first spe builtin.  */
  { 0, CODE_FOR_spe_evaddw, "__builtin_spe_evaddw", SPE_BUILTIN_EVADDW },
  { 0, CODE_FOR_spe_evand, "__builtin_spe_evand", SPE_BUILTIN_EVAND },
  { 0, CODE_FOR_spe_evandc, "__builtin_spe_evandc", SPE_BUILTIN_EVANDC },
  { 0, CODE_FOR_spe_evdivws, "__builtin_spe_evdivws", SPE_BUILTIN_EVDIVWS },
  { 0, CODE_FOR_spe_evdivwu, "__builtin_spe_evdivwu", SPE_BUILTIN_EVDIVWU },
  { 0, CODE_FOR_spe_eveqv, "__builtin_spe_eveqv", SPE_BUILTIN_EVEQV },
  { 0, CODE_FOR_spe_evfsadd, "__builtin_spe_evfsadd", SPE_BUILTIN_EVFSADD },
  { 0, CODE_FOR_spe_evfsdiv, "__builtin_spe_evfsdiv", SPE_BUILTIN_EVFSDIV },
  { 0, CODE_FOR_spe_evfsmul, "__builtin_spe_evfsmul", SPE_BUILTIN_EVFSMUL },
  { 0, CODE_FOR_spe_evfssub, "__builtin_spe_evfssub", SPE_BUILTIN_EVFSSUB },
  { 0, CODE_FOR_spe_evmergehi, "__builtin_spe_evmergehi", SPE_BUILTIN_EVMERGEHI },
  { 0, CODE_FOR_spe_evmergehilo, "__builtin_spe_evmergehilo", SPE_BUILTIN_EVMERGEHILO },
  { 0, CODE_FOR_spe_evmergelo, "__builtin_spe_evmergelo", SPE_BUILTIN_EVMERGELO },
  { 0, CODE_FOR_spe_evmergelohi, "__builtin_spe_evmergelohi", SPE_BUILTIN_EVMERGELOHI },
  { 0, CODE_FOR_spe_evmhegsmfaa, "__builtin_spe_evmhegsmfaa", SPE_BUILTIN_EVMHEGSMFAA },
  { 0, CODE_FOR_spe_evmhegsmfan, "__builtin_spe_evmhegsmfan", SPE_BUILTIN_EVMHEGSMFAN },
  { 0, CODE_FOR_spe_evmhegsmiaa, "__builtin_spe_evmhegsmiaa", SPE_BUILTIN_EVMHEGSMIAA },
  { 0, CODE_FOR_spe_evmhegsmian, "__builtin_spe_evmhegsmian", SPE_BUILTIN_EVMHEGSMIAN },
  { 0, CODE_FOR_spe_evmhegumiaa, "__builtin_spe_evmhegumiaa", SPE_BUILTIN_EVMHEGUMIAA },
  { 0, CODE_FOR_spe_evmhegumian, "__builtin_spe_evmhegumian", SPE_BUILTIN_EVMHEGUMIAN },
  { 0, CODE_FOR_spe_evmhesmf, "__builtin_spe_evmhesmf", SPE_BUILTIN_EVMHESMF },
  { 0, CODE_FOR_spe_evmhesmfa, "__builtin_spe_evmhesmfa", SPE_BUILTIN_EVMHESMFA },
  { 0, CODE_FOR_spe_evmhesmfaaw, "__builtin_spe_evmhesmfaaw", SPE_BUILTIN_EVMHESMFAAW },
  { 0, CODE_FOR_spe_evmhesmfanw, "__builtin_spe_evmhesmfanw", SPE_BUILTIN_EVMHESMFANW },
  { 0, CODE_FOR_spe_evmhesmi, "__builtin_spe_evmhesmi", SPE_BUILTIN_EVMHESMI },
  { 0, CODE_FOR_spe_evmhesmia, "__builtin_spe_evmhesmia", SPE_BUILTIN_EVMHESMIA },
  { 0, CODE_FOR_spe_evmhesmiaaw, "__builtin_spe_evmhesmiaaw", SPE_BUILTIN_EVMHESMIAAW },
  { 0, CODE_FOR_spe_evmhesmianw, "__builtin_spe_evmhesmianw", SPE_BUILTIN_EVMHESMIANW },
  { 0, CODE_FOR_spe_evmhessf, "__builtin_spe_evmhessf", SPE_BUILTIN_EVMHESSF },
  { 0, CODE_FOR_spe_evmhessfa, "__builtin_spe_evmhessfa", SPE_BUILTIN_EVMHESSFA },
  { 0, CODE_FOR_spe_evmhessfaaw, "__builtin_spe_evmhessfaaw", SPE_BUILTIN_EVMHESSFAAW },
  { 0, CODE_FOR_spe_evmhessfanw, "__builtin_spe_evmhessfanw", SPE_BUILTIN_EVMHESSFANW },
  { 0, CODE_FOR_spe_evmhessiaaw, "__builtin_spe_evmhessiaaw", SPE_BUILTIN_EVMHESSIAAW },
  { 0, CODE_FOR_spe_evmhessianw, "__builtin_spe_evmhessianw", SPE_BUILTIN_EVMHESSIANW },
  { 0, CODE_FOR_spe_evmheumi, "__builtin_spe_evmheumi", SPE_BUILTIN_EVMHEUMI },
  { 0, CODE_FOR_spe_evmheumia, "__builtin_spe_evmheumia", SPE_BUILTIN_EVMHEUMIA },
  { 0, CODE_FOR_spe_evmheumiaaw, "__builtin_spe_evmheumiaaw", SPE_BUILTIN_EVMHEUMIAAW },
  { 0, CODE_FOR_spe_evmheumianw, "__builtin_spe_evmheumianw", SPE_BUILTIN_EVMHEUMIANW },
  { 0, CODE_FOR_spe_evmheusiaaw, "__builtin_spe_evmheusiaaw", SPE_BUILTIN_EVMHEUSIAAW },
  { 0, CODE_FOR_spe_evmheusianw, "__builtin_spe_evmheusianw", SPE_BUILTIN_EVMHEUSIANW },
  { 0, CODE_FOR_spe_evmhogsmfaa, "__builtin_spe_evmhogsmfaa", SPE_BUILTIN_EVMHOGSMFAA },
  { 0, CODE_FOR_spe_evmhogsmfan, "__builtin_spe_evmhogsmfan", SPE_BUILTIN_EVMHOGSMFAN },
  { 0, CODE_FOR_spe_evmhogsmiaa, "__builtin_spe_evmhogsmiaa", SPE_BUILTIN_EVMHOGSMIAA },
  { 0, CODE_FOR_spe_evmhogsmian, "__builtin_spe_evmhogsmian", SPE_BUILTIN_EVMHOGSMIAN },
  { 0, CODE_FOR_spe_evmhogumiaa, "__builtin_spe_evmhogumiaa", SPE_BUILTIN_EVMHOGUMIAA },
  { 0, CODE_FOR_spe_evmhogumian, "__builtin_spe_evmhogumian", SPE_BUILTIN_EVMHOGUMIAN },
  { 0, CODE_FOR_spe_evmhosmf, "__builtin_spe_evmhosmf", SPE_BUILTIN_EVMHOSMF },
  { 0, CODE_FOR_spe_evmhosmfa, "__builtin_spe_evmhosmfa", SPE_BUILTIN_EVMHOSMFA },
  { 0, CODE_FOR_spe_evmhosmfaaw, "__builtin_spe_evmhosmfaaw", SPE_BUILTIN_EVMHOSMFAAW },
  { 0, CODE_FOR_spe_evmhosmfanw, "__builtin_spe_evmhosmfanw", SPE_BUILTIN_EVMHOSMFANW },
  { 0, CODE_FOR_spe_evmhosmi, "__builtin_spe_evmhosmi", SPE_BUILTIN_EVMHOSMI },
  { 0, CODE_FOR_spe_evmhosmia, "__builtin_spe_evmhosmia", SPE_BUILTIN_EVMHOSMIA },
  { 0, CODE_FOR_spe_evmhosmiaaw, "__builtin_spe_evmhosmiaaw", SPE_BUILTIN_EVMHOSMIAAW },
  { 0, CODE_FOR_spe_evmhosmianw, "__builtin_spe_evmhosmianw", SPE_BUILTIN_EVMHOSMIANW },
  { 0, CODE_FOR_spe_evmhossf, "__builtin_spe_evmhossf", SPE_BUILTIN_EVMHOSSF },
  { 0, CODE_FOR_spe_evmhossfa, "__builtin_spe_evmhossfa", SPE_BUILTIN_EVMHOSSFA },
  { 0, CODE_FOR_spe_evmhossfaaw, "__builtin_spe_evmhossfaaw", SPE_BUILTIN_EVMHOSSFAAW },
  { 0, CODE_FOR_spe_evmhossfanw, "__builtin_spe_evmhossfanw", SPE_BUILTIN_EVMHOSSFANW },
  { 0, CODE_FOR_spe_evmhossiaaw, "__builtin_spe_evmhossiaaw", SPE_BUILTIN_EVMHOSSIAAW },
  { 0, CODE_FOR_spe_evmhossianw, "__builtin_spe_evmhossianw", SPE_BUILTIN_EVMHOSSIANW },
  { 0, CODE_FOR_spe_evmhoumi, "__builtin_spe_evmhoumi", SPE_BUILTIN_EVMHOUMI },
  { 0, CODE_FOR_spe_evmhoumia, "__builtin_spe_evmhoumia", SPE_BUILTIN_EVMHOUMIA },
  { 0, CODE_FOR_spe_evmhoumiaaw, "__builtin_spe_evmhoumiaaw", SPE_BUILTIN_EVMHOUMIAAW },
  { 0, CODE_FOR_spe_evmhoumianw, "__builtin_spe_evmhoumianw", SPE_BUILTIN_EVMHOUMIANW },
  { 0, CODE_FOR_spe_evmhousiaaw, "__builtin_spe_evmhousiaaw", SPE_BUILTIN_EVMHOUSIAAW },
  { 0, CODE_FOR_spe_evmhousianw, "__builtin_spe_evmhousianw", SPE_BUILTIN_EVMHOUSIANW },
  { 0, CODE_FOR_spe_evmwhsmf, "__builtin_spe_evmwhsmf", SPE_BUILTIN_EVMWHSMF },
  { 0, CODE_FOR_spe_evmwhsmfa, "__builtin_spe_evmwhsmfa", SPE_BUILTIN_EVMWHSMFA },
  { 0, CODE_FOR_spe_evmwhsmi, "__builtin_spe_evmwhsmi", SPE_BUILTIN_EVMWHSMI },
  { 0, CODE_FOR_spe_evmwhsmia, "__builtin_spe_evmwhsmia", SPE_BUILTIN_EVMWHSMIA },
  { 0, CODE_FOR_spe_evmwhssf, "__builtin_spe_evmwhssf", SPE_BUILTIN_EVMWHSSF },
  { 0, CODE_FOR_spe_evmwhssfa, "__builtin_spe_evmwhssfa", SPE_BUILTIN_EVMWHSSFA },
  { 0, CODE_FOR_spe_evmwhumi, "__builtin_spe_evmwhumi", SPE_BUILTIN_EVMWHUMI },
  { 0, CODE_FOR_spe_evmwhumia, "__builtin_spe_evmwhumia", SPE_BUILTIN_EVMWHUMIA },
  { 0, CODE_FOR_spe_evmwlsmiaaw, "__builtin_spe_evmwlsmiaaw", SPE_BUILTIN_EVMWLSMIAAW },
  { 0, CODE_FOR_spe_evmwlsmianw, "__builtin_spe_evmwlsmianw", SPE_BUILTIN_EVMWLSMIANW },
  { 0, CODE_FOR_spe_evmwlssiaaw, "__builtin_spe_evmwlssiaaw", SPE_BUILTIN_EVMWLSSIAAW },
  { 0, CODE_FOR_spe_evmwlssianw, "__builtin_spe_evmwlssianw", SPE_BUILTIN_EVMWLSSIANW },
  { 0, CODE_FOR_spe_evmwlumi, "__builtin_spe_evmwlumi", SPE_BUILTIN_EVMWLUMI },
  { 0, CODE_FOR_spe_evmwlumia, "__builtin_spe_evmwlumia", SPE_BUILTIN_EVMWLUMIA },
  { 0, CODE_FOR_spe_evmwlumiaaw, "__builtin_spe_evmwlumiaaw", SPE_BUILTIN_EVMWLUMIAAW },
  { 0, CODE_FOR_spe_evmwlumianw, "__builtin_spe_evmwlumianw", SPE_BUILTIN_EVMWLUMIANW },
  { 0, CODE_FOR_spe_evmwlusiaaw, "__builtin_spe_evmwlusiaaw", SPE_BUILTIN_EVMWLUSIAAW },
  { 0, CODE_FOR_spe_evmwlusianw, "__builtin_spe_evmwlusianw", SPE_BUILTIN_EVMWLUSIANW },
  { 0, CODE_FOR_spe_evmwsmf, "__builtin_spe_evmwsmf", SPE_BUILTIN_EVMWSMF },
  { 0, CODE_FOR_spe_evmwsmfa, "__builtin_spe_evmwsmfa", SPE_BUILTIN_EVMWSMFA },
  { 0, CODE_FOR_spe_evmwsmfaa, "__builtin_spe_evmwsmfaa", SPE_BUILTIN_EVMWSMFAA },
  { 0, CODE_FOR_spe_evmwsmfan, "__builtin_spe_evmwsmfan", SPE_BUILTIN_EVMWSMFAN },
  { 0, CODE_FOR_spe_evmwsmi, "__builtin_spe_evmwsmi", SPE_BUILTIN_EVMWSMI },
  { 0, CODE_FOR_spe_evmwsmia, "__builtin_spe_evmwsmia", SPE_BUILTIN_EVMWSMIA },
  { 0, CODE_FOR_spe_evmwsmiaa, "__builtin_spe_evmwsmiaa", SPE_BUILTIN_EVMWSMIAA },
  { 0, CODE_FOR_spe_evmwsmian, "__builtin_spe_evmwsmian", SPE_BUILTIN_EVMWSMIAN },
  { 0, CODE_FOR_spe_evmwssf, "__builtin_spe_evmwssf", SPE_BUILTIN_EVMWSSF },
  { 0, CODE_FOR_spe_evmwssfa, "__builtin_spe_evmwssfa", SPE_BUILTIN_EVMWSSFA },
  { 0, CODE_FOR_spe_evmwssfaa, "__builtin_spe_evmwssfaa", SPE_BUILTIN_EVMWSSFAA },
  { 0, CODE_FOR_spe_evmwssfan, "__builtin_spe_evmwssfan", SPE_BUILTIN_EVMWSSFAN },
  { 0, CODE_FOR_spe_evmwumi, "__builtin_spe_evmwumi", SPE_BUILTIN_EVMWUMI },
  { 0, CODE_FOR_spe_evmwumia, "__builtin_spe_evmwumia", SPE_BUILTIN_EVMWUMIA },
  { 0, CODE_FOR_spe_evmwumiaa, "__builtin_spe_evmwumiaa", SPE_BUILTIN_EVMWUMIAA },
  { 0, CODE_FOR_spe_evmwumian, "__builtin_spe_evmwumian", SPE_BUILTIN_EVMWUMIAN },
  { 0, CODE_FOR_spe_evnand, "__builtin_spe_evnand", SPE_BUILTIN_EVNAND },
  { 0, CODE_FOR_spe_evnor, "__builtin_spe_evnor", SPE_BUILTIN_EVNOR },
  { 0, CODE_FOR_spe_evor, "__builtin_spe_evor", SPE_BUILTIN_EVOR },
  { 0, CODE_FOR_spe_evorc, "__builtin_spe_evorc", SPE_BUILTIN_EVORC },
  { 0, CODE_FOR_spe_evrlw, "__builtin_spe_evrlw", SPE_BUILTIN_EVRLW },
  { 0, CODE_FOR_spe_evslw, "__builtin_spe_evslw", SPE_BUILTIN_EVSLW },
  { 0, CODE_FOR_spe_evsrws, "__builtin_spe_evsrws", SPE_BUILTIN_EVSRWS },
  { 0, CODE_FOR_spe_evsrwu, "__builtin_spe_evsrwu", SPE_BUILTIN_EVSRWU },
  { 0, CODE_FOR_spe_evsubfw, "__builtin_spe_evsubfw", SPE_BUILTIN_EVSUBFW },

  /* SPE binary operations expecting a 5-bit unsigned literal.  */
  { 0, CODE_FOR_spe_evaddiw, "__builtin_spe_evaddiw", SPE_BUILTIN_EVADDIW },

  { 0, CODE_FOR_spe_evrlwi, "__builtin_spe_evrlwi", SPE_BUILTIN_EVRLWI },
  { 0, CODE_FOR_spe_evslwi, "__builtin_spe_evslwi", SPE_BUILTIN_EVSLWI },
  { 0, CODE_FOR_spe_evsrwis, "__builtin_spe_evsrwis", SPE_BUILTIN_EVSRWIS },
  { 0, CODE_FOR_spe_evsrwiu, "__builtin_spe_evsrwiu", SPE_BUILTIN_EVSRWIU },
  { 0, CODE_FOR_spe_evsubifw, "__builtin_spe_evsubifw", SPE_BUILTIN_EVSUBIFW },
  { 0, CODE_FOR_spe_evmwhssfaa, "__builtin_spe_evmwhssfaa", SPE_BUILTIN_EVMWHSSFAA },
  { 0, CODE_FOR_spe_evmwhssmaa, "__builtin_spe_evmwhssmaa", SPE_BUILTIN_EVMWHSSMAA },
  { 0, CODE_FOR_spe_evmwhsmfaa, "__builtin_spe_evmwhsmfaa", SPE_BUILTIN_EVMWHSMFAA },
  { 0, CODE_FOR_spe_evmwhsmiaa, "__builtin_spe_evmwhsmiaa", SPE_BUILTIN_EVMWHSMIAA },
  { 0, CODE_FOR_spe_evmwhusiaa, "__builtin_spe_evmwhusiaa", SPE_BUILTIN_EVMWHUSIAA },
  { 0, CODE_FOR_spe_evmwhumiaa, "__builtin_spe_evmwhumiaa", SPE_BUILTIN_EVMWHUMIAA },
  { 0, CODE_FOR_spe_evmwhssfan, "__builtin_spe_evmwhssfan", SPE_BUILTIN_EVMWHSSFAN },
  { 0, CODE_FOR_spe_evmwhssian, "__builtin_spe_evmwhssian", SPE_BUILTIN_EVMWHSSIAN },
  { 0, CODE_FOR_spe_evmwhsmfan, "__builtin_spe_evmwhsmfan", SPE_BUILTIN_EVMWHSMFAN },
  { 0, CODE_FOR_spe_evmwhsmian, "__builtin_spe_evmwhsmian", SPE_BUILTIN_EVMWHSMIAN },
  { 0, CODE_FOR_spe_evmwhusian, "__builtin_spe_evmwhusian", SPE_BUILTIN_EVMWHUSIAN },
  { 0, CODE_FOR_spe_evmwhumian, "__builtin_spe_evmwhumian", SPE_BUILTIN_EVMWHUMIAN },
  { 0, CODE_FOR_spe_evmwhgssfaa, "__builtin_spe_evmwhgssfaa", SPE_BUILTIN_EVMWHGSSFAA },
  { 0, CODE_FOR_spe_evmwhgsmfaa, "__builtin_spe_evmwhgsmfaa", SPE_BUILTIN_EVMWHGSMFAA },
  { 0, CODE_FOR_spe_evmwhgsmiaa, "__builtin_spe_evmwhgsmiaa", SPE_BUILTIN_EVMWHGSMIAA },
  { 0, CODE_FOR_spe_evmwhgumiaa, "__builtin_spe_evmwhgumiaa", SPE_BUILTIN_EVMWHGUMIAA },
  { 0, CODE_FOR_spe_evmwhgssfan, "__builtin_spe_evmwhgssfan", SPE_BUILTIN_EVMWHGSSFAN },
  { 0, CODE_FOR_spe_evmwhgsmfan, "__builtin_spe_evmwhgsmfan", SPE_BUILTIN_EVMWHGSMFAN },
  { 0, CODE_FOR_spe_evmwhgsmian, "__builtin_spe_evmwhgsmian", SPE_BUILTIN_EVMWHGSMIAN },
  { 0, CODE_FOR_spe_evmwhgumian, "__builtin_spe_evmwhgumian", SPE_BUILTIN_EVMWHGUMIAN },
  { 0, CODE_FOR_spe_brinc, "__builtin_spe_brinc", SPE_BUILTIN_BRINC },

  /* Place-holder.  Leave as last binary SPE builtin.  */
  { 0, CODE_FOR_spe_evxor, "__builtin_spe_evxor", SPE_BUILTIN_EVXOR },
};

/* AltiVec predicates.  */

struct builtin_description_predicates
{
  const unsigned int mask;
  const enum insn_code icode;
  const char *opcode;
  const char *const name;
  const enum rs6000_builtins code;
};

static const struct builtin_description_predicates bdesc_altivec_preds[] =
{
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4sf, "*vcmpbfp.", "__builtin_altivec_vcmpbfp_p", ALTIVEC_BUILTIN_VCMPBFP_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4sf, "*vcmpeqfp.", "__builtin_altivec_vcmpeqfp_p", ALTIVEC_BUILTIN_VCMPEQFP_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4sf, "*vcmpgefp.", "__builtin_altivec_vcmpgefp_p", ALTIVEC_BUILTIN_VCMPGEFP_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4sf, "*vcmpgtfp.", "__builtin_altivec_vcmpgtfp_p", ALTIVEC_BUILTIN_VCMPGTFP_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4si, "*vcmpequw.", "__builtin_altivec_vcmpequw_p", ALTIVEC_BUILTIN_VCMPEQUW_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4si, "*vcmpgtsw.", "__builtin_altivec_vcmpgtsw_p", ALTIVEC_BUILTIN_VCMPGTSW_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v4si, "*vcmpgtuw.", "__builtin_altivec_vcmpgtuw_p", ALTIVEC_BUILTIN_VCMPGTUW_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v8hi, "*vcmpgtuh.", "__builtin_altivec_vcmpgtuh_p", ALTIVEC_BUILTIN_VCMPGTUH_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v8hi, "*vcmpgtsh.", "__builtin_altivec_vcmpgtsh_p", ALTIVEC_BUILTIN_VCMPGTSH_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v8hi, "*vcmpequh.", "__builtin_altivec_vcmpequh_p", ALTIVEC_BUILTIN_VCMPEQUH_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v16qi, "*vcmpequb.", "__builtin_altivec_vcmpequb_p", ALTIVEC_BUILTIN_VCMPEQUB_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v16qi, "*vcmpgtsb.", "__builtin_altivec_vcmpgtsb_p", ALTIVEC_BUILTIN_VCMPGTSB_P },
  { MASK_ALTIVEC, CODE_FOR_altivec_predicate_v16qi, "*vcmpgtub.", "__builtin_altivec_vcmpgtub_p", ALTIVEC_BUILTIN_VCMPGTUB_P }
};

/* SPE predicates.  */
static struct builtin_description bdesc_spe_predicates[] =
{
  /* Place-holder.  Leave as first.  */
  { 0, CODE_FOR_spe_evcmpeq, "__builtin_spe_evcmpeq", SPE_BUILTIN_EVCMPEQ },
  { 0, CODE_FOR_spe_evcmpgts, "__builtin_spe_evcmpgts", SPE_BUILTIN_EVCMPGTS },
  { 0, CODE_FOR_spe_evcmpgtu, "__builtin_spe_evcmpgtu", SPE_BUILTIN_EVCMPGTU },
  { 0, CODE_FOR_spe_evcmplts, "__builtin_spe_evcmplts", SPE_BUILTIN_EVCMPLTS },
  { 0, CODE_FOR_spe_evcmpltu, "__builtin_spe_evcmpltu", SPE_BUILTIN_EVCMPLTU },
  { 0, CODE_FOR_spe_evfscmpeq, "__builtin_spe_evfscmpeq", SPE_BUILTIN_EVFSCMPEQ },
  { 0, CODE_FOR_spe_evfscmpgt, "__builtin_spe_evfscmpgt", SPE_BUILTIN_EVFSCMPGT },
  { 0, CODE_FOR_spe_evfscmplt, "__builtin_spe_evfscmplt", SPE_BUILTIN_EVFSCMPLT },
  { 0, CODE_FOR_spe_evfststeq, "__builtin_spe_evfststeq", SPE_BUILTIN_EVFSTSTEQ },
  { 0, CODE_FOR_spe_evfststgt, "__builtin_spe_evfststgt", SPE_BUILTIN_EVFSTSTGT },
  /* Place-holder.  Leave as last.  */
  { 0, CODE_FOR_spe_evfststlt, "__builtin_spe_evfststlt", SPE_BUILTIN_EVFSTSTLT },
};

/* SPE evsel predicates.  */
static struct builtin_description bdesc_spe_evsel[] =
{
  /* Place-holder.  Leave as first.  */
  { 0, CODE_FOR_spe_evcmpgts, "__builtin_spe_evsel_gts", SPE_BUILTIN_EVSEL_CMPGTS },
  { 0, CODE_FOR_spe_evcmpgtu, "__builtin_spe_evsel_gtu", SPE_BUILTIN_EVSEL_CMPGTU },
  { 0, CODE_FOR_spe_evcmplts, "__builtin_spe_evsel_lts", SPE_BUILTIN_EVSEL_CMPLTS },
  { 0, CODE_FOR_spe_evcmpltu, "__builtin_spe_evsel_ltu", SPE_BUILTIN_EVSEL_CMPLTU },
  { 0, CODE_FOR_spe_evcmpeq, "__builtin_spe_evsel_eq", SPE_BUILTIN_EVSEL_CMPEQ },
  { 0, CODE_FOR_spe_evfscmpgt, "__builtin_spe_evsel_fsgt", SPE_BUILTIN_EVSEL_FSCMPGT },
  { 0, CODE_FOR_spe_evfscmplt, "__builtin_spe_evsel_fslt", SPE_BUILTIN_EVSEL_FSCMPLT },
  { 0, CODE_FOR_spe_evfscmpeq, "__builtin_spe_evsel_fseq", SPE_BUILTIN_EVSEL_FSCMPEQ },
  { 0, CODE_FOR_spe_evfststgt, "__builtin_spe_evsel_fststgt", SPE_BUILTIN_EVSEL_FSTSTGT },
  { 0, CODE_FOR_spe_evfststlt, "__builtin_spe_evsel_fststlt", SPE_BUILTIN_EVSEL_FSTSTLT },
  /* Place-holder.  Leave as last.  */
  { 0, CODE_FOR_spe_evfststeq, "__builtin_spe_evsel_fststeq", SPE_BUILTIN_EVSEL_FSTSTEQ },
};

/* ABS* opreations.  */

static const struct builtin_description bdesc_abs[] =
{
  { MASK_ALTIVEC, CODE_FOR_absv4si2, "__builtin_altivec_abs_v4si", ALTIVEC_BUILTIN_ABS_V4SI },
  { MASK_ALTIVEC, CODE_FOR_absv8hi2, "__builtin_altivec_abs_v8hi", ALTIVEC_BUILTIN_ABS_V8HI },
  { MASK_ALTIVEC, CODE_FOR_absv4sf2, "__builtin_altivec_abs_v4sf", ALTIVEC_BUILTIN_ABS_V4SF },
  { MASK_ALTIVEC, CODE_FOR_absv16qi2, "__builtin_altivec_abs_v16qi", ALTIVEC_BUILTIN_ABS_V16QI },
  { MASK_ALTIVEC, CODE_FOR_altivec_abss_v4si, "__builtin_altivec_abss_v4si", ALTIVEC_BUILTIN_ABSS_V4SI },
  { MASK_ALTIVEC, CODE_FOR_altivec_abss_v8hi, "__builtin_altivec_abss_v8hi", ALTIVEC_BUILTIN_ABSS_V8HI },
  { MASK_ALTIVEC, CODE_FOR_altivec_abss_v16qi, "__builtin_altivec_abss_v16qi", ALTIVEC_BUILTIN_ABSS_V16QI }
};

/* Simple unary operations: VECb = foo (unsigned literal) or VECb =
   foo (VECa).  */

static struct builtin_description bdesc_1arg[] =
{
  { MASK_ALTIVEC, CODE_FOR_altivec_vexptefp, "__builtin_altivec_vexptefp", ALTIVEC_BUILTIN_VEXPTEFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vlogefp, "__builtin_altivec_vlogefp", ALTIVEC_BUILTIN_VLOGEFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrefp, "__builtin_altivec_vrefp", ALTIVEC_BUILTIN_VREFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrfim, "__builtin_altivec_vrfim", ALTIVEC_BUILTIN_VRFIM },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrfin, "__builtin_altivec_vrfin", ALTIVEC_BUILTIN_VRFIN },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrfip, "__builtin_altivec_vrfip", ALTIVEC_BUILTIN_VRFIP },
  { MASK_ALTIVEC, CODE_FOR_ftruncv4sf2, "__builtin_altivec_vrfiz", ALTIVEC_BUILTIN_VRFIZ },
  { MASK_ALTIVEC, CODE_FOR_altivec_vrsqrtefp, "__builtin_altivec_vrsqrtefp", ALTIVEC_BUILTIN_VRSQRTEFP },
  { MASK_ALTIVEC, CODE_FOR_altivec_vspltisb, "__builtin_altivec_vspltisb", ALTIVEC_BUILTIN_VSPLTISB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vspltish, "__builtin_altivec_vspltish", ALTIVEC_BUILTIN_VSPLTISH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vspltisw, "__builtin_altivec_vspltisw", ALTIVEC_BUILTIN_VSPLTISW },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupkhsb, "__builtin_altivec_vupkhsb", ALTIVEC_BUILTIN_VUPKHSB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupkhpx, "__builtin_altivec_vupkhpx", ALTIVEC_BUILTIN_VUPKHPX },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupkhsh, "__builtin_altivec_vupkhsh", ALTIVEC_BUILTIN_VUPKHSH },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupklsb, "__builtin_altivec_vupklsb", ALTIVEC_BUILTIN_VUPKLSB },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupklpx, "__builtin_altivec_vupklpx", ALTIVEC_BUILTIN_VUPKLPX },
  { MASK_ALTIVEC, CODE_FOR_altivec_vupklsh, "__builtin_altivec_vupklsh", ALTIVEC_BUILTIN_VUPKLSH },

  /* The SPE unary builtins must start with SPE_BUILTIN_EVABS and
     end with SPE_BUILTIN_EVSUBFUSIAAW.  */
  { 0, CODE_FOR_spe_evabs, "__builtin_spe_evabs", SPE_BUILTIN_EVABS },
  { 0, CODE_FOR_spe_evaddsmiaaw, "__builtin_spe_evaddsmiaaw", SPE_BUILTIN_EVADDSMIAAW },
  { 0, CODE_FOR_spe_evaddssiaaw, "__builtin_spe_evaddssiaaw", SPE_BUILTIN_EVADDSSIAAW },
  { 0, CODE_FOR_spe_evaddumiaaw, "__builtin_spe_evaddumiaaw", SPE_BUILTIN_EVADDUMIAAW },
  { 0, CODE_FOR_spe_evaddusiaaw, "__builtin_spe_evaddusiaaw", SPE_BUILTIN_EVADDUSIAAW },
  { 0, CODE_FOR_spe_evcntlsw, "__builtin_spe_evcntlsw", SPE_BUILTIN_EVCNTLSW },
  { 0, CODE_FOR_spe_evcntlzw, "__builtin_spe_evcntlzw", SPE_BUILTIN_EVCNTLZW },
  { 0, CODE_FOR_spe_evextsb, "__builtin_spe_evextsb", SPE_BUILTIN_EVEXTSB },
  { 0, CODE_FOR_spe_evextsh, "__builtin_spe_evextsh", SPE_BUILTIN_EVEXTSH },
  { 0, CODE_FOR_spe_evfsabs, "__builtin_spe_evfsabs", SPE_BUILTIN_EVFSABS },
  { 0, CODE_FOR_spe_evfscfsf, "__builtin_spe_evfscfsf", SPE_BUILTIN_EVFSCFSF },
  { 0, CODE_FOR_spe_evfscfsi, "__builtin_spe_evfscfsi", SPE_BUILTIN_EVFSCFSI },
  { 0, CODE_FOR_spe_evfscfuf, "__builtin_spe_evfscfuf", SPE_BUILTIN_EVFSCFUF },
  { 0, CODE_FOR_spe_evfscfui, "__builtin_spe_evfscfui", SPE_BUILTIN_EVFSCFUI },
  { 0, CODE_FOR_spe_evfsctsf, "__builtin_spe_evfsctsf", SPE_BUILTIN_EVFSCTSF },
  { 0, CODE_FOR_spe_evfsctsi, "__builtin_spe_evfsctsi", SPE_BUILTIN_EVFSCTSI },
  { 0, CODE_FOR_spe_evfsctsiz, "__builtin_spe_evfsctsiz", SPE_BUILTIN_EVFSCTSIZ },
  { 0, CODE_FOR_spe_evfsctuf, "__builtin_spe_evfsctuf", SPE_BUILTIN_EVFSCTUF },
  { 0, CODE_FOR_spe_evfsctui, "__builtin_spe_evfsctui", SPE_BUILTIN_EVFSCTUI },
  { 0, CODE_FOR_spe_evfsctuiz, "__builtin_spe_evfsctuiz", SPE_BUILTIN_EVFSCTUIZ },
  { 0, CODE_FOR_spe_evfsnabs, "__builtin_spe_evfsnabs", SPE_BUILTIN_EVFSNABS },
  { 0, CODE_FOR_spe_evfsneg, "__builtin_spe_evfsneg", SPE_BUILTIN_EVFSNEG },
  { 0, CODE_FOR_spe_evmra, "__builtin_spe_evmra", SPE_BUILTIN_EVMRA },
  { 0, CODE_FOR_spe_evneg, "__builtin_spe_evneg", SPE_BUILTIN_EVNEG },
  { 0, CODE_FOR_spe_evrndw, "__builtin_spe_evrndw", SPE_BUILTIN_EVRNDW },
  { 0, CODE_FOR_spe_evsubfsmiaaw, "__builtin_spe_evsubfsmiaaw", SPE_BUILTIN_EVSUBFSMIAAW },
  { 0, CODE_FOR_spe_evsubfssiaaw, "__builtin_spe_evsubfssiaaw", SPE_BUILTIN_EVSUBFSSIAAW },
  { 0, CODE_FOR_spe_evsubfumiaaw, "__builtin_spe_evsubfumiaaw", SPE_BUILTIN_EVSUBFUMIAAW },
  { 0, CODE_FOR_spe_evsplatfi, "__builtin_spe_evsplatfi", SPE_BUILTIN_EVSPLATFI },
  { 0, CODE_FOR_spe_evsplati, "__builtin_spe_evsplati", SPE_BUILTIN_EVSPLATI },

  /* Place-holder.  Leave as last unary SPE builtin.  */
  { 0, CODE_FOR_spe_evsubfusiaaw, "__builtin_spe_evsubfusiaaw", SPE_BUILTIN_EVSUBFUSIAAW },
};

static rtx
rs6000_expand_unop_builtin (icode, arglist, target)
     enum insn_code icode;
     tree arglist;
     rtx target;
{
  rtx pat;
  tree arg0 = TREE_VALUE (arglist);
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  enum machine_mode tmode = insn_data[icode].operand[0].mode;
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;

  if (icode == CODE_FOR_nothing)
    /* Builtin not supported on this processor.  */
    return 0;

  /* If we got invalid arguments bail out before generating bad rtl.  */
  if (arg0 == error_mark_node)
    return const0_rtx;

  if (icode == CODE_FOR_altivec_vspltisb
      || icode == CODE_FOR_altivec_vspltish
      || icode == CODE_FOR_altivec_vspltisw
      || icode == CODE_FOR_spe_evsplatfi
      || icode == CODE_FOR_spe_evsplati)
    {
      /* Only allow 5-bit *signed* literals.  */
      if (GET_CODE (op0) != CONST_INT
	  || INTVAL (op0) > 0x1f
	  || INTVAL (op0) < -0x1f)
	{
	  error ("argument 1 must be a 5-bit signed literal");
	  return const0_rtx;
	}
    }

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);

  pat = GEN_FCN (icode) (target, op0);
  if (! pat)
    return 0;
  emit_insn (pat);

  return target;
}

static rtx
altivec_expand_abs_builtin (icode, arglist, target)
     enum insn_code icode;
     tree arglist;
     rtx target;
{
  rtx pat, scratch1, scratch2;
  tree arg0 = TREE_VALUE (arglist);
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  enum machine_mode tmode = insn_data[icode].operand[0].mode;
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;

  /* If we have invalid arguments, bail out before generating bad rtl.  */
  if (arg0 == error_mark_node)
    return const0_rtx;

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);

  scratch1 = gen_reg_rtx (mode0);
  scratch2 = gen_reg_rtx (mode0);

  pat = GEN_FCN (icode) (target, op0, scratch1, scratch2);
  if (! pat)
    return 0;
  emit_insn (pat);

  return target;
}

static rtx
rs6000_expand_binop_builtin (icode, arglist, target)
     enum insn_code icode;
     tree arglist;
     rtx target;
{
  rtx pat;
  tree arg0 = TREE_VALUE (arglist);
  tree arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  enum machine_mode tmode = insn_data[icode].operand[0].mode;
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;
  enum machine_mode mode1 = insn_data[icode].operand[2].mode;

  if (icode == CODE_FOR_nothing)
    /* Builtin not supported on this processor.  */
    return 0;

  /* If we got invalid arguments bail out before generating bad rtl.  */
  if (arg0 == error_mark_node || arg1 == error_mark_node)
    return const0_rtx;

  if (icode == CODE_FOR_altivec_vcfux
      || icode == CODE_FOR_altivec_vcfsx
      || icode == CODE_FOR_altivec_vctsxs
      || icode == CODE_FOR_altivec_vctuxs
      || icode == CODE_FOR_altivec_vspltb
      || icode == CODE_FOR_altivec_vsplth
      || icode == CODE_FOR_altivec_vspltw
      || icode == CODE_FOR_spe_evaddiw
      || icode == CODE_FOR_spe_evldd
      || icode == CODE_FOR_spe_evldh
      || icode == CODE_FOR_spe_evldw
      || icode == CODE_FOR_spe_evlhhesplat
      || icode == CODE_FOR_spe_evlhhossplat
      || icode == CODE_FOR_spe_evlhhousplat
      || icode == CODE_FOR_spe_evlwhe
      || icode == CODE_FOR_spe_evlwhos
      || icode == CODE_FOR_spe_evlwhou
      || icode == CODE_FOR_spe_evlwhsplat
      || icode == CODE_FOR_spe_evlwwsplat
      || icode == CODE_FOR_spe_evrlwi
      || icode == CODE_FOR_spe_evslwi
      || icode == CODE_FOR_spe_evsrwis
      || icode == CODE_FOR_spe_evsrwiu)
    {
      /* Only allow 5-bit unsigned literals.  */
      if (TREE_CODE (arg1) != INTEGER_CST
	  || TREE_INT_CST_LOW (arg1) & ~0x1f)
	{
	  error ("argument 2 must be a 5-bit unsigned literal");
	  return const0_rtx;
	}
    }

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);
  if (! (*insn_data[icode].operand[2].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode1, op1);

  pat = GEN_FCN (icode) (target, op0, op1);
  if (! pat)
    return 0;
  emit_insn (pat);

  return target;
}

static rtx
altivec_expand_predicate_builtin (icode, opcode, arglist, target)
     enum insn_code icode;
     const char *opcode;
     tree arglist;
     rtx target;
{
  rtx pat, scratch;
  tree cr6_form = TREE_VALUE (arglist);
  tree arg0 = TREE_VALUE (TREE_CHAIN (arglist));
  tree arg1 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  enum machine_mode tmode = SImode;
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;
  enum machine_mode mode1 = insn_data[icode].operand[2].mode;
  int cr6_form_int;

  if (TREE_CODE (cr6_form) != INTEGER_CST)
    {
      error ("argument 1 of __builtin_altivec_predicate must be a constant");
      return const0_rtx;
    }
  else
    cr6_form_int = TREE_INT_CST_LOW (cr6_form);

  if (mode0 != mode1)
    abort ();

  /* If we have invalid arguments, bail out before generating bad rtl.  */
  if (arg0 == error_mark_node || arg1 == error_mark_node)
    return const0_rtx;

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);
  if (! (*insn_data[icode].operand[2].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode1, op1);

  scratch = gen_reg_rtx (mode0);

  pat = GEN_FCN (icode) (scratch, op0, op1,
			 gen_rtx (SYMBOL_REF, Pmode, opcode));
  if (! pat)
    return 0;
  emit_insn (pat);

  /* The vec_any* and vec_all* predicates use the same opcodes for two
     different operations, but the bits in CR6 will be different
     depending on what information we want.  So we have to play tricks
     with CR6 to get the right bits out.

     If you think this is disgusting, look at the specs for the
     AltiVec predicates.  */

     switch (cr6_form_int)
       {
       case 0:
	 emit_insn (gen_cr6_test_for_zero (target));
	 break;
       case 1:
	 emit_insn (gen_cr6_test_for_zero_reverse (target));
	 break;
       case 2:
	 emit_insn (gen_cr6_test_for_lt (target));
	 break;
       case 3:
	 emit_insn (gen_cr6_test_for_lt_reverse (target));
	 break;
       default:
	 error ("argument 1 of __builtin_altivec_predicate is out of range");
	 break;
       }

  return target;
}

static rtx
altivec_expand_stv_builtin (icode, arglist)
     enum insn_code icode;
     tree arglist;
{
  tree arg0 = TREE_VALUE (arglist);
  tree arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  tree arg2 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  rtx op2 = expand_expr (arg2, NULL_RTX, VOIDmode, 0);
  rtx pat;
  enum machine_mode mode0 = insn_data[icode].operand[0].mode;
  enum machine_mode mode1 = insn_data[icode].operand[1].mode;
  enum machine_mode mode2 = insn_data[icode].operand[2].mode;

  /* Invalid arguments.  Bail before doing anything stoopid!  */
  if (arg0 == error_mark_node
      || arg1 == error_mark_node
      || arg2 == error_mark_node)
    return const0_rtx;

  if (! (*insn_data[icode].operand[2].predicate) (op0, mode2))
    op0 = copy_to_mode_reg (mode2, op0);
  if (! (*insn_data[icode].operand[0].predicate) (op1, mode0))
    op1 = copy_to_mode_reg (mode0, op1);
  if (! (*insn_data[icode].operand[1].predicate) (op2, mode1))
    op2 = copy_to_mode_reg (mode1, op2);

  pat = GEN_FCN (icode) (op1, op2, op0);
  if (pat)
    emit_insn (pat);
  return NULL_RTX;
}

static rtx
rs6000_expand_ternop_builtin (icode, arglist, target)
     enum insn_code icode;
     tree arglist;
     rtx target;
{
  rtx pat;
  tree arg0 = TREE_VALUE (arglist);
  tree arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  tree arg2 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  rtx op2 = expand_expr (arg2, NULL_RTX, VOIDmode, 0);
  enum machine_mode tmode = insn_data[icode].operand[0].mode;
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;
  enum machine_mode mode1 = insn_data[icode].operand[2].mode;
  enum machine_mode mode2 = insn_data[icode].operand[3].mode;

  if (icode == CODE_FOR_nothing)
    /* Builtin not supported on this processor.  */
    return 0;

  /* If we got invalid arguments bail out before generating bad rtl.  */
  if (arg0 == error_mark_node
      || arg1 == error_mark_node
      || arg2 == error_mark_node)
    return const0_rtx;

  if (icode == CODE_FOR_altivec_vsldoi_4sf
      || icode == CODE_FOR_altivec_vsldoi_4si
      || icode == CODE_FOR_altivec_vsldoi_8hi
      || icode == CODE_FOR_altivec_vsldoi_16qi)
    {
      /* Only allow 4-bit unsigned literals.  */
      if (TREE_CODE (arg2) != INTEGER_CST
	  || TREE_INT_CST_LOW (arg2) & ~0xf)
	{
	  error ("argument 3 must be a 4-bit unsigned literal");
	  return const0_rtx;
	}
    }

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);
  if (! (*insn_data[icode].operand[2].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode1, op1);
  if (! (*insn_data[icode].operand[3].predicate) (op2, mode2))
    op2 = copy_to_mode_reg (mode2, op2);

  pat = GEN_FCN (icode) (target, op0, op1, op2);
  if (! pat)
    return 0;
  emit_insn (pat);

  return target;
}

/* Expand the lvx builtins.  */
static rtx
altivec_expand_ld_builtin (exp, target, expandedp)
     tree exp;
     rtx target;
     bool *expandedp;
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);
  tree arg0;
  enum machine_mode tmode, mode0;
  rtx pat, op0;
  enum insn_code icode;

  switch (fcode)
    {
    case ALTIVEC_BUILTIN_LD_INTERNAL_16qi:
      icode = CODE_FOR_altivec_lvx_16qi;
      break;
    case ALTIVEC_BUILTIN_LD_INTERNAL_8hi:
      icode = CODE_FOR_altivec_lvx_8hi;
      break;
    case ALTIVEC_BUILTIN_LD_INTERNAL_4si:
      icode = CODE_FOR_altivec_lvx_4si;
      break;
    case ALTIVEC_BUILTIN_LD_INTERNAL_4sf:
      icode = CODE_FOR_altivec_lvx_4sf;
      break;
    default:
      *expandedp = false;
      return NULL_RTX;
    }

  *expandedp = true;

  arg0 = TREE_VALUE (arglist);
  op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  tmode = insn_data[icode].operand[0].mode;
  mode0 = insn_data[icode].operand[1].mode;

  if (target == 0
      || GET_MODE (target) != tmode
      || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
    target = gen_reg_rtx (tmode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = gen_rtx_MEM (mode0, copy_to_mode_reg (Pmode, op0));

  pat = GEN_FCN (icode) (target, op0);
  if (! pat)
    return 0;
  emit_insn (pat);
  return target;
}

/* Expand the stvx builtins.  */
static rtx
altivec_expand_st_builtin (exp, target, expandedp)
     tree exp;
     rtx target ATTRIBUTE_UNUSED;
     bool *expandedp;
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);
  tree arg0, arg1;
  enum machine_mode mode0, mode1;
  rtx pat, op0, op1;
  enum insn_code icode;

  switch (fcode)
    {
    case ALTIVEC_BUILTIN_ST_INTERNAL_16qi:
      icode = CODE_FOR_altivec_stvx_16qi;
      break;
    case ALTIVEC_BUILTIN_ST_INTERNAL_8hi:
      icode = CODE_FOR_altivec_stvx_8hi;
      break;
    case ALTIVEC_BUILTIN_ST_INTERNAL_4si:
      icode = CODE_FOR_altivec_stvx_4si;
      break;
    case ALTIVEC_BUILTIN_ST_INTERNAL_4sf:
      icode = CODE_FOR_altivec_stvx_4sf;
      break;
    default:
      *expandedp = false;
      return NULL_RTX;
    }

  arg0 = TREE_VALUE (arglist);
  arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  mode0 = insn_data[icode].operand[0].mode;
  mode1 = insn_data[icode].operand[1].mode;

  if (! (*insn_data[icode].operand[0].predicate) (op0, mode0))
    op0 = gen_rtx_MEM (mode0, copy_to_mode_reg (Pmode, op0));
  if (! (*insn_data[icode].operand[1].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode1, op1);

  pat = GEN_FCN (icode) (op0, op1);
  if (pat)
    emit_insn (pat);

  *expandedp = true;
  return NULL_RTX;
}

/* Expand the dst builtins.  */
static rtx
altivec_expand_dst_builtin (exp, target, expandedp)
     tree exp;
     rtx target ATTRIBUTE_UNUSED;
     bool *expandedp;
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);
  tree arg0, arg1, arg2;
  enum machine_mode mode0, mode1, mode2;
  rtx pat, op0, op1, op2;
  struct builtin_description *d;
  size_t i;

  *expandedp = false;

  /* Handle DST variants.  */
  d = (struct builtin_description *) bdesc_dst;
  for (i = 0; i < ARRAY_SIZE (bdesc_dst); i++, d++)
    if (d->code == fcode)
      {
	arg0 = TREE_VALUE (arglist);
	arg1 = TREE_VALUE (TREE_CHAIN (arglist));
	arg2 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
	op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
	op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
	op2 = expand_expr (arg2, NULL_RTX, VOIDmode, 0);
	mode0 = insn_data[d->icode].operand[0].mode;
	mode1 = insn_data[d->icode].operand[1].mode;
	mode2 = insn_data[d->icode].operand[2].mode;

	/* Invalid arguments, bail out before generating bad rtl.  */
	if (arg0 == error_mark_node
	    || arg1 == error_mark_node
	    || arg2 == error_mark_node)
	  return const0_rtx;

	if (TREE_CODE (arg2) != INTEGER_CST
	    || TREE_INT_CST_LOW (arg2) & ~0x3)
	  {
	    error ("argument to `%s' must be a 2-bit unsigned literal", d->name);
	    return const0_rtx;
	  }

	if (! (*insn_data[d->icode].operand[0].predicate) (op0, mode0))
	  op0 = copy_to_mode_reg (mode0, op0);
	if (! (*insn_data[d->icode].operand[1].predicate) (op1, mode1))
	  op1 = copy_to_mode_reg (mode1, op1);

	pat = GEN_FCN (d->icode) (op0, op1, op2);
	if (pat != 0)
	  emit_insn (pat);

	*expandedp = true;
	return NULL_RTX;
      }

  return NULL_RTX;
}

/* Expand the builtin in EXP and store the result in TARGET.  Store
   true in *EXPANDEDP if we found a builtin to expand.  */
static rtx
altivec_expand_builtin (exp, target, expandedp)
     tree exp;
     rtx target;
     bool *expandedp;
{
  struct builtin_description *d;
  struct builtin_description_predicates *dp;
  size_t i;
  enum insn_code icode;
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  tree arg0;
  rtx op0, pat;
  enum machine_mode tmode, mode0;
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);

  target = altivec_expand_ld_builtin (exp, target, expandedp);
  if (*expandedp)
    return target;

  target = altivec_expand_st_builtin (exp, target, expandedp);
  if (*expandedp)
    return target;

  target = altivec_expand_dst_builtin (exp, target, expandedp);
  if (*expandedp)
    return target;

  *expandedp = true;

  switch (fcode)
    {
    case ALTIVEC_BUILTIN_STVX:
      return altivec_expand_stv_builtin (CODE_FOR_altivec_stvx, arglist);
    case ALTIVEC_BUILTIN_STVEBX:
      return altivec_expand_stv_builtin (CODE_FOR_altivec_stvebx, arglist);
    case ALTIVEC_BUILTIN_STVEHX:
      return altivec_expand_stv_builtin (CODE_FOR_altivec_stvehx, arglist);
    case ALTIVEC_BUILTIN_STVEWX:
      return altivec_expand_stv_builtin (CODE_FOR_altivec_stvewx, arglist);
    case ALTIVEC_BUILTIN_STVXL:
      return altivec_expand_stv_builtin (CODE_FOR_altivec_stvxl, arglist);

    case ALTIVEC_BUILTIN_MFVSCR:
      icode = CODE_FOR_altivec_mfvscr;
      tmode = insn_data[icode].operand[0].mode;

      if (target == 0
	  || GET_MODE (target) != tmode
	  || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
	target = gen_reg_rtx (tmode);
      
      pat = GEN_FCN (icode) (target);
      if (! pat)
	return 0;
      emit_insn (pat);
      return target;

    case ALTIVEC_BUILTIN_MTVSCR:
      icode = CODE_FOR_altivec_mtvscr;
      arg0 = TREE_VALUE (arglist);
      op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
      mode0 = insn_data[icode].operand[0].mode;

      /* If we got invalid arguments bail out before generating bad rtl.  */
      if (arg0 == error_mark_node)
	return const0_rtx;

      if (! (*insn_data[icode].operand[0].predicate) (op0, mode0))
	op0 = copy_to_mode_reg (mode0, op0);

      pat = GEN_FCN (icode) (op0);
      if (pat)
	emit_insn (pat);
      return NULL_RTX;

    case ALTIVEC_BUILTIN_DSSALL:
      emit_insn (gen_altivec_dssall ());
      return NULL_RTX;

    case ALTIVEC_BUILTIN_DSS:
      icode = CODE_FOR_altivec_dss;
      arg0 = TREE_VALUE (arglist);
      op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
      mode0 = insn_data[icode].operand[0].mode;

      /* If we got invalid arguments bail out before generating bad rtl.  */
      if (arg0 == error_mark_node)
	return const0_rtx;

      if (TREE_CODE (arg0) != INTEGER_CST
	  || TREE_INT_CST_LOW (arg0) & ~0x3)
	{
	  error ("argument to dss must be a 2-bit unsigned literal");
	  return const0_rtx;
	}

      if (! (*insn_data[icode].operand[0].predicate) (op0, mode0))
	op0 = copy_to_mode_reg (mode0, op0);

      emit_insn (gen_altivec_dss (op0));
      return NULL_RTX;
    }

  /* Expand abs* operations.  */
  d = (struct builtin_description *) bdesc_abs;
  for (i = 0; i < ARRAY_SIZE (bdesc_abs); i++, d++)
    if (d->code == fcode)
      return altivec_expand_abs_builtin (d->icode, arglist, target);

  /* Expand the AltiVec predicates.  */
  dp = (struct builtin_description_predicates *) bdesc_altivec_preds;
  for (i = 0; i < ARRAY_SIZE (bdesc_altivec_preds); i++, dp++)
    if (dp->code == fcode)
      return altivec_expand_predicate_builtin (dp->icode, dp->opcode, arglist, target);

  /* LV* are funky.  We initialized them differently.  */
  switch (fcode)
    {
    case ALTIVEC_BUILTIN_LVSL:
      return rs6000_expand_binop_builtin (CODE_FOR_altivec_lvsl,
					   arglist, target);
    case ALTIVEC_BUILTIN_LVSR:
      return rs6000_expand_binop_builtin (CODE_FOR_altivec_lvsr,
					  arglist, target);
    case ALTIVEC_BUILTIN_LVEBX:
      return rs6000_expand_binop_builtin (CODE_FOR_altivec_lvebx,
					  arglist, target);
    case ALTIVEC_BUILTIN_LVEHX:
      return rs6000_expand_binop_builtin (CODE_FOR_altivec_lvehx,
					  arglist, target);
    case ALTIVEC_BUILTIN_LVEWX:
      return rs6000_expand_binop_builtin (CODE_FOR_altivec_lvewx,
					  arglist, target);
    case ALTIVEC_BUILTIN_LVXL:
      return rs6000_expand_binop_builtin (CODE_FOR_altivec_lvxl,
					  arglist, target);
    case ALTIVEC_BUILTIN_LVX:
      return rs6000_expand_binop_builtin (CODE_FOR_altivec_lvx,
					  arglist, target);
    default:
      break;
      /* Fall through.  */
    }

  *expandedp = false;
  return NULL_RTX;
}

/* Binops that need to be initialized manually, but can be expanded
   automagically by rs6000_expand_binop_builtin.  */
static struct builtin_description bdesc_2arg_spe[] =
{
  { 0, CODE_FOR_spe_evlddx, "__builtin_spe_evlddx", SPE_BUILTIN_EVLDDX },
  { 0, CODE_FOR_spe_evldwx, "__builtin_spe_evldwx", SPE_BUILTIN_EVLDWX },
  { 0, CODE_FOR_spe_evldhx, "__builtin_spe_evldhx", SPE_BUILTIN_EVLDHX },
  { 0, CODE_FOR_spe_evlwhex, "__builtin_spe_evlwhex", SPE_BUILTIN_EVLWHEX },
  { 0, CODE_FOR_spe_evlwhoux, "__builtin_spe_evlwhoux", SPE_BUILTIN_EVLWHOUX },
  { 0, CODE_FOR_spe_evlwhosx, "__builtin_spe_evlwhosx", SPE_BUILTIN_EVLWHOSX },
  { 0, CODE_FOR_spe_evlwwsplatx, "__builtin_spe_evlwwsplatx", SPE_BUILTIN_EVLWWSPLATX },
  { 0, CODE_FOR_spe_evlwhsplatx, "__builtin_spe_evlwhsplatx", SPE_BUILTIN_EVLWHSPLATX },
  { 0, CODE_FOR_spe_evlhhesplatx, "__builtin_spe_evlhhesplatx", SPE_BUILTIN_EVLHHESPLATX },
  { 0, CODE_FOR_spe_evlhhousplatx, "__builtin_spe_evlhhousplatx", SPE_BUILTIN_EVLHHOUSPLATX },
  { 0, CODE_FOR_spe_evlhhossplatx, "__builtin_spe_evlhhossplatx", SPE_BUILTIN_EVLHHOSSPLATX },
  { 0, CODE_FOR_spe_evldd, "__builtin_spe_evldd", SPE_BUILTIN_EVLDD },
  { 0, CODE_FOR_spe_evldw, "__builtin_spe_evldw", SPE_BUILTIN_EVLDW },
  { 0, CODE_FOR_spe_evldh, "__builtin_spe_evldh", SPE_BUILTIN_EVLDH },
  { 0, CODE_FOR_spe_evlwhe, "__builtin_spe_evlwhe", SPE_BUILTIN_EVLWHE },
  { 0, CODE_FOR_spe_evlwhou, "__builtin_spe_evlwhou", SPE_BUILTIN_EVLWHOU },
  { 0, CODE_FOR_spe_evlwhos, "__builtin_spe_evlwhos", SPE_BUILTIN_EVLWHOS },
  { 0, CODE_FOR_spe_evlwwsplat, "__builtin_spe_evlwwsplat", SPE_BUILTIN_EVLWWSPLAT },
  { 0, CODE_FOR_spe_evlwhsplat, "__builtin_spe_evlwhsplat", SPE_BUILTIN_EVLWHSPLAT },
  { 0, CODE_FOR_spe_evlhhesplat, "__builtin_spe_evlhhesplat", SPE_BUILTIN_EVLHHESPLAT },
  { 0, CODE_FOR_spe_evlhhousplat, "__builtin_spe_evlhhousplat", SPE_BUILTIN_EVLHHOUSPLAT },
  { 0, CODE_FOR_spe_evlhhossplat, "__builtin_spe_evlhhossplat", SPE_BUILTIN_EVLHHOSSPLAT }
};

/* Expand the builtin in EXP and store the result in TARGET.  Store
   true in *EXPANDEDP if we found a builtin to expand.

   This expands the SPE builtins that are not simple unary and binary
   operations.  */
static rtx
spe_expand_builtin (exp, target, expandedp)
     tree exp;
     rtx target;
     bool *expandedp;
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  tree arg1, arg0;
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);
  enum insn_code icode;
  enum machine_mode tmode, mode0;
  rtx pat, op0;
  struct builtin_description *d;
  size_t i;

  *expandedp = true;

  /* Syntax check for a 5-bit unsigned immediate.  */
  switch (fcode)
    {
    case SPE_BUILTIN_EVSTDD:
    case SPE_BUILTIN_EVSTDH:
    case SPE_BUILTIN_EVSTDW:
    case SPE_BUILTIN_EVSTWHE:
    case SPE_BUILTIN_EVSTWHO:
    case SPE_BUILTIN_EVSTWWE:
    case SPE_BUILTIN_EVSTWWO:
      arg1 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
      if (TREE_CODE (arg1) != INTEGER_CST
	  || TREE_INT_CST_LOW (arg1) & ~0x1f)
	{
	  error ("argument 2 must be a 5-bit unsigned literal");
	  return const0_rtx;
	}
      break;
    default:
      break;
    }

  d = (struct builtin_description *) bdesc_2arg_spe;
  for (i = 0; i < ARRAY_SIZE (bdesc_2arg_spe); ++i, ++d)
    if (d->code == fcode)
      return rs6000_expand_binop_builtin (d->icode, arglist, target);

  d = (struct builtin_description *) bdesc_spe_predicates;
  for (i = 0; i < ARRAY_SIZE (bdesc_spe_predicates); ++i, ++d)
    if (d->code == fcode)
      return spe_expand_predicate_builtin (d->icode, arglist, target);

  d = (struct builtin_description *) bdesc_spe_evsel;
  for (i = 0; i < ARRAY_SIZE (bdesc_spe_evsel); ++i, ++d)
    if (d->code == fcode)
      return spe_expand_evsel_builtin (d->icode, arglist, target);

  switch (fcode)
    {
    case SPE_BUILTIN_EVSTDDX:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstddx, arglist);
    case SPE_BUILTIN_EVSTDHX:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstdhx, arglist);
    case SPE_BUILTIN_EVSTDWX:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstdwx, arglist);
    case SPE_BUILTIN_EVSTWHEX:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstwhex, arglist);
    case SPE_BUILTIN_EVSTWHOX:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstwhox, arglist);
    case SPE_BUILTIN_EVSTWWEX:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstwwex, arglist);
    case SPE_BUILTIN_EVSTWWOX:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstwwox, arglist);
    case SPE_BUILTIN_EVSTDD:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstdd, arglist);
    case SPE_BUILTIN_EVSTDH:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstdh, arglist);
    case SPE_BUILTIN_EVSTDW:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstdw, arglist);
    case SPE_BUILTIN_EVSTWHE:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstwhe, arglist);
    case SPE_BUILTIN_EVSTWHO:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstwho, arglist);
    case SPE_BUILTIN_EVSTWWE:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstwwe, arglist);
    case SPE_BUILTIN_EVSTWWO:
      return altivec_expand_stv_builtin (CODE_FOR_spe_evstwwo, arglist);
    case SPE_BUILTIN_MFSPEFSCR:
      icode = CODE_FOR_spe_mfspefscr;
      tmode = insn_data[icode].operand[0].mode;

      if (target == 0
	  || GET_MODE (target) != tmode
	  || ! (*insn_data[icode].operand[0].predicate) (target, tmode))
	target = gen_reg_rtx (tmode);
      
      pat = GEN_FCN (icode) (target);
      if (! pat)
	return 0;
      emit_insn (pat);
      return target;
    case SPE_BUILTIN_MTSPEFSCR:
      icode = CODE_FOR_spe_mtspefscr;
      arg0 = TREE_VALUE (arglist);
      op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
      mode0 = insn_data[icode].operand[0].mode;

      if (arg0 == error_mark_node)
	return const0_rtx;

      if (! (*insn_data[icode].operand[0].predicate) (op0, mode0))
	op0 = copy_to_mode_reg (mode0, op0);

      pat = GEN_FCN (icode) (op0);
      if (pat)
	emit_insn (pat);
      return NULL_RTX;
    default:
      break;
    }

  *expandedp = false;
  return NULL_RTX;
}

static rtx
spe_expand_predicate_builtin (icode, arglist, target)
     enum insn_code icode;
     tree arglist;
     rtx target;
{
  rtx pat, scratch, tmp;
  tree form = TREE_VALUE (arglist);
  tree arg0 = TREE_VALUE (TREE_CHAIN (arglist));
  tree arg1 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;
  enum machine_mode mode1 = insn_data[icode].operand[2].mode;
  int form_int;
  enum rtx_code code;

  if (TREE_CODE (form) != INTEGER_CST)
    {
      error ("argument 1 of __builtin_spe_predicate must be a constant");
      return const0_rtx;
    }
  else
    form_int = TREE_INT_CST_LOW (form);

  if (mode0 != mode1)
    abort ();

  if (arg0 == error_mark_node || arg1 == error_mark_node)
    return const0_rtx;

  if (target == 0
      || GET_MODE (target) != SImode
      || ! (*insn_data[icode].operand[0].predicate) (target, SImode))
    target = gen_reg_rtx (SImode);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);
  if (! (*insn_data[icode].operand[2].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode1, op1);

  scratch = gen_reg_rtx (CCmode);

  pat = GEN_FCN (icode) (scratch, op0, op1);
  if (! pat)
    return const0_rtx;
  emit_insn (pat);

  /* There are 4 variants for each predicate: _any_, _all_, _upper_,
     _lower_.  We use one compare, but look in different bits of the
     CR for each variant.

     There are 2 elements in each SPE simd type (upper/lower).  The CR
     bits are set as follows:

     BIT0  | BIT 1  | BIT 2   | BIT 3
     U     |   L    | (U | L) | (U & L)

     So, for an "all" relationship, BIT 3 would be set.
     For an "any" relationship, BIT 2 would be set.  Etc.

     Following traditional nomenclature, these bits map to:

     BIT0  | BIT 1  | BIT 2   | BIT 3
     LT    | GT     | EQ      | OV

     Later, we will generate rtl to look in the LT/EQ/EQ/OV bits.
  */

  switch (form_int)
    {
      /* All variant.  OV bit.  */
    case 0:
      /* We need to get to the OV bit, which is the ORDERED bit.  We
	 could generate (ordered:SI (reg:CC xx) (const_int 0)), but
	 that's ugly and will trigger a validate_condition_mode abort.
	 So let's just use another pattern.  */
      emit_insn (gen_move_from_CR_ov_bit (target, scratch));
      return target;
      /* Any variant.  EQ bit.  */
    case 1:
      code = EQ;
      break;
      /* Upper variant.  LT bit.  */
    case 2:
      code = LT;
      break;
      /* Lower variant.  GT bit.  */
    case 3:
      code = GT;
      break;
    default:
      error ("argument 1 of __builtin_spe_predicate is out of range");
      return const0_rtx;
    }

  tmp = gen_rtx_fmt_ee (code, SImode, scratch, const0_rtx);
  emit_move_insn (target, tmp);

  return target;
}

/* The evsel builtins look like this:

     e = __builtin_spe_evsel_OP (a, b, c, d);

   and work like this:

     e[upper] = a[upper] *OP* b[upper] ? c[upper] : d[upper];
     e[lower] = a[lower] *OP* b[lower] ? c[lower] : d[lower];
*/

static rtx
spe_expand_evsel_builtin (icode, arglist, target)
     enum insn_code icode;
     tree arglist;
     rtx target;
{
  rtx pat, scratch;
  tree arg0 = TREE_VALUE (arglist);
  tree arg1 = TREE_VALUE (TREE_CHAIN (arglist));
  tree arg2 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (arglist)));
  tree arg3 = TREE_VALUE (TREE_CHAIN (TREE_CHAIN (TREE_CHAIN (arglist))));
  rtx op0 = expand_expr (arg0, NULL_RTX, VOIDmode, 0);
  rtx op1 = expand_expr (arg1, NULL_RTX, VOIDmode, 0);
  rtx op2 = expand_expr (arg2, NULL_RTX, VOIDmode, 0);
  rtx op3 = expand_expr (arg3, NULL_RTX, VOIDmode, 0);
  enum machine_mode mode0 = insn_data[icode].operand[1].mode;
  enum machine_mode mode1 = insn_data[icode].operand[2].mode;

  if (mode0 != mode1)
    abort ();

  if (arg0 == error_mark_node || arg1 == error_mark_node
      || arg2 == error_mark_node || arg3 == error_mark_node)
    return const0_rtx;

  if (target == 0
      || GET_MODE (target) != mode0
      || ! (*insn_data[icode].operand[0].predicate) (target, mode0))
    target = gen_reg_rtx (mode0);

  if (! (*insn_data[icode].operand[1].predicate) (op0, mode0))
    op0 = copy_to_mode_reg (mode0, op0);
  if (! (*insn_data[icode].operand[1].predicate) (op1, mode1))
    op1 = copy_to_mode_reg (mode0, op1);
  if (! (*insn_data[icode].operand[1].predicate) (op2, mode1))
    op2 = copy_to_mode_reg (mode0, op2);
  if (! (*insn_data[icode].operand[1].predicate) (op3, mode1))
    op3 = copy_to_mode_reg (mode0, op3);

  /* Generate the compare.  */
  scratch = gen_reg_rtx (CCmode);
  pat = GEN_FCN (icode) (scratch, op0, op1);
  if (! pat)
    return const0_rtx;
  emit_insn (pat);

  if (mode0 == V2SImode)
    emit_insn (gen_spe_evsel (target, op2, op3, scratch));
  else
    emit_insn (gen_spe_evsel_fs (target, op2, op3, scratch));

  return target;
}

/* Expand an expression EXP that calls a built-in function,
   with result going to TARGET if that's convenient
   (and in mode MODE if that's convenient).
   SUBTARGET may be used as the target for computing one of EXP's operands.
   IGNORE is nonzero if the value is to be ignored.  */

static rtx
rs6000_expand_builtin (exp, target, subtarget, mode, ignore)
     tree exp;
     rtx target;
     rtx subtarget ATTRIBUTE_UNUSED;
     enum machine_mode mode ATTRIBUTE_UNUSED;
     int ignore ATTRIBUTE_UNUSED;
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  tree arglist = TREE_OPERAND (exp, 1);
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl);
  struct builtin_description *d;
  size_t i;
  rtx ret;
  bool success;
  
  /* APPLE LOCAL begin AltiVec */
  if (flag_altivec)
    return old_expand_builtin (exp, target);
  /* APPLE LOCAL end AltiVec */

  if (TARGET_ALTIVEC)
    {
      ret = altivec_expand_builtin (exp, target, &success);

      if (success)
	return ret;
    }
  if (TARGET_SPE)
    {
      ret = spe_expand_builtin (exp, target, &success);

      if (success)
	return ret;
    }

  if (TARGET_ALTIVEC || TARGET_SPE)
    {
      /* Handle simple unary operations.  */
      d = (struct builtin_description *) bdesc_1arg;
      for (i = 0; i < ARRAY_SIZE (bdesc_1arg); i++, d++)
	if (d->code == fcode)
	  return rs6000_expand_unop_builtin (d->icode, arglist, target);

      /* Handle simple binary operations.  */
      d = (struct builtin_description *) bdesc_2arg;
      for (i = 0; i < ARRAY_SIZE (bdesc_2arg); i++, d++)
	if (d->code == fcode)
	  return rs6000_expand_binop_builtin (d->icode, arglist, target);

      /* Handle simple ternary operations.  */
      d = (struct builtin_description *) bdesc_3arg;
      for (i = 0; i < ARRAY_SIZE  (bdesc_3arg); i++, d++)
	if (d->code == fcode)
	  return rs6000_expand_ternop_builtin (d->icode, arglist, target);
    }

  abort ();
  return NULL_RTX;
}

static void
rs6000_init_builtins ()
{
  /* APPLE LOCAL begin AltiVec */
  if (flag_altivec)
    {
      old_init_builtins ();
      return;
    }
  /* APPLE LOCAL end AltiVec */
  if (TARGET_SPE)
    spe_init_builtins ();
  if (TARGET_ALTIVEC)
    altivec_init_builtins ();
  if (TARGET_ALTIVEC || TARGET_SPE)
    rs6000_common_init_builtins ();
}

/* Search through a set of builtins and enable the mask bits.
   DESC is an array of builtins.
   SIZE is the totaly number of builtins.
   START is the builtin enum at which to start.
   END is the builtin enum at which to end.  */
static void
enable_mask_for_builtins (desc, size, start, end)
     struct builtin_description *desc;
     int size;
     enum rs6000_builtins start, end;
{
  int i;

  for (i = 0; i < size; ++i)
    if (desc[i].code == start)
      break;

  if (i == size)
    return;

  for (; i < size; ++i)
    {
      /* Flip all the bits on.  */
      desc[i].mask = target_flags;
      if (desc[i].code == end)
	break;
    }
}

static void
spe_init_builtins ()
{
  tree endlink = void_list_node;
  tree puint_type_node = build_pointer_type (unsigned_type_node);
  tree pushort_type_node = build_pointer_type (short_unsigned_type_node);
  tree pv2si_type_node = build_pointer_type (V2SI_type_node);
  struct builtin_description *d;
  size_t i;

  tree v2si_ftype_4_v2si
    = build_function_type
    (V2SI_type_node,
     tree_cons (NULL_TREE, V2SI_type_node,
		tree_cons (NULL_TREE, V2SI_type_node,
			   tree_cons (NULL_TREE, V2SI_type_node,
				      tree_cons (NULL_TREE, V2SI_type_node,
						 endlink)))));

  tree v2sf_ftype_4_v2sf
    = build_function_type
    (V2SF_type_node,
     tree_cons (NULL_TREE, V2SF_type_node,
		tree_cons (NULL_TREE, V2SF_type_node,
			   tree_cons (NULL_TREE, V2SF_type_node,
				      tree_cons (NULL_TREE, V2SF_type_node,
						 endlink)))));

  tree int_ftype_int_v2si_v2si
    = build_function_type
    (integer_type_node,
     tree_cons (NULL_TREE, integer_type_node,
		tree_cons (NULL_TREE, V2SI_type_node,
			   tree_cons (NULL_TREE, V2SI_type_node,
				      endlink))));

  tree int_ftype_int_v2sf_v2sf
    = build_function_type
    (integer_type_node,
     tree_cons (NULL_TREE, integer_type_node,
		tree_cons (NULL_TREE, V2SF_type_node,
			   tree_cons (NULL_TREE, V2SF_type_node,
				      endlink))));

  tree void_ftype_v2si_puint_int
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, V2SI_type_node,
				      tree_cons (NULL_TREE, puint_type_node,
						 tree_cons (NULL_TREE,
							    integer_type_node,
							    endlink))));

  tree void_ftype_v2si_puint_char
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, V2SI_type_node,
				      tree_cons (NULL_TREE, puint_type_node,
						 tree_cons (NULL_TREE,
							    char_type_node,
							    endlink))));

  tree void_ftype_v2si_pv2si_int
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, V2SI_type_node,
				      tree_cons (NULL_TREE, pv2si_type_node,
						 tree_cons (NULL_TREE,
							    integer_type_node,
							    endlink))));

  tree void_ftype_v2si_pv2si_char
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, V2SI_type_node,
				      tree_cons (NULL_TREE, pv2si_type_node,
						 tree_cons (NULL_TREE,
							    char_type_node,
							    endlink))));

  tree void_ftype_int
    = build_function_type (void_type_node,
			   tree_cons (NULL_TREE, integer_type_node, endlink));

  tree int_ftype_void
    = build_function_type (integer_type_node,
			   tree_cons (NULL_TREE, void_type_node, endlink));

  tree v2si_ftype_pv2si_int
    = build_function_type (V2SI_type_node,
			   tree_cons (NULL_TREE, pv2si_type_node,
				      tree_cons (NULL_TREE, integer_type_node,
						 endlink)));

  tree v2si_ftype_puint_int
    = build_function_type (V2SI_type_node,
			   tree_cons (NULL_TREE, puint_type_node,
				      tree_cons (NULL_TREE, integer_type_node,
						 endlink)));

  tree v2si_ftype_pushort_int
    = build_function_type (V2SI_type_node,
			   tree_cons (NULL_TREE, pushort_type_node,
				      tree_cons (NULL_TREE, integer_type_node,
						 endlink)));

  /* The initialization of the simple binary and unary builtins is
     done in rs6000_common_init_builtins, but we have to enable the
     mask bits here manually because we have run out of `target_flags'
     bits.  We really need to redesign this mask business.  */

  enable_mask_for_builtins ((struct builtin_description *) bdesc_2arg,
			    ARRAY_SIZE (bdesc_2arg),
			    SPE_BUILTIN_EVADDW,
			    SPE_BUILTIN_EVXOR);
  enable_mask_for_builtins ((struct builtin_description *) bdesc_1arg,
			    ARRAY_SIZE (bdesc_1arg),
			    SPE_BUILTIN_EVABS,
			    SPE_BUILTIN_EVSUBFUSIAAW);
  enable_mask_for_builtins ((struct builtin_description *) bdesc_spe_predicates,
			    ARRAY_SIZE (bdesc_spe_predicates),
			    SPE_BUILTIN_EVCMPEQ,
			    SPE_BUILTIN_EVFSTSTLT);
  enable_mask_for_builtins ((struct builtin_description *) bdesc_spe_evsel,
			    ARRAY_SIZE (bdesc_spe_evsel),
			    SPE_BUILTIN_EVSEL_CMPGTS,
			    SPE_BUILTIN_EVSEL_FSTSTEQ);

  /* Initialize irregular SPE builtins.  */
  
  def_builtin (target_flags, "__builtin_spe_mtspefscr", void_ftype_int, SPE_BUILTIN_MTSPEFSCR);
  def_builtin (target_flags, "__builtin_spe_mfspefscr", int_ftype_void, SPE_BUILTIN_MFSPEFSCR);
  def_builtin (target_flags, "__builtin_spe_evstddx", void_ftype_v2si_pv2si_int, SPE_BUILTIN_EVSTDDX);
  def_builtin (target_flags, "__builtin_spe_evstdhx", void_ftype_v2si_pv2si_int, SPE_BUILTIN_EVSTDHX);
  def_builtin (target_flags, "__builtin_spe_evstdwx", void_ftype_v2si_pv2si_int, SPE_BUILTIN_EVSTDWX);
  def_builtin (target_flags, "__builtin_spe_evstwhex", void_ftype_v2si_puint_int, SPE_BUILTIN_EVSTWHEX);
  def_builtin (target_flags, "__builtin_spe_evstwhox", void_ftype_v2si_puint_int, SPE_BUILTIN_EVSTWHOX);
  def_builtin (target_flags, "__builtin_spe_evstwwex", void_ftype_v2si_puint_int, SPE_BUILTIN_EVSTWWEX);
  def_builtin (target_flags, "__builtin_spe_evstwwox", void_ftype_v2si_puint_int, SPE_BUILTIN_EVSTWWOX);
  def_builtin (target_flags, "__builtin_spe_evstdd", void_ftype_v2si_pv2si_char, SPE_BUILTIN_EVSTDD);
  def_builtin (target_flags, "__builtin_spe_evstdh", void_ftype_v2si_pv2si_char, SPE_BUILTIN_EVSTDH);
  def_builtin (target_flags, "__builtin_spe_evstdw", void_ftype_v2si_pv2si_char, SPE_BUILTIN_EVSTDW);
  def_builtin (target_flags, "__builtin_spe_evstwhe", void_ftype_v2si_puint_char, SPE_BUILTIN_EVSTWHE);
  def_builtin (target_flags, "__builtin_spe_evstwho", void_ftype_v2si_puint_char, SPE_BUILTIN_EVSTWHO);
  def_builtin (target_flags, "__builtin_spe_evstwwe", void_ftype_v2si_puint_char, SPE_BUILTIN_EVSTWWE);
  def_builtin (target_flags, "__builtin_spe_evstwwo", void_ftype_v2si_puint_char, SPE_BUILTIN_EVSTWWO);

  /* Loads.  */
  def_builtin (target_flags, "__builtin_spe_evlddx", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDDX);
  def_builtin (target_flags, "__builtin_spe_evldwx", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDWX);
  def_builtin (target_flags, "__builtin_spe_evldhx", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDHX);
  def_builtin (target_flags, "__builtin_spe_evlwhex", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHEX);
  def_builtin (target_flags, "__builtin_spe_evlwhoux", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHOUX);
  def_builtin (target_flags, "__builtin_spe_evlwhosx", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHOSX);
  def_builtin (target_flags, "__builtin_spe_evlwwsplatx", v2si_ftype_puint_int, SPE_BUILTIN_EVLWWSPLATX);
  def_builtin (target_flags, "__builtin_spe_evlwhsplatx", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHSPLATX);
  def_builtin (target_flags, "__builtin_spe_evlhhesplatx", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHESPLATX);
  def_builtin (target_flags, "__builtin_spe_evlhhousplatx", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHOUSPLATX);
  def_builtin (target_flags, "__builtin_spe_evlhhossplatx", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHOSSPLATX);
  def_builtin (target_flags, "__builtin_spe_evldd", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDD);
  def_builtin (target_flags, "__builtin_spe_evldw", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDW);
  def_builtin (target_flags, "__builtin_spe_evldh", v2si_ftype_pv2si_int, SPE_BUILTIN_EVLDH);
  def_builtin (target_flags, "__builtin_spe_evlhhesplat", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHESPLAT);
  def_builtin (target_flags, "__builtin_spe_evlhhossplat", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHOSSPLAT);
  def_builtin (target_flags, "__builtin_spe_evlhhousplat", v2si_ftype_pushort_int, SPE_BUILTIN_EVLHHOUSPLAT);
  def_builtin (target_flags, "__builtin_spe_evlwhe", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHE);
  def_builtin (target_flags, "__builtin_spe_evlwhos", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHOS);
  def_builtin (target_flags, "__builtin_spe_evlwhou", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHOU);
  def_builtin (target_flags, "__builtin_spe_evlwhsplat", v2si_ftype_puint_int, SPE_BUILTIN_EVLWHSPLAT);
  def_builtin (target_flags, "__builtin_spe_evlwwsplat", v2si_ftype_puint_int, SPE_BUILTIN_EVLWWSPLAT);

  /* Predicates.  */
  d = (struct builtin_description *) bdesc_spe_predicates;
  for (i = 0; i < ARRAY_SIZE (bdesc_spe_predicates); ++i, d++)
    {
      tree type;

      switch (insn_data[d->icode].operand[1].mode)
	{
	case V2SImode:
	  type = int_ftype_int_v2si_v2si;
	  break;
	case V2SFmode:
	  type = int_ftype_int_v2sf_v2sf;
	  break;
	default:
	  abort ();
	}

      def_builtin (d->mask, d->name, type, d->code);
    }

  /* Evsel predicates.  */
  d = (struct builtin_description *) bdesc_spe_evsel;
  for (i = 0; i < ARRAY_SIZE (bdesc_spe_evsel); ++i, d++)
    {
      tree type;

      switch (insn_data[d->icode].operand[1].mode)
	{
	case V2SImode:
	  type = v2si_ftype_4_v2si;
	  break;
	case V2SFmode:
	  type = v2sf_ftype_4_v2sf;
	  break;
	default:
	  abort ();
	}

      def_builtin (d->mask, d->name, type, d->code);
    }
}

static void
altivec_init_builtins ()
{
  struct builtin_description *d;
  struct builtin_description_predicates *dp;
  size_t i;
  tree pfloat_type_node = build_pointer_type (float_type_node);
  tree pint_type_node = build_pointer_type (integer_type_node);
  tree pshort_type_node = build_pointer_type (short_integer_type_node);
  tree pchar_type_node = build_pointer_type (char_type_node);

  tree pvoid_type_node = build_pointer_type (void_type_node);

  tree pcfloat_type_node = build_pointer_type (build_qualified_type (float_type_node, TYPE_QUAL_CONST));
  tree pcint_type_node = build_pointer_type (build_qualified_type (integer_type_node, TYPE_QUAL_CONST));
  tree pcshort_type_node = build_pointer_type (build_qualified_type (short_integer_type_node, TYPE_QUAL_CONST));
  tree pcchar_type_node = build_pointer_type (build_qualified_type (char_type_node, TYPE_QUAL_CONST));

  tree pcvoid_type_node = build_pointer_type (build_qualified_type (void_type_node, TYPE_QUAL_CONST));

  tree int_ftype_int_v4si_v4si
    = build_function_type_list (integer_type_node,
				integer_type_node, V4SI_type_node,
				V4SI_type_node, NULL_TREE);
  tree v4sf_ftype_pcfloat
    = build_function_type_list (V4SF_type_node, pcfloat_type_node, NULL_TREE);
  tree void_ftype_pfloat_v4sf
    = build_function_type_list (void_type_node,
				pfloat_type_node, V4SF_type_node, NULL_TREE);
  tree v4si_ftype_pcint
    = build_function_type_list (V4SI_type_node, pcint_type_node, NULL_TREE);
  tree void_ftype_pint_v4si
    = build_function_type_list (void_type_node,
				pint_type_node, V4SI_type_node, NULL_TREE);
  tree v8hi_ftype_pcshort
    = build_function_type_list (V8HI_type_node, pcshort_type_node, NULL_TREE);
  tree void_ftype_pshort_v8hi
    = build_function_type_list (void_type_node,
				pshort_type_node, V8HI_type_node, NULL_TREE);
  tree v16qi_ftype_pcchar
    = build_function_type_list (V16QI_type_node, pcchar_type_node, NULL_TREE);
  tree void_ftype_pchar_v16qi
    = build_function_type_list (void_type_node,
				pchar_type_node, V16QI_type_node, NULL_TREE);
  tree void_ftype_v4si
    = build_function_type_list (void_type_node, V4SI_type_node, NULL_TREE);
  tree v8hi_ftype_void
    = build_function_type (V8HI_type_node, void_list_node);
  tree void_ftype_void
    = build_function_type (void_type_node, void_list_node);
  tree void_ftype_qi
    = build_function_type_list (void_type_node, char_type_node, NULL_TREE);

  tree v16qi_ftype_int_pcvoid
    = build_function_type_list (V16QI_type_node,
				integer_type_node, pcvoid_type_node, NULL_TREE);
  tree v8hi_ftype_int_pcvoid
    = build_function_type_list (V8HI_type_node,
				integer_type_node, pcvoid_type_node, NULL_TREE);
  tree v4si_ftype_int_pcvoid
    = build_function_type_list (V4SI_type_node,
				integer_type_node, pcvoid_type_node, NULL_TREE);

  tree void_ftype_v4si_int_pvoid
    = build_function_type_list (void_type_node,
				V4SI_type_node, integer_type_node,
				pvoid_type_node, NULL_TREE);
  tree void_ftype_v16qi_int_pvoid
    = build_function_type_list (void_type_node,
				V16QI_type_node, integer_type_node,
				pvoid_type_node, NULL_TREE);
  tree void_ftype_v8hi_int_pvoid
    = build_function_type_list (void_type_node,
				V8HI_type_node, integer_type_node,
				pvoid_type_node, NULL_TREE);
  tree int_ftype_int_v8hi_v8hi
    = build_function_type_list (integer_type_node,
				integer_type_node, V8HI_type_node,
				V8HI_type_node, NULL_TREE);
  tree int_ftype_int_v16qi_v16qi
    = build_function_type_list (integer_type_node,
				integer_type_node, V16QI_type_node,
				V16QI_type_node, NULL_TREE);
  tree int_ftype_int_v4sf_v4sf
    = build_function_type_list (integer_type_node,
				integer_type_node, V4SF_type_node,
				V4SF_type_node, NULL_TREE);
  tree v4si_ftype_v4si
    = build_function_type_list (V4SI_type_node, V4SI_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi
    = build_function_type_list (V8HI_type_node, V8HI_type_node, NULL_TREE);
  tree v16qi_ftype_v16qi
    = build_function_type_list (V16QI_type_node, V16QI_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf
    = build_function_type_list (V4SF_type_node, V4SF_type_node, NULL_TREE);
  tree void_ftype_pcvoid_int_char
    = build_function_type_list (void_type_node,
				pcvoid_type_node, integer_type_node,
				char_type_node, NULL_TREE);
  
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_ld_internal_4sf", v4sf_ftype_pcfloat,
	       ALTIVEC_BUILTIN_LD_INTERNAL_4sf);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_st_internal_4sf", void_ftype_pfloat_v4sf,
	       ALTIVEC_BUILTIN_ST_INTERNAL_4sf);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_ld_internal_4si", v4si_ftype_pcint,
	       ALTIVEC_BUILTIN_LD_INTERNAL_4si);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_st_internal_4si", void_ftype_pint_v4si,
	       ALTIVEC_BUILTIN_ST_INTERNAL_4si);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_ld_internal_8hi", v8hi_ftype_pcshort,
	       ALTIVEC_BUILTIN_LD_INTERNAL_8hi);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_st_internal_8hi", void_ftype_pshort_v8hi,
	       ALTIVEC_BUILTIN_ST_INTERNAL_8hi);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_ld_internal_16qi", v16qi_ftype_pcchar,
	       ALTIVEC_BUILTIN_LD_INTERNAL_16qi);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_st_internal_16qi", void_ftype_pchar_v16qi,
	       ALTIVEC_BUILTIN_ST_INTERNAL_16qi);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_mtvscr", void_ftype_v4si, ALTIVEC_BUILTIN_MTVSCR);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_mfvscr", v8hi_ftype_void, ALTIVEC_BUILTIN_MFVSCR);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_dssall", void_ftype_void, ALTIVEC_BUILTIN_DSSALL);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_dss", void_ftype_qi, ALTIVEC_BUILTIN_DSS);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvsl", v16qi_ftype_int_pcvoid, ALTIVEC_BUILTIN_LVSL);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvsr", v16qi_ftype_int_pcvoid, ALTIVEC_BUILTIN_LVSR);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvebx", v16qi_ftype_int_pcvoid, ALTIVEC_BUILTIN_LVEBX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvehx", v8hi_ftype_int_pcvoid, ALTIVEC_BUILTIN_LVEHX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvewx", v4si_ftype_int_pcvoid, ALTIVEC_BUILTIN_LVEWX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvxl", v4si_ftype_int_pcvoid, ALTIVEC_BUILTIN_LVXL);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_lvx", v4si_ftype_int_pcvoid, ALTIVEC_BUILTIN_LVX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_stvx", void_ftype_v4si_int_pvoid, ALTIVEC_BUILTIN_STVX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_stvewx", void_ftype_v4si_int_pvoid, ALTIVEC_BUILTIN_STVEWX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_stvxl", void_ftype_v4si_int_pvoid, ALTIVEC_BUILTIN_STVXL);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_stvebx", void_ftype_v16qi_int_pvoid, ALTIVEC_BUILTIN_STVEBX);
  def_builtin (MASK_ALTIVEC, "__builtin_altivec_stvehx", void_ftype_v8hi_int_pvoid, ALTIVEC_BUILTIN_STVEHX);

  /* Add the DST variants.  */
  d = (struct builtin_description *) bdesc_dst;
  for (i = 0; i < ARRAY_SIZE (bdesc_dst); i++, d++)
    def_builtin (d->mask, d->name, void_ftype_pcvoid_int_char, d->code);

  /* Initialize the predicates.  */
  dp = (struct builtin_description_predicates *) bdesc_altivec_preds;
  for (i = 0; i < ARRAY_SIZE (bdesc_altivec_preds); i++, dp++)
    {
      enum machine_mode mode1;
      tree type;

      mode1 = insn_data[dp->icode].operand[1].mode;

      switch (mode1)
	{
	case V4SImode:
	  type = int_ftype_int_v4si_v4si;
	  break;
	case V8HImode:
	  type = int_ftype_int_v8hi_v8hi;
	  break;
	case V16QImode:
	  type = int_ftype_int_v16qi_v16qi;
	  break;
	case V4SFmode:
	  type = int_ftype_int_v4sf_v4sf;
	  break;
	default:
	  abort ();
	}
      
      def_builtin (dp->mask, dp->name, type, dp->code);
    }

  /* Initialize the abs* operators.  */
  d = (struct builtin_description *) bdesc_abs;
  for (i = 0; i < ARRAY_SIZE (bdesc_abs); i++, d++)
    {
      enum machine_mode mode0;
      tree type;

      mode0 = insn_data[d->icode].operand[0].mode;

      switch (mode0)
	{
	case V4SImode:
	  type = v4si_ftype_v4si;
	  break;
	case V8HImode:
	  type = v8hi_ftype_v8hi;
	  break;
	case V16QImode:
	  type = v16qi_ftype_v16qi;
	  break;
	case V4SFmode:
	  type = v4sf_ftype_v4sf;
	  break;
	default:
	  abort ();
	}
      
      def_builtin (d->mask, d->name, type, d->code);
    }
}

static void
rs6000_common_init_builtins ()
{
  struct builtin_description *d;
  size_t i;

  tree v4sf_ftype_v4sf_v4sf_v16qi
    = build_function_type_list (V4SF_type_node,
				V4SF_type_node, V4SF_type_node,
				V16QI_type_node, NULL_TREE);
  tree v4si_ftype_v4si_v4si_v16qi
    = build_function_type_list (V4SI_type_node,
				V4SI_type_node, V4SI_type_node,
				V16QI_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi_v8hi_v16qi
    = build_function_type_list (V8HI_type_node,
				V8HI_type_node, V8HI_type_node,
				V16QI_type_node, NULL_TREE);
  tree v16qi_ftype_v16qi_v16qi_v16qi
    = build_function_type_list (V16QI_type_node,
				V16QI_type_node, V16QI_type_node,
				V16QI_type_node, NULL_TREE);
  tree v4si_ftype_char
    = build_function_type_list (V4SI_type_node, char_type_node, NULL_TREE);
  tree v8hi_ftype_char
    = build_function_type_list (V8HI_type_node, char_type_node, NULL_TREE);
  tree v16qi_ftype_char
    = build_function_type_list (V16QI_type_node, char_type_node, NULL_TREE);
  tree v8hi_ftype_v16qi
    = build_function_type_list (V8HI_type_node, V16QI_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf
    = build_function_type_list (V4SF_type_node, V4SF_type_node, NULL_TREE);

  tree v2si_ftype_v2si_v2si
    = build_function_type_list (V2SI_type_node,
				V2SI_type_node, V2SI_type_node, NULL_TREE);

  tree v2sf_ftype_v2sf_v2sf
    = build_function_type_list (V2SF_type_node,
				V2SF_type_node, V2SF_type_node, NULL_TREE);

  tree v2si_ftype_int_int
    = build_function_type_list (V2SI_type_node,
				integer_type_node, integer_type_node,
				NULL_TREE);

  tree v2si_ftype_v2si
    = build_function_type_list (V2SI_type_node, V2SI_type_node, NULL_TREE);

  tree v2sf_ftype_v2sf
    = build_function_type_list (V2SF_type_node,
				V2SF_type_node, NULL_TREE);
  
  tree v2sf_ftype_v2si
    = build_function_type_list (V2SF_type_node,
				V2SI_type_node, NULL_TREE);

  tree v2si_ftype_v2sf
    = build_function_type_list (V2SI_type_node,
				V2SF_type_node, NULL_TREE);

  tree v2si_ftype_v2si_char
    = build_function_type_list (V2SI_type_node,
				V2SI_type_node, char_type_node, NULL_TREE);

  tree v2si_ftype_int_char
    = build_function_type_list (V2SI_type_node,
				integer_type_node, char_type_node, NULL_TREE);

  tree v2si_ftype_char
    = build_function_type_list (V2SI_type_node, char_type_node, NULL_TREE);

  tree int_ftype_int_int
    = build_function_type_list (integer_type_node,
				integer_type_node, integer_type_node,
				NULL_TREE);

  tree v4si_ftype_v4si_v4si
    = build_function_type_list (V4SI_type_node,
				V4SI_type_node, V4SI_type_node, NULL_TREE);
  tree v4sf_ftype_v4si_char
    = build_function_type_list (V4SF_type_node,
				V4SI_type_node, char_type_node, NULL_TREE);
  tree v4si_ftype_v4sf_char
    = build_function_type_list (V4SI_type_node,
				V4SF_type_node, char_type_node, NULL_TREE);
  tree v4si_ftype_v4si_char
    = build_function_type_list (V4SI_type_node,
				V4SI_type_node, char_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi_char
    = build_function_type_list (V8HI_type_node,
				V8HI_type_node, char_type_node, NULL_TREE);
  tree v16qi_ftype_v16qi_char
    = build_function_type_list (V16QI_type_node,
				V16QI_type_node, char_type_node, NULL_TREE);
  tree v16qi_ftype_v16qi_v16qi_char
    = build_function_type_list (V16QI_type_node,
				V16QI_type_node, V16QI_type_node,
				char_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi_v8hi_char
    = build_function_type_list (V8HI_type_node,
				V8HI_type_node, V8HI_type_node,
				char_type_node, NULL_TREE);
  tree v4si_ftype_v4si_v4si_char
    = build_function_type_list (V4SI_type_node,
				V4SI_type_node, V4SI_type_node,
				char_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf_v4sf_char
    = build_function_type_list (V4SF_type_node,
				V4SF_type_node, V4SF_type_node,
				char_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf_v4sf
    = build_function_type_list (V4SF_type_node,
				V4SF_type_node, V4SF_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf_v4sf_v4si
    = build_function_type_list (V4SF_type_node,
				V4SF_type_node, V4SF_type_node,
				V4SI_type_node, NULL_TREE);
  tree v4sf_ftype_v4sf_v4sf_v4sf
    = build_function_type_list (V4SF_type_node,
				V4SF_type_node, V4SF_type_node,
				V4SF_type_node, NULL_TREE);
  tree v4si_ftype_v4si_v4si_v4si 
    = build_function_type_list (V4SI_type_node,
				V4SI_type_node, V4SI_type_node,
				V4SI_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi_v8hi
    = build_function_type_list (V8HI_type_node,
				V8HI_type_node, V8HI_type_node, NULL_TREE);
  tree v8hi_ftype_v8hi_v8hi_v8hi
    = build_function_type_list (V8HI_type_node,
				V8HI_type_node, V8HI_type_node,
				V8HI_type_node, NULL_TREE);
 tree v4si_ftype_v8hi_v8hi_v4si
    = build_function_type_list (V4SI_type_node,
				V8HI_type_node, V8HI_type_node,
				V4SI_type_node, NULL_TREE);
 tree v4si_ftype_v16qi_v16qi_v4si
    = build_function_type_list (V4SI_type_node,
				V16QI_type_node, V16QI_type_node,
				V4SI_type_node, NULL_TREE);
  tree v16qi_ftype_v16qi_v16qi
    = build_function_type_list (V16QI_type_node,
				V16QI_type_node, V16QI_type_node, NULL_TREE);
  tree v4si_ftype_v4sf_v4sf
    = build_function_type_list (V4SI_type_node,
				V4SF_type_node, V4SF_type_node, NULL_TREE);
  tree v8hi_ftype_v16qi_v16qi
    = build_function_type_list (V8HI_type_node,
				V16QI_type_node, V16QI_type_node, NULL_TREE);
  tree v4si_ftype_v8hi_v8hi
    = build_function_type_list (V4SI_type_node,
				V8HI_type_node, V8HI_type_node, NULL_TREE);
  tree v8hi_ftype_v4si_v4si
    = build_function_type_list (V8HI_type_node,
				V4SI_type_node, V4SI_type_node, NULL_TREE);
  tree v16qi_ftype_v8hi_v8hi
    = build_function_type_list (V16QI_type_node,
				V8HI_type_node, V8HI_type_node, NULL_TREE);
  tree v4si_ftype_v16qi_v4si
    = build_function_type_list (V4SI_type_node,
				V16QI_type_node, V4SI_type_node, NULL_TREE);
  tree v4si_ftype_v16qi_v16qi
    = build_function_type_list (V4SI_type_node,
				V16QI_type_node, V16QI_type_node, NULL_TREE);
  tree v4si_ftype_v8hi_v4si
    = build_function_type_list (V4SI_type_node,
				V8HI_type_node, V4SI_type_node, NULL_TREE);
  tree v4si_ftype_v8hi
    = build_function_type_list (V4SI_type_node, V8HI_type_node, NULL_TREE);
  tree int_ftype_v4si_v4si
    = build_function_type_list (integer_type_node,
				V4SI_type_node, V4SI_type_node, NULL_TREE);
  tree int_ftype_v4sf_v4sf
    = build_function_type_list (integer_type_node,
				V4SF_type_node, V4SF_type_node, NULL_TREE);
  tree int_ftype_v16qi_v16qi
    = build_function_type_list (integer_type_node,
				V16QI_type_node, V16QI_type_node, NULL_TREE);
  tree int_ftype_v8hi_v8hi
    = build_function_type_list (integer_type_node,
				V8HI_type_node, V8HI_type_node, NULL_TREE);

  /* Add the simple ternary operators.  */
  d = (struct builtin_description *) bdesc_3arg;
  for (i = 0; i < ARRAY_SIZE (bdesc_3arg); i++, d++)
    {
      
      enum machine_mode mode0, mode1, mode2, mode3;
      tree type;

      if (d->name == 0 || d->icode == CODE_FOR_nothing)
	continue;
      
      mode0 = insn_data[d->icode].operand[0].mode;
      mode1 = insn_data[d->icode].operand[1].mode;
      mode2 = insn_data[d->icode].operand[2].mode;
      mode3 = insn_data[d->icode].operand[3].mode;
      
      /* When all four are of the same mode.  */
      if (mode0 == mode1 && mode1 == mode2 && mode2 == mode3)
	{
	  switch (mode0)
	    {
	    case V4SImode:
	      type = v4si_ftype_v4si_v4si_v4si;
	      break;
	    case V4SFmode:
	      type = v4sf_ftype_v4sf_v4sf_v4sf;
	      break;
	    case V8HImode:
	      type = v8hi_ftype_v8hi_v8hi_v8hi;
	      break;	      
	    case V16QImode:
	      type = v16qi_ftype_v16qi_v16qi_v16qi;
	      break;	      
	    default:
	      abort();	      
	    }
	}
      else if (mode0 == mode1 && mode1 == mode2 && mode3 == V16QImode)
        {
	  switch (mode0)
	    {
	    case V4SImode:
	      type = v4si_ftype_v4si_v4si_v16qi;
	      break;
	    case V4SFmode:
	      type = v4sf_ftype_v4sf_v4sf_v16qi;
	      break;
	    case V8HImode:
	      type = v8hi_ftype_v8hi_v8hi_v16qi;
	      break;	      
	    case V16QImode:
	      type = v16qi_ftype_v16qi_v16qi_v16qi;
	      break;	      
	    default:
	      abort();	      
	    }
	}
      else if (mode0 == V4SImode && mode1 == V16QImode && mode2 == V16QImode 
	       && mode3 == V4SImode)
	type = v4si_ftype_v16qi_v16qi_v4si;
      else if (mode0 == V4SImode && mode1 == V8HImode && mode2 == V8HImode 
	       && mode3 == V4SImode)
	type = v4si_ftype_v8hi_v8hi_v4si;
      else if (mode0 == V4SFmode && mode1 == V4SFmode && mode2 == V4SFmode 
	       && mode3 == V4SImode)
	type = v4sf_ftype_v4sf_v4sf_v4si;

      /* vchar, vchar, vchar, 4 bit literal.  */
      else if (mode0 == V16QImode && mode1 == mode0 && mode2 == mode0
	       && mode3 == QImode)
	type = v16qi_ftype_v16qi_v16qi_char;

      /* vshort, vshort, vshort, 4 bit literal.  */
      else if (mode0 == V8HImode && mode1 == mode0 && mode2 == mode0
	       && mode3 == QImode)
	type = v8hi_ftype_v8hi_v8hi_char;

      /* vint, vint, vint, 4 bit literal.  */
      else if (mode0 == V4SImode && mode1 == mode0 && mode2 == mode0
	       && mode3 == QImode)
	type = v4si_ftype_v4si_v4si_char;

      /* vfloat, vfloat, vfloat, 4 bit literal.  */
      else if (mode0 == V4SFmode && mode1 == mode0 && mode2 == mode0
	       && mode3 == QImode)
	type = v4sf_ftype_v4sf_v4sf_char;

      else
	abort ();

      def_builtin (d->mask, d->name, type, d->code);
    }

  /* Add the simple binary operators.  */
  d = (struct builtin_description *) bdesc_2arg;
  for (i = 0; i < ARRAY_SIZE (bdesc_2arg); i++, d++)
    {
      enum machine_mode mode0, mode1, mode2;
      tree type;

      if (d->name == 0 || d->icode == CODE_FOR_nothing)
	continue;
      
      mode0 = insn_data[d->icode].operand[0].mode;
      mode1 = insn_data[d->icode].operand[1].mode;
      mode2 = insn_data[d->icode].operand[2].mode;

      /* When all three operands are of the same mode.  */
      if (mode0 == mode1 && mode1 == mode2)
	{
	  switch (mode0)
	    {
	    case V4SFmode:
	      type = v4sf_ftype_v4sf_v4sf;
	      break;
	    case V4SImode:
	      type = v4si_ftype_v4si_v4si;
	      break;
	    case V16QImode:
	      type = v16qi_ftype_v16qi_v16qi;
	      break;
	    case V8HImode:
	      type = v8hi_ftype_v8hi_v8hi;
	      break;
	    case V2SImode:
	      type = v2si_ftype_v2si_v2si;
	      break;
	    case V2SFmode:
	      type = v2sf_ftype_v2sf_v2sf;
	      break;
	    case SImode:
	      type = int_ftype_int_int;
	      break;
	    default:
	      abort ();
	    }
	}

      /* A few other combos we really don't want to do manually.  */

      /* vint, vfloat, vfloat.  */
      else if (mode0 == V4SImode && mode1 == V4SFmode && mode2 == V4SFmode)
	type = v4si_ftype_v4sf_v4sf;

      /* vshort, vchar, vchar.  */
      else if (mode0 == V8HImode && mode1 == V16QImode && mode2 == V16QImode)
	type = v8hi_ftype_v16qi_v16qi;

      /* vint, vshort, vshort.  */
      else if (mode0 == V4SImode && mode1 == V8HImode && mode2 == V8HImode)
	type = v4si_ftype_v8hi_v8hi;

      /* vshort, vint, vint.  */
      else if (mode0 == V8HImode && mode1 == V4SImode && mode2 == V4SImode)
	type = v8hi_ftype_v4si_v4si;

      /* vchar, vshort, vshort.  */
      else if (mode0 == V16QImode && mode1 == V8HImode && mode2 == V8HImode)
	type = v16qi_ftype_v8hi_v8hi;

      /* vint, vchar, vint.  */
      else if (mode0 == V4SImode && mode1 == V16QImode && mode2 == V4SImode)
	type = v4si_ftype_v16qi_v4si;

      /* vint, vchar, vchar.  */
      else if (mode0 == V4SImode && mode1 == V16QImode && mode2 == V16QImode)
	type = v4si_ftype_v16qi_v16qi;

      /* vint, vshort, vint.  */
      else if (mode0 == V4SImode && mode1 == V8HImode && mode2 == V4SImode)
	type = v4si_ftype_v8hi_v4si;
      
      /* vint, vint, 5 bit literal.  */
      else if (mode0 == V4SImode && mode1 == V4SImode && mode2 == QImode)
	type = v4si_ftype_v4si_char;
      
      /* vshort, vshort, 5 bit literal.  */
      else if (mode0 == V8HImode && mode1 == V8HImode && mode2 == QImode)
	type = v8hi_ftype_v8hi_char;
      
      /* vchar, vchar, 5 bit literal.  */
      else if (mode0 == V16QImode && mode1 == V16QImode && mode2 == QImode)
	type = v16qi_ftype_v16qi_char;

      /* vfloat, vint, 5 bit literal.  */
      else if (mode0 == V4SFmode && mode1 == V4SImode && mode2 == QImode)
	type = v4sf_ftype_v4si_char;
      
      /* vint, vfloat, 5 bit literal.  */
      else if (mode0 == V4SImode && mode1 == V4SFmode && mode2 == QImode)
	type = v4si_ftype_v4sf_char;

      else if (mode0 == V2SImode && mode1 == SImode && mode2 == SImode)
	type = v2si_ftype_int_int;

      else if (mode0 == V2SImode && mode1 == V2SImode && mode2 == QImode)
	type = v2si_ftype_v2si_char;

      else if (mode0 == V2SImode && mode1 == SImode && mode2 == QImode)
	type = v2si_ftype_int_char;

      /* int, x, x.  */
      else if (mode0 == SImode)
	{
	  switch (mode1)
	    {
	    case V4SImode:
	      type = int_ftype_v4si_v4si;
	      break;
	    case V4SFmode:
	      type = int_ftype_v4sf_v4sf;
	      break;
	    case V16QImode:
	      type = int_ftype_v16qi_v16qi;
	      break;
	    case V8HImode:
	      type = int_ftype_v8hi_v8hi;
	      break;
	    default:
	      abort ();
	    }
	}

      else
	abort ();

      def_builtin (d->mask, d->name, type, d->code);
    }

  /* Add the simple unary operators.  */
  d = (struct builtin_description *) bdesc_1arg;
  for (i = 0; i < ARRAY_SIZE (bdesc_1arg); i++, d++)
    {
      enum machine_mode mode0, mode1;
      tree type;

      if (d->name == 0 || d->icode == CODE_FOR_nothing)
	continue;
      
      mode0 = insn_data[d->icode].operand[0].mode;
      mode1 = insn_data[d->icode].operand[1].mode;

      if (mode0 == V4SImode && mode1 == QImode)
        type = v4si_ftype_char;
      else if (mode0 == V8HImode && mode1 == QImode)
        type = v8hi_ftype_char;
      else if (mode0 == V16QImode && mode1 == QImode)
        type = v16qi_ftype_char;
      else if (mode0 == V4SFmode && mode1 == V4SFmode)
	type = v4sf_ftype_v4sf;
      else if (mode0 == V8HImode && mode1 == V16QImode)
	type = v8hi_ftype_v16qi;
      else if (mode0 == V4SImode && mode1 == V8HImode)
	type = v4si_ftype_v8hi;
      else if (mode0 == V2SImode && mode1 == V2SImode)
	type = v2si_ftype_v2si;
      else if (mode0 == V2SFmode && mode1 == V2SFmode)
	type = v2sf_ftype_v2sf;
      else if (mode0 == V2SFmode && mode1 == V2SImode)
	type = v2sf_ftype_v2si;
      else if (mode0 == V2SImode && mode1 == V2SFmode)
	type = v2si_ftype_v2sf;
      else if (mode0 == V2SImode && mode1 == QImode)
	type = v2si_ftype_char;
      else
	abort ();

      def_builtin (d->mask, d->name, type, d->code);
    }
}

/* APPLE LOCAL MEM_OFFSET setting */
/* Generate a memory reference for expand_block_move, copying volatile,
   and other bits from an original memory reference.  Update the
   MEM_OFFSET to reflect the supplied offset. */

static rtx
/* APPLE LOCAL MEM_OFFSET setting */
expand_block_move_mem (mode, addr, orig_mem, offset)
     enum machine_mode mode;
     rtx addr;
     rtx orig_mem;
     /* APPLE LOCAL MEM_OFFSET setting */
     int offset;
{
  rtx mem = gen_rtx_MEM (mode, addr);

  MEM_COPY_ATTRIBUTES (mem, orig_mem);
  /* APPLE LOCAL begin MEM_OFFSET setting */
  if (MEM_OFFSET (orig_mem) && offset != 0)
     set_mem_offset (mem, GEN_INT (INTVAL (MEM_OFFSET (orig_mem)) + offset));
  /* APPLE LOCAL end MEM_OFFSET setting */
  return mem;
}


/* Expand a block move operation, and return 1 if successful.  Return 0
   if we should let the compiler generate normal code.

   operands[0] is the destination
   operands[1] is the source
   operands[2] is the length
   operands[3] is the alignment */

#define MAX_MOVE_REG 4

int
expand_block_move (operands)
     rtx operands[];
{
  rtx orig_dest = operands[0];
  rtx orig_src	= operands[1];
  rtx bytes_rtx	= operands[2];
  rtx align_rtx = operands[3];
  int constp	= (GET_CODE (bytes_rtx) == CONST_INT);
  int align;
  int bytes;
  int offset;
  int move_bytes;

  /* If this is not a fixed size move, just call memcpy */
  if (! constp)
    return 0;

  /* If this is not a fixed size alignment, abort */
  if (GET_CODE (align_rtx) != CONST_INT)
    abort ();
  align = INTVAL (align_rtx);

  /* Anything to move? */
  bytes = INTVAL (bytes_rtx);
  if (bytes <= 0)
    return 1;

  /* store_one_arg depends on expand_block_move to handle at least the size of
     reg_parm_stack_space.  */	
  if (bytes > (TARGET_POWERPC64 ? 64 : 32))
    return 0;

  if (TARGET_STRING)	/* string instructions are available */
    {
      for (offset = 0; bytes > 0; offset += move_bytes, bytes -= move_bytes)
	{
	  union {
	    rtx (*movstrsi) PARAMS ((rtx, rtx, rtx, rtx));
	    rtx (*mov) PARAMS ((rtx, rtx));
	  } gen_func;
	  enum machine_mode mode = BLKmode;
	  rtx src, dest;

	  if (bytes > 24		/* move up to 32 bytes at a time */
	      && ! fixed_regs[5]
	      && ! fixed_regs[6]
	      && ! fixed_regs[7]
	      && ! fixed_regs[8]
	      && ! fixed_regs[9]
	      && ! fixed_regs[10]
	      && ! fixed_regs[11]
	      && ! fixed_regs[12])
	    {
	      move_bytes = (bytes > 32) ? 32 : bytes;
	      gen_func.movstrsi = gen_movstrsi_8reg;
	    }
	  else if (bytes > 16	/* move up to 24 bytes at a time */
		   && ! fixed_regs[5]
		   && ! fixed_regs[6]
		   && ! fixed_regs[7]
		   && ! fixed_regs[8]
		   && ! fixed_regs[9]
		   && ! fixed_regs[10])
	    {
	      move_bytes = (bytes > 24) ? 24 : bytes;
	      gen_func.movstrsi = gen_movstrsi_6reg;
	    }
	  else if (bytes > 8	/* move up to 16 bytes at a time */
		   && ! fixed_regs[5]
		   && ! fixed_regs[6]
		   && ! fixed_regs[7]
		   && ! fixed_regs[8])
	    {
	      move_bytes = (bytes > 16) ? 16 : bytes;
	      gen_func.movstrsi = gen_movstrsi_4reg;
	    }
	  else if (bytes >= 8 && TARGET_POWERPC64
		   /* 64-bit loads and stores require word-aligned
                      displacements.  */
		   && (align >= 8 || (! STRICT_ALIGNMENT && align >= 4)))
	    {
	      move_bytes = 8;
	      mode = DImode;
	      gen_func.mov = gen_movdi;
	    }
	  else if (bytes > 4 && !TARGET_POWERPC64)
	    {			/* move up to 8 bytes at a time */
	      move_bytes = (bytes > 8) ? 8 : bytes;
	      gen_func.movstrsi = gen_movstrsi_2reg;
	    }
	  else if (bytes >= 4 && (align >= 4 || ! STRICT_ALIGNMENT))
	    {			/* move 4 bytes */
	      move_bytes = 4;
	      mode = SImode;
	      gen_func.mov = gen_movsi;
	    }
	  else if (bytes == 2 && (align >= 2 || ! STRICT_ALIGNMENT))
	    {			/* move 2 bytes */
	      move_bytes = 2;
	      mode = HImode;
	      gen_func.mov = gen_movhi;
	    }
	  else if (bytes == 1)	/* move 1 byte */
	    {
	      move_bytes = 1;
	      mode = QImode;
	      gen_func.mov = gen_movqi;
	    }
	  else
	    {			/* move up to 4 bytes at a time */
	      move_bytes = (bytes > 4) ? 4 : bytes;
	      gen_func.movstrsi = gen_movstrsi_1reg;
	    }

	  src = adjust_address (orig_src, mode, offset);
	  dest = adjust_address (orig_dest, mode, offset);

	  if (mode == BLKmode)
	    {
	      /* Move the address into scratch registers.  The movstrsi
		 patterns require zero offset.  */
	      if (!REG_P (XEXP (src, 0)))
		{
		  rtx src_reg = copy_addr_to_reg (XEXP (src, 0));
		  src = replace_equiv_address (src, src_reg);
		}
	      set_mem_size (src, GEN_INT (move_bytes));

	      if (!REG_P (XEXP (dest, 0)))
		{
		  rtx dest_reg = copy_addr_to_reg (XEXP (dest, 0));
		  dest = replace_equiv_address (dest, dest_reg);
		}
	      set_mem_size (dest, GEN_INT (move_bytes));

	      emit_insn ((*gen_func.movstrsi) (dest, src,
					       GEN_INT (move_bytes & 31),
					       align_rtx));
	    }
	  else
	    {
	      rtx tmp_reg = gen_reg_rtx (mode);

	      emit_insn ((*gen_func.mov) (tmp_reg, src));
	      emit_insn ((*gen_func.mov) (dest, tmp_reg));
	    }
	}
    }

  else			/* string instructions not available */
    {
      rtx stores[MAX_MOVE_REG];
      int num_reg = 0;
      int i;

      for (offset = 0; bytes > 0; offset += move_bytes, bytes -= move_bytes)
	{
	  rtx (*gen_mov_func) PARAMS ((rtx, rtx));
	  enum machine_mode mode;
	  rtx src, dest, tmp_reg;

	  /* Generate the appropriate load and store, saving the stores
	     for later.  */
	  if (bytes >= 8 && TARGET_POWERPC64
	      /* 64-bit loads and stores require word-aligned
                 displacements.  */
	      && (align >= 8 || (! STRICT_ALIGNMENT && align >= 4)))
	    {
	      move_bytes = 8;
	      mode = DImode;
	      gen_mov_func = gen_movdi;
	    }
	  else if (bytes >= 4 && (align >= 4 || ! STRICT_ALIGNMENT))
	    {
	      move_bytes = 4;
	      mode = SImode;
	      gen_mov_func = gen_movsi;
	    }
	  else if (bytes >= 2 && (align >= 2 || ! STRICT_ALIGNMENT))
	    {
	      move_bytes = 2;
	      mode = HImode;
	      gen_mov_func = gen_movhi;
	    }
	  else
	    {
	      move_bytes = 1;
	      mode = QImode;
	      gen_mov_func = gen_movqi;
	    }

	  src = adjust_address (orig_src, mode, offset);
	  dest = adjust_address (orig_dest, mode, offset);
	  tmp_reg = gen_reg_rtx (mode);

	  emit_insn ((*gen_mov_func) (tmp_reg, src));
	  stores[num_reg++] = (*gen_mov_func) (dest, tmp_reg);

	  if (num_reg >= MAX_MOVE_REG)
	    {
	      for (i = 0; i < num_reg; i++)
		emit_insn (stores[i]);
	      num_reg = 0;
	    }
	}

      for (i = 0; i < num_reg; i++)
	emit_insn (stores[i]);
    }

  return 1;
}


/* Return 1 if OP is a load multiple operation.  It is known to be a
   PARALLEL and the first section will be tested.  */

int
load_multiple_operation (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  int count = XVECLEN (op, 0);
  unsigned int dest_regno;
  rtx src_addr;
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != REG
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != MEM)
    return 0;

  dest_regno = REGNO (SET_DEST (XVECEXP (op, 0, 0)));
  src_addr = XEXP (SET_SRC (XVECEXP (op, 0, 0)), 0);

  for (i = 1; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i);

      if (GET_CODE (elt) != SET
	  || GET_CODE (SET_DEST (elt)) != REG
	  || GET_MODE (SET_DEST (elt)) != SImode
	  || REGNO (SET_DEST (elt)) != dest_regno + i
	  || GET_CODE (SET_SRC (elt)) != MEM
	  || GET_MODE (SET_SRC (elt)) != SImode
	  || GET_CODE (XEXP (SET_SRC (elt), 0)) != PLUS
	  || ! rtx_equal_p (XEXP (XEXP (SET_SRC (elt), 0), 0), src_addr)
	  || GET_CODE (XEXP (XEXP (SET_SRC (elt), 0), 1)) != CONST_INT
	  || INTVAL (XEXP (XEXP (SET_SRC (elt), 0), 1)) != i * 4)
	return 0;
    }

  return 1;
}

/* Similar, but tests for store multiple.  Here, the second vector element
   is a CLOBBER.  It will be tested later.  */

int
store_multiple_operation (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  int count = XVECLEN (op, 0) - 1;
  unsigned int src_regno;
  rtx dest_addr;
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != MEM
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != REG)
    return 0;

  src_regno = REGNO (SET_SRC (XVECEXP (op, 0, 0)));
  dest_addr = XEXP (SET_DEST (XVECEXP (op, 0, 0)), 0);

  for (i = 1; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i + 1);

      if (GET_CODE (elt) != SET
	  || GET_CODE (SET_SRC (elt)) != REG
	  || GET_MODE (SET_SRC (elt)) != SImode
	  || REGNO (SET_SRC (elt)) != src_regno + i
	  || GET_CODE (SET_DEST (elt)) != MEM
	  || GET_MODE (SET_DEST (elt)) != SImode
	  || GET_CODE (XEXP (SET_DEST (elt), 0)) != PLUS
	  || ! rtx_equal_p (XEXP (XEXP (SET_DEST (elt), 0), 0), dest_addr)
	  || GET_CODE (XEXP (XEXP (SET_DEST (elt), 0), 1)) != CONST_INT
	  || INTVAL (XEXP (XEXP (SET_DEST (elt), 0), 1)) != i * 4)
	return 0;
    }

  return 1;
}

/* Return a string to perform a load_multiple operation.
   operands[0] is the vector.
   operands[1] is the source address.
   operands[2] is the first destination register.  */

const char *
rs6000_output_load_multiple (operands)
     rtx operands[3];
{
  /* We have to handle the case where the pseudo used to contain the address
     is assigned to one of the output registers.  */
  int i, j;
  int words = XVECLEN (operands[0], 0);
  rtx xop[10];

  if (XVECLEN (operands[0], 0) == 1)
    return "{l|lwz} %2,0(%1)";

  for (i = 0; i < words; i++)
    if (refers_to_regno_p (REGNO (operands[2]) + i,
			   REGNO (operands[2]) + i + 1, operands[1], 0))
      {
	if (i == words-1)
	  {
	    xop[0] = GEN_INT (4 * (words-1));
	    xop[1] = operands[1];
	    xop[2] = operands[2];
	    output_asm_insn ("{lsi|lswi} %2,%1,%0\n\t{l|lwz} %1,%0(%1)", xop);
	    return "";
	  }
	else if (i == 0)
	  {
	    xop[0] = GEN_INT (4 * (words-1));
	    xop[1] = operands[1];
	    xop[2] = gen_rtx_REG (SImode, REGNO (operands[2]) + 1);
	    output_asm_insn ("{cal %1,4(%1)|addi %1,%1,4}\n\t{lsi|lswi} %2,%1,%0\n\t{l|lwz} %1,-4(%1)", xop);
	    return "";
	  }
	else
	  {
	    for (j = 0; j < words; j++)
	      if (j != i)
		{
		  xop[0] = GEN_INT (j * 4);
		  xop[1] = operands[1];
		  xop[2] = gen_rtx_REG (SImode, REGNO (operands[2]) + j);
		  output_asm_insn ("{l|lwz} %2,%0(%1)", xop);
		}
	    xop[0] = GEN_INT (i * 4);
	    xop[1] = operands[1];
	    output_asm_insn ("{l|lwz} %1,%0(%1)", xop);
	    return "";
	  }
      }

  return "{lsi|lswi} %2,%1,%N0";
}

/* Return 1 for a parallel vrsave operation.  */

int
vrsave_operation (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  int count = XVECLEN (op, 0);
  unsigned int dest_regno, src_regno;
  int i;

  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != REG
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != UNSPEC_VOLATILE)
    return 0;

  dest_regno = REGNO (SET_DEST (XVECEXP (op, 0, 0)));
  src_regno  = REGNO (SET_SRC (XVECEXP (op, 0, 0)));

  if (dest_regno != VRSAVE_REGNO
      && src_regno != VRSAVE_REGNO)
    return 0;

  for (i = 1; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i);

      if (GET_CODE (elt) != CLOBBER
	  && GET_CODE (elt) != SET)
	return 0;
    }

  return 1;
}

/* APPLE LOCAL begin optimized mfcr instruction generation */

/* Return 1 for an PARALLEL suitable for mfcr.  */

int
mfcr_operation (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  int count = XVECLEN (op, 0);
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count < 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != UNSPEC
      || XVECLEN (SET_SRC (XVECEXP (op, 0, 0)), 0) != 2)
    return 0;

  for (i = 0; i < count; i++)
    {
      rtx exp = XVECEXP (op, 0, i);
      rtx unspec;
      int maskval;
      rtx src_reg;
      
      src_reg = XVECEXP (SET_SRC (exp), 0, 0);
  
      if (GET_CODE (src_reg) != REG
          || GET_MODE (src_reg) != CCmode
          || ! CR_REGNO_P (REGNO (src_reg)))
    	return 0;

      if (GET_CODE (exp) != SET
	  || GET_CODE (SET_DEST (exp)) != REG
	  || GET_MODE (SET_DEST (exp)) != SImode
	  || ! INT_REGNO_P (REGNO (SET_DEST (exp))))
	return 0;
      unspec = SET_SRC (exp);
      maskval = 1 << (MAX_CR_REGNO - REGNO (src_reg));
      
      if (GET_CODE (unspec) != UNSPEC
	  || XINT (unspec, 1) != 20
	  || XVECLEN (unspec, 0) != 2
	  || XVECEXP (unspec, 0, 0) != src_reg
	  || GET_CODE (XVECEXP (unspec, 0, 1)) != CONST_INT
	  || INTVAL (XVECEXP (unspec, 0, 1)) != maskval)
	return 0;
    }
  return 1;
}

/* APPLE LOCAL end optimized mfcr instruction generation */

/* Return 1 for an PARALLEL suitable for mtcrf.  */

int
mtcrf_operation (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  int count = XVECLEN (op, 0);
  int i;
  rtx src_reg;

  /* Perform a quick check so we don't blow up below.  */
  if (count < 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != UNSPEC
      || XVECLEN (SET_SRC (XVECEXP (op, 0, 0)), 0) != 2)
    return 0;
  src_reg = XVECEXP (SET_SRC (XVECEXP (op, 0, 0)), 0, 0);
  
  if (GET_CODE (src_reg) != REG
      || GET_MODE (src_reg) != SImode
      || ! INT_REGNO_P (REGNO (src_reg)))
    return 0;

  for (i = 0; i < count; i++)
    {
      rtx exp = XVECEXP (op, 0, i);
      rtx unspec;
      int maskval;
      
      if (GET_CODE (exp) != SET
	  || GET_CODE (SET_DEST (exp)) != REG
	  || GET_MODE (SET_DEST (exp)) != CCmode
	  || ! CR_REGNO_P (REGNO (SET_DEST (exp))))
	return 0;
      unspec = SET_SRC (exp);
      maskval = 1 << (MAX_CR_REGNO - REGNO (SET_DEST (exp)));
      
      if (GET_CODE (unspec) != UNSPEC
	  || XINT (unspec, 1) != 20
	  || XVECLEN (unspec, 0) != 2
	  || XVECEXP (unspec, 0, 0) != src_reg
	  || GET_CODE (XVECEXP (unspec, 0, 1)) != CONST_INT
	  || INTVAL (XVECEXP (unspec, 0, 1)) != maskval)
	return 0;
    }
  return 1;
}

/* APPLE LOCAL begin AltiVec */
/* Return 1 for an PARALLEL suitable for mtspr VRsave.  */
int
mov_to_vrsave_operation (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  int count = XVECLEN (op, 0);
  int i;
  rtx src_reg;

  /* Perform a quick check so we don't blow up below.  */
  if (count < 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != UNSPEC_VOLATILE
      || XVECLEN (SET_DEST (XVECEXP (op, 0, 0)), 0) != 1)
    return 0;
  src_reg = SET_SRC (XVECEXP (op, 0, 0));
  
  if (GET_CODE (src_reg) != REG
      || GET_MODE (src_reg) != SImode
      || ! INT_REGNO_P (REGNO (src_reg)))
    return 0;

  for (i = 1; i < count; i++)
    {
      rtx exp = XVECEXP (op, 0, i);
      
      if ((GET_CODE (exp) != CLOBBER && GET_CODE (exp) != USE)
	  || GET_CODE (XEXP (exp, 0)) != REG
	  || ! VECTOR_MODE_P (GET_MODE (XEXP (exp, 0))))
	return 0;
    }
  return 1;
}
/* APPLE LOCAL end AltiVec */

/* Return 1 for an PARALLEL suitable for lmw.  */

int
lmw_operation (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  int count = XVECLEN (op, 0);
  unsigned int dest_regno;
  rtx src_addr;
  unsigned int base_regno;
  HOST_WIDE_INT offset;
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != REG
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != MEM)
    return 0;

  dest_regno = REGNO (SET_DEST (XVECEXP (op, 0, 0)));
  src_addr = XEXP (SET_SRC (XVECEXP (op, 0, 0)), 0);

  if (dest_regno > 31
      || count != 32 - (int) dest_regno)
    return 0;

  if (LEGITIMATE_INDIRECT_ADDRESS_P (src_addr, 0))
    {
      offset = 0;
      base_regno = REGNO (src_addr);
      if (base_regno == 0)
	return 0;
    }
  else if (LEGITIMATE_OFFSET_ADDRESS_P (SImode, src_addr, 0))
    {
      offset = INTVAL (XEXP (src_addr, 1));
      base_regno = REGNO (XEXP (src_addr, 0));
    }
  else
    return 0;

  for (i = 0; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i);
      rtx newaddr;
      rtx addr_reg;
      HOST_WIDE_INT newoffset;

      if (GET_CODE (elt) != SET
	  || GET_CODE (SET_DEST (elt)) != REG
	  || GET_MODE (SET_DEST (elt)) != SImode
	  || REGNO (SET_DEST (elt)) != dest_regno + i
	  || GET_CODE (SET_SRC (elt)) != MEM
	  || GET_MODE (SET_SRC (elt)) != SImode)
	return 0;
      newaddr = XEXP (SET_SRC (elt), 0);
      if (LEGITIMATE_INDIRECT_ADDRESS_P (newaddr, 0))
	{
	  newoffset = 0;
	  addr_reg = newaddr;
	}
      else if (LEGITIMATE_OFFSET_ADDRESS_P (SImode, newaddr, 0))
	{
	  addr_reg = XEXP (newaddr, 0);
	  newoffset = INTVAL (XEXP (newaddr, 1));
	}
      else
	return 0;
      if (REGNO (addr_reg) != base_regno
	  || newoffset != offset + 4 * i)
	return 0;
    }

  return 1;
}

/* Return 1 for an PARALLEL suitable for stmw.  */

int
stmw_operation (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  int count = XVECLEN (op, 0);
  unsigned int src_regno;
  rtx dest_addr;
  unsigned int base_regno;
  HOST_WIDE_INT offset;
  int i;

  /* Perform a quick check so we don't blow up below.  */
  if (count <= 1
      || GET_CODE (XVECEXP (op, 0, 0)) != SET
      || GET_CODE (SET_DEST (XVECEXP (op, 0, 0))) != MEM
      || GET_CODE (SET_SRC (XVECEXP (op, 0, 0))) != REG)
    return 0;

  src_regno = REGNO (SET_SRC (XVECEXP (op, 0, 0)));
  dest_addr = XEXP (SET_DEST (XVECEXP (op, 0, 0)), 0);

  if (src_regno > 31
      || count != 32 - (int) src_regno)
    return 0;

  if (LEGITIMATE_INDIRECT_ADDRESS_P (dest_addr, 0))
    {
      offset = 0;
      base_regno = REGNO (dest_addr);
      if (base_regno == 0)
	return 0;
    }
  else if (LEGITIMATE_OFFSET_ADDRESS_P (SImode, dest_addr, 0))
    {
      offset = INTVAL (XEXP (dest_addr, 1));
      base_regno = REGNO (XEXP (dest_addr, 0));
    }
  else
    return 0;

  for (i = 0; i < count; i++)
    {
      rtx elt = XVECEXP (op, 0, i);
      rtx newaddr;
      rtx addr_reg;
      HOST_WIDE_INT newoffset;

      if (GET_CODE (elt) != SET
	  || GET_CODE (SET_SRC (elt)) != REG
	  || GET_MODE (SET_SRC (elt)) != SImode
	  || REGNO (SET_SRC (elt)) != src_regno + i
	  || GET_CODE (SET_DEST (elt)) != MEM
	  || GET_MODE (SET_DEST (elt)) != SImode)
	return 0;
      newaddr = XEXP (SET_DEST (elt), 0);
      if (LEGITIMATE_INDIRECT_ADDRESS_P (newaddr, 0))
	{
	  newoffset = 0;
	  addr_reg = newaddr;
	}
      else if (LEGITIMATE_OFFSET_ADDRESS_P (SImode, newaddr, 0))
	{
	  addr_reg = XEXP (newaddr, 0);
	  newoffset = INTVAL (XEXP (newaddr, 1));
	}
      else
	return 0;
      if (REGNO (addr_reg) != base_regno
	  || newoffset != offset + 4 * i)
	return 0;
    }

  return 1;
}

/* A validation routine: say whether CODE, a condition code, and MODE
   match.  The other alternatives either don't make sense or should
   never be generated.  */

static void
validate_condition_mode (code, mode)
     enum rtx_code code;
     enum machine_mode mode;
{
  if (GET_RTX_CLASS (code) != '<' 
      || GET_MODE_CLASS (mode) != MODE_CC)
    abort ();

  /* These don't make sense.  */
  if ((code == GT || code == LT || code == GE || code == LE)
      && mode == CCUNSmode)
    abort ();

  if ((code == GTU || code == LTU || code == GEU || code == LEU)
      && mode != CCUNSmode)
    abort ();

  if (mode != CCFPmode
      && (code == ORDERED || code == UNORDERED
	  || code == UNEQ || code == LTGT
	  || code == UNGT || code == UNLT
	  || code == UNGE || code == UNLE))
    abort ();
  
  /* These should never be generated except for 
     flag_unsafe_math_optimizations and flag_finite_math_only.  */
  if (mode == CCFPmode
      && ! flag_unsafe_math_optimizations
      && ! flag_finite_math_only
      && (code == LE || code == GE
	  || code == UNEQ || code == LTGT
	  || code == UNGT || code == UNLT))
    abort ();

  /* These are invalid; the information is not there.  */
  if (mode == CCEQmode 
      && code != EQ && code != NE)
    abort ();
}

/* APPLE LOCAL begin AltiVec */
/* Return 1 if OP is an equality operator.  */

int
equality_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  enum rtx_code code = GET_CODE (op);
  if (mode == VOIDmode && (code == EQ || code == NE))
    return 1;
  return 0;
}

/* Return 1 if OP is a vector comparison operator.  */

int
vector_comparison_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  enum rtx_code code = GET_CODE (op);
  if (mode == SImode && (code == EQ || code == LT))
    return 1;
  return 0;
}
/* APPLE LOCAL end AltiVec */

/* Return 1 if OP is a comparison operation that is valid for a branch insn.
   We only check the opcode against the mode of the CC value here.  */

int
branch_comparison_operator (op, mode)
     rtx op;
     enum machine_mode mode ATTRIBUTE_UNUSED;
{
  enum rtx_code code = GET_CODE (op);
  enum machine_mode cc_mode;

  if (GET_RTX_CLASS (code) != '<')
    return 0;

  cc_mode = GET_MODE (XEXP (op, 0));
  if (GET_MODE_CLASS (cc_mode) != MODE_CC)
    return 0;

  validate_condition_mode (code, cc_mode);

  return 1;
}

/* Return 1 if OP is a comparison operation that is valid for a branch
   insn and which is true if the corresponding bit in the CC register
   is set.  */

int
branch_positive_comparison_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  enum rtx_code code;

  if (! branch_comparison_operator (op, mode))
    return 0;

  code = GET_CODE (op);
  return (code == EQ || code == LT || code == GT
	  || (TARGET_SPE && TARGET_HARD_FLOAT && !TARGET_FPRS && code == NE)
	  || code == LTU || code == GTU
	  || code == UNORDERED);
}

/* Return 1 if OP is a comparison operation that is valid for an scc insn.
   We check the opcode against the mode of the CC value and disallow EQ or
   NE comparisons for integers.  */

int
scc_comparison_operator (op, mode)
     rtx op;
     enum machine_mode mode;
{
  enum rtx_code code = GET_CODE (op);
  enum machine_mode cc_mode;

  if (GET_MODE (op) != mode && mode != VOIDmode)
    return 0;

  if (GET_RTX_CLASS (code) != '<')
    return 0;

  cc_mode = GET_MODE (XEXP (op, 0));
  if (GET_MODE_CLASS (cc_mode) != MODE_CC)
    return 0;

  validate_condition_mode (code, cc_mode);

  if (code == NE && cc_mode != CCFPmode)
    return 0;

  return 1;
}

int
trap_comparison_operator (op, mode)
    rtx op;
    enum machine_mode mode;
{
  if (mode != VOIDmode && mode != GET_MODE (op))
    return 0;
  return GET_RTX_CLASS (GET_CODE (op)) == '<';
}

int
boolean_operator (op, mode)
    rtx op;
    enum machine_mode mode ATTRIBUTE_UNUSED;
{
  enum rtx_code code = GET_CODE (op);
  return (code == AND || code == IOR || code == XOR);
}

int
boolean_or_operator (op, mode)
    rtx op;
    enum machine_mode mode ATTRIBUTE_UNUSED;
{
  enum rtx_code code = GET_CODE (op);
  return (code == IOR || code == XOR);
}

int
min_max_operator (op, mode)
    rtx op;
    enum machine_mode mode ATTRIBUTE_UNUSED;
{
  enum rtx_code code = GET_CODE (op);
  return (code == SMIN || code == SMAX || code == UMIN || code == UMAX);
}

/* Return 1 if ANDOP is a mask that has no bits on that are not in the
   mask required to convert the result of a rotate insn into a shift
   left insn of SHIFTOP bits.  Both are known to be SImode CONST_INT.  */

int
includes_lshift_p (shiftop, andop)
     rtx shiftop;
     rtx andop;
{
  unsigned HOST_WIDE_INT shift_mask = ~(unsigned HOST_WIDE_INT) 0;

  shift_mask <<= INTVAL (shiftop);

  return (INTVAL (andop) & 0xffffffff & ~shift_mask) == 0;
}

/* Similar, but for right shift.  */

int
includes_rshift_p (shiftop, andop)
     rtx shiftop;
     rtx andop;
{
  unsigned HOST_WIDE_INT shift_mask = ~(unsigned HOST_WIDE_INT) 0;

  shift_mask >>= INTVAL (shiftop);

  return (INTVAL (andop) & 0xffffffff & ~shift_mask) == 0;
}

/* Return 1 if ANDOP is a mask suitable for use with an rldic insn
   to perform a left shift.  It must have exactly SHIFTOP least
   signifigant 0's, then one or more 1's, then zero or more 0's.  */

int
includes_rldic_lshift_p (shiftop, andop)
     rtx shiftop;
     rtx andop;
{
  if (GET_CODE (andop) == CONST_INT)
    {
      HOST_WIDE_INT c, lsb, shift_mask;

      c = INTVAL (andop);
      if (c == 0 || c == ~0)
	return 0;

      shift_mask = ~0;
      shift_mask <<= INTVAL (shiftop);

      /* Find the least signifigant one bit.  */
      lsb = c & -c;

      /* It must coincide with the LSB of the shift mask.  */
      if (-lsb != shift_mask)
	return 0;

      /* Invert to look for the next transition (if any).  */
      c = ~c;

      /* Remove the low group of ones (originally low group of zeros).  */
      c &= -lsb;

      /* Again find the lsb, and check we have all 1's above.  */
      lsb = c & -c;
      return c == -lsb;
    }
  else if (GET_CODE (andop) == CONST_DOUBLE
	   && (GET_MODE (andop) == VOIDmode || GET_MODE (andop) == DImode))
    {
      HOST_WIDE_INT low, high, lsb;
      HOST_WIDE_INT shift_mask_low, shift_mask_high;

      low = CONST_DOUBLE_LOW (andop);
      if (HOST_BITS_PER_WIDE_INT < 64)
	high = CONST_DOUBLE_HIGH (andop);

      if ((low == 0 && (HOST_BITS_PER_WIDE_INT >= 64 || high == 0))
	  || (low == ~0 && (HOST_BITS_PER_WIDE_INT >= 64 || high == ~0)))
	return 0;

      if (HOST_BITS_PER_WIDE_INT < 64 && low == 0)
	{
	  shift_mask_high = ~0;
	  if (INTVAL (shiftop) > 32)
	    shift_mask_high <<= INTVAL (shiftop) - 32;

	  lsb = high & -high;

	  if (-lsb != shift_mask_high || INTVAL (shiftop) < 32)
	    return 0;

	  high = ~high;
	  high &= -lsb;

	  lsb = high & -high;
	  return high == -lsb;
	}

      shift_mask_low = ~0;
      shift_mask_low <<= INTVAL (shiftop);

      lsb = low & -low;

      if (-lsb != shift_mask_low)
	return 0;

      if (HOST_BITS_PER_WIDE_INT < 64)
	high = ~high;
      low = ~low;
      low &= -lsb;

      if (HOST_BITS_PER_WIDE_INT < 64 && low == 0)
	{
	  lsb = high & -high;
	  return high == -lsb;
	}

      lsb = low & -low;
      return low == -lsb && (HOST_BITS_PER_WIDE_INT >= 64 || high == ~0);
    }
  else
    return 0;
}

/* Return 1 if ANDOP is a mask suitable for use with an rldicr insn
   to perform a left shift.  It must have SHIFTOP or more least
   signifigant 0's, with the remainder of the word 1's.  */

int
includes_rldicr_lshift_p (shiftop, andop)
     rtx shiftop;
     rtx andop;
{
  if (GET_CODE (andop) == CONST_INT)
    {
      HOST_WIDE_INT c, lsb, shift_mask;

      shift_mask = ~0;
      shift_mask <<= INTVAL (shiftop);
      c = INTVAL (andop);

      /* Find the least signifigant one bit.  */
      lsb = c & -c;

      /* It must be covered by the shift mask.
	 This test also rejects c == 0.  */
      if ((lsb & shift_mask) == 0)
	return 0;

      /* Check we have all 1's above the transition, and reject all 1's.  */
      return c == -lsb && lsb != 1;
    }
  else if (GET_CODE (andop) == CONST_DOUBLE
	   && (GET_MODE (andop) == VOIDmode || GET_MODE (andop) == DImode))
    {
      HOST_WIDE_INT low, lsb, shift_mask_low;

      low = CONST_DOUBLE_LOW (andop);

      if (HOST_BITS_PER_WIDE_INT < 64)
	{
	  HOST_WIDE_INT high, shift_mask_high;

	  high = CONST_DOUBLE_HIGH (andop);

	  if (low == 0)
	    {
	      shift_mask_high = ~0;
	      if (INTVAL (shiftop) > 32)
		shift_mask_high <<= INTVAL (shiftop) - 32;

	      lsb = high & -high;

	      if ((lsb & shift_mask_high) == 0)
		return 0;

	      return high == -lsb;
	    }
	  if (high != ~0)
	    return 0;
	}

      shift_mask_low = ~0;
      shift_mask_low <<= INTVAL (shiftop);

      lsb = low & -low;

      if ((lsb & shift_mask_low) == 0)
	return 0;

      return low == -lsb && lsb != 1;
    }
  else
    return 0;
}

/* Return 1 if REGNO (reg1) == REGNO (reg2) - 1 making them candidates
   for lfq and stfq insns.

   Note reg1 and reg2 *must* be hard registers.  To be sure we will
   abort if we are passed pseudo registers.  */

int
registers_ok_for_quad_peep (reg1, reg2)
     rtx reg1, reg2;
{
  /* We might have been passed a SUBREG.  */
  if (GET_CODE (reg1) != REG || GET_CODE (reg2) != REG) 
    return 0;

  return (REGNO (reg1) == REGNO (reg2) - 1);
}

/* Return 1 if addr1 and addr2 are suitable for lfq or stfq insn.
   addr1 and addr2 must be in consecutive memory locations
   (addr2 == addr1 + 8).  */

int
addrs_ok_for_quad_peep (addr1, addr2)
     rtx addr1;
     rtx addr2;
{
  unsigned int reg1;
  int offset1;

  /* Extract an offset (if used) from the first addr.  */
  if (GET_CODE (addr1) == PLUS)
    {
      /* If not a REG, return zero.  */
      if (GET_CODE (XEXP (addr1, 0)) != REG)
	return 0;
      else
	{
          reg1 = REGNO (XEXP (addr1, 0));
	  /* The offset must be constant!  */
	  if (GET_CODE (XEXP (addr1, 1)) != CONST_INT)
            return 0;
          offset1 = INTVAL (XEXP (addr1, 1));
	}
    }
  else if (GET_CODE (addr1) != REG)
    return 0;
  else
    {
      reg1 = REGNO (addr1);
      /* This was a simple (mem (reg)) expression.  Offset is 0.  */
      offset1 = 0;
    }

  /* Make sure the second address is a (mem (plus (reg) (const_int))).  */
  if (GET_CODE (addr2) != PLUS)
    return 0;

  if (GET_CODE (XEXP (addr2, 0)) != REG
      || GET_CODE (XEXP (addr2, 1)) != CONST_INT)
    return 0;

  if (reg1 != REGNO (XEXP (addr2, 0)))
    return 0;

  /* The offset for the second addr must be 8 more than the first addr.  */
  if (INTVAL (XEXP (addr2, 1)) != offset1 + 8)
    return 0;

  /* All the tests passed.  addr1 and addr2 are valid for lfq or stfq
     instructions.  */
  return 1;
}

/* Return the register class of a scratch register needed to copy IN into
   or out of a register in CLASS in MODE.  If it can be done directly,
   NO_REGS is returned.  */

enum reg_class
secondary_reload_class (class, mode, in)
     enum reg_class class;
     enum machine_mode mode ATTRIBUTE_UNUSED;
     rtx in;
{
  int regno;

  /* APPLE LOCAL  dynamic-no-pic  */
  if (TARGET_ELF || (DEFAULT_ABI == ABI_DARWIN 
#if TARGET_MACHO
		     && MACHOPIC_INDIRECT
#endif
		     ))
    {
      /* We cannot copy a symbolic operand directly into anything
         other than BASE_REGS for TARGET_ELF.  So indicate that a
         register from BASE_REGS is needed as an intermediate
         register.
         
	 On Darwin, pic addresses require a load from memory, which
	 needs a base register.  */
      if (class != BASE_REGS
          && (GET_CODE (in) == SYMBOL_REF
              || GET_CODE (in) == HIGH
              || GET_CODE (in) == LABEL_REF
              || GET_CODE (in) == CONST))
        return BASE_REGS;
    }

  if (GET_CODE (in) == REG)
    {
      regno = REGNO (in);
      if (regno >= FIRST_PSEUDO_REGISTER)
	{
	  regno = true_regnum (in);
	  if (regno >= FIRST_PSEUDO_REGISTER)
	    regno = -1;
	}
    }
  else if (GET_CODE (in) == SUBREG)
    {
      regno = true_regnum (in);
      if (regno >= FIRST_PSEUDO_REGISTER)
	regno = -1;
    }
  else
    regno = -1;

  /* We can place anything into GENERAL_REGS and can put GENERAL_REGS
     into anything.  */
  if (class == GENERAL_REGS || class == BASE_REGS
      || (regno >= 0 && INT_REGNO_P (regno)))
    return NO_REGS;

  /* Constants, memory, and FP registers can go into FP registers.  */
  if ((regno == -1 || FP_REGNO_P (regno))
      && (class == FLOAT_REGS || class == NON_SPECIAL_REGS))
    return NO_REGS;

  /* APPLE LOCAL begin AltiVec */
  /* now partly redundant? */
  /* Easy constants, memory, and vector registers can go into vector
     registers.  */
  if (class == ALTIVEC_REGS
      && (ALTIVEC_REGNO_P (regno)
	  || (regno == -1
	      && (GET_CODE (in) != CONST_VECTOR
		  || easy_vector_constant (in)))))
    return NO_REGS;

  /* Memory vector constants need BASE_REGS in order to be loaded.  */
  if (GET_CODE (in) == CONST_VECTOR && ! easy_vector_constant (in))
    return BASE_REGS;
  /* APPLE LOCAL end AltiVec */

  /* Memory, and AltiVec registers can go into AltiVec registers.  */
  if ((regno == -1 || ALTIVEC_REGNO_P (regno))
      && class == ALTIVEC_REGS)
    return NO_REGS;

  /* We can copy among the CR registers.  */
  if ((class == CR_REGS || class == CR0_REGS)
      && regno >= 0 && CR_REGNO_P (regno))
    return NO_REGS;

  /* Otherwise, we need GENERAL_REGS.  */
  return GENERAL_REGS;
}

/* Given a comparison operation, return the bit number in CCR to test.  We
   know this is a valid comparison.  

   SCC_P is 1 if this is for an scc.  That means that %D will have been
   used instead of %C, so the bits will be in different places.

   Return -1 if OP isn't a valid comparison for some reason.  */

int
ccr_bit (op, scc_p)
     rtx op;
     int scc_p;
{
  enum rtx_code code = GET_CODE (op);
  enum machine_mode cc_mode;
  int cc_regnum;
  int base_bit;
  rtx reg;

  if (GET_RTX_CLASS (code) != '<')
    return -1;

  reg = XEXP (op, 0);

  if (GET_CODE (reg) != REG
      || ! CR_REGNO_P (REGNO (reg)))
    abort ();

  cc_mode = GET_MODE (reg);
  cc_regnum = REGNO (reg);
  base_bit = 4 * (cc_regnum - CR0_REGNO);

  validate_condition_mode (code, cc_mode);

  switch (code)
    {
    case NE:
      if (TARGET_SPE && TARGET_HARD_FLOAT && cc_mode == CCFPmode)
	return base_bit + 1;
      return scc_p ? base_bit + 3 : base_bit + 2;
    case EQ:
      if (TARGET_SPE && TARGET_HARD_FLOAT && cc_mode == CCFPmode)
	return base_bit + 1;
      return base_bit + 2;
    case GT:  case GTU:  case UNLE:
      return base_bit + 1;
    case LT:  case LTU:  case UNGE:
      return base_bit;
    case ORDERED:  case UNORDERED:
      return base_bit + 3;

    case GE:  case GEU:
      /* If scc, we will have done a cror to put the bit in the
	 unordered position.  So test that bit.  For integer, this is ! LT
	 unless this is an scc insn.  */
      return scc_p ? base_bit + 3 : base_bit;

    case LE:  case LEU:
      return scc_p ? base_bit + 3 : base_bit + 1;

    default:
      abort ();
    }
}

/* Return the GOT register.  */

struct rtx_def *
rs6000_got_register (value)
     rtx value ATTRIBUTE_UNUSED;
{
  /* The second flow pass currently (June 1999) can't update
     regs_ever_live without disturbing other parts of the compiler, so
     update it here to make the prolog/epilogue code happy.  */
  if (no_new_pseudos && ! regs_ever_live[RS6000_PIC_OFFSET_TABLE_REGNUM])
    regs_ever_live[RS6000_PIC_OFFSET_TABLE_REGNUM] = 1;

  current_function_uses_pic_offset_table = 1;

  return pic_offset_table_rtx;
}

/* Function to init struct machine_function.
   This will be called, via a pointer variable,
   from push_function_context.  */

static struct machine_function *
rs6000_init_machine_status ()
{
  machine_function *mf = (machine_function *) ggc_alloc_cleared (sizeof (machine_function));
  /* APPLE LOCAL volatile pic base reg in leaves */
  mf->substitute_pic_base_reg = -1;
  return mf;
}

/* These macros test for integers and extract the low-order bits.  */
#define INT_P(X)  \
((GET_CODE (X) == CONST_INT || GET_CODE (X) == CONST_DOUBLE)	\
 && GET_MODE (X) == VOIDmode)

#define INT_LOWPART(X) \
  (GET_CODE (X) == CONST_INT ? INTVAL (X) : CONST_DOUBLE_LOW (X))

int
extract_MB (op)
     rtx op;
{
  int i;
  unsigned long val = INT_LOWPART (op);

  /* If the high bit is zero, the value is the first 1 bit we find
     from the left.  */
  if ((val & 0x80000000) == 0)
    {
      if ((val & 0xffffffff) == 0)
	abort ();

      i = 1;
      while (((val <<= 1) & 0x80000000) == 0)
	++i;
      return i;
    }

  /* If the high bit is set and the low bit is not, or the mask is all
     1's, the value is zero.  */
  if ((val & 1) == 0 || (val & 0xffffffff) == 0xffffffff)
    return 0;

  /* Otherwise we have a wrap-around mask.  Look for the first 0 bit
     from the right.  */
  i = 31;
  while (((val >>= 1) & 1) != 0)
    --i;

  return i;
}

int
extract_ME (op)
     rtx op;
{
  int i;
  unsigned long val = INT_LOWPART (op);

  /* If the low bit is zero, the value is the first 1 bit we find from
     the right.  */
  if ((val & 1) == 0)
    {
      if ((val & 0xffffffff) == 0)
	abort ();

      i = 30;
      while (((val >>= 1) & 1) == 0)
	--i;

      return i;
    }

  /* If the low bit is set and the high bit is not, or the mask is all
     1's, the value is 31.  */
  if ((val & 0x80000000) == 0 || (val & 0xffffffff) == 0xffffffff)
    return 31;

  /* Otherwise we have a wrap-around mask.  Look for the first 0 bit
     from the left.  */
  i = 0;
  while (((val <<= 1) & 0x80000000) != 0)
    ++i;

  return i;
}

/* Print an operand.  Recognize special options, documented below.  */

#if TARGET_ELF
#define SMALL_DATA_RELOC ((rs6000_sdata == SDATA_EABI) ? "sda21" : "sdarel")
#define SMALL_DATA_REG ((rs6000_sdata == SDATA_EABI) ? 0 : 13)
#else
#define SMALL_DATA_RELOC "sda21"
#define SMALL_DATA_REG 0
#endif

void
print_operand (file, x, code)
    FILE *file;
    rtx x;
    int code;
{
  int i;
  HOST_WIDE_INT val;
  unsigned HOST_WIDE_INT uval;

  switch (code)
    {
    case '.':
      /* Write out an instruction after the call which may be replaced
	 with glue code by the loader.  This depends on the AIX version.  */
      asm_fprintf (file, RS6000_CALL_GLUE);
      return;

      /* %a is output_address.  */

    case 'A':
      /* If X is a constant integer whose low-order 5 bits are zero,
	 write 'l'.  Otherwise, write 'r'.  This is a kludge to fix a bug
	 in the AIX assembler where "sri" with a zero shift count
	 writes a trash instruction.  */
      if (GET_CODE (x) == CONST_INT && (INTVAL (x) & 31) == 0)
	putc ('l', file);
      else
	putc ('r', file);
      return;

    case 'b':
      /* If constant, low-order 16 bits of constant, unsigned.
	 Otherwise, write normally.  */
      if (INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INT_LOWPART (x) & 0xffff);
      else
	print_operand (file, x, 0);
      return;

    case 'B':
      /* If the low-order bit is zero, write 'r'; otherwise, write 'l'
	 for 64-bit mask direction.  */
      putc (((INT_LOWPART(x) & 1) == 0 ? 'r' : 'l'), file);
      return;

      /* %c is output_addr_const if a CONSTANT_ADDRESS_P, otherwise
	 output_operand.  */

    case 'D':
      /* There used to be a comment for 'C' reading "This is an
	   optional cror needed for certain floating-point
	   comparisons.  Otherwise write nothing."  */

      /* Similar, except that this is for an scc, so we must be able to
	 encode the test in a single bit that is one.  We do the above
	 for any LE, GE, GEU, or LEU and invert the bit for NE.  */
      if (GET_CODE (x) == LE || GET_CODE (x) == GE
	  || GET_CODE (x) == LEU || GET_CODE (x) == GEU)
	{
	  int base_bit = 4 * (REGNO (XEXP (x, 0)) - CR0_REGNO);

	  fprintf (file, "cror %d,%d,%d\n\t", base_bit + 3,
		   base_bit + 2,
		   base_bit + (GET_CODE (x) == GE || GET_CODE (x) == GEU));
	}

      else if (GET_CODE (x) == NE)
	{
	  int base_bit = 4 * (REGNO (XEXP (x, 0)) - CR0_REGNO);

	  fprintf (file, "crnor %d,%d,%d\n\t", base_bit + 3,
		   base_bit + 2, base_bit + 2);
	}
      else if (TARGET_SPE && TARGET_HARD_FLOAT
	       && GET_CODE (x) == EQ
	       && GET_MODE (XEXP (x, 0)) == CCFPmode)
	{
	  int base_bit = 4 * (REGNO (XEXP (x, 0)) - CR0_REGNO);

	  fprintf (file, "crnor %d,%d,%d\n\t", base_bit + 1,
		   base_bit + 1, base_bit + 1);
	}
      return;

    case 'E':
      /* X is a CR register.  Print the number of the EQ bit of the CR */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%E value");
      else
	fprintf (file, "%d", 4 * (REGNO (x) - CR0_REGNO) + 2);
      return;

    case 'f':
      /* X is a CR register.  Print the shift count needed to move it
	 to the high-order four bits.  */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%f value");
      else
	fprintf (file, "%d", 4 * (REGNO (x) - CR0_REGNO));
      return;

    case 'F':
      /* Similar, but print the count for the rotate in the opposite
	 direction.  */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%F value");
      else
	fprintf (file, "%d", 32 - 4 * (REGNO (x) - CR0_REGNO));
      return;

    case 'G':
      /* X is a constant integer.  If it is negative, print "m",
	 otherwise print "z".  This is to make an aze or ame insn.  */
      if (GET_CODE (x) != CONST_INT)
	output_operand_lossage ("invalid %%G value");
      else if (INTVAL (x) >= 0)
	putc ('z', file);
      else
	putc ('m', file);
      return;

    case 'h':
      /* If constant, output low-order five bits.  Otherwise, write
	 normally.  */
      if (INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INT_LOWPART (x) & 31);
      else
	print_operand (file, x, 0);
      return;

    case 'H':
      /* If constant, output low-order six bits.  Otherwise, write
	 normally.  */
      if (INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, INT_LOWPART (x) & 63);
      else
	print_operand (file, x, 0);
      return;

    case 'I':
      /* Print `i' if this is a constant, else nothing.  */
      if (INT_P (x))
	putc ('i', file);
      return;

    case 'j':
      /* Write the bit number in CCR for jump.  */
      i = ccr_bit (x, 0);
      if (i == -1)
	output_operand_lossage ("invalid %%j code");
      else
	fprintf (file, "%d", i);
      return;

    case 'J':
      /* Similar, but add one for shift count in rlinm for scc and pass
	 scc flag to `ccr_bit'.  */
      i = ccr_bit (x, 1);
      if (i == -1)
	output_operand_lossage ("invalid %%J code");
      else
	/* If we want bit 31, write a shift count of zero, not 32.  */
	fprintf (file, "%d", i == 31 ? 0 : i + 1);
      return;

    case 'k':
      /* X must be a constant.  Write the 1's complement of the
	 constant.  */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%k value");
      else
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, ~ INT_LOWPART (x));
      return;

    case 'K':
      /* X must be a symbolic constant on ELF.  Write an
	 expression suitable for an 'addi' that adds in the low 16
	 bits of the MEM.  */
      if (GET_CODE (x) != CONST)
	{
	  print_operand_address (file, x);
	  fputs ("@l", file);
	}
      else
	{
	  if (GET_CODE (XEXP (x, 0)) != PLUS
	      || (GET_CODE (XEXP (XEXP (x, 0), 0)) != SYMBOL_REF
		  && GET_CODE (XEXP (XEXP (x, 0), 0)) != LABEL_REF)
	      || GET_CODE (XEXP (XEXP (x, 0), 1)) != CONST_INT)
	    output_operand_lossage ("invalid %%K value");
	  print_operand_address (file, XEXP (XEXP (x, 0), 0));
	  fputs ("@l", file);
	  /* For GNU as, there must be a non-alphanumeric character
	     between 'l' and the number.  The '-' is added by
	     print_operand() already.  */
	  if (INTVAL (XEXP (XEXP (x, 0), 1)) >= 0)
	    fputs ("+", file);
	  print_operand (file, XEXP (XEXP (x, 0), 1), 0);
	}
      return;

      /* %l is output_asm_label.  */

    case 'L':
      /* Write second word of DImode or DFmode reference.  Works on register
	 or non-indexed memory only.  */
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x) + 1]);
      else if (GET_CODE (x) == MEM)
	{
	  /* Handle possible auto-increment.  Since it is pre-increment and
	     we have already done it, we can just use an offset of word.  */
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    output_address (plus_constant (XEXP (XEXP (x, 0), 0),
					   ABI_UNITS_PER_WORD));
	  else
	    output_address (XEXP (adjust_address_nv (x, SImode,
						     ABI_UNITS_PER_WORD),
				  0));

	  if (small_data_operand (x, GET_MODE (x)))
	    fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		     reg_names[SMALL_DATA_REG]);
	}
      return;
			    
    case 'm':
      /* MB value for a mask operand.  */
      if (! mask_operand (x, SImode))
	output_operand_lossage ("invalid %%m value");

      fprintf (file, "%d", extract_MB (x));
      return;

    case 'M':
      /* ME value for a mask operand.  */
      if (! mask_operand (x, SImode))
	output_operand_lossage ("invalid %%M value");

      fprintf (file, "%d", extract_ME (x));
      return;

      /* %n outputs the negative of its operand.  */

    case 'N':
      /* Write the number of elements in the vector times 4.  */
      if (GET_CODE (x) != PARALLEL)
	output_operand_lossage ("invalid %%N value");
      else
	fprintf (file, "%d", XVECLEN (x, 0) * 4);
      return;

    case 'O':
      /* Similar, but subtract 1 first.  */
      if (GET_CODE (x) != PARALLEL)
	output_operand_lossage ("invalid %%O value");
      else
	fprintf (file, "%d", (XVECLEN (x, 0) - 1) * 4);
      return;

    case 'p':
      /* X is a CONST_INT that is a power of two.  Output the logarithm.  */
      if (! INT_P (x)
	  || INT_LOWPART (x) < 0
	  || (i = exact_log2 (INT_LOWPART (x))) < 0)
	output_operand_lossage ("invalid %%p value");
      else
	fprintf (file, "%d", i);
      return;

    case 'P':
      /* The operand must be an indirect memory reference.  The result
	 is the register number.  */
      if (GET_CODE (x) != MEM || GET_CODE (XEXP (x, 0)) != REG
	  || REGNO (XEXP (x, 0)) >= 32)
	output_operand_lossage ("invalid %%P value");
      else
	fprintf (file, "%d", REGNO (XEXP (x, 0)));
      return;

    case 'q':
      /* This outputs the logical code corresponding to a boolean
	 expression.  The expression may have one or both operands
	 negated (if one, only the first one).  For condition register
         logical operations, it will also treat the negated
         CR codes as NOTs, but not handle NOTs of them.  */
      {
	const char *const *t = 0;
	const char *s;
	enum rtx_code code = GET_CODE (x);
	static const char * const tbl[3][3] = {
	  { "and", "andc", "nor" },
	  { "or", "orc", "nand" },
	  { "xor", "eqv", "xor" } };

	if (code == AND)
	  t = tbl[0];
	else if (code == IOR)
	  t = tbl[1];
	else if (code == XOR)
	  t = tbl[2];
	else
	  output_operand_lossage ("invalid %%q value");

	if (GET_CODE (XEXP (x, 0)) != NOT)
	  s = t[0];
	else
	  {
	    if (GET_CODE (XEXP (x, 1)) == NOT)
	      s = t[2];
	    else
	      s = t[1];
	  }
	
	fputs (s, file);
      }
      return;

    case 'R':
      /* X is a CR register.  Print the mask for `mtcrf'.  */
      if (GET_CODE (x) != REG || ! CR_REGNO_P (REGNO (x)))
	output_operand_lossage ("invalid %%R value");
      else
	fprintf (file, "%d", 128 >> (REGNO (x) - CR0_REGNO));
      return;

    case 's':
      /* Low 5 bits of 32 - value */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%s value");
      else
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, (32 - INT_LOWPART (x)) & 31);
      return;

    case 'S':
      /* PowerPC64 mask position.  All 0's is excluded.
	 CONST_INT 32-bit mask is considered sign-extended so any
	 transition must occur within the CONST_INT, not on the boundary.  */
      if (! mask64_operand (x, DImode))
	output_operand_lossage ("invalid %%S value");

      uval = INT_LOWPART (x);

      if (uval & 1)	/* Clear Left */
	{
	  uval &= ((unsigned HOST_WIDE_INT) 1 << 63 << 1) - 1;
	  i = 64;
	}
      else		/* Clear Right */
	{
	  uval = ~uval;
	  uval &= ((unsigned HOST_WIDE_INT) 1 << 63 << 1) - 1;
	  i = 63;
	}
      while (uval != 0)
	--i, uval >>= 1;
      if (i < 0)
	abort ();
      fprintf (file, "%d", i);
      return;

    case 't':
      /* Like 'J' but get to the OVERFLOW/UNORDERED bit.  */
      if (GET_CODE (x) != REG || GET_MODE (x) != CCmode)
	abort ();

      /* Bit 3 is OV bit.  */
      i = 4 * (REGNO (x) - CR0_REGNO) + 3;

      /* If we want bit 31, write a shift count of zero, not 32.  */
      fprintf (file, "%d", i == 31 ? 0 : i + 1);
      return;

    case 'T':
      /* Print the symbolic name of a branch target register.  */
      if (GET_CODE (x) != REG || (REGNO (x) != LINK_REGISTER_REGNUM
				  && REGNO (x) != COUNT_REGISTER_REGNUM))
	output_operand_lossage ("invalid %%T value");
      else if (REGNO (x) == LINK_REGISTER_REGNUM)
	fputs (TARGET_NEW_MNEMONICS ? "lr" : "r", file);
      else
	fputs ("ctr", file);
      return;

    case 'u':
      /* High-order 16 bits of constant for use in unsigned operand.  */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%u value");
      else
	fprintf (file, HOST_WIDE_INT_PRINT_HEX, 
		 (INT_LOWPART (x) >> 16) & 0xffff);
      return;

    case 'v':
      /* High-order 16 bits of constant for use in signed operand.  */
      if (! INT_P (x))
	output_operand_lossage ("invalid %%v value");
      else
	fprintf (file, HOST_WIDE_INT_PRINT_HEX,
		 (INT_LOWPART (x) >> 16) & 0xffff);
      return;

    case 'U':
      /* Print `u' if this has an auto-increment or auto-decrement.  */
      if (GET_CODE (x) == MEM
	  && (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC))
	putc ('u', file);
      return;

    case 'V':
      /* Print the trap code for this operand.  */
      switch (GET_CODE (x))
	{
	case EQ:
	  fputs ("eq", file);   /* 4 */
	  break;
	case NE:
	  fputs ("ne", file);   /* 24 */
	  break;
	case LT:
	  fputs ("lt", file);   /* 16 */
	  break;
	case LE:
	  fputs ("le", file);   /* 20 */
	  break;
	case GT:
	  fputs ("gt", file);   /* 8 */
	  break;
	case GE:
	  fputs ("ge", file);   /* 12 */
	  break;
	case LTU:
	  fputs ("llt", file);  /* 2 */
	  break;
	case LEU:
	  fputs ("lle", file);  /* 6 */
	  break;
	case GTU:
	  fputs ("lgt", file);  /* 1 */
	  break;
	case GEU:
	  fputs ("lge", file);  /* 5 */
	  break;
	default:
	  abort ();
	}
      break;

    case 'w':
      /* If constant, low-order 16 bits of constant, signed.  Otherwise, write
	 normally.  */
      if (INT_P (x))
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, 
		 ((INT_LOWPART (x) & 0xffff) ^ 0x8000) - 0x8000);
      else
	print_operand (file, x, 0);
      return;

    case 'W':
      /* MB value for a PowerPC64 rldic operand.  */
      val = (GET_CODE (x) == CONST_INT
	     ? INTVAL (x) : CONST_DOUBLE_HIGH (x));

      if (val < 0)
	i = -1;
      else
	for (i = 0; i < HOST_BITS_PER_WIDE_INT; i++)
	  if ((val <<= 1) < 0)
	    break;

#if HOST_BITS_PER_WIDE_INT == 32
      if (GET_CODE (x) == CONST_INT && i >= 0)
	i += 32;  /* zero-extend high-part was all 0's */
      else if (GET_CODE (x) == CONST_DOUBLE && i == 32)
	{
	  val = CONST_DOUBLE_LOW (x);

	  if (val == 0)
	    abort ();
	  else if (val < 0)
	    --i;
	  else
	    for ( ; i < 64; i++)
	      if ((val <<= 1) < 0)
		break;
	}
#endif

      fprintf (file, "%d", i + 1);
      return;

    case 'X':
      if (GET_CODE (x) == MEM
	  && LEGITIMATE_INDEXED_ADDRESS_P (XEXP (x, 0), 0))
	putc ('x', file);
      return;

    case 'Y':
      /* Like 'L', for third word of TImode  */
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x) + 2]);
      else if (GET_CODE (x) == MEM)
	{
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    output_address (plus_constant (XEXP (XEXP (x, 0), 0), 8));
	  else
	    output_address (XEXP (adjust_address_nv (x, SImode, 8), 0));
	  if (small_data_operand (x, GET_MODE (x)))
	    fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		     reg_names[SMALL_DATA_REG]);
	}
      return;
			    
    case 'z':
      /* X is a SYMBOL_REF.  Write out the name preceded by a
	 period and without any trailing data in brackets.  Used for function
	 names.  If we are configured for System V (or the embedded ABI) on
	 the PowerPC, do not emit the period, since those systems do not use
	 TOCs and the like.  */
      if (GET_CODE (x) != SYMBOL_REF)
	abort ();

      if (XSTR (x, 0)[0] != '.')
	{
	  switch (DEFAULT_ABI)
	    {
	    default:
	      abort ();

	    case ABI_AIX:
	      putc ('.', file);
	      break;

	    case ABI_V4:
	    case ABI_AIX_NODESC:
	    case ABI_DARWIN:
	      break;
	    }
	}
#if TARGET_AIX
      RS6000_OUTPUT_BASENAME (file, XSTR (x, 0));
#else
      assemble_name (file, XSTR (x, 0));
#endif
      return;

    case 'Z':
      /* Like 'L', for last word of TImode.  */
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x) + 3]);
      else if (GET_CODE (x) == MEM)
	{
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC
	      || GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    output_address (plus_constant (XEXP (XEXP (x, 0), 0), 12));
	  else
	    output_address (XEXP (adjust_address_nv (x, SImode, 12), 0));
	  if (small_data_operand (x, GET_MODE (x)))
	    fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		     reg_names[SMALL_DATA_REG]);
	}
      return;

      /* Print AltiVec or SPE memory operand.  */
    case 'y':
      {
	rtx tmp;

	if (GET_CODE (x) != MEM)
	  abort ();

	tmp = XEXP (x, 0);

	if (TARGET_SPE)
	  {
	    /* Handle [reg].  */
	    if (GET_CODE (tmp) == REG)
	      {
		fprintf (file, "0(%s)", reg_names[REGNO (tmp)]);
		break;
	      }
	    /* Handle [reg+UIMM].  */
	    else if (GET_CODE (tmp) == PLUS &&
		     GET_CODE (XEXP (tmp, 1)) == CONST_INT)
	      {
		int x;

		if (GET_CODE (XEXP (tmp, 0)) != REG)
		  abort ();

		x = INTVAL (XEXP (tmp, 1));
		fprintf (file, "%d(%s)", x, reg_names[REGNO (XEXP (tmp, 0))]);
		break;
	      }

	    /* Fall through.  Must be [reg+reg].  */
	  }
	if (GET_CODE (tmp) == REG)
	  fprintf (file, "0,%s", reg_names[REGNO (tmp)]);
	else if (GET_CODE (tmp) == PLUS && GET_CODE (XEXP (tmp, 1)) == REG)
	  {
	    if (REGNO (XEXP (tmp, 0)) == 0)
	      fprintf (file, "%s,%s", reg_names[ REGNO (XEXP (tmp, 1)) ],
		       reg_names[ REGNO (XEXP (tmp, 0)) ]);
	    else
	      fprintf (file, "%s,%s", reg_names[ REGNO (XEXP (tmp, 0)) ],
		       reg_names[ REGNO (XEXP (tmp, 1)) ]);
	  }
	else
	  abort ();
	break;
      }
			    
    case 0:
      if (GET_CODE (x) == REG)
	fprintf (file, "%s", reg_names[REGNO (x)]);
      else if (GET_CODE (x) == MEM)
	{
	  /* We need to handle PRE_INC and PRE_DEC here, since we need to
	     know the width from the mode.  */
	  if (GET_CODE (XEXP (x, 0)) == PRE_INC)
	    fprintf (file, "%d(%s)", GET_MODE_SIZE (GET_MODE (x)),
		     reg_names[REGNO (XEXP (XEXP (x, 0), 0))]);
	  else if (GET_CODE (XEXP (x, 0)) == PRE_DEC)
	    fprintf (file, "%d(%s)", - GET_MODE_SIZE (GET_MODE (x)),
		     reg_names[REGNO (XEXP (XEXP (x, 0), 0))]);
	  else
	    output_address (XEXP (x, 0));
	}
      else
	output_addr_const (file, x);
      return;

    default:
      output_operand_lossage ("invalid %%xn code");
    }
}

/* Print the address of an operand.  */

void
print_operand_address (file, x)
     FILE *file;
     rtx x;
{
  if (GET_CODE (x) == REG)
    fprintf (file, "0(%s)", reg_names[ REGNO (x) ]);
  else if (GET_CODE (x) == SYMBOL_REF || GET_CODE (x) == CONST
	   || GET_CODE (x) == LABEL_REF)
    {
      output_addr_const (file, x);
      if (small_data_operand (x, GET_MODE (x)))
	fprintf (file, "@%s(%s)", SMALL_DATA_RELOC,
		 reg_names[SMALL_DATA_REG]);
      else if (TARGET_TOC)
	abort ();
    }
  else if (GET_CODE (x) == PLUS && GET_CODE (XEXP (x, 1)) == REG)
    {
      if (REGNO (XEXP (x, 0)) == 0)
	fprintf (file, "%s,%s", reg_names[ REGNO (XEXP (x, 1)) ],
		 reg_names[ REGNO (XEXP (x, 0)) ]);
      else
	fprintf (file, "%s,%s", reg_names[ REGNO (XEXP (x, 0)) ],
		 reg_names[ REGNO (XEXP (x, 1)) ]);
    }
  else if (GET_CODE (x) == PLUS && GET_CODE (XEXP (x, 1)) == CONST_INT)
    {
      fprintf (file, HOST_WIDE_INT_PRINT_DEC, INTVAL (XEXP (x, 1)));
      fprintf (file, "(%s)", reg_names[ REGNO (XEXP (x, 0)) ]);
    }
#if TARGET_ELF
  else if (GET_CODE (x) == LO_SUM && GET_CODE (XEXP (x, 0)) == REG
           && CONSTANT_P (XEXP (x, 1)))
    {
      output_addr_const (file, XEXP (x, 1));
      fprintf (file, "@l(%s)", reg_names[ REGNO (XEXP (x, 0)) ]);
    }
#endif
#if TARGET_MACHO
  else if (GET_CODE (x) == LO_SUM && GET_CODE (XEXP (x, 0)) == REG
           && CONSTANT_P (XEXP (x, 1)))
    {
      fprintf (file, "lo16(");
      output_addr_const (file, XEXP (x, 1));
      fprintf (file, ")(%s)", reg_names[ REGNO (XEXP (x, 0)) ]);
    }
#endif
  else if (LEGITIMATE_CONSTANT_POOL_ADDRESS_P (x))
    {
      if (TARGET_AIX && (!TARGET_ELF || !TARGET_MINIMAL_TOC))
	{
	  rtx contains_minus = XEXP (x, 1);
	  rtx minus, symref;
	  const char *name;
	  
	  /* Find the (minus (sym) (toc)) buried in X, and temporarily
	     turn it into (sym) for output_addr_const.  */
	  while (GET_CODE (XEXP (contains_minus, 0)) != MINUS)
	    contains_minus = XEXP (contains_minus, 0);

	  minus = XEXP (contains_minus, 0);
	  symref = XEXP (minus, 0);
	  XEXP (contains_minus, 0) = symref;
	  if (TARGET_ELF)
	    {
	      char *newname;

	      name = XSTR (symref, 0);
	      newname = alloca (strlen (name) + sizeof ("@toc"));
	      strcpy (newname, name);
	      strcat (newname, "@toc");
	      XSTR (symref, 0) = newname;
	    }
	  output_addr_const (file, XEXP (x, 1));
	  if (TARGET_ELF)
	    XSTR (symref, 0) = name;
	  XEXP (contains_minus, 0) = minus;
	}
      else
	output_addr_const (file, XEXP (x, 1));

      fprintf (file, "(%s)", reg_names[REGNO (XEXP (x, 0))]);
    }
  else
    abort ();
}

/* APPLE LOCAL begin weak import */
static void
find_weak_imports (x)
    rtx x;
{
  /* Patterns accepted here follow output_addr_const in final.c.  */
  switch ( GET_CODE (x))
    {
      case CONST:
      case ZERO_EXTEND:
      case SIGN_EXTEND:
      case SUBREG:
	find_weak_imports (XEXP (x, 0));
	break;
 
      case CONST_INT:
      case CONST_DOUBLE:
      case CODE_LABEL:
      case LABEL_REF:
      default:
	break;

      case PLUS:
      case MINUS:
	find_weak_imports (XEXP (x, 0));
	find_weak_imports (XEXP (x, 1));
	break;

      case SYMBOL_REF:
	if ( SYMBOL_REF_WEAK_IMPORT (x))
	  {
	    fprintf (asm_out_file, "\t.weak_reference ");
	    assemble_name (asm_out_file, XSTR (x, 0));
	    fprintf (asm_out_file, "\n");
	    /* Attempt to prevent multiple weak_reference directives. */
	    SYMBOL_REF_WEAK_IMPORT (x) = 0;
	  }
	break;
    }
}
/* APPLE LOCAL end weak import */

/* Target hook for assembling integer objects.  The PowerPC version has
   to handle fixup entries for relocatable code if RELOCATABLE_NEEDS_FIXUP
   is defined.  It also needs to handle DI-mode objects on 64-bit
   targets.  */

static bool
rs6000_assemble_integer (x, size, aligned_p)
     rtx x;
     unsigned int size;
     int aligned_p;
{
#ifdef RELOCATABLE_NEEDS_FIXUP
  /* Special handling for SI values.  */
  if (size == 4 && aligned_p)
    {
      extern int in_toc_section PARAMS ((void));
      static int recurse = 0;
      
      /* For -mrelocatable, we mark all addresses that need to be fixed up
	 in the .fixup section.  */
      if (TARGET_RELOCATABLE
	  && !in_toc_section ()
	  && !in_text_section ()
	  /* APPLE LOCAL begin - rarely executed bb optimization */
	  && !in_unlikely_text_section ()
	  /* APPLE LOCAL end - rarely executed bb optimization */
	  && !recurse
	  && GET_CODE (x) != CONST_INT
	  && GET_CODE (x) != CONST_DOUBLE
	  && CONSTANT_P (x))
	{
	  char buf[256];

	  recurse = 1;
	  ASM_GENERATE_INTERNAL_LABEL (buf, "LCP", fixuplabelno);
	  fixuplabelno++;
	  ASM_OUTPUT_LABEL (asm_out_file, buf);
	  fprintf (asm_out_file, "\t.long\t(");
	  output_addr_const (asm_out_file, x);
	  fprintf (asm_out_file, ")@fixup\n");
	  fprintf (asm_out_file, "\t.section\t\".fixup\",\"aw\"\n");
	  ASM_OUTPUT_ALIGN (asm_out_file, 2);
	  fprintf (asm_out_file, "\t.long\t");
	  assemble_name (asm_out_file, buf);
	  fprintf (asm_out_file, "\n\t.previous\n");
	  recurse = 0;
	  return true;
	}
      /* Remove initial .'s to turn a -mcall-aixdesc function
	 address into the address of the descriptor, not the function
	 itself.  */
      else if (GET_CODE (x) == SYMBOL_REF
	       && XSTR (x, 0)[0] == '.'
	       && DEFAULT_ABI == ABI_AIX)
	{
	  const char *name = XSTR (x, 0);
	  while (*name == '.')
	    name++;

	  fprintf (asm_out_file, "\t.long\t%s\n", name);
	  return true;
	}
    }
#endif /* RELOCATABLE_NEEDS_FIXUP */
  /* APPLE LOCAL weak import */
  if (DEFAULT_ABI == ABI_DARWIN)
    find_weak_imports (x);
  return default_assemble_integer (x, size, aligned_p);
}

#ifdef HAVE_GAS_HIDDEN
/* Emit an assembler directive to set symbol visibility for DECL to
   VISIBILITY_TYPE.  */

static void
rs6000_assemble_visibility (decl, vis)
     tree decl;
     int vis;
{
  /* Functions need to have their entry point symbol visibility set as
     well as their descriptor symbol visibility.  */
  if (DEFAULT_ABI == ABI_AIX && TREE_CODE (decl) == FUNCTION_DECL)
    {
      static const char * const visibility_types[] = {
        NULL, "internal", "hidden", "protected"
      };

      const char *name, *type;

      name = ((* targetm.strip_name_encoding)
	      (IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl))));
      type = visibility_types[vis];

      fprintf (asm_out_file, "\t.%s\t%s\n", type, name);
      fprintf (asm_out_file, "\t.%s\t.%s\n", type, name);
    }
  else
    default_assemble_visibility (decl, vis);
}
#endif

enum rtx_code
rs6000_reverse_condition (mode, code)
     enum machine_mode mode;
     enum rtx_code code;
{
  /* Reversal of FP compares takes care -- an ordered compare
     becomes an unordered compare and vice versa.  */
  if (mode == CCFPmode && !flag_unsafe_math_optimizations)
    return reverse_condition_maybe_unordered (code);
  else
    return reverse_condition (code);
}

/* Generate a compare for CODE.  Return a brand-new rtx that
   represents the result of the compare.  */

static rtx
rs6000_generate_compare (code)
     enum rtx_code code;
{
  enum machine_mode comp_mode;
  rtx compare_result;

  if (rs6000_compare_fp_p)
    comp_mode = CCFPmode;
  else if (code == GTU || code == LTU
	  || code == GEU || code == LEU)
    comp_mode = CCUNSmode;
  else
    comp_mode = CCmode;

  /* First, the compare.  */
  compare_result = gen_reg_rtx (comp_mode);

  /* SPE FP compare instructions on the GPRs.  Yuck!  */
  if ((TARGET_SPE && TARGET_HARD_FLOAT) && rs6000_compare_fp_p)
    {
      rtx cmp, or1, or2, or_result, compare_result2;

      switch (code)
	{
	case EQ:
	case UNEQ:
	case NE:
	case LTGT:
	  cmp = flag_unsafe_math_optimizations
	    ? gen_tstsfeq_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1)
	    : gen_cmpsfeq_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1);
	  break;
	case GT:
	case GTU:
	case UNGT:
	case UNGE:
	case GE:
	case GEU:
	  cmp = flag_unsafe_math_optimizations
	    ? gen_tstsfgt_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1)
	    : gen_cmpsfgt_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1);
	  break;
	case LT:
	case LTU:
	case UNLT:
	case UNLE:
	case LE:
	case LEU:
	  cmp = flag_unsafe_math_optimizations
	    ? gen_tstsflt_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1)
	    : gen_cmpsflt_gpr (compare_result, rs6000_compare_op0,
			       rs6000_compare_op1);
	  break;
	default:
	  abort ();
	}

      /* Synthesize LE and GE from LT/GT || EQ.  */
      if (code == LE || code == GE || code == LEU || code == GEU)
	{
	  /* Synthesize GE/LE frome GT/LT || EQ.  */

	  emit_insn (cmp);

	  switch (code)
	    {
	    case LE: code = LT; break;
	    case GE: code = GT; break;
	    case LEU: code = LT; break;
	    case GEU: code = GT; break;
	    default: abort ();
	    }

	  or1 = gen_reg_rtx (SImode);
	  or2 = gen_reg_rtx (SImode);
	  or_result = gen_reg_rtx (CCEQmode);
	  compare_result2 = gen_reg_rtx (CCFPmode);

	  /* Do the EQ.  */
	  cmp = flag_unsafe_math_optimizations
	    ? gen_tstsfeq_gpr (compare_result2, rs6000_compare_op0,
			       rs6000_compare_op1)
	    : gen_cmpsfeq_gpr (compare_result2, rs6000_compare_op0,
			       rs6000_compare_op1);
	  emit_insn (cmp);

	  /* The MC8540 FP compare instructions set the CR bits
	     differently than other PPC compare instructions.  For
	     that matter, there is no generic test instruction, but a
	     testgt, testlt, and testeq.  For a true condition, bit 2
	     is set (x1xx) in the CR.  Following the traditional CR
	     values:

	     LT    GT    EQ    OV
	     bit3  bit2  bit1  bit0

	     ... bit 2 would be a GT CR alias, so later on we
	     look in the GT bits for the branch instructins.
	     However, we must be careful to emit correct RTL in
	     the meantime, so optimizations don't get confused.  */

	  or1 = gen_rtx (NE, SImode, compare_result, const0_rtx);
	  or2 = gen_rtx (NE, SImode, compare_result2, const0_rtx);

	  /* OR them together.  */
	  cmp = gen_rtx_SET (VOIDmode, or_result,
			     gen_rtx_COMPARE (CCEQmode,
					      gen_rtx_IOR (SImode, or1, or2),
					      const_true_rtx));
	  compare_result = or_result;
	  code = EQ;
	}
      else
	{
	  /* We only care about 1 bit (x1xx), so map everything to NE to
	     maintain rtl sanity.  We'll get to the right bit (x1xx) at
	     code output time.  */
	  if (code == NE || code == LTGT)
	    /* Do the inverse here because we have no cmpne
	       instruction.  We use the cmpeq instruction and expect
	       to get a 0 instead.  */
	    code = EQ;
	  else
	    code = NE;
	}

      emit_insn (cmp);
    }
  else
    emit_insn (gen_rtx_SET (VOIDmode, compare_result,
			    gen_rtx_COMPARE (comp_mode,
					     rs6000_compare_op0, 
					     rs6000_compare_op1)));
  
  /* Some kinds of FP comparisons need an OR operation;
     except for flag_unsafe_math_optimizations we don't bother.  */
  if (rs6000_compare_fp_p
      && ! flag_unsafe_math_optimizations
      && ! (TARGET_HARD_FLOAT && TARGET_SPE)
      && (code == LE || code == GE
	  || code == UNEQ || code == LTGT
	  || code == UNGT || code == UNLT))
    {
      enum rtx_code or1, or2;
      rtx or1_rtx, or2_rtx, compare2_rtx;
      rtx or_result = gen_reg_rtx (CCEQmode);
      
      switch (code)
	{
	case LE: or1 = LT;  or2 = EQ;  break;
	case GE: or1 = GT;  or2 = EQ;  break;
	case UNEQ: or1 = UNORDERED;  or2 = EQ;  break;
	case LTGT: or1 = LT;  or2 = GT;  break;
	case UNGT: or1 = UNORDERED;  or2 = GT;  break;
	case UNLT: or1 = UNORDERED;  or2 = LT;  break;
	default:  abort ();
	}
      validate_condition_mode (or1, comp_mode);
      validate_condition_mode (or2, comp_mode);
      or1_rtx = gen_rtx (or1, SImode, compare_result, const0_rtx);
      or2_rtx = gen_rtx (or2, SImode, compare_result, const0_rtx);
      compare2_rtx = gen_rtx_COMPARE (CCEQmode,
				      gen_rtx_IOR (SImode, or1_rtx, or2_rtx),
				      const_true_rtx);
      emit_insn (gen_rtx_SET (VOIDmode, or_result, compare2_rtx));

      compare_result = or_result;
      code = EQ;
    }

  validate_condition_mode (code, GET_MODE (compare_result));
  
  return gen_rtx (code, VOIDmode, compare_result, const0_rtx);
}


/* Emit the RTL for an sCOND pattern.  */

void
rs6000_emit_sCOND (code, result)
     enum rtx_code code;
     rtx result;
{
  rtx condition_rtx;
  enum machine_mode op_mode;

  condition_rtx = rs6000_generate_compare (code);

  op_mode = GET_MODE (rs6000_compare_op0);
  if (op_mode == VOIDmode)
    op_mode = GET_MODE (rs6000_compare_op1);

  if (TARGET_POWERPC64 && (op_mode == DImode || rs6000_compare_fp_p))
    {
      PUT_MODE (condition_rtx, DImode);
      convert_move (result, condition_rtx, 0);
    }
  else
    {
      PUT_MODE (condition_rtx, SImode);
      emit_insn (gen_rtx_SET (VOIDmode, result, condition_rtx));
    }
}

/* Emit a branch of kind CODE to location LOC.  */

void
rs6000_emit_cbranch (code, loc)
     enum rtx_code code;
     rtx loc;
{
  rtx condition_rtx, loc_ref;

  condition_rtx = rs6000_generate_compare (code);
  loc_ref = gen_rtx_LABEL_REF (VOIDmode, loc);
  emit_jump_insn (gen_rtx_SET (VOIDmode, pc_rtx,
			       gen_rtx_IF_THEN_ELSE (VOIDmode, condition_rtx,
						     loc_ref, pc_rtx)));
}

/* APPLE LOCAL begin - add 970 branch prediction for bdxx insns */

char *
output_bdxx_branch (label, attr_len, version, insn)
     const char *label;
     int attr_len;
     int version;
     rtx insn;
{
  static char string[64];
  char *s = string;
  const char *pred;
  const char *rev_pred; 
  rtx note;
  int threshold = (branch_predictions_threshold > 50) ? 
                               (branch_predictions_threshold - 50) : 
                               branch_predictions_threshold;

  /* Maybe we have a guess as to how likely the branch is.  
     The old mnemonics don't have a way to specify this information.  */
  pred = "";
  rev_pred = "";
  note = find_reg_note (insn, REG_BR_PROB, NULL_RTX);
  if (note != NULL_RTX)
    {
      /* PROB is the difference from 50%.  */
      int prob = INTVAL (XEXP (note, 0)) - REG_BR_PROB_BASE / 2;
      bool always_hint = rs6000_cpu != PROCESSOR_POWER4;

      /* Only hint for highly probable/improbable branches on newer
	 cpus as static prediction overrides processor dynamic
	 prediction.  For older cpus we may as well always hint, but
	 assume not taken for branches that are very close to 50% as a
	 mispredicted taken branch is more expensive than a
	 mispredicted not-taken branch.  */ 
      if (flag_branch_predictions
	  && (always_hint || abs (prob) > REG_BR_PROB_BASE / 100 * threshold))
	{
	  if (abs (prob) > REG_BR_PROB_BASE / 20
	      && (prob > 0))
	    {
	      if (DEFAULT_ABI == ABI_DARWIN
		  && rs6000_cpu == PROCESSOR_POWER4)
		{
		  pred = "++";
		  rev_pred = "--";
		}
	      else
		{
		  pred = "+";
		  rev_pred = "-";
		}
	    }
	  else
	    {
	      if (DEFAULT_ABI == ABI_DARWIN
		  && rs6000_cpu == PROCESSOR_POWER4)
		{
		  pred = "--";
		  rev_pred = "++";
		}
	      else
		{
		  pred = "-";
		  rev_pred = "+";
		}
	    }
	}
    }

  if (! version)
    {
      if (attr_len == 4)
	{
	  s += sprintf (s, "{bdn%s|bdnz%s} %s", pred, pred, label);
	}
      else
	{
	  s += sprintf (s, "bdz%s $+8\n\tb %s", rev_pred, label);
	}
    }
  else if (version == 1)
    {
      if (attr_len == 4)
	{
	  s += sprintf (s, "bdz%s %s", pred, label);
	}
      else
	{
	  s += sprintf (s, "{bdn%s|bdnz%s} $+8\n\tb %s", rev_pred, rev_pred, 
			label);
	}
    }
    
  return  string;

}

/* APPLE LOCAL end - add 970 branch prediction for bdxx insns */

/* Return the string to output a conditional branch to LABEL, which is
   the operand number of the label, or -1 if the branch is really a
   conditional return.  

   OP is the conditional expression.  XEXP (OP, 0) is assumed to be a
   condition code register and its mode specifies what kind of
   comparison we made.

   REVERSED is nonzero if we should reverse the sense of the comparison.

   INSN is the insn.  */

char *
output_cbranch (op, label, reversed, insn)
     rtx op;
     const char * label;
     int reversed;
     rtx insn;
{
  static char string[64];
  enum rtx_code code = GET_CODE (op);
  rtx cc_reg = XEXP (op, 0);
  enum machine_mode mode = GET_MODE (cc_reg);
  int cc_regno = REGNO (cc_reg) - CR0_REGNO;
  int need_longbranch = label != NULL && get_attr_length (insn) == 8;
  int really_reversed = reversed ^ need_longbranch;
  char *s = string;
  const char *ccode;
  const char *pred;
  rtx note;
  int threshold = (branch_predictions_threshold > 50) ? 
                               (branch_predictions_threshold - 50) : 
                               branch_predictions_threshold;

  validate_condition_mode (code, mode);

  /* Work out which way this really branches.  We could use
     reverse_condition_maybe_unordered here always but this
     makes the resulting assembler clearer.  */
  if (really_reversed)
    {
      /* Reversal of FP compares takes care -- an ordered compare
	 becomes an unordered compare and vice versa.  */
      if (mode == CCFPmode)
	code = reverse_condition_maybe_unordered (code);
      else
	code = reverse_condition (code);
    }

  if ((TARGET_SPE && TARGET_HARD_FLOAT) && mode == CCFPmode)
    {
      /* The efscmp/tst* instructions twiddle bit 2, which maps nicely
	 to the GT bit.  */
      if (code == EQ)
	/* Opposite of GT.  */
	code = UNLE;
      else if (code == NE)
	code = GT;
      else
	abort ();
    }

  switch (code)
    {
      /* Not all of these are actually distinct opcodes, but
	 we distinguish them for clarity of the resulting assembler.  */
    case NE: case LTGT:
      ccode = "ne"; break;
    case EQ: case UNEQ:
      ccode = "eq"; break;
    case GE: case GEU: 
      ccode = "ge"; break;
    case GT: case GTU: case UNGT: 
      ccode = "gt"; break;
    case LE: case LEU: 
      ccode = "le"; break;
    case LT: case LTU: case UNLT: 
      ccode = "lt"; break;
    case UNORDERED: ccode = "un"; break;
    case ORDERED: ccode = "nu"; break;
    case UNGE: ccode = "nl"; break;
    case UNLE: ccode = "ng"; break;
    default:
      abort ();
    }
  
  /* Maybe we have a guess as to how likely the branch is.  
     The old mnemonics don't have a way to specify this information.  */
  pred = "";
  note = find_reg_note (insn, REG_BR_PROB, NULL_RTX);
  if (note != NULL_RTX)
    {
      /* PROB is the difference from 50%.  */
      int prob = INTVAL (XEXP (note, 0)) - REG_BR_PROB_BASE / 2;
      bool always_hint = rs6000_cpu != PROCESSOR_POWER4;

      /* Only hint for highly probable/improbable branches on newer
	 cpus as static prediction overrides processor dynamic
	 prediction.  For older cpus we may as well always hint, but
	 assume not taken for branches that are very close to 50% as a
	 mispredicted taken branch is more expensive than a
	 mispredicted not-taken branch.  */ 
      if (flag_branch_predictions
	  && (always_hint || abs (prob) > REG_BR_PROB_BASE / 100 * threshold))
	{
	  if (abs (prob) > REG_BR_PROB_BASE / 20
	      && ((prob > 0) ^ need_longbranch))
	    {
	      if (DEFAULT_ABI == ABI_DARWIN
		  && rs6000_cpu == PROCESSOR_POWER4)
	        pred = "++";
	      else
	        pred = "+";
	    }
	  else
	    {
	      if (DEFAULT_ABI == ABI_DARWIN
		  && rs6000_cpu == PROCESSOR_POWER4)
	        pred = "--";
	      else
	        pred = "-";
	    }
	}
    }

  if (label == NULL)
    s += sprintf (s, "{b%sr|b%slr%s} ", ccode, ccode, pred);
  else
    s += sprintf (s, "{b%s|b%s%s} ", ccode, ccode, pred);

  /* We need to escape any '%' characters in the reg_names string.
     Assume they'd only be the first character...  */
  if (reg_names[cc_regno + CR0_REGNO][0] == '%')
    *s++ = '%';
  s += sprintf (s, "%s", reg_names[cc_regno + CR0_REGNO]);

  if (label != NULL)
    {
      /* If the branch distance was too far, we may have to use an
	 unconditional branch to go the distance.  */
      if (need_longbranch)
	s += sprintf (s, ",$+8\n\tb %s", label);
      else
	s += sprintf (s, ",%s", label);
    }

  return string;
}

/* APPLE LOCAL begin AltiVec */
/* ALLOC_VOLATILE_REG allocates a volatile register AFTER all gcc
   register allocations have been done; we use it to reserve an
   unused reg for holding VRsave.  Returns -1 in case of failure (all
   volatile regs are in use.)  */
/* Note, this is called from both the prologue and epilogue code,
   with the assumption that it will return the same result both
   times!  Since the register arrays are not changed in between
   this is valid, if a bit fragile.  */
/* In future we may also use this to grab an unused volatile reg to
   hold the PIC base reg in the event that the current function makes
   no procedure calls; this was done in 2.95.  */
static int
alloc_volatile_reg ()
{
  if (current_function_is_leaf
      && reload_completed
      && !cfun->machine->ra_needs_full_frame)
    {
      int r;
      for (r = 10; r >= 2; --r)
	if (! fixed_regs[r] && ! regs_ever_live[r])
	  return r;
    }

  return -1;					/* fail  */
}
/* APPLE LOCAL end AltiVec */

/* Emit a conditional move: move TRUE_COND to DEST if OP of the
   operands of the last comparison is nonzero/true, FALSE_COND if it
   is zero/false.  Return 0 if the hardware has no such operation.  */

int
rs6000_emit_cmove (dest, op, true_cond, false_cond)
     rtx dest;
     rtx op;
     rtx true_cond;
     rtx false_cond;
{
  enum rtx_code code = GET_CODE (op);
  rtx op0 = rs6000_compare_op0;
  rtx op1 = rs6000_compare_op1;
  REAL_VALUE_TYPE c1;
  enum machine_mode compare_mode = GET_MODE (op0);
  enum machine_mode result_mode = GET_MODE (dest);
  rtx temp;

  /* These modes should always match. */
  if (GET_MODE (op1) != compare_mode
      /* In the isel case however, we can use a compare immediate, so
	 op1 may be a small constant.  */
      && (!TARGET_ISEL || !short_cint_operand (op1, VOIDmode)))
    return 0;
  if (GET_MODE (true_cond) != result_mode)
    return 0;
  if (GET_MODE (false_cond) != result_mode)
    return 0;

  /* First, work out if the hardware can do this at all, or
     if it's too slow...  */
  if (! rs6000_compare_fp_p)
    {
      if (TARGET_ISEL)
	return rs6000_emit_int_cmove (dest, op, true_cond, false_cond);
      return 0;
    }

  /* Eliminate half of the comparisons by switching operands, this
     makes the remaining code simpler.  */
  if (code == UNLT || code == UNGT || code == UNORDERED || code == NE
      || code == LTGT || code == LT)
    {
      code = reverse_condition_maybe_unordered (code);
      temp = true_cond;
      true_cond = false_cond;
      false_cond = temp;
    }

  /* UNEQ and LTGT take four instructions for a comparison with zero,
     it'll probably be faster to use a branch here too.  */
  if (code == UNEQ)
    return 0;
  
  if (GET_CODE (op1) == CONST_DOUBLE)
    REAL_VALUE_FROM_CONST_DOUBLE (c1, op1);
    
  /* We're going to try to implement comparions by performing
     a subtract, then comparing against zero.  Unfortunately,
     Inf - Inf is NaN which is not zero, and so if we don't
     know that the operand is finite and the comparison
     would treat EQ different to UNORDERED, we can't do it.  */
  if (! flag_unsafe_math_optimizations
      && code != GT && code != UNGE
      && (GET_CODE (op1) != CONST_DOUBLE || real_isinf (&c1))
      /* Constructs of the form (a OP b ? a : b) are safe.  */
      && ((! rtx_equal_p (op0, false_cond) && ! rtx_equal_p (op1, false_cond))
	  || (! rtx_equal_p (op0, true_cond) 
	      && ! rtx_equal_p (op1, true_cond))))
    return 0;
  /* At this point we know we can use fsel.  */

  /* Reduce the comparison to a comparison against zero.  */
  temp = gen_reg_rtx (compare_mode);
  emit_insn (gen_rtx_SET (VOIDmode, temp,
			  gen_rtx_MINUS (compare_mode, op0, op1)));
  op0 = temp;
  op1 = CONST0_RTX (compare_mode);

  /* If we don't care about NaNs we can reduce some of the comparisons
     down to faster ones.  */
  if (flag_unsafe_math_optimizations)
    switch (code)
      {
      case GT:
	code = LE;
	temp = true_cond;
	true_cond = false_cond;
	false_cond = temp;
	break;
      case UNGE:
	code = GE;
	break;
      case UNEQ:
	code = EQ;
	break;
      default:
	break;
      }

  /* Now, reduce everything down to a GE.  */
  switch (code)
    {
    case GE:
      break;

    case LE:
      temp = gen_reg_rtx (compare_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp, gen_rtx_NEG (compare_mode, op0)));
      op0 = temp;
      break;

    case ORDERED:
      temp = gen_reg_rtx (compare_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp, gen_rtx_ABS (compare_mode, op0)));
      op0 = temp;
      break;

    case EQ:
      temp = gen_reg_rtx (compare_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp, 
			      gen_rtx_NEG (compare_mode,
					   gen_rtx_ABS (compare_mode, op0))));
      op0 = temp;
      break;

    case UNGE:
      temp = gen_reg_rtx (result_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp,
			      gen_rtx_IF_THEN_ELSE (result_mode,
						    gen_rtx_GE (VOIDmode,
								op0, op1),
						    true_cond, false_cond)));
      false_cond = temp;
      true_cond = false_cond;

      temp = gen_reg_rtx (compare_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp, gen_rtx_NEG (compare_mode, op0)));
      op0 = temp;
      break;

    case GT:
      temp = gen_reg_rtx (result_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp,
			      gen_rtx_IF_THEN_ELSE (result_mode, 
						    gen_rtx_GE (VOIDmode,
								op0, op1),
						    true_cond, false_cond)));
      true_cond = temp;
      false_cond = true_cond;

      temp = gen_reg_rtx (compare_mode);
      emit_insn (gen_rtx_SET (VOIDmode, temp, gen_rtx_NEG (compare_mode, op0)));
      op0 = temp;
      break;

    default:
      abort ();
    }

  emit_insn (gen_rtx_SET (VOIDmode, dest,
			  gen_rtx_IF_THEN_ELSE (result_mode,
						gen_rtx_GE (VOIDmode,
							    op0, op1),
						true_cond, false_cond)));
  return 1;
}

/* Same as above, but for ints (isel).  */

static int
rs6000_emit_int_cmove (dest, op, true_cond, false_cond)
     rtx dest;
     rtx op;
     rtx true_cond;
     rtx false_cond;
{
  rtx condition_rtx, cr;

  /* All isel implementations thus far are 32-bits.  */
  if (GET_MODE (rs6000_compare_op0) != SImode)
    return 0;

  /* We still have to do the compare, because isel doesn't do a
     compare, it just looks at the CRx bits set by a previous compare
     instruction.  */
  condition_rtx = rs6000_generate_compare (GET_CODE (op));
  cr = XEXP (condition_rtx, 0);

  if (GET_MODE (cr) == CCmode)
    emit_insn (gen_isel_signed (dest, condition_rtx,
				true_cond, false_cond, cr));
  else
    emit_insn (gen_isel_unsigned (dest, condition_rtx,
				  true_cond, false_cond, cr));

  return 1;
}

const char *
output_isel (operands)
     rtx *operands;
{
  enum rtx_code code;

  code = GET_CODE (operands[1]);
  if (code == GE || code == GEU || code == LE || code == LEU || code == NE)
    {
      PUT_CODE (operands[1], reverse_condition (code));
      return "isel %0,%3,%2,%j1";
    }
  else
    return "isel %0,%2,%3,%j1";
}

void
rs6000_emit_minmax (dest, code, op0, op1)
     rtx dest;
     enum rtx_code code;
     rtx op0;
     rtx op1;
{
  enum machine_mode mode = GET_MODE (op0);
  enum rtx_code c;
  rtx target;

  if (code == SMAX || code == SMIN)
    c = GE;
  else
    c = GEU;

  if (code == SMAX || code == UMAX)
    target = emit_conditional_move (dest, c, op0, op1, mode, 
				    op0, op1, mode, 0);
  else
    target = emit_conditional_move (dest, c, op0, op1, mode, 
				    op1, op0, mode, 0);
  if (target == NULL_RTX)
    abort ();
  if (target != dest)
    emit_move_insn (dest, target);
}

/* This page contains routines that are used to determine what the
   function prologue and epilogue code will do and write them out.  */

/* Return the first fixed-point register that is required to be
   saved. 32 if none.  */

int
first_reg_to_save ()
{
  int first_reg;

  /* Find lowest numbered live register.  */
  for (first_reg = 13; first_reg <= 31; first_reg++)
    if (regs_ever_live[first_reg] 
	&& (! call_used_regs[first_reg]
	    || (first_reg == RS6000_PIC_OFFSET_TABLE_REGNUM
		&& ((DEFAULT_ABI == ABI_V4 && flag_pic != 0)
		    || (DEFAULT_ABI == ABI_DARWIN && flag_pic)))))
      break;

#if TARGET_MACHO
  if (flag_pic
      && current_function_uses_pic_offset_table
      /* APPLE LOCAL volatile pic base reg in leaves */
      && cfun->machine->substitute_pic_base_reg == -1
      && first_reg > RS6000_PIC_OFFSET_TABLE_REGNUM)
    return RS6000_PIC_OFFSET_TABLE_REGNUM;
#endif

  return first_reg;
}

/* Similar, for FP regs.  */

int
first_fp_reg_to_save ()
{
  int first_reg;

  /* Find lowest numbered live register.  */
  for (first_reg = 14 + 32; first_reg <= 63; first_reg++)
    if (regs_ever_live[first_reg])
      break;

  return first_reg;
}

/* Similar, for AltiVec regs.  */

static int
first_altivec_reg_to_save ()
{
  int i;

  /* APPLE LOCAL begin AltiVec */
  /* Consider none to be live if -faltivec isn't specified.  */

  /* Calling setjmp while C++ exceptions are enabled results in undefined
     behavior in the DWARF stack unwinder.  Temporary workaround to stop
     calls to save_VEC being generated when -faltivec is not specified.
     This actually needs to be done whether exception info is being
     generated or not, as any code calling setjmp will attempt to save
     the vector regs (they are marked used: a mistake, IMO.)  */

  if (!flag_altivec && DEFAULT_ABI == ABI_DARWIN
      && current_function_calls_setjmp)
    return LAST_ALTIVEC_REGNO + 1;

  if (!flag_altivec
      && (DEFAULT_ABI != ABI_DARWIN
	  /* When targeting Darwin, which does the SAVE/RESTORE WORLD stuff
	     when builtin_setjmp, etc., is called, having "live" vector regs
	     when flag_altivec is zero is perfectly OK.  But we should be
	     using setjmp (), and *ALL* vector regs must be live, which
	     we take as v20.  */
	  || (!current_function_calls_setjmp
	      && !regs_ever_live[FIRST_SAVED_ALTIVEC_REGNO])))
    return LAST_ALTIVEC_REGNO + 1;
  /* APPLE LOCAL end AltiVec */

  /* Stack frame remains as is unless we are in AltiVec ABI.  */
  if (! TARGET_ALTIVEC_ABI)
    return LAST_ALTIVEC_REGNO + 1;

  /* Find lowest numbered live register.  */
  for (i = FIRST_ALTIVEC_REGNO + 20; i <= LAST_ALTIVEC_REGNO; ++i)
    if (regs_ever_live[i])
      break;

  return i;
}

/* Return a 32-bit mask of the AltiVec registers we need to set in
   VRSAVE.  Bit n of the return value is 1 if Vn is live.  The MSB in
   the 32-bit word is 0.  */

static unsigned int
compute_vrsave_mask ()
{
  unsigned int i, mask = 0;

  /* First, find out if we use _any_ altivec registers.  */
  for (i = FIRST_ALTIVEC_REGNO; i <= LAST_ALTIVEC_REGNO; ++i)
    if (regs_ever_live[i])
      mask |= ALTIVEC_REG_BIT (i);

  if (mask == 0)
    return mask;

  /* APPLE LOCAL setting of all callee-saved regs removed */

  /* Next, remove the argument registers from the set.  These must
     be in the VRSAVE mask set by the caller, so we don't need to add
     them in again.  More importantly, the mask we compute here is
     used to generate CLOBBERs in the set_vrsave insn, and we do not
     wish the argument registers to die.  */
  /* APPLE LOCAL subtract 1 */
  for (i = cfun->args_info.vregno - 1; i >= ALTIVEC_ARG_MIN_REG; --i)
    mask &= ~ALTIVEC_REG_BIT (i);

  /* Similarly, remove the return value from the set.  */
  {
    bool yes = false;
    diddle_return_value (is_altivec_return_reg, &yes);
    if (yes)
      mask &= ~ALTIVEC_REG_BIT (ALTIVEC_ARG_RETURN);
  }

  return mask;
}

/* APPLE LOCAL AltiVec */
/* Only permit Altivec registers to be renamed if both candidates
   have their VRsave mask bits set, are parameter regs, or are the return
   value reg.  Other Altivec regs are subject to destruction by the OS. */

static int
altivec_hard_regno_rename_ok (reg)
    int reg;
{
  unsigned int mask;
  bool yes = false;
  diddle_return_value (is_altivec_return_reg, &yes);
  if (yes && reg == ALTIVEC_ARG_RETURN)
    return 1;
  if ( reg >= ALTIVEC_ARG_MIN_REG 
       && reg <= cfun->args_info.vregno - 1)
    return 1;
  mask = compute_vrsave_mask ();
  if ( mask & ALTIVEC_REG_BIT (reg))
    return 1;
  return 0;
}

int
hard_regno_rename_ok (reg1, reg2)
    int reg1;
    int reg2;
{
  if (TARGET_ALTIVEC
      && reg1 >= FIRST_ALTIVEC_REGNO && reg1 <= LAST_ALTIVEC_REGNO
      && reg2 >= FIRST_ALTIVEC_REGNO && reg2 <= LAST_ALTIVEC_REGNO)
    {
      if (altivec_hard_regno_rename_ok (reg1)
	  && altivec_hard_regno_rename_ok (reg2))
	return 1;
      return 0;
    }
  return 1;
}
/* APPLE LOCAL end AltiVec */

static void
is_altivec_return_reg (reg, xyes)
     rtx reg;
     void *xyes;
{
  bool *yes = (bool *) xyes;
  if (REGNO (reg) == ALTIVEC_ARG_RETURN)
    *yes = true;
}


/* Calculate the stack information for the current function.  This is
   complicated by having two separate calling sequences, the AIX calling
   sequence and the V.4 calling sequence.

   AIX (and Darwin/Mac OS X) stack frames look like:
							  32-bit  64-bit
	SP---->	+---------------------------------------+
		| back chain to caller			| 0	  0
		+---------------------------------------+
		| saved CR				| 4       8 (8-11)
		+---------------------------------------+
		| saved LR				| 8       16
		+---------------------------------------+
		| reserved for compilers		| 12      24
		+---------------------------------------+
		| reserved for binders			| 16      32
		+---------------------------------------+
		| saved TOC pointer			| 20      40
		+---------------------------------------+
		| Parameter save area (P)		| 24      48
		+---------------------------------------+
		| Alloca space (A)			| 24+P    etc.
		+---------------------------------------+
		| Local variable space (L)		| 24+P+A
		+---------------------------------------+
		| Float/int conversion temporary (X)	| 24+P+A+L
		+---------------------------------------+
		| Save area for AltiVec registers (W)	| 24+P+A+L+X
		+---------------------------------------+
		| AltiVec alignment padding (Y)		| 24+P+A+L+X+W
		+---------------------------------------+
		| Save area for VRSAVE register (Z)	| 24+P+A+L+X+W+Y
		+---------------------------------------+
		| Save area for GP registers (G)	| 24+P+A+X+L+X+W+Y+Z
		+---------------------------------------+
		| Save area for FP registers (F)	| 24+P+A+X+L+X+W+Y+Z+G
		+---------------------------------------+
	old SP->| back chain to caller's caller		|
		+---------------------------------------+

   The required alignment for AIX configurations is two words (i.e., 8
   or 16 bytes).
   APPLE LOCAL darwin native
   For Darwin/Mac OS X, it is 16 bytes.


   V.4 stack frames look like:

	SP---->	+---------------------------------------+
		| back chain to caller			| 0
		+---------------------------------------+
		| caller's saved LR			| 4
		+---------------------------------------+
		| Parameter save area (P)		| 8
		+---------------------------------------+
		| Alloca space (A)			| 8+P
		+---------------------------------------+    
		| Varargs save area (V)			| 8+P+A
		+---------------------------------------+    
		| Local variable space (L)		| 8+P+A+V
		+---------------------------------------+    
		| Float/int conversion temporary (X)	| 8+P+A+V+L
		+---------------------------------------+
		| Save area for AltiVec registers (W)	| 8+P+A+V+L+X
		+---------------------------------------+
		| AltiVec alignment padding (Y)		| 8+P+A+V+L+X+W
		+---------------------------------------+
		| Save area for VRSAVE register (Z)	| 8+P+A+V+L+X+W+Y
		+---------------------------------------+
                | SPE: area for 64-bit GP registers     |
                +---------------------------------------+
                | SPE alignment padding                 |
                +---------------------------------------+
		| saved CR (C)				| 8+P+A+V+L+X+W+Y+Z
		+---------------------------------------+    
		| Save area for GP registers (G)	| 8+P+A+V+L+X+W+Y+Z+C
		+---------------------------------------+    
		| Save area for FP registers (F)	| 8+P+A+V+L+X+W+Y+Z+C+G
		+---------------------------------------+
	old SP->| back chain to caller's caller		|
		+---------------------------------------+

   The required alignment for V.4 is 16 bytes, or 8 bytes if -meabi is
   given.  (But note below and in sysv4.h that we require only 8 and
   may round up the size of our stack frame anyways.  The historical
   reason is early versions of powerpc-linux which didn't properly
   align the stack at program startup.  A happy side-effect is that
   -mno-eabi libraries can be used with -meabi programs.)

   The EABI configuration defaults to the V.4 layout, unless
   -mcall-aix is used, in which case the AIX layout is used.  However,
   the stack alignment requirements may differ.  If -mno-eabi is not
   given, the required stack alignment is 8 bytes; if -mno-eabi is
   given, the required alignment is 16 bytes.  (But see V.4 comment
   above.)  */

#ifndef ABI_STACK_BOUNDARY
#define ABI_STACK_BOUNDARY STACK_BOUNDARY
#endif

rs6000_stack_t *
rs6000_stack_info ()
{
  static rs6000_stack_t info, zero_info;
  rs6000_stack_t *info_ptr = &info;
  int reg_size = (TARGET_POWERPC64 && !TARGET_32BIT) ? 8 : 4;
  enum rs6000_abi abi;
  int ehrd_size;
  int total_raw_size;

  /* Zero all fields portably.  */
  info = zero_info;

  /* Select which calling sequence.  */
  info_ptr->abi = abi = DEFAULT_ABI;

  /* Calculate which registers need to be saved & save area size.  */
  info_ptr->first_gp_reg_save = first_reg_to_save ();
  /* Assume that we will have to save RS6000_PIC_OFFSET_TABLE_REGNUM, 
     even if it currently looks like we won't.  */
  if (((TARGET_TOC && TARGET_MINIMAL_TOC)
       || (flag_pic == 1 && abi == ABI_V4)
       || (flag_pic && abi == ABI_DARWIN))
      && info_ptr->first_gp_reg_save > RS6000_PIC_OFFSET_TABLE_REGNUM)
    info_ptr->gp_size = reg_size * (32 - RS6000_PIC_OFFSET_TABLE_REGNUM);
  else
    info_ptr->gp_size = reg_size * (32 - info_ptr->first_gp_reg_save);

  /* For the SPE, we have an additional upper 32-bits on each GPR.
     Ideally we should save the entire 64-bits only when the upper
     half is used in SIMD instructions.  Since we only record
     registers live (not the size they are used in), this proves
     difficult because we'd have to traverse the instruction chain at
     the right time, taking reload into account.  This is a real pain,
     so we opt to save the GPRs in 64-bits always.  Anyone overly
     concerned with frame size can fix this.  ;-).

     So... since we save all GPRs (except the SP) in 64-bits, the
     traditional GP save area will be empty.  */
  if (TARGET_SPE_ABI)
    info_ptr->gp_size = 0;

  info_ptr->first_fp_reg_save = first_fp_reg_to_save ();
  info_ptr->fp_size = 8 * (64 - info_ptr->first_fp_reg_save);

  info_ptr->first_altivec_reg_save = first_altivec_reg_to_save ();
  info_ptr->altivec_size = 16 * (LAST_ALTIVEC_REGNO + 1
				 - info_ptr->first_altivec_reg_save);

  /* Does this function call anything?  */
  info_ptr->calls_p = (! current_function_is_leaf
		       || cfun->machine->ra_needs_full_frame);

  /* Determine if we need to save the link register.  */
  if (rs6000_ra_ever_killed ()
      || (DEFAULT_ABI == ABI_AIX && current_function_profile)
#ifdef TARGET_RELOCATABLE
      || (TARGET_RELOCATABLE && (get_pool_size () != 0))
#endif
      || (info_ptr->first_fp_reg_save != 64
	  && !FP_SAVE_INLINE (info_ptr->first_fp_reg_save))
      /* APPLE LOCAL begin AltiVec */
      || (info_ptr->first_altivec_reg_save <= LAST_ALTIVEC_REGNO
	  && !VECTOR_SAVE_INLINE (info_ptr->first_altivec_reg_save))
      /* APPLE LOCAL end AltiVec */
      || (abi == ABI_V4 && current_function_calls_alloca)
      /* APPLE LOCAL test for flag_pic deleted deliberately */
      || info_ptr->calls_p)
    {
      info_ptr->lr_save_p = 1;
      regs_ever_live[LINK_REGISTER_REGNUM] = 1;
    }

  /* Determine if we need to save the condition code registers.  */
  if (regs_ever_live[CR2_REGNO] 
      || regs_ever_live[CR3_REGNO]
      || regs_ever_live[CR4_REGNO])
    {
      info_ptr->cr_save_p = 1;
      if (abi == ABI_V4)
	info_ptr->cr_size = reg_size;
    }

  /* If the current function calls __builtin_eh_return, then we need
     to allocate stack space for registers that will hold data for
     the exception handler.  */
  if (current_function_calls_eh_return)
    {
      unsigned int i;
      for (i = 0; EH_RETURN_DATA_REGNO (i) != INVALID_REGNUM; ++i)
	continue;

      /* SPE saves EH registers in 64-bits.  */
      ehrd_size = i * (TARGET_SPE_ABI ? UNITS_PER_SPE_WORD : ABI_UNITS_PER_WORD);
    }
  else
    ehrd_size = 0;

  /* Determine various sizes.  */
  info_ptr->reg_size     = reg_size;
  info_ptr->fixed_size   = RS6000_SAVE_AREA;
  info_ptr->varargs_size = RS6000_VARARGS_AREA;
  /* APPLE LOCAL AltiVec */
  /* TODO: Not considering 16-byte alignment here.  */
  info_ptr->vars_size    = RS6000_ALIGN (get_frame_size (), 8);
  info_ptr->parm_size    = RS6000_ALIGN (current_function_outgoing_args_size,
					 8);

  if (TARGET_SPE_ABI)
    info_ptr->spe_gp_size = 8 * (32 - info_ptr->first_gp_reg_save);
  else
    info_ptr->spe_gp_size = 0;

  /* APPLE LOCAL AltiVec */
  if (0 /*TARGET_ALTIVEC_ABI && TARGET_ALTIVEC_VRSAVE*/)
    {
      info_ptr->vrsave_mask = compute_vrsave_mask ();
      info_ptr->vrsave_size  = info_ptr->vrsave_mask ? 4 : 0;
    }
  else
    {
      info_ptr->vrsave_mask = 0;
      info_ptr->vrsave_size = 0;
    }

  /* APPLE LOCAL begin AltiVec */
  /* If nothing else is being saved, see if we can avoid saving VRsave
     on the stack by using a volatile register to hold it.  */
  if (flag_altivec && compute_vrsave_mask ())
    {
      info_ptr->vrsave_save_p = 1;	/* Need to save VRsave.  */

      /* If we're not calling other functions, we can stash VRsave in
	 a free volatile register.  */
      if (info_ptr->lr_save_p == 0
	  && alloc_volatile_reg () > 0)
	info_ptr->vrsave_size = 0;	/* no need to save on stack  */
      /* Assume we need to save on stack.  */
      else
	info_ptr->vrsave_size = reg_size;
    }
  /* Even if we're not touching VRsave, make sure there's room on the
     stack for it, if it looks like we're calling SAVE_WORLD, which
     *will* attempt to save it.  */
  else if (info_ptr->first_altivec_reg_save == FIRST_SAVED_ALTIVEC_REGNO)
    info_ptr->vrsave_size = reg_size;
  /* APPLE LOCAL end AltiVec */

  /* Calculate the offsets.  */
  switch (abi)
    {
    case ABI_NONE:
    default:
      abort ();

    case ABI_AIX:
    case ABI_AIX_NODESC:
    case ABI_DARWIN:
      info_ptr->fp_save_offset   = - info_ptr->fp_size;
      info_ptr->gp_save_offset   = info_ptr->fp_save_offset - info_ptr->gp_size;

      if (TARGET_ALTIVEC_ABI)
	{
	  info_ptr->vrsave_save_offset
	    = info_ptr->gp_save_offset - info_ptr->vrsave_size;

	  /* Align stack so vector save area is on a quadword boundary.  */
	  if (info_ptr->altivec_size != 0)
	    info_ptr->altivec_padding_size
	      /* APPLE LOCAL AltiVec */
	      = (16 - (-info_ptr->vrsave_save_offset % 16)) % 16;
	  else
	    info_ptr->altivec_padding_size = 0;

	  info_ptr->altivec_save_offset
	    = info_ptr->vrsave_save_offset
	    - info_ptr->altivec_padding_size
	    - info_ptr->altivec_size;

	  /* APPLE LOCAL begin AltiVec */
	  if (- info_ptr->altivec_save_offset > 220)
	    info_ptr->vector_outside_red_zone_p = 1;
	  /* APPLE LOCAL end AltiVec */

	  /* Adjust for AltiVec case.  */
	  info_ptr->ehrd_offset = info_ptr->altivec_save_offset - ehrd_size;
	}
      else
	info_ptr->ehrd_offset      = info_ptr->gp_save_offset - ehrd_size;
      info_ptr->cr_save_offset   = reg_size; /* first word when 64-bit.  */
      info_ptr->lr_save_offset   = 2*reg_size;
      break;

    case ABI_V4:
      info_ptr->fp_save_offset   = - info_ptr->fp_size;
      info_ptr->gp_save_offset   = info_ptr->fp_save_offset - info_ptr->gp_size;
      info_ptr->cr_save_offset   = info_ptr->gp_save_offset - info_ptr->cr_size;

      if (TARGET_SPE_ABI)
      {
        /* Align stack so SPE GPR save area is aligned on a
           double-word boundary.  */
        if (info_ptr->spe_gp_size != 0)
          info_ptr->spe_padding_size
            = 8 - (-info_ptr->cr_save_offset % 8);
        else
          info_ptr->spe_padding_size = 0;

        info_ptr->spe_gp_save_offset
          = info_ptr->cr_save_offset
          - info_ptr->spe_padding_size
          - info_ptr->spe_gp_size;

        /* Adjust for SPE case.  */
        info_ptr->toc_save_offset
          = info_ptr->spe_gp_save_offset - info_ptr->toc_size;
      }
      else if (TARGET_ALTIVEC_ABI)
	{
	  info_ptr->vrsave_save_offset
	    = info_ptr->cr_save_offset - info_ptr->vrsave_size;

	  /* Align stack so vector save area is on a quadword boundary.  */
	  if (info_ptr->altivec_size != 0)
	    info_ptr->altivec_padding_size
	      = 16 - (-info_ptr->vrsave_save_offset % 16);
	  else
	    info_ptr->altivec_padding_size = 0;

	  info_ptr->altivec_save_offset
	    = info_ptr->vrsave_save_offset
	    - info_ptr->altivec_padding_size
	    - info_ptr->altivec_size;

	  /* Adjust for AltiVec case.  */
	  info_ptr->toc_save_offset
	    = info_ptr->altivec_save_offset - info_ptr->toc_size;
	}
      else
	info_ptr->toc_save_offset  = info_ptr->cr_save_offset - info_ptr->toc_size;
      info_ptr->ehrd_offset      = info_ptr->toc_save_offset - ehrd_size;
      info_ptr->lr_save_offset   = reg_size;
      break;
    }

  info_ptr->save_size    = RS6000_ALIGN (info_ptr->fp_size
					 + info_ptr->gp_size
					 + info_ptr->altivec_size
					 + info_ptr->altivec_padding_size
					 + info_ptr->vrsave_size
					 + info_ptr->spe_gp_size
					 + info_ptr->spe_padding_size
					 + ehrd_size
					 + info_ptr->cr_size
					 + info_ptr->lr_size
					 /* APPLE LOCAL fix redundant add? */
					 + info_ptr->toc_size,
					 /* APPLE LOCAL darwin native */
					 (TARGET_ALTIVEC_ABI ? 16 : 8));

  total_raw_size	 = (info_ptr->vars_size
			    + info_ptr->parm_size
			    + info_ptr->save_size
			    + info_ptr->varargs_size
			    + info_ptr->fixed_size);

  /* APPLE LOCAL begin CW asm blocks */
  /* If we have an assembly function, maybe use an explicit size.  To
     be consistent with CW behavior (and because it's safer), let
     RS6000_ALIGN round the explicit size up if necessary.  */
  if (cfun->cw_asm_function && cfun->cw_asm_frame_size != -2)
    {
      if (cfun->cw_asm_frame_size == -1)
	total_raw_size = 32;
      else if (cfun->cw_asm_frame_size < 32)
	error ("fralloc frame size must be at least 32");
      else
	total_raw_size = cfun->cw_asm_frame_size;
      total_raw_size += 24;
    }
  /* APPLE LOCAL end CW asm blocks */

  info_ptr->total_size =
    RS6000_ALIGN (total_raw_size, ABI_STACK_BOUNDARY / BITS_PER_UNIT);

  /* Determine if we need to allocate any stack frame:

     For AIX we need to push the stack if a frame pointer is needed
     (because the stack might be dynamically adjusted), if we are
     debugging, if we make calls, or if the sum of fp_save, gp_save,
     and local variables are more than the space needed to save all
     non-volatile registers: 32-bit: 18*8 + 19*4 = 220 or 64-bit: 18*8
     + 18*8 = 288 (GPR13 reserved).

     For V.4 we don't have the stack cushion that AIX uses, but assume
     that the debugger can handle stackless frames.  */

  /* APPLE LOCAL CW asm blocks */
  if (info_ptr->calls_p || (cfun->cw_asm_function && cfun->cw_asm_frame_size != -2))
    info_ptr->push_p = 1;

  else if (abi == ABI_V4)
    info_ptr->push_p = total_raw_size > info_ptr->fixed_size;

  else
    info_ptr->push_p = (frame_pointer_needed
			|| (abi != ABI_DARWIN && write_symbols != NO_DEBUG)
			|| ((total_raw_size - info_ptr->fixed_size)
			    > (TARGET_32BIT ? 220 : 288)));

  /* APPLE LOCAL begin AltiVec */
#if TARGET_MACHO
  /* For a *very* restricted set of circumstances, we can cut down the
     size of prologs/epilogs by calling our own save/restore-the-world
     routines.  This would normally be used for C++ routines which use
     EH.  */
  info_ptr->world_save_p =
    ! (current_function_calls_setjmp && flag_exceptions)
    && info_ptr->first_fp_reg_save == 14 + 32
    && info_ptr->first_gp_reg_save == 13
    && info_ptr->first_altivec_reg_save == FIRST_SAVED_ALTIVEC_REGNO
    && info_ptr->cr_save_p;
  
  /* This will not work in conjunction with sibcalls.  Make sure there
     are none.  (This check is expensive, but seldom executed.) */
  if ( info_ptr->world_save_p )
    {
      rtx insn;
      for ( insn = get_last_insn_anywhere (); insn; insn = PREV_INSN (insn))
	if ( GET_CODE (insn) == CALL_INSN
	     && SIBLING_CALL_P (insn))
	  {
	    info_ptr->world_save_p = 0;
	    break;
	  }
    }

  /* "Save" the VRsave register too if we're saving the world.  */
  if (info_ptr->world_save_p)
    info_ptr->vrsave_save_p = 1;

  /* Because the Darwin register save/restore routines only handle
     F14 .. F31 and V20 .. V31 as per the ABI, abort if there's something
     funny going on.  */
  if (info_ptr->first_fp_reg_save < 14 + 32
      || info_ptr->first_altivec_reg_save < FIRST_SAVED_ALTIVEC_REGNO)
    abort ();
#endif
  /* APPLE LOCAL end AltiVec */

  /* Zero offsets if we're not saving those registers.  */
  if (info_ptr->fp_size == 0)
    info_ptr->fp_save_offset = 0;

  if (info_ptr->gp_size == 0)
    info_ptr->gp_save_offset = 0;

  if (! TARGET_ALTIVEC_ABI || info_ptr->altivec_size == 0)
    info_ptr->altivec_save_offset = 0;

  /* APPLE LOCAL AltiVec */
  if (0 /* ! TARGET_ALTIVEC_ABI || info_ptr->vrsave_mask == 0 */)
    info_ptr->vrsave_save_offset = 0;

  if (! TARGET_SPE_ABI || info_ptr->spe_gp_size == 0)
    info_ptr->spe_gp_save_offset = 0;

  if (! info_ptr->lr_save_p)
    info_ptr->lr_save_offset = 0;

  if (! info_ptr->cr_save_p)
    info_ptr->cr_save_offset = 0;

  /* APPLE LOCAL begin AltiVec */
  if (!info_ptr->vrsave_save_p)
    info_ptr->vrsave_save_offset = 0;
  /* APPLE LOCAL end AltiVec */

  if (! info_ptr->toc_save_p)
    info_ptr->toc_save_offset = 0;

  return info_ptr;
}

void
debug_stack_info (info)
     rs6000_stack_t *info;
{
  const char *abi_string;

  if (! info)
    info = rs6000_stack_info ();

  fprintf (stderr, "\nStack information for function %s:\n",
	   ((current_function_decl && DECL_NAME (current_function_decl))
	    ? IDENTIFIER_POINTER (DECL_NAME (current_function_decl))
	    : "<unknown>"));

  switch (info->abi)
    {
    default:		 abi_string = "Unknown";	break;
    case ABI_NONE:	 abi_string = "NONE";		break;
    case ABI_AIX:
    case ABI_AIX_NODESC: abi_string = "AIX";		break;
    case ABI_DARWIN:	 abi_string = "Darwin";		break;
    case ABI_V4:	 abi_string = "V.4";		break;
    }

  fprintf (stderr, "\tABI                 = %5s\n", abi_string);

  if (TARGET_ALTIVEC_ABI)
    fprintf (stderr, "\tALTIVEC ABI extensions enabled.\n");

  if (TARGET_SPE_ABI)
    fprintf (stderr, "\tSPE ABI extensions enabled.\n");

  if (info->first_gp_reg_save != 32)
    fprintf (stderr, "\tfirst_gp_reg_save   = %5d\n", info->first_gp_reg_save);

  if (info->first_fp_reg_save != 64)
    fprintf (stderr, "\tfirst_fp_reg_save   = %5d\n", info->first_fp_reg_save);

  if (info->first_altivec_reg_save <= LAST_ALTIVEC_REGNO)
    fprintf (stderr, "\tfirst_altivec_reg_save = %5d\n",
	     info->first_altivec_reg_save);

  if (info->lr_save_p)
    fprintf (stderr, "\tlr_save_p           = %5d\n", info->lr_save_p);

  if (info->cr_save_p)
    fprintf (stderr, "\tcr_save_p           = %5d\n", info->cr_save_p);

  if (info->toc_save_p)
    fprintf (stderr, "\ttoc_save_p          = %5d\n", info->toc_save_p);

  /* APPLE LOCAL begin AltiVec */
  if (info->vrsave_save_p)
    fprintf (stderr, "\tvrsave_save_p       = %5d  (live_mask=0x%08x)\n",
	     info->vrsave_save_p, compute_vrsave_mask ());

  if (info->vector_outside_red_zone_p)
    fprintf (stderr, "\tvector_outside_red_zone_p = %d\n",
	     info->vector_outside_red_zone_p);
  /* APPLE LOCAL end AltiVec */

  if (info->vrsave_mask)
    fprintf (stderr, "\tvrsave_mask         = 0x%x\n", info->vrsave_mask);

  if (info->push_p)
    fprintf (stderr, "\tpush_p              = %5d\n", info->push_p);

  if (info->calls_p)
    fprintf (stderr, "\tcalls_p             = %5d\n", info->calls_p);

  if (info->gp_save_offset)
    fprintf (stderr, "\tgp_save_offset      = %5d\n", info->gp_save_offset);

  if (info->fp_save_offset)
    fprintf (stderr, "\tfp_save_offset      = %5d\n", info->fp_save_offset);

  if (info->altivec_save_offset)
    fprintf (stderr, "\taltivec_save_offset = %5d\n",
	     info->altivec_save_offset);

  if (info->spe_gp_save_offset)
    fprintf (stderr, "\tspe_gp_save_offset  = %5d\n",
	     info->spe_gp_save_offset);

  if (info->vrsave_save_offset)
    fprintf (stderr, "\tvrsave_save_offset  = %5d\n",
	     info->vrsave_save_offset);

  if (info->lr_save_offset)
    fprintf (stderr, "\tlr_save_offset      = %5d\n", info->lr_save_offset);

  if (info->cr_save_offset)
    fprintf (stderr, "\tcr_save_offset      = %5d\n", info->cr_save_offset);

  if (info->toc_save_offset)
    fprintf (stderr, "\ttoc_save_offset     = %5d\n", info->toc_save_offset);

  if (info->varargs_save_offset)
    fprintf (stderr, "\tvarargs_save_offset = %5d\n", info->varargs_save_offset);

  if (info->total_size)
    fprintf (stderr, "\ttotal_size          = %5d\n", info->total_size);

  if (info->varargs_size)
    fprintf (stderr, "\tvarargs_size        = %5d\n", info->varargs_size);

  if (info->vars_size)
    fprintf (stderr, "\tvars_size           = %5d\n", info->vars_size);

  if (info->parm_size)
    fprintf (stderr, "\tparm_size           = %5d\n", info->parm_size);

  if (info->fixed_size)
    fprintf (stderr, "\tfixed_size          = %5d\n", info->fixed_size);

  if (info->gp_size)
    fprintf (stderr, "\tgp_size             = %5d\n", info->gp_size);

  if (info->spe_gp_size)
    fprintf (stderr, "\tspe_gp_size         = %5d\n", info->spe_gp_size);

  if (info->fp_size)
    fprintf (stderr, "\tfp_size             = %5d\n", info->fp_size);

  if (info->altivec_size)
    fprintf (stderr, "\taltivec_size        = %5d\n", info->altivec_size);

  if (info->vrsave_size)
    fprintf (stderr, "\tvrsave_size         = %5d\n", info->vrsave_size);

  if (info->altivec_padding_size)
    fprintf (stderr, "\taltivec_padding_size= %5d\n",
	     info->altivec_padding_size);

  if (info->spe_padding_size)
    fprintf (stderr, "\tspe_padding_size    = %5d\n",
	     info->spe_padding_size);

  if (info->lr_size)
    fprintf (stderr, "\tlr_size             = %5d\n", info->lr_size);

  if (info->cr_size)
    fprintf (stderr, "\tcr_size             = %5d\n", info->cr_size);

  if (info->toc_size)
    fprintf (stderr, "\ttoc_size            = %5d\n", info->toc_size);

  if (info->save_size)
    fprintf (stderr, "\tsave_size           = %5d\n", info->save_size);

  if (info->reg_size != 4)
    fprintf (stderr, "\treg_size            = %5d\n", info->reg_size);

  fprintf (stderr, "\n");
}

rtx
rs6000_return_addr (count, frame)
     int count;
     rtx frame;
{
  /* Currently we don't optimize very well between prolog and body
     code and for PIC code the code can be actually quite bad, so
     don't try to be too clever here.  */
  if (count != 0 || flag_pic != 0)
    {
      cfun->machine->ra_needs_full_frame = 1;

      return
	gen_rtx_MEM
	  (Pmode,
	   memory_address
	   (Pmode,
	    plus_constant (copy_to_reg
			   (gen_rtx_MEM (Pmode,
					 memory_address (Pmode, frame))),
			   RETURN_ADDRESS_OFFSET)));
    }

  return get_hard_reg_initial_val (Pmode, LINK_REGISTER_REGNUM);
}

/* Say whether a function is a candidate for sibcall handling or not.
   We do not allow indirect calls to be optimized into sibling calls.
   Also, we can't do it if there are any vector parameters; there's
   nowhere to put the VRsave code so it works; note that functions with
   vector parameters are required to have a prototype, so the argument
   type info must be available here.  (The tail recursion case can work
   with vector parameters, but there's no way to distinguish here.) */
int
function_ok_for_sibcall (fndecl)
    tree fndecl;
{
  tree type;
  /* APPLE LOCAL -mlong-branch */
  if (TARGET_LONG_BRANCH)
    return 0;
  if (fndecl)
    {
      if (TARGET_ALTIVEC_VRSAVE)
        {
	  for (type = TYPE_ARG_TYPES (TREE_TYPE (fndecl));
	       type; type = TREE_CHAIN (type))
	    {
	      if (TREE_CODE (TREE_VALUE (type)) == VECTOR_TYPE)
		return 0;
	    }
        }
      if (DEFAULT_ABI == ABI_DARWIN
	  || (*targetm.binds_local_p) (fndecl))
	{
	  tree attr_list = TYPE_ATTRIBUTES (TREE_TYPE (fndecl));

	  if (!lookup_attribute ("longcall", attr_list)
	      || lookup_attribute ("shortcall", attr_list))
	    return 1;
	}
    }
  return 0;
}

static int
rs6000_ra_ever_killed ()
{
  rtx top;
  rtx reg;
  rtx insn;

  /* Irritatingly, there are two kinds of thunks -- those created with
     TARGET_ASM_OUTPUT_MI_THUNK and those with DECL_THUNK_P that go
     through the regular part of the compiler.  This is a very hacky
     way to tell them apart.  */
  if (current_function_is_thunk && !no_new_pseudos)
    return 0;

  /* regs_ever_live has LR marked as used if any sibcalls are present,
     but this should not force saving and restoring in the
     pro/epilogue.  Likewise, reg_set_between_p thinks a sibcall
     clobbers LR, so that is inappropriate. */

  /* Also, the prologue can generate a store into LR that
     doesn't really count, like this:

        move LR->R0
        bcl to set PIC register
        move LR->R31
        move R0->LR

     When we're called from the epilogue, we need to avoid counting
     this as a store.  */
         
  push_topmost_sequence ();
  top = get_insns ();
  pop_topmost_sequence ();
  reg = gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM);

  for (insn = NEXT_INSN (top); insn != NULL_RTX; insn = NEXT_INSN (insn))
    {
      if (INSN_P (insn))
	{
	  if (FIND_REG_INC_NOTE (insn, reg))
	    return 1;
	  else if (GET_CODE (insn) == CALL_INSN 
		   && !SIBLING_CALL_P (insn))
	    return 1;
	  else if (set_of (reg, insn) != NULL_RTX
		   && !prologue_epilogue_contains (insn))
	    return 1;
    	}
    }
  return 0;
}

/* Add a REG_MAYBE_DEAD note to the insn.  */
static void
rs6000_maybe_dead (insn)
     rtx insn;
{
  REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_MAYBE_DEAD,
					const0_rtx,
					REG_NOTES (insn));
}

/* Emit instructions needed to load the TOC register.
   This is only needed when TARGET_TOC, TARGET_MINIMAL_TOC, and there is
   a constant pool; or for SVR4 -fpic.  */

void
rs6000_emit_load_toc_table (fromprolog)
     int fromprolog;
{
  rtx dest;
  dest = gen_rtx_REG (Pmode, RS6000_PIC_OFFSET_TABLE_REGNUM);

  if (TARGET_ELF && DEFAULT_ABI == ABI_V4 && flag_pic == 1)
    {
      rtx temp = (fromprolog
		  ? gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM)
		  : gen_reg_rtx (Pmode));
      rs6000_maybe_dead (emit_insn (gen_load_toc_v4_pic_si (temp)));
      rs6000_maybe_dead (emit_move_insn (dest, temp));
    }
  else if (TARGET_ELF && DEFAULT_ABI != ABI_AIX && flag_pic == 2)
    {
      char buf[30];
      rtx tempLR = (fromprolog
		    ? gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM)
		    : gen_reg_rtx (Pmode));
      rtx temp0 = (fromprolog
		   ? gen_rtx_REG (Pmode, 0)
		   : gen_reg_rtx (Pmode));
      rtx symF;

      /* possibly create the toc section */
      if (! toc_initialized)
	{
	  toc_section ();
	  function_section (current_function_decl);
	}

      if (fromprolog)
	{
	  rtx symL;

	  ASM_GENERATE_INTERNAL_LABEL (buf, "LCF", rs6000_pic_labelno);
	  symF = gen_rtx_SYMBOL_REF (Pmode, ggc_strdup (buf));

	  ASM_GENERATE_INTERNAL_LABEL (buf, "LCL", rs6000_pic_labelno);
	  symL = gen_rtx_SYMBOL_REF (Pmode, ggc_strdup (buf));

	  rs6000_maybe_dead (emit_insn (gen_load_toc_v4_PIC_1 (tempLR,
							       symF)));
	  rs6000_maybe_dead (emit_move_insn (dest, tempLR));
	  rs6000_maybe_dead (emit_insn (gen_load_toc_v4_PIC_2 (temp0, dest,
							       symL,
							       symF)));
	}
      else
	{
	  rtx tocsym;
	  static int reload_toc_labelno = 0;

	  tocsym = gen_rtx_SYMBOL_REF (Pmode, toc_label_name);

	  ASM_GENERATE_INTERNAL_LABEL (buf, "LCG", reload_toc_labelno++);
	  symF = gen_rtx_SYMBOL_REF (Pmode, ggc_strdup (buf));

	  rs6000_maybe_dead (emit_insn (gen_load_toc_v4_PIC_1b (tempLR,
								symF,
								tocsym)));
	  rs6000_maybe_dead (emit_move_insn (dest, tempLR));
	  rs6000_maybe_dead (emit_move_insn (temp0,
					     gen_rtx_MEM (Pmode, dest)));
	}
      rs6000_maybe_dead (emit_insn (gen_addsi3 (dest, temp0, dest)));
    }
  else if (TARGET_ELF && !TARGET_AIX && flag_pic == 0 && TARGET_MINIMAL_TOC)
    {
      /* This is for AIX code running in non-PIC ELF32.  */
      char buf[30];
      rtx realsym;
      ASM_GENERATE_INTERNAL_LABEL (buf, "LCTOC", 1);
      realsym = gen_rtx_SYMBOL_REF (Pmode, ggc_strdup (buf));

      rs6000_maybe_dead (emit_insn (gen_elf_high (dest, realsym)));
      rs6000_maybe_dead (emit_insn (gen_elf_low (dest, dest, realsym)));
    }
  else if (DEFAULT_ABI == ABI_AIX)
    {
      if (TARGET_32BIT)
	rs6000_maybe_dead (emit_insn (gen_load_toc_aix_si (dest)));
      else
	rs6000_maybe_dead (emit_insn (gen_load_toc_aix_di (dest)));
    }
  else
    abort ();
}

int   
get_TOC_alias_set ()
{
    static int set = -1;
    if (set == -1)
      set = new_alias_set ();
    return set;
}   

/* This retuns nonzero if the current function uses the TOC.  This is
   determined by the presence of (unspec ... 7), which is generated by
   the various load_toc_* patterns.  */

int
uses_TOC () 
{
    rtx insn;

    for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
      if (INSN_P (insn))
	{
	  rtx pat = PATTERN (insn);
	  int i;

	  if (GET_CODE (pat) == PARALLEL) 
	    for (i = 0; i < XVECLEN (PATTERN (insn), 0); i++)
	      if (GET_CODE (XVECEXP (PATTERN (insn), 0, i)) == UNSPEC 
		 && XINT (XVECEXP (PATTERN (insn), 0, i), 1) == 7)
		  return 1;
	}
    return 0;
}

rtx
create_TOC_reference (symbol) 
    rtx symbol;
{
  return gen_rtx_PLUS (Pmode, 
	   gen_rtx_REG (Pmode, TOC_REGISTER),
	     gen_rtx_CONST (Pmode, 
	       gen_rtx_MINUS (Pmode, symbol, 
		 gen_rtx_SYMBOL_REF (Pmode, toc_label_name))));
}

#if TARGET_AIX
/* __throw will restore its own return address to be the same as the
   return address of the function that the throw is being made to.
   This is unfortunate, because we want to check the original
   return address to see if we need to restore the TOC.
   So we have to squirrel it away here.  
   This is used only in compiling __throw and __rethrow. 

   Most of this code should be removed by CSE.  */
static rtx insn_after_throw;

/* This does the saving...  */
void
rs6000_aix_emit_builtin_unwind_init ()
{
  rtx mem;
  rtx stack_top = gen_reg_rtx (Pmode);
  rtx opcode_addr = gen_reg_rtx (Pmode);

  insn_after_throw = gen_reg_rtx (SImode);

  mem = gen_rtx_MEM (Pmode, hard_frame_pointer_rtx);
  emit_move_insn (stack_top, mem);

  mem = gen_rtx_MEM (Pmode, 
		     gen_rtx_PLUS (Pmode, stack_top, 
				   GEN_INT (2 * GET_MODE_SIZE (Pmode))));
  emit_move_insn (opcode_addr, mem);
  emit_move_insn (insn_after_throw, gen_rtx_MEM (SImode, opcode_addr));
}

/* Emit insns to _restore_ the TOC register, at runtime (specifically
   in _eh.o).  Only used on AIX.

   The idea is that on AIX, function calls look like this:
	bl  somefunction-trampoline
	lwz r2,20(sp)

   and later,
	somefunction-trampoline:
	stw r2,20(sp)
	 ... load function address in the count register ...
	bctr
   or like this, if the linker determines that this is not a cross-module call
   and so the TOC need not be restored:
	bl  somefunction
	nop
   or like this, if the compiler could determine that this is not a
   cross-module call:
	bl  somefunction
   now, the tricky bit here is that register 2 is saved and restored
   by the _linker_, so we can't readily generate debugging information
   for it.  So we need to go back up the call chain looking at the
   insns at return addresses to see which calls saved the TOC register
   and so see where it gets restored from.

   Oh, and all this gets done in RTL inside the eh_epilogue pattern,
   just before the actual epilogue.

   On the bright side, this incurs no space or time overhead unless an
   exception is thrown, except for the extra code in libgcc.a.  

   The parameter STACKSIZE is a register containing (at runtime)
   the amount to be popped off the stack in addition to the stack frame
   of this routine (which will be __throw or __rethrow, and so is
   guaranteed to have a stack frame).  */

void
rs6000_emit_eh_toc_restore (stacksize)
     rtx stacksize;
{
  rtx top_of_stack;
  rtx bottom_of_stack = gen_reg_rtx (Pmode);
  rtx tocompare = gen_reg_rtx (SImode);
  rtx opcode = gen_reg_rtx (SImode);
  rtx opcode_addr = gen_reg_rtx (Pmode);
  rtx mem;
  rtx loop_start = gen_label_rtx ();
  rtx no_toc_restore_needed = gen_label_rtx ();
  rtx loop_exit = gen_label_rtx ();
  
  mem = gen_rtx_MEM (Pmode, hard_frame_pointer_rtx);
  set_mem_alias_set (mem, rs6000_sr_alias_set);
  emit_move_insn (bottom_of_stack, mem);

  top_of_stack = expand_binop (Pmode, add_optab, 
			       bottom_of_stack, stacksize,
			       NULL_RTX, 1, OPTAB_WIDEN);

  emit_move_insn (tocompare, gen_int_mode (TARGET_32BIT ? 0x80410014 
					   : 0xE8410028, SImode));

  if (insn_after_throw == NULL_RTX)
    abort ();
  emit_move_insn (opcode, insn_after_throw);
  
  emit_note (NULL, NOTE_INSN_LOOP_BEG);
  emit_label (loop_start);
  
  do_compare_rtx_and_jump (opcode, tocompare, NE, 1,
			   SImode, NULL_RTX, NULL_RTX,
			   no_toc_restore_needed);
  
  mem = gen_rtx_MEM (Pmode, 
		     gen_rtx_PLUS (Pmode, bottom_of_stack, 
				   GEN_INT (5 * GET_MODE_SIZE (Pmode))));
  emit_move_insn (gen_rtx_REG (Pmode, 2), mem);

  emit_label (no_toc_restore_needed);
  do_compare_rtx_and_jump (top_of_stack, bottom_of_stack, EQ, 1,
			   Pmode, NULL_RTX, NULL_RTX,
			   loop_exit);

  mem = gen_rtx_MEM (Pmode, bottom_of_stack);
  set_mem_alias_set (mem, rs6000_sr_alias_set);
  emit_move_insn (bottom_of_stack, mem);
  
  mem = gen_rtx_MEM (Pmode, 
		     gen_rtx_PLUS (Pmode, bottom_of_stack, 
				   GEN_INT (2 * GET_MODE_SIZE (Pmode))));
  emit_move_insn (opcode_addr, mem);
  emit_move_insn (opcode, gen_rtx_MEM (SImode, opcode_addr));

  emit_note (NULL, NOTE_INSN_LOOP_CONT);
  emit_jump (loop_start);
  emit_note (NULL, NOTE_INSN_LOOP_END);
  emit_label (loop_exit);
}
#endif /* TARGET_AIX */

/* This ties together stack memory (MEM with an alias set of
   rs6000_sr_alias_set) and the change to the stack pointer.  */

static void
rs6000_emit_stack_tie ()
{
  rtx mem = gen_rtx_MEM (BLKmode, gen_rtx_REG (Pmode, STACK_POINTER_REGNUM));

  set_mem_alias_set (mem, rs6000_sr_alias_set);
  emit_insn (gen_stack_tie (mem));
}

/* Emit the correct code for allocating stack space, as insns.
   If COPY_R12, make sure a copy of the old frame is left in r12.
   The generated code may use hard register 0 as a temporary.  */

static void
rs6000_emit_allocate_stack (size, copy_r12)
     HOST_WIDE_INT size;
     int copy_r12;
{
  rtx insn;
  rtx stack_reg = gen_rtx_REG (Pmode, STACK_POINTER_REGNUM);
  rtx tmp_reg = gen_rtx_REG (Pmode, 0);
  rtx todec = GEN_INT (-size);

  if (current_function_limit_stack)
    {
      if (REG_P (stack_limit_rtx)
	  && REGNO (stack_limit_rtx) > 1 
	  && REGNO (stack_limit_rtx) <= 31)
	{
	  emit_insn (Pmode == SImode
		     ? gen_addsi3 (tmp_reg,
				   stack_limit_rtx,
				   GEN_INT (size))
		     : gen_adddi3 (tmp_reg,
				   stack_limit_rtx,
				   GEN_INT (size)));
	  
	  emit_insn (gen_cond_trap (LTU, stack_reg, tmp_reg,
				    const0_rtx));
	}
      else if (GET_CODE (stack_limit_rtx) == SYMBOL_REF
	       && TARGET_32BIT
	       && DEFAULT_ABI == ABI_V4)
	{
	  rtx toload = gen_rtx_CONST (VOIDmode,
				      gen_rtx_PLUS (Pmode, 
						    stack_limit_rtx, 
						    GEN_INT (size)));
	  
	  emit_insn (gen_elf_high (tmp_reg, toload));
	  emit_insn (gen_elf_low (tmp_reg, tmp_reg, toload));
	  emit_insn (gen_cond_trap (LTU, stack_reg, tmp_reg,
				    const0_rtx));
	}
      else
	warning ("stack limit expression is not supported");
    }

  if (copy_r12 || ! TARGET_UPDATE)
    emit_move_insn (gen_rtx_REG (Pmode, 12), stack_reg);

  if (TARGET_UPDATE)
    {
      if (size > 32767)
	{
	  /* Need a note here so that try_split doesn't get confused.  */
	  if (get_last_insn() == NULL_RTX)
	    emit_note (0, NOTE_INSN_DELETED);
	  insn = emit_move_insn (tmp_reg, todec);
	  try_split (PATTERN (insn), insn, 0);
	  todec = tmp_reg;
	}
      
      if (Pmode == SImode)
	insn = emit_insn (gen_movsi_update (stack_reg, stack_reg, 
					    todec, stack_reg));
      else
	insn = emit_insn (gen_movdi_update (stack_reg, stack_reg, 
					    todec, stack_reg));
    }
  else
    {
      if (Pmode == SImode)
	insn = emit_insn (gen_addsi3 (stack_reg, stack_reg, todec));
      else
	insn = emit_insn (gen_adddi3 (stack_reg, stack_reg, todec));
      emit_move_insn (gen_rtx_MEM (Pmode, stack_reg),
		      gen_rtx_REG (Pmode, 12));
    }
  
  RTX_FRAME_RELATED_P (insn) = 1;
  REG_NOTES (insn) = 
    gen_rtx_EXPR_LIST (REG_FRAME_RELATED_EXPR,
		       gen_rtx_SET (VOIDmode, stack_reg, 
				    gen_rtx_PLUS (Pmode, stack_reg,
						  GEN_INT (-size))),
		       REG_NOTES (insn));
}

/* Add a RTX_FRAME_RELATED note so that dwarf2out_frame_debug_expr
   knows that:

     (mem (plus (blah) (regXX)))

   is really:

     (mem (plus (blah) (const VALUE_OF_REGXX))).  */

static void
altivec_frame_fixup (insn, reg, val)
     rtx insn, reg;
     HOST_WIDE_INT val;
{
  rtx real;

  real = copy_rtx (PATTERN (insn));

  real = replace_rtx (real, reg, GEN_INT (val));

  RTX_FRAME_RELATED_P (insn) = 1;
  REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_FRAME_RELATED_EXPR,
					real,
					REG_NOTES (insn));
}

/* Add to 'insn' a note which is PATTERN (INSN) but with REG replaced
   with (plus:P (reg 1) VAL), and with REG2 replaced with RREG if REG2
   is not NULL.  It would be nice if dwarf2out_frame_debug_expr could
   deduce these equivalences by itself so it wasn't necessary to hold
   its hand so much.  */

static void
rs6000_frame_related (insn, reg, val, reg2, rreg)
     rtx insn;
     rtx reg;
     HOST_WIDE_INT val;
     rtx reg2;
     rtx rreg;
{
  rtx real, temp;

  /* copy_rtx will not make unique copies of registers, so we need to
     ensure we don't have unwanted sharing here.  */
  if (reg == reg2)
    reg = gen_raw_REG (GET_MODE (reg), REGNO (reg));

  if (reg == rreg)
    reg = gen_raw_REG (GET_MODE (reg), REGNO (reg));

  real = copy_rtx (PATTERN (insn));

  if (reg2 != NULL_RTX)
    real = replace_rtx (real, reg2, rreg);
  
  real = replace_rtx (real, reg, 
		      gen_rtx_PLUS (Pmode, gen_rtx_REG (Pmode,
							STACK_POINTER_REGNUM),
				    GEN_INT (val)));
  
  /* We expect that 'real' is either a SET or a PARALLEL containing
     SETs (and possibly other stuff).  In a PARALLEL, all the SETs
     are important so they all have to be marked RTX_FRAME_RELATED_P.  */

  if (GET_CODE (real) == SET)
    {
      rtx set = real;
      
      temp = simplify_rtx (SET_SRC (set));
      if (temp)
	SET_SRC (set) = temp;
      temp = simplify_rtx (SET_DEST (set));
      if (temp)
	SET_DEST (set) = temp;
      if (GET_CODE (SET_DEST (set)) == MEM)
	{
	  temp = simplify_rtx (XEXP (SET_DEST (set), 0));
	  if (temp)
	    XEXP (SET_DEST (set), 0) = temp;
	}
    }
  else if (GET_CODE (real) == PARALLEL)
    {
      int i;
      for (i = 0; i < XVECLEN (real, 0); i++)
	if (GET_CODE (XVECEXP (real, 0, i)) == SET)
	  {
	    rtx set = XVECEXP (real, 0, i);
	    
	    temp = simplify_rtx (SET_SRC (set));
	    if (temp)
	      SET_SRC (set) = temp;
	    temp = simplify_rtx (SET_DEST (set));
	    if (temp)
	      SET_DEST (set) = temp;
	    if (GET_CODE (SET_DEST (set)) == MEM)
	      {
		temp = simplify_rtx (XEXP (SET_DEST (set), 0));
		if (temp)
		  XEXP (SET_DEST (set), 0) = temp;
	      }
	    RTX_FRAME_RELATED_P (set) = 1;
	  }
    }
  else
    abort ();
  
  RTX_FRAME_RELATED_P (insn) = 1;
  REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_FRAME_RELATED_EXPR,
					real,
					REG_NOTES (insn));
}

/* Returns an insn that has a vrsave set operation with the
   appropriate CLOBBERs.  */

static rtx
generate_set_vrsave (reg, info, epiloguep)
     rtx reg;
     rs6000_stack_t *info;
     int epiloguep;
{
  int nclobs, i;
  rtx insn, clobs[TOTAL_ALTIVEC_REGS + 1];
  rtx vrsave = gen_rtx_REG (SImode, VRSAVE_REGNO);

  clobs[0]
    = gen_rtx_SET (VOIDmode,
		   vrsave,
		   gen_rtx_UNSPEC_VOLATILE (SImode,
					    gen_rtvec (2, reg, vrsave),
					    30));

  nclobs = 1;

  /* We need to clobber the registers in the mask so the scheduler
     does not move sets to VRSAVE before sets of AltiVec registers.

     However, if the function receives nonlocal gotos, reload will set
     all call saved registers live.  We will end up with:

     	(set (reg 999) (mem))
	(parallel [ (set (reg vrsave) (unspec blah))
		    (clobber (reg 999))])

     The clobber will cause the store into reg 999 to be dead, and
     flow will attempt to delete an epilogue insn.  In this case, we
     need an unspec use/set of the register.  */

  for (i = FIRST_ALTIVEC_REGNO; i <= LAST_ALTIVEC_REGNO; ++i)
    if (info->vrsave_mask != 0 && ALTIVEC_REG_BIT (i) != 0)
      {
	if (!epiloguep || call_used_regs [i])
	  clobs[nclobs++] = gen_rtx_CLOBBER (VOIDmode,
					     gen_rtx_REG (V4SImode, i));
	else
	  {
	    rtx reg = gen_rtx_REG (V4SImode, i);

	    clobs[nclobs++]
	      = gen_rtx_SET (VOIDmode,
			     reg,
			     gen_rtx_UNSPEC (V4SImode,
					     gen_rtvec (1, reg), 27));
	  }
      }

  insn = gen_rtx_PARALLEL (VOIDmode, rtvec_alloc (nclobs));

  for (i = 0; i < nclobs; ++i)
    XVECEXP (insn, 0, i) = clobs[i];

  return insn;
}

/* APPLE LOCAL special ObjC method use of R12 */
static int objc_method_using_pic = 0;

/* Determine whether a name is an ObjC method.  */
static int name_encodes_objc_method_p (const char *piclabel_name)
{
  return (piclabel_name[0] == '*' && piclabel_name[1] == '"' 
       ? (piclabel_name[2] == 'L'
	  && (piclabel_name[3] == '+' || piclabel_name[3] == '-'))
       : (piclabel_name[1] == 'L'
	  && (piclabel_name[2] == '+' || piclabel_name[2] == '-')));
}

/* APPLE LOCAL recompute PIC register use */
/* Sometimes a function has references that require the PIC register,
   but optimization removes them all.  To catch this case
   recompute current_function_uses_pic_offset_table here. 
   This may allow us to eliminate the prologue and epilogue. */

static int recompute_PIC_register_use ()
{
  if (DEFAULT_ABI == ABI_DARWIN 
       && flag_pic && current_function_uses_pic_offset_table 
       && !cfun->machine->ra_needs_full_frame)
    {
      rtx insn;
      current_function_uses_pic_offset_table = 0;
      push_topmost_sequence ();
      for (insn = get_insns (); insn != NULL; insn = NEXT_INSN (insn))
	if ( reg_mentioned_p (pic_offset_table_rtx, insn))
	  {
	    current_function_uses_pic_offset_table = 1;
	    break;
	  }
      pop_topmost_sequence ();
    }
  return 0;
}
       
/* APPLE LOCAL volatile pic base reg in leaves */
/* If this is a leaf function and we used any pic-based references,
   see if there is an unused volatile reg we can use instead of R31.
   If so set substitute_pic_base_reg to this reg, set its reg_ever_used
   bit (to avoid confusing later calls to alloc_volatile_reg), and
   make a pass through the existing RTL, substituting the new reg for
   the old one wherever it appears.
   Logically this is a void function; it is int so it can be used to
   initialize a dummy variable, thus getting executed ahead of other
   initializations.  Technicolour yawn.  */

static int try_leaf_pic_optimization ()
{
  if ( DEFAULT_ABI==ABI_DARWIN
       && flag_pic && current_function_uses_pic_offset_table
       && current_function_is_leaf 
       && !cfun->machine->ra_needs_full_frame )
    {
      int reg = alloc_volatile_reg ();
      if ( reg != -1 )
	{
	  /* Run through the insns, changing references to the original
	     PIC_OFFSET_TABLE_REGNUM to our new one.  */
	  rtx insn;
	  const int nregs = PIC_OFFSET_TABLE_REGNUM + 1;
	  rtx *reg_map = (rtx *) xmalloc (nregs * sizeof (rtx));
	  memset (reg_map, 0, nregs * sizeof (rtx));
	  reg_map[PIC_OFFSET_TABLE_REGNUM] = gen_rtx_REG (SImode, reg);

	  push_topmost_sequence ();
	  for (insn = get_insns (); insn != NULL; insn = NEXT_INSN (insn))
	    {
	      if (GET_CODE (insn) == INSN || GET_CODE (insn) == JUMP_INSN)
		{
		  replace_regs (PATTERN (insn), reg_map, nregs, 1);
		  replace_regs (REG_NOTES (insn), reg_map, nregs, 1);
		}
	      else if (GET_CODE (insn) == CALL_INSN)
		{
		  if ( !SIBLING_CALL_P (insn))
		    abort ();
		}
	    }
	  pop_topmost_sequence ();
	  free (reg_map);

	  regs_ever_live[reg] = 1;
	  regs_ever_live[PIC_OFFSET_TABLE_REGNUM] = 0;
	  cfun->machine->substitute_pic_base_reg = reg;
	}
    }
  return 0;
}

/* Save a register into the frame, and emit RTX_FRAME_RELATED_P notes.
   Save REGNO into [FRAME_REG + OFFSET] in mode MODE.  */

static void
emit_frame_save (frame_reg, frame_ptr, mode, regno, offset, total_size)
     rtx frame_reg;
     rtx frame_ptr;
     enum machine_mode mode;
     unsigned int regno;
     int offset;
     int total_size;
{
  rtx reg, offset_rtx, insn, mem, addr, int_rtx;
  rtx replacea, replaceb;

  int_rtx = GEN_INT (offset);

  /* Some cases that need register indexed addressing.  */
  if ((TARGET_ALTIVEC_ABI && ALTIVEC_VECTOR_MODE (mode))
      || (TARGET_SPE_ABI
	  && SPE_VECTOR_MODE (mode)
	  && !SPE_CONST_OFFSET_OK (offset)))
    {
      /* Whomever calls us must make sure r11 is available in the
         flow path of instructions in the prologue.  */
      offset_rtx = gen_rtx_REG (Pmode, 11);
      emit_move_insn (offset_rtx, int_rtx);

      replacea = offset_rtx;
      replaceb = int_rtx;
    }
  else
    {
      offset_rtx = int_rtx;
      replacea = NULL_RTX;
      replaceb = NULL_RTX;
    }

  reg = gen_rtx_REG (mode, regno);
  addr = gen_rtx_PLUS (Pmode, frame_reg, offset_rtx);
  mem = gen_rtx_MEM (mode, addr);
  set_mem_alias_set (mem, rs6000_sr_alias_set);

  insn = emit_move_insn (mem, reg);

  rs6000_frame_related (insn, frame_ptr, total_size, replacea, replaceb);
}

/* Emit an offset memory reference suitable for a frame store, while
   converting to a valid addressing mode.  */

static rtx
gen_frame_mem_offset (mode, reg, offset)
     enum machine_mode mode;
     rtx reg;
     int offset;
{
  rtx int_rtx, offset_rtx;

  int_rtx = GEN_INT (offset);

  if (TARGET_SPE_ABI && SPE_VECTOR_MODE (mode))
    {
      offset_rtx = gen_rtx_REG (Pmode, FIXED_SCRATCH);
      emit_move_insn (offset_rtx, int_rtx);
    }
  else
    offset_rtx = int_rtx;

  return gen_rtx_MEM (mode, gen_rtx_PLUS (Pmode, reg, offset_rtx));
}

/* Emit function prologue as insns.  */

void
rs6000_emit_prologue ()
{
  /* APPLE LOCAL recompute PIC register use */
  int dummy ATTRIBUTE_UNUSED = recompute_PIC_register_use ();
  /* APPLE LOCAL volatile pic base reg in leaves */
  int ignored ATTRIBUTE_UNUSED = try_leaf_pic_optimization ();
  rs6000_stack_t *info = rs6000_stack_info ();
  /* APPLE LOCAL begin 64bit registers, ABI32bit */
  enum machine_mode reg_mode = (TARGET_POWERPC64 && ! TARGET_32BIT) ? DImode : SImode;
  int reg_size = (TARGET_POWERPC64  && ! TARGET_32BIT) ? 8 : 4;
  /* APPLE LOCAL end 64bit registers, ABI32bit */
  rtx sp_reg_rtx = gen_rtx_REG (Pmode, STACK_POINTER_REGNUM);
  rtx frame_ptr_rtx = gen_rtx_REG (Pmode, 12);
  rtx frame_reg_rtx = sp_reg_rtx;
  rtx cr_save_rtx = NULL;
  rtx insn;
  int saving_FPRs_inline;
  int using_store_multiple;
  HOST_WIDE_INT sp_offset = 0;
  /* APPLE LOCAL begin AltiVec */
  int callers_lr_already_saved = 0;
#if TARGET_MACHO
  int lr_already_set_up_for_pic = 0;
  int saved_world = 0;
#endif
  /* APPLE LOCAL end AltiVec */
  /* APPLE LOCAL special ObjC method use of R12 */
  objc_method_using_pic = 0;
  
  /* APPLE LOCAL BEGIN fix-and-continue mrs  */
  if (flag_fix_and_continue)
    {
      emit_insn (gen_nop ());
      emit_insn (gen_nop ());
      emit_insn (gen_nop ());
      emit_insn (gen_nop ());
    }
  /* APPLE LOCAL END fix-and-continue mrs  */

   if (TARGET_SPE_ABI)
     {
       reg_mode = V2SImode;
       reg_size = 8;
     }

  using_store_multiple = (TARGET_MULTIPLE && (! TARGET_POWERPC64 || TARGET_32BIT)
			  && !TARGET_SPE_ABI
			  && info->first_gp_reg_save < 31);
  saving_FPRs_inline = (info->first_fp_reg_save == 64
			|| FP_SAVE_INLINE (info->first_fp_reg_save));

  /* For V.4, update stack before we do any saving and set back pointer.  */
  if (info->push_p && DEFAULT_ABI == ABI_V4)
    {
      if (info->total_size < 32767)
	sp_offset = info->total_size;
      else
	frame_reg_rtx = frame_ptr_rtx;
      rs6000_emit_allocate_stack (info->total_size, 
				  (frame_reg_rtx != sp_reg_rtx
				   && (info->cr_save_p
				       || info->lr_save_p
				       || info->first_fp_reg_save < 64
				       || info->first_gp_reg_save < 32
				       )));
      if (frame_reg_rtx != sp_reg_rtx)
	rs6000_emit_stack_tie ();
    }

  /* APPLE LOCAL begin special ObjC method use of R12 */
#if TARGET_MACHO
  if (DEFAULT_ABI == ABI_DARWIN 
	&& current_function_uses_pic_offset_table && flag_pic)
    {
      const char *piclabel_name = machopic_function_base_name ();
      
      if (name_encodes_objc_method_p (piclabel_name)
	  /* If we're saving vector or FP regs via a function call,
	     then don't bother with this ObjC R12 optimization.
	     This test also eliminates world_save.  */
	  && (!info->world_save_p)
	  && (info->first_altivec_reg_save > LAST_ALTIVEC_REGNO
	      || VECTOR_SAVE_INLINE (info->first_altivec_reg_save))
	  && (info->first_fp_reg_save == 64 
	      || FP_SAVE_INLINE (info->first_fp_reg_save)))
	{
	   /* We cannot output the label now; there seems to be no 
	     way to prevent cfgcleanup from deleting it.  It is done
	     in rs6000_output_function_prologue with fprintf!  */
	  objc_method_using_pic = 1;
	}
    }
#endif /* TARGET_MACHO */
  /* APPLE LOCAL end special ObjC method use of R12 */

  /* Save AltiVec registers if needed.  */
  /* APPLE LOCAL AltiVec */
  if (0 /* TARGET_ALTIVEC_ABI && info->altivec_size != 0 */)
    {
      int i;

      /* There should be a non inline version of this, for when we
	 are saving lots of vector registers.  */
      for (i = info->first_altivec_reg_save; i <= LAST_ALTIVEC_REGNO; ++i)
	if (info->vrsave_mask & ALTIVEC_REG_BIT (i))
	  {
	    rtx areg, savereg, mem;
	    int offset;

	    offset = info->altivec_save_offset + sp_offset
	      + 16 * (i - info->first_altivec_reg_save);

	    savereg = gen_rtx_REG (V4SImode, i);

	    areg = gen_rtx_REG (Pmode, 0);
	    emit_move_insn (areg, GEN_INT (offset));

	    /* AltiVec addressing mode is [reg+reg].  */
	    mem = gen_rtx_MEM (V4SImode,
			       gen_rtx_PLUS (Pmode, frame_reg_rtx, areg));
			       
	    set_mem_alias_set (mem, rs6000_sr_alias_set);

	    insn = emit_move_insn (mem, savereg);

	    altivec_frame_fixup (insn, areg, offset);
	  }
    }

  /* VRSAVE is a bit vector representing which AltiVec registers
     are used.  The OS uses this to determine which vector
     registers to save on a context switch.  We need to save
     VRSAVE on the stack frame, add whatever AltiVec registers we
     used in this function, and do the corresponding magic in the
     epilogue.  */

  /* APPLE LOCAL AltiVec */
  if (0 /* TARGET_ALTIVEC && info->vrsave_mask != 0 */)
    {
      rtx reg, mem, vrsave;
      int offset;

      /* Get VRSAVE onto a GPR.  */
      reg = gen_rtx_REG (SImode, 12);
      vrsave = gen_rtx_REG (SImode, VRSAVE_REGNO);
      if (TARGET_MACHO)
	emit_insn (gen_get_vrsave_internal (reg));
      else
	emit_insn (gen_rtx_SET (VOIDmode, reg, vrsave));

      /* Save VRSAVE.  */
      offset = info->vrsave_save_offset + sp_offset;
      mem
	= gen_rtx_MEM (SImode,
		       gen_rtx_PLUS (Pmode, frame_reg_rtx, GEN_INT (offset)));
      set_mem_alias_set (mem, rs6000_sr_alias_set);
      insn = emit_move_insn (mem, reg);

      /* Include the registers in the mask.  */
      emit_insn (gen_iorsi3 (reg, reg, GEN_INT ((int) info->vrsave_mask)));

      insn = emit_insn (generate_set_vrsave (reg, info, 0));
    }

  /* If we use the link register, get it into r0.  */
  if (info->lr_save_p)
    emit_move_insn (gen_rtx_REG (Pmode, 0),
		    gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM));

  /* APPLE LOCAL begin AltiVec */
#if TARGET_MACHO
  /* Handle world saves specially here.  */
  if (info->world_save_p)
    {
      int i, j, sz;
      rtx treg;
      rtvec p;
      int gen_following_label = 0;

#ifdef INSN_SCHEDULING
      /* Prevent the compiler from crashing
	 while scheduling insns after global_alloc!  */
      if (optimize == 0 || !flag_schedule_insns_after_reload)
#endif
      if (current_function_uses_pic_offset_table && flag_pic)
	gen_following_label = lr_already_set_up_for_pic = 1;

      /* The SAVE_WORLD and RESTORE_WORLD routines make a number of
	 assumptions about the offsets of various bits of the stack
	 frame.  Abort if things aren't what they should be.  */
      if (info->gp_save_offset != -220
	  || info->fp_save_offset != -144
	  || info->lr_save_offset != 8
	  || info->cr_save_offset != 4
	  || !info->push_p
	  || !info->lr_save_p
	  || (current_function_calls_eh_return && info->ehrd_offset != -432)
	  || (info->vrsave_save_offset != -224
	      || info->altivec_save_offset != (-224 - 192)))
	abort ();

      treg = gen_rtx_REG (SImode, 11);
      emit_move_insn (treg, GEN_INT (-info->total_size));

      /* SAVE_WORLD takes the caller's LR in R0 and the frame size
	 in R11.  It also clobbers R12, so beware!  */

      /* APPLE LOCAL preserve CR2 for save_world prologues */
      sz = 6;
      sz += gen_following_label;
      sz += 32 - info->first_gp_reg_save;
      sz += 64 - info->first_fp_reg_save;
      sz += LAST_ALTIVEC_REGNO - info->first_altivec_reg_save + 1;
      p = rtvec_alloc (sz);
      j = 0;
      RTVEC_ELT (p, j++) = gen_rtx_CLOBBER (VOIDmode, 
					    gen_rtx_REG (Pmode, 
							 LINK_REGISTER_REGNUM));
      RTVEC_ELT (p, j++) = gen_rtx_USE (VOIDmode,
					gen_rtx_SYMBOL_REF (Pmode,
							    "*save_world"));
      if ( gen_following_label )
	RTVEC_ELT (p, j++) = gen_rtx_USE (VOIDmode, const0_rtx);
      /* We do floats first so that the instruction pattern matches
	 properly.  */
      for (i = 0; i < 64 - info->first_fp_reg_save; i++)
	{
	  rtx reg = gen_rtx_REG (DFmode, info->first_fp_reg_save + i);
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
				   GEN_INT (info->fp_save_offset 
					    + sp_offset + 8 * i));
	  rtx mem = gen_rtx_MEM (DFmode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, j++) = gen_rtx_SET (VOIDmode, mem, reg);
	}
      for (i = 0; info->first_altivec_reg_save + i <= LAST_ALTIVEC_REGNO; i++)
	{
	  rtx reg = gen_rtx_REG (V4SImode, info->first_altivec_reg_save + i);
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
				   GEN_INT (info->altivec_save_offset 
					    + sp_offset + 16 * i));
	  rtx mem = gen_rtx_MEM (V4SImode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, j++) = gen_rtx_SET (VOIDmode, mem, reg);
	}
      for (i = 0; i < 32 - info->first_gp_reg_save; i++)
	{
	  rtx reg = gen_rtx_REG (reg_mode, info->first_gp_reg_save + i);
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
				   GEN_INT (info->gp_save_offset 
					    + sp_offset + reg_size * i));
	  rtx mem = gen_rtx_MEM (reg_mode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, j++) = gen_rtx_SET (VOIDmode, mem, reg);
	}

      /* APPLE LOCAL begin preserve CR2 for save_world prologues */
	{
	  /* CR register traditionally saved as CR2.  */
	  rtx reg = gen_rtx_REG (reg_mode, CR2_REGNO);
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
				   GEN_INT (info->cr_save_offset
					    + sp_offset));
	  rtx mem = gen_rtx_MEM (reg_mode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, j++) = gen_rtx_SET (VOIDmode, mem, reg);
	}
      /* APPLE LOCAL end preserve CR2 for save_world prologues */

#if 0
      /* Generating the following RTL causes a crash in dwarf2out.c,
	 possibly due to cfa_temp.reg being uninitialized in
	 dwarf2out_frame_debug_expr()!!!  */
      RTVEC_ELT (p, j++) = gen_rtx_SET (VOIDmode, sp_reg_rtx,
					  gen_rtx_PLUS (Pmode,
							sp_reg_rtx, treg));
#endif
      /* Prevent any attempt to delete the setting of r0 and treg!  */
      RTVEC_ELT (p, j++) = gen_rtx_USE (VOIDmode, gen_rtx_REG (Pmode, 0));
      RTVEC_ELT (p, j++) = gen_rtx_USE (VOIDmode, treg);
      RTVEC_ELT (p, j++) = gen_rtx_CLOBBER (VOIDmode, sp_reg_rtx);

      insn = emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
			    NULL_RTX, NULL_RTX);

      /* The goto below skips over this, so replicate here.  */
      if (current_function_calls_eh_return)
	{
	  unsigned int i, regno;

	  for (i = 0; ; ++i)
	    {
	      rtx addr, reg, mem;

	      regno = EH_RETURN_DATA_REGNO (i);
	      if (regno == INVALID_REGNUM)
		break;

	      reg = gen_rtx_REG (reg_mode, regno);
	      addr = plus_constant (frame_reg_rtx,
				    info->ehrd_offset + sp_offset
				    + reg_size * (int) i);
	      mem = gen_rtx_MEM (reg_mode, addr);
	      set_mem_alias_set (mem, rs6000_sr_alias_set);

	      insn = emit_move_insn (mem, reg);
	      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
				    NULL_RTX, NULL_RTX);
	    }
	}
      callers_lr_already_saved = 1;
      saved_world = 1;
      goto world_saved;
    }
#endif /* TARGET_MACHO */
  /* APPLE LOCAL end AltiVec */

  /* If we need to save CR, put it into r12.  */
  if (info->cr_save_p && frame_reg_rtx != frame_ptr_rtx)
    {
      int i, count=0;

      /* APPLE LOCAL begin special ObjC method use of R12 */
      /* For Darwin, use R2, so we don't clobber the special ObjC
	 method use of R12.  R11 has a special meaning for Ada, so we
	 can't use that.  */
      cr_save_rtx = gen_rtx_REG (SImode, DEFAULT_ABI == ABI_DARWIN ? 2 : 12);
      /* APPLE LOCAL end special ObjC method use of R12 */
      /* APPLE LOCAL begin optimized mfcr instruction generation */
      for (i = 0; i < 8; i++)
      if (regs_ever_live[CR0_REGNO+i] && ! call_used_regs[CR0_REGNO+i])
        count++;      
      if (count > 1)
	emit_insn (gen_movesi_from_cr (cr_save_rtx));
      else if (count == 1)
      {
        rtvec p;
        int ndx; 

        p = rtvec_alloc (count);

        ndx = 0;       
        for (i = 0; i < 8; i++)
          if (regs_ever_live[CR0_REGNO+i] && ! call_used_regs[CR0_REGNO+i])
          {
                rtvec r = rtvec_alloc (2);
		RTVEC_ELT (r, 0) = gen_rtx_REG (CCmode, CR0_REGNO+i);
		RTVEC_ELT (r, 1) = GEN_INT (1 << (7-i));
		RTVEC_ELT (p, ndx) =
		  gen_rtx_SET (VOIDmode, cr_save_rtx, gen_rtx_UNSPEC (SImode, r, 20));
                ndx++;
          }    
          emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
          if (ndx != count)
            abort ();   
      }
      /* APPLE LOCAL end optimized mfcr instruction generation */
    }

  /* Do any required saving of fpr's.  If only one or two to save, do
     it ourselves.  Otherwise, call function.  */
  if (saving_FPRs_inline)
    {
      int i;
      for (i = 0; i < 64 - info->first_fp_reg_save; i++)
	if ((regs_ever_live[info->first_fp_reg_save+i] 
	     && ! call_used_regs[info->first_fp_reg_save+i]))
	  emit_frame_save (frame_reg_rtx, frame_ptr_rtx, DFmode,
			   info->first_fp_reg_save + i,
			   info->fp_save_offset + sp_offset + 8 * i,
			   info->total_size);
    }
  else if (info->first_fp_reg_save != 64)
    {
      int i;
      char rname[30];
      const char *alloc_rname;
      rtvec p;

      /* APPLE LOCAL begin Reduce code size / improve performance */
      int gen_following_label = 0;
      int count = 0;

      if (current_function_uses_pic_offset_table && flag_pic
#ifdef INSN_SCHEDULING
	  /* Prevent the compiler from crashing
	     while scheduling insns after global_alloc!  */
	  && (optimize == 0 || !flag_schedule_insns_after_reload)
#endif
	  /* If this is the last CALL in the prolog, then we've got our PC.
	     If we're saving AltiVec regs via a function, we're not last.  */
	  && (info->first_altivec_reg_save > LAST_ALTIVEC_REGNO 
	      || VECTOR_SAVE_INLINE (info->first_altivec_reg_save)))
	gen_following_label = lr_already_set_up_for_pic = 1;

      /* APPLE LOCAL: +2 (could be conditionalized) */
      p = rtvec_alloc (2 + 64 - info->first_fp_reg_save + 2
			+ gen_following_label);
      
      RTVEC_ELT (p, count++) = gen_rtx_CLOBBER (VOIDmode, 
					  gen_rtx_REG (Pmode, 
						       LINK_REGISTER_REGNUM));
      /* APPLE LOCAL begin reduce code size */
#if TARGET_MACHO
      /* We have to calculate the offset into saveFP to where we must
	 call (!!)  SAVEFP also saves the caller's LR -- placed into
	 R0 above -- into 8(R1).  SAVEFP/RESTOREFP should never be
	 called to save or restore only F31.  */

      if (info->lr_save_offset != 8 || info->first_fp_reg_save == 63)
	abort ();

      sprintf (rname, "*saveFP%s%.0d ; save f%d-f31",
	       (info->first_fp_reg_save - 32 == 14 ? "" : "+"),
	       (info->first_fp_reg_save - 46) * 4,
	       info->first_fp_reg_save - 32);
#else
      /* APPLE LOCAL end reduce code size */
      sprintf (rname, "%s%d%s", SAVE_FP_PREFIX,
	       info->first_fp_reg_save - 32, SAVE_FP_SUFFIX);
      /* APPLE LOCAL reduce code size */
#endif /* TARGET_MACHO */
      alloc_rname = ggc_strdup (rname);
      RTVEC_ELT (p, count++) = gen_rtx_USE (VOIDmode,
				      gen_rtx_SYMBOL_REF (Pmode,
							  alloc_rname));
      /* APPLE LOCAL reduce code size */
      if ( gen_following_label )
	RTVEC_ELT (p, count++) = gen_rtx_USE (VOIDmode, const0_rtx);
      for (i = 0; i < 64 - info->first_fp_reg_save; i++)
	{
	  rtx addr, reg, mem;
	  reg = gen_rtx_REG (DFmode, info->first_fp_reg_save + i);
	  addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->fp_save_offset 
					+ sp_offset + 8*i));
	  mem = gen_rtx_MEM (DFmode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, count++) = gen_rtx_SET (VOIDmode, mem, reg);
	}
      /* APPLE LOCAL begin fix 2866661 */
#if TARGET_MACHO
      /* Darwin version of these functions stores R0.  */
      RTVEC_ELT (p, count++) = gen_rtx_USE (VOIDmode, gen_rtx_REG (Pmode, 0));

      /* If we saved LR, *tell* people about it!  */
      if (info->lr_save_p)
	{
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
				GEN_INT (info->lr_save_offset + sp_offset));
	  rtx mem = gen_rtx_MEM (Pmode, addr);
	  /* This should not be of rs6000_sr_alias_set, because of
	     __builtin_return_address.  */
	  RTVEC_ELT (p, count++) = gen_rtx_SET (Pmode, mem, 
				gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM));
	}
#endif
      /* APPLE LOCAL end fix 2866661 */
      insn = emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
			    NULL_RTX, NULL_RTX);
      callers_lr_already_saved = 1;
    }

  /* Save GPRs.  This is done as a PARALLEL if we are using
     the store-multiple instructions.  */
  if (using_store_multiple)
    {
      rtvec p;
      int i;
      p = rtvec_alloc (32 - info->first_gp_reg_save);
      for (i = 0; i < 32 - info->first_gp_reg_save; i++)
	{
	  rtx addr, reg, mem;
	  reg = gen_rtx_REG (reg_mode, info->first_gp_reg_save + i);
	  addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
			       GEN_INT (info->gp_save_offset 
					+ sp_offset 
					+ reg_size * i));
	  mem = gen_rtx_MEM (reg_mode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, i) = gen_rtx_SET (VOIDmode, mem, reg);
	}
      insn = emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
			    NULL_RTX, NULL_RTX);
    }
  else
    {
      int i;
      for (i = 0; i < 32 - info->first_gp_reg_save; i++)
	if ((regs_ever_live[info->first_gp_reg_save+i] 
	     && ! call_used_regs[info->first_gp_reg_save+i])
	    || (i+info->first_gp_reg_save == RS6000_PIC_OFFSET_TABLE_REGNUM
		&& ((DEFAULT_ABI == ABI_V4 && flag_pic == 1)
		    /* APPLE LOCAL begin volatile pic base reg in leaves */
		    || (DEFAULT_ABI == ABI_DARWIN && flag_pic
			&& current_function_uses_pic_offset_table
			&& cfun->machine->substitute_pic_base_reg == -1))))
	            /* APPLE LOCAL end volatile pic base reg in leaves */
	  {
	    rtx addr, reg, mem;
	    reg = gen_rtx_REG (reg_mode, info->first_gp_reg_save + i);

	    if (TARGET_SPE_ABI)
	      {
		int offset = info->spe_gp_save_offset + sp_offset + 8 * i;
		rtx b;

		if (!SPE_CONST_OFFSET_OK (offset))
		  {
		    b = gen_rtx_REG (Pmode, FIXED_SCRATCH);
		    emit_move_insn (b, GEN_INT (offset));
		  }
		else
		  b = GEN_INT (offset);

		addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, b);
		mem = gen_rtx_MEM (V2SImode, addr);
		set_mem_alias_set (mem, rs6000_sr_alias_set);
		insn = emit_move_insn (mem, reg);

		if (GET_CODE (b) == CONST_INT)
		  rs6000_frame_related (insn, frame_ptr_rtx, info->total_size,
					NULL_RTX, NULL_RTX);
		else
		  rs6000_frame_related (insn, frame_ptr_rtx, info->total_size,
					b, GEN_INT (offset));
	      }
	    else
	      {
		addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
				     GEN_INT (info->gp_save_offset 
					      + sp_offset 
					      + reg_size * i));
		mem = gen_rtx_MEM (reg_mode, addr);
		set_mem_alias_set (mem, rs6000_sr_alias_set);

		insn = emit_move_insn (mem, reg);
		rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
				      NULL_RTX, NULL_RTX);
	      }
	  }
    }

  /* ??? There's no need to emit actual instructions here, but it's the
     easiest way to get the frame unwind information emitted.  */
  if (current_function_calls_eh_return)
    {
      unsigned int i, regno;

      for (i = 0; ; ++i)
	{
	  regno = EH_RETURN_DATA_REGNO (i);
	  if (regno == INVALID_REGNUM)
	    break;

	  emit_frame_save (frame_reg_rtx, frame_ptr_rtx, reg_mode, regno,
			   info->ehrd_offset + sp_offset
			   + reg_size * (int) i,
			   info->total_size);
	}
    }

  /* APPLE LOCAL special ObjC method use of R12 */
  if (objc_method_using_pic)
      rs6000_maybe_dead (
	   emit_move_insn (gen_rtx_REG (Pmode,
				  cfun->machine->substitute_pic_base_reg == -1 
				  ? PIC_OFFSET_TABLE_REGNUM 
				  : cfun->machine->substitute_pic_base_reg),
			   gen_rtx_REG (Pmode, 12)));

  /* Save lr if we used it.  */
  /* APPLE LOCAL: callers_lr_already_saved */
  if (info->lr_save_p && !callers_lr_already_saved)
    {
      rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->lr_save_offset + sp_offset));
      rtx reg = gen_rtx_REG (Pmode, 0);
      rtx mem = gen_rtx_MEM (Pmode, addr);
      /* This should not be of rs6000_sr_alias_set, because of
	 __builtin_return_address.  */
      
      insn = emit_move_insn (mem, reg);
      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
			    reg, gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM));
    }

  /* Save CR if we use any that must be preserved.  */
  if (info->cr_save_p)
    {
      rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->cr_save_offset + sp_offset));
      rtx mem = gen_rtx_MEM (SImode, addr);

      set_mem_alias_set (mem, rs6000_sr_alias_set);

      /* If r12 was used to hold the original sp, copy cr into r0 now
	 that it's free.  */
      if (REGNO (frame_reg_rtx) == 12)
	{
	  cr_save_rtx = gen_rtx_REG (SImode, 0);
	  emit_insn (gen_movesi_from_cr (cr_save_rtx));
	}
      insn = emit_move_insn (mem, cr_save_rtx);

      /* Now, there's no way that dwarf2out_frame_debug_expr is going
	 to understand '(unspec:SI [(reg:CC 68) ...] 19)'.  But that's
	 OK.  All we have to do is specify that _one_ condition code
	 register is saved in this stack slot.  The thrower's epilogue
	 will then restore all the call-saved registers.
	 We use CR2_REGNO (70) to be compatible with gcc-2.95 on Linux.  */
      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
			    cr_save_rtx, gen_rtx_REG (SImode, CR2_REGNO));
    }

  /* Update stack and set back pointer unless this is V.4, 
     for which it was done previously.  */
  if (info->push_p && DEFAULT_ABI != ABI_V4)
    /* APPLE LOCAL begin AltiVec */
    {
      /* We need to set SP_OFFSET or SP_REG if we're saving *any*
         vector register -- INCLUDING VRsave.  */
      if (info->vrsave_size
	  || info->first_altivec_reg_save <= LAST_ALTIVEC_REGNO)
	{
	  if (info->total_size < 32767)
	    sp_offset = info->total_size;
	  else
	    frame_reg_rtx = frame_ptr_rtx;
	}
      rs6000_emit_allocate_stack (info->total_size, 
				  (frame_reg_rtx != sp_reg_rtx));
      if (frame_reg_rtx != sp_reg_rtx)
	rs6000_emit_stack_tie ();
    }

  /* Save AltiVec registers if needed.  */
  if (TARGET_ALTIVEC_ABI && info->altivec_size != 0
      && VECTOR_SAVE_INLINE (info->first_altivec_reg_save))
    {
      int i;

      /* There should be a non inline version of this, for when we
	 are saving lots of vector registers.  */
      for (i = info->first_altivec_reg_save; i <= LAST_ALTIVEC_REGNO; ++i)
	if (regs_ever_live[i] && ! call_used_regs[i])
	  {
	    rtx addr, areg, savereg, mem;

	    savereg = gen_rtx_REG (V4SImode, i);

	    areg = gen_rtx_REG (Pmode, 0);
	    emit_move_insn
	      (areg, GEN_INT (info->altivec_save_offset
			      + sp_offset
			      + 16 * (i - info->first_altivec_reg_save)));

	    /* AltiVec addressing mode is [reg+reg].  */
	    addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, areg);
	    mem = gen_rtx_MEM (V4SImode, addr);
	    set_mem_alias_set (mem, rs6000_sr_alias_set);

	    insn = emit_move_insn (mem, savereg);
	    rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
				  NULL_RTX, NULL_RTX);
	  }
    }
  else if (info->first_altivec_reg_save <= LAST_ALTIVEC_REGNO)
    {
      int i;
      char rname[30];
      const char *alloc_rname;
      int vregno = info->first_altivec_reg_save - FIRST_ALTIVEC_REGNO;
      rtvec p;
      /* APPLE LOCAL begin performance enhancement */
      int j=0;
      int gen_following_label = 0;
      /* APPLE LOCAL end performance enhancement */
      rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->altivec_save_offset + 
					sp_offset + info->altivec_size));
      rtx treg = gen_rtx_REG (Pmode, 0);
      emit_move_insn (treg, addr);

      /* APPLE LOCAL begin performance enhancement */
#ifdef INSN_SCHEDULING
      /* Prevent the compiler from crashing
	 while scheduling insns after global_alloc!  */
      if (optimize == 0 || !flag_schedule_insns_after_reload)
#endif
      if (current_function_uses_pic_offset_table && flag_pic)
	gen_following_label = lr_already_set_up_for_pic = 1;
      /* APPLE LOCAL end performance enhancement */

#if TARGET_MACHO
	/* A Darwin extension to the vector save routine is to have a
	   variant which returns with VRsave in R11.  A clobber is
	   added for R11 below.  */
	sprintf (rname, "*saveVEC%s%s%.0d ; save v%d-v31",
		 (info->vrsave_save_p ? "_vr11" : ""),
		 (vregno == 20 ? "" : "+"), (vregno - 20) * 8, vregno);
#else
	sprintf (rname, "%s%d%s", SAVE_VECTOR_PREFIX,
		 vregno, SAVE_VECTOR_SUFFIX);
#endif
      alloc_rname = ggc_strdup (rname);

      p = rtvec_alloc (3 + LAST_ALTIVEC_REGNO+1
		+ gen_following_label
		- info->first_altivec_reg_save
		+ ((TARGET_MACHO && info->vrsave_save_p) ? 1 : 0));
      RTVEC_ELT (p, j++) = gen_rtx_CLOBBER (VOIDmode, 
					  gen_rtx_REG (Pmode, 
						       LINK_REGISTER_REGNUM));
      RTVEC_ELT (p, j++) = gen_rtx_USE (VOIDmode,
				      gen_rtx_SYMBOL_REF (Pmode, alloc_rname));	
      /* APPLE LOCAL performance enhancement */
      if ( gen_following_label )
	RTVEC_ELT (p, j++) = gen_rtx_USE (VOIDmode, const0_rtx);

      for (i = 0; info->first_altivec_reg_save + i <= LAST_ALTIVEC_REGNO; i++)
	{
	  rtx reg = gen_rtx_REG (V4SImode, info->first_altivec_reg_save + i);
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
				   GEN_INT (info->altivec_save_offset 
					    + sp_offset + 16 * i));
	  rtx mem = gen_rtx_MEM (V4SImode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, j++) = gen_rtx_SET (VOIDmode, mem, reg);
	}
      /* Prevent any attempt to delete the setting of treg!  */
      RTVEC_ELT (p, j++) = gen_rtx_USE (VOIDmode, treg);

      if ( TARGET_MACHO && info->vrsave_save_p )
	RTVEC_ELT (p, j++) = gen_rtx_CLOBBER (VOIDmode,
				    gen_rtx_REG (SImode, 11));

      insn = emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
			    NULL_RTX, NULL_RTX);
    }

#if TARGET_MACHO
world_saved:
#endif

  /* Save VRsave if required. (Apple version)  */
  /* APPLE LOCAL AltiVec */
  if (info->vrsave_save_p)
    {
      rtx reg = NULL_RTX;
      unsigned long mask = compute_vrsave_mask ();

#if TARGET_MACHO
      if (saved_world)
	{
	  /* When calling save_world with -faltivec off, we cannot generate
	     any Altivec instructions here.  When -faltivec is on, save_world
	     stores VRsave.  In either case we needn't do it here. */
	  mask = 0;
	}
      else				/* "normal" function  */
#endif
	{
	  if (!mask)
	    abort ();

	  /* If we're calling ._savevxx_vr, it returns with r11 set to VRsave,
	     so there's no need to mfspr here.  */
	  if (info->first_altivec_reg_save > LAST_ALTIVEC_REGNO
	      || VECTOR_SAVE_INLINE (info->first_altivec_reg_save))
	    {
	      int savevr = alloc_volatile_reg (); 
	      if (savevr <= 0)
		savevr = 11;
	      reg = gen_rtx_REG (SImode, savevr);
	      emit_insn (gen_mov_from_vrsave (reg));
	    }
	  else
	    reg = gen_rtx_REG (SImode, 11); /* VRsave returned in R11.  */

 	  if (info->vrsave_size != 0)	/* saving VRsave on stack.  */
	    {
	      rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
				       GEN_INT (info->vrsave_save_offset 
						+ sp_offset));
	      rtx mem = gen_rtx_MEM (SImode, addr);
	      set_mem_alias_set (mem, rs6000_sr_alias_set);

	      insn = emit_move_insn (mem, reg);
	      rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
				    NULL_RTX, NULL_RTX);
	    }

        }					/* not saving the world...  */
      if (mask)
	{
	  int count, r;
	  rtx r0;
	  rtvec p;

	  /* Cast to long necessary here so GEN_INT sign-extends
	     correctly, without it you get an infinite loop OR'ing
	     with 0xf8000000 ! */
	  /* "reg" is where the old VRsave value currently lives.
	     In the case where alloc_volatile_reg was called above,
	     we must not store into it at this point; the epilogue
	     expects it to contain the same value. */
	  r0 = gen_rtx_REG (SImode, 0);
	  emit_insn (gen_iorsi3 (r0, reg, GEN_INT ((long)mask)));

	  /* Instructions that use a particular V reg cannot be moved
	     back across the setting of VRsave, since they would then
	     be fair game for the OS to clobber.  To indicate this,
	     mark the VRsave instruction as clobbering all used V
	     regs.  */
	  /* Parameter regs do not appear in the mask; these need 
	     to be marked used. */
	  count = 0;
	  for (r = FIRST_ALTIVEC_REGNO; r <= LAST_ALTIVEC_REGNO; ++r)
	    if ((mask & ALTIVEC_REG_BIT (r)) != 0)
	      count++;
	  count += cfun->args_info.vregno - ALTIVEC_ARG_MIN_REG;
	  p = rtvec_alloc (1 + count);
	  count = 0;
	  RTVEC_ELT (p, count++) = gen_mov_to_vrsave (r0);
	  for (r = FIRST_ALTIVEC_REGNO; r <= LAST_ALTIVEC_REGNO; ++r)
	    if ((mask & ALTIVEC_REG_BIT (r)) != 0)
	      RTVEC_ELT (p, count++) =
		gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (V4SImode, r));
	  for (r = ALTIVEC_ARG_MIN_REG; r <= cfun->args_info.vregno - 1; r++ )
	    RTVEC_ELT (p, count++) = 
	      gen_rtx_USE (VOIDmode, gen_rtx_REG (V16QImode, r));
	  insn = emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
	  rs6000_frame_related (insn, frame_ptr_rtx, info->total_size, 
				NULL_RTX, NULL_RTX);
        }
    }
  /* APPLE LOCAL end AltiVec */

  /* Set frame pointer, if needed.  */
  if (frame_pointer_needed)
    {
      insn = emit_move_insn (gen_rtx_REG (Pmode, FRAME_POINTER_REGNUM), 
			     sp_reg_rtx);
      RTX_FRAME_RELATED_P (insn) = 1;
    }

  /* If we are using RS6000_PIC_OFFSET_TABLE_REGNUM, we need to set it up.  */
  if ((TARGET_TOC && TARGET_MINIMAL_TOC && get_pool_size () != 0)
      || (DEFAULT_ABI == ABI_V4 && flag_pic == 1
	  && regs_ever_live[RS6000_PIC_OFFSET_TABLE_REGNUM]))
  {
    /* If emit_load_toc_table will use the link register, we need to save
       it.  We use R11 for this purpose because emit_load_toc_table
       can use register 0.  This allows us to use a plain 'blr' to return
       from the procedure more often.  */
    int save_LR_around_toc_setup = (TARGET_ELF && flag_pic != 0
				    && ! info->lr_save_p
				    && EXIT_BLOCK_PTR->pred != NULL);
    if (save_LR_around_toc_setup)
      emit_move_insn (gen_rtx_REG (Pmode, 11), 
		      gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM));
    
    rs6000_emit_load_toc_table (TRUE);

    if (save_LR_around_toc_setup)
      emit_move_insn (gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM), 
		      gen_rtx_REG (Pmode, 11));
  }

#if TARGET_MACHO
  if (DEFAULT_ABI == ABI_DARWIN
      /* APPLE LOCAL special ObjC method use of R12 */
      && !objc_method_using_pic
      && flag_pic && current_function_uses_pic_offset_table)
    {
      rtx dest = gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM);
      char *picbase = machopic_function_base_name ();
      rtx src = gen_rtx_SYMBOL_REF (Pmode, picbase);

      /* APPLE LOCAL begin save and restore LR */
      /* Save and restore LR locally around this call (in R0).  */
      if (!info->lr_save_p)
	rs6000_maybe_dead (emit_move_insn (gen_rtx_REG (Pmode, 0), dest));
      /* APPLE LOCAL end save and restore LR */

      /* APPLE LOCAL begin performance enhancement */
#if TARGET_MACHO
      if (!lr_already_set_up_for_pic)
	rs6000_maybe_dead (emit_insn (gen_load_macho_picbase (dest, src)));
#endif
      /* APPLE LOCAL end performance enhancement */

      /* APPLE LOCAL begin volatile pic base reg in leaves */
      rs6000_maybe_dead (
	 emit_move_insn (
		 gen_rtx_REG (Pmode,
			      cfun->machine->substitute_pic_base_reg == -1
			      ? RS6000_PIC_OFFSET_TABLE_REGNUM
			      : cfun->machine->substitute_pic_base_reg),
		 dest));
      if (!info->lr_save_p)
	rs6000_maybe_dead (emit_move_insn (dest, gen_rtx_REG (Pmode, 0)));
      /* APPLE LOCAL end */
    }
#endif
}

/* Write function prologue.  */

static void
rs6000_output_function_prologue (file, size)
     FILE *file;
     HOST_WIDE_INT size ATTRIBUTE_UNUSED;
{
  rs6000_stack_t *info = rs6000_stack_info ();

  if (TARGET_DEBUG_STACK)
    debug_stack_info (info);

  /* APPLE LOCAL do not extern fp save/restore */
#if !TARGET_MACHO
  /* Write .extern for any function we will call to save and restore
     fp values.  */
  if (info->first_fp_reg_save < 64
      && !FP_SAVE_INLINE (info->first_fp_reg_save))
    fprintf (file, "\t.extern %s%d%s\n\t.extern %s%d%s\n",
	     SAVE_FP_PREFIX, info->first_fp_reg_save - 32, SAVE_FP_SUFFIX,
	     RESTORE_FP_PREFIX, info->first_fp_reg_save - 32,
	     RESTORE_FP_SUFFIX);
  /* APPLE LOCAL do not extern fp save/restore */
#endif /* !TARGET_MACHO */

  /* APPLE LOCAL begin AltiVec */
#if !TARGET_MACHO
  /* Write .extern for any function we will call to save and restore vector
     values.  */
  if (info->first_altivec_reg_save <= LAST_ALTIVEC_REGNO
      && !VECTOR_SAVE_INLINE (info->first_altivec_reg_save))
    fprintf (file, "\t.extern %s%d%s\n\t.extern %s%d%s\n",
	     SAVE_VECTOR_PREFIX,
	     info->first_altivec_reg_save - FIRST_ALTIVEC_REGNO,
	     SAVE_VECTOR_SUFFIX, RESTORE_VECTOR_PREFIX,
	     info->first_altivec_reg_save - FIRST_ALTIVEC_REGNO,
	     RESTORE_VECTOR_SUFFIX);
#endif /* TARGET_MACHO */
  /* APPLE LOCAL end AltiVec */

  /* Write .extern for AIX common mode routines, if needed.  */
  if (! TARGET_POWER && ! TARGET_POWERPC && ! common_mode_defined)
    {
      fputs ("\t.extern __mulh\n", file);
      fputs ("\t.extern __mull\n", file);
      fputs ("\t.extern __divss\n", file);
      fputs ("\t.extern __divus\n", file);
      fputs ("\t.extern __quoss\n", file);
      fputs ("\t.extern __quous\n", file);
      common_mode_defined = 1;
    }

  /* APPLE LOCAL special ObjC method use of R12 */
#if TARGET_MACHO
  if ( HAVE_prologue && DEFAULT_ABI == ABI_DARWIN && objc_method_using_pic )
    {
      /* APPLE FIXME isn't there an asm macro to do all this? */
      const char* piclabel = machopic_function_base_name ();
      fprintf(file, "%s:\n", (*piclabel == '*') ? piclabel + 1 : piclabel);
    }
#endif

  if (! HAVE_prologue)
    {
      start_sequence ();

      /* A NOTE_INSN_DELETED is supposed to be at the start and end of
	 the "toplevel" insn chain.  */
      emit_note (0, NOTE_INSN_DELETED);
      rs6000_emit_prologue ();
      emit_note (0, NOTE_INSN_DELETED);

      /* Expand INSN_ADDRESSES so final() doesn't crash. */
      {
	rtx insn;
	unsigned addr = 0;
	for (insn = get_insns (); insn != 0; insn = NEXT_INSN (insn))
	  {
	    INSN_ADDRESSES_NEW (insn, addr);
	    addr += 4;
	  }
      }

      if (TARGET_DEBUG_STACK)
	debug_rtx_list (get_insns (), 100);
      final (get_insns (), file, FALSE, FALSE);
      end_sequence ();
    }

  rs6000_pic_labelno++;
}
  
/* Emit function epilogue as insns.

   At present, dwarf2out_frame_debug_expr doesn't understand
   register restores, so we don't bother setting RTX_FRAME_RELATED_P
   anywhere in the epilogue.  Most of the insns below would in any case
   need special notes to explain where r11 is in relation to the stack.  */

void
rs6000_emit_epilogue (sibcall)
     int sibcall;
{
  rs6000_stack_t *info;
  int restoring_FPRs_inline;
  int using_load_multiple;
  int using_mfcr_multiple;
  int use_backchain_to_restore_sp;
  int sp_offset = 0;
  rtx sp_reg_rtx = gen_rtx_REG (Pmode, 1);
  rtx frame_reg_rtx = sp_reg_rtx;
  /* APPLE LOCAL begin 64bit registers, ABI32bit */
  enum machine_mode reg_mode = (TARGET_POWERPC64 && ! TARGET_32BIT) ? DImode : SImode;
  int reg_size = (TARGET_POWERPC64 && ! TARGET_32BIT) ? 8 : 4;
  /* APPLE LOCAL end 64bit registers, ABI32bit */
  int i;
  /* APPLE LOCAL begin AltiVec */
  int dont_touch_lr = 0;
  int lr_extra_offset = 0;
  enum { none, call, branch } vrsave = none;
  int j;
  /* APPLE LOCAL end AltiVec */

  if (TARGET_SPE_ABI)
    {
      reg_mode = V2SImode;
      reg_size = 8;
    }

  info = rs6000_stack_info ();
  using_load_multiple = (TARGET_MULTIPLE && (! TARGET_POWERPC64 || TARGET_32BIT)
			 && !TARGET_SPE_ABI
			 && info->first_gp_reg_save < 31);
  restoring_FPRs_inline = (sibcall
			   || current_function_calls_eh_return
			   || info->first_fp_reg_save == 64
			   || FP_SAVE_INLINE (info->first_fp_reg_save));
  use_backchain_to_restore_sp = (frame_pointer_needed 
				 || current_function_calls_alloca
				 || info->total_size > 32767);
  using_mfcr_multiple = (rs6000_cpu == PROCESSOR_PPC601
			 || rs6000_cpu == PROCESSOR_PPC603
			 || rs6000_cpu == PROCESSOR_PPC750
			 /* APPLE LOCAL ? */
			 || rs6000_cpu == PROCESSOR_PPC7400
			 || optimize_size);

  /* APPLE LOCAL begin AltiVec */
#ifdef TARGET_MACHO
  if (info->world_save_p)
    {
      int i, j;
      char rname[30], *rnamep;
      const char *alloc_rname;
      rtvec p;

#if 0 /* I'm pretty certain that this is necessary, but including
	 causes a failure in cfgrtl.c when optimizing, because
	 direct_return() is false.  sts 2001-10-25 */
  /* Load exception handler data registers, if needed.  */
  if (current_function_calls_eh_return)
    {
      unsigned int i, regno;

      for (i = 0; ; ++i)
	{
	  rtx addr, mem;

	  regno = EH_RETURN_DATA_REGNO (i);
	  if (regno == INVALID_REGNUM)
	    break;

	  addr = plus_constant (frame_reg_rtx,
				info->ehrd_offset + sp_offset
				+ reg_size * (int) i);
	  mem = gen_rtx_MEM (reg_mode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  emit_move_insn (gen_rtx_REG (reg_mode, regno), mem);
	}
    }
#endif

      /* eh_rest_world_r10 will return to the location saved in the LR
	 stack slot (which is not likely to be our caller.)
	 Input: R10 -- stack adjustment.  Clobbers R0, R11, R12, R7, R8.
	 rest_world is similar, except any R10 parameter is ignored. 
	 The exception-handling stuff that was here in 2.95 is no
	 longer necessary.  */

      p = rtvec_alloc (9
		       + LAST_ALTIVEC_REGNO + 1 - info->first_altivec_reg_save
		       + 63 + 1 - info->first_fp_reg_save);

      strcpy (rname, (current_function_calls_eh_return) ?
			"*eh_rest_world_r10" : "*rest_world");
      alloc_rname = ggc_strdup (rname);

      j = 0;
      RTVEC_ELT (p, j++) = gen_rtx_RETURN (VOIDmode);
      RTVEC_ELT (p, j++) = gen_rtx_USE (VOIDmode, 
					gen_rtx_REG (Pmode, 
						     LINK_REGISTER_REGNUM));
      RTVEC_ELT (p, j++)
	= gen_rtx_USE (VOIDmode, gen_rtx_SYMBOL_REF (Pmode, alloc_rname));
      /* The instruction pattern requires a clobber here;
	 it is shared with the restVEC helper. */
      RTVEC_ELT (p, j++)
	= gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (Pmode, 11));

      for (i = 0; info->first_altivec_reg_save + i <= LAST_ALTIVEC_REGNO; i++)
	{
	  rtx reg = gen_rtx_REG (V4SImode, info->first_altivec_reg_save + i);
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
				   GEN_INT (info->altivec_save_offset 
					    + sp_offset + 16 * i));
	  rtx mem = gen_rtx_MEM (V4SImode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, j++) = gen_rtx_SET (VOIDmode, reg, mem);
	}
      for (i = 0; info->first_fp_reg_save + i <= 63; i++)
	{
	  rtx reg = gen_rtx_REG (DFmode, info->first_fp_reg_save + i);
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
				   GEN_INT (info->fp_save_offset 
					    + sp_offset + 8 * i));
	  rtx mem = gen_rtx_MEM (DFmode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, j++) = gen_rtx_SET (VOIDmode, reg, mem);
	}

      RTVEC_ELT (p, j++)
	= gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (Pmode, 0));
      RTVEC_ELT (p, j++)
	= gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (SImode, 12));
      RTVEC_ELT (p, j++)
	= gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (SImode, 7));
      RTVEC_ELT (p, j++)
	= gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (SImode, 8));
      RTVEC_ELT (p, j++)
	= gen_rtx_USE (VOIDmode, gen_rtx_REG (SImode, 10));
      emit_jump_insn (gen_rtx_PARALLEL (VOIDmode, p));

      return;
    }

  /* APPLE LOCAL sibcall */
  /* If we have to restore more than two FP registers, we can branch to
     the RESTFP restore function.  It will pickup LR from 8(R1) and
     return to our caller.  This doesn't work for sibcalls.  */

  dont_touch_lr = (!sibcall 
		   && info->first_fp_reg_save != 64 
		   && !FP_SAVE_INLINE (info->first_fp_reg_save));

#endif	/* TARGET_MACHO */
  /* APPLE LOCAL end AltiVec */

  /* If we have a frame pointer, a call to alloca,  or a large stack
     frame, restore the old stack pointer using the backchain.  Otherwise,
     we know what size to update it with.  */
  if (use_backchain_to_restore_sp)
    {
      /* Under V.4, don't reset the stack pointer until after we're done
	 loading the saved registers.  */
      /* APPLE LOCAL begin AltiVec */
      if (DEFAULT_ABI == ABI_V4 || info->vector_outside_red_zone_p)
        /* On Darwin, R11 would be clobbered by restVEC, so use R2. */
	frame_reg_rtx = gen_rtx_REG (Pmode, DEFAULT_ABI == ABI_DARWIN ? 2 : 11);
      /* APPLE LOCAL end AltiVec */

      /* APPLE LOCAL emit stack tie */
      /* Prevent scheduler from moving references to the stack, which
	 might fall below the red zone after R1 is updated,
	 across the insn that restores R1. */
      rs6000_emit_stack_tie ();

      emit_move_insn (frame_reg_rtx,
		      gen_rtx_MEM (Pmode, sp_reg_rtx));
      
    }
  else if (info->push_p)
    {
      /* APPLE LOCAL begin AltiVec */
      if (DEFAULT_ABI == ABI_V4
	  || info->vector_outside_red_zone_p)
	/* APPLE LOCAL end AltiVec */
	sp_offset = info->total_size;
      /* APPLE LOCAL begin AltiVec */
      /* Emit: "lwz r0,xx+8(r1); la r1,xx(r1)" so the load comes first.  */
      else if (info->lr_save_p && ! info->vrsave_save_p
	       && info->total_size < 32767 + sp_offset)
	lr_extra_offset = info->total_size;
      /* APPLE LOCAL end AltiVec */
      else
	{
	  emit_insn (TARGET_32BIT
		     ? gen_addsi3 (sp_reg_rtx, sp_reg_rtx,
				   GEN_INT (info->total_size))
		     : gen_adddi3 (sp_reg_rtx, sp_reg_rtx,
				   GEN_INT (info->total_size)));
	}
    }
  
  /* APPLE LOCAL begin AltiVec */
  /* Determine if we will call or branch to a vector restore routine.
     This affects how and when the link register is restored.  */
  if (info->first_altivec_reg_save != LAST_ALTIVEC_REGNO + 1
      && !VECTOR_SAVE_INLINE (info->first_altivec_reg_save))
    vrsave = (sp_offset == 0 && REGNO (sp_reg_rtx) == 1
	      && !sibcall
	      && !(info->first_fp_reg_save != 64
		   && !FP_SAVE_INLINE (info->first_fp_reg_save))
	      && !current_function_calls_eh_return
	      && !(info->vector_outside_red_zone_p
		   && use_backchain_to_restore_sp)
		 ? branch : call);

  /* APPLE FIXME redundant with new code just below? */
  /* Restore VRsave if required.  Interleave with loading of LR.  */
  if (info->vrsave_save_p)
    {
      int regnum;
      rtx reg;

      /* If saved on stack, pick it up  */
      if (info->vrsave_size)
        {
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
				   GEN_INT (info->vrsave_save_offset 
					    + sp_offset));
	  rtx mem = gen_rtx_MEM (SImode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  /* r10 is unlikely to be used from here on in :-)
	     So use it as a parameter to ._restvxx   */ 
	  reg = gen_rtx_REG (SImode, (vrsave == none) ? 12 : 10);
	  emit_move_insn (reg, mem);
	}
      else if ((regnum = alloc_volatile_reg ()) > 0)
	reg = gen_rtx_REG (SImode, regnum);
      else
	abort ();

      /* Get the old lr if we saved it.  */
      if (info->lr_save_p && vrsave != call && ! dont_touch_lr)
	{
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
				   GEN_INT (info->lr_save_offset + sp_offset));
	  rtx mem = gen_rtx_MEM (Pmode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  emit_move_insn (gen_rtx_REG (Pmode, 0), mem);
	}

      /* ._restvxx will restore VRsave from r10.  */
      if (vrsave == none)
        {
	  /* Instructions that use a particular V reg cannot be moved
	     forward across the setting of VRsave, since they would then
	     be fair game for the OS to clobber.  To indicate this,
	     mark the VRsave instruction as clobbering all used V
	     regs.  */
	  /* The return value is not present in the mask, but should
	     be marked used here. */
	  unsigned long mask = compute_vrsave_mask ();
	  bool yes = false;
	  int count = 0;
	  int r;
	  rtvec p;

	  for (r = FIRST_ALTIVEC_REGNO; r <= LAST_ALTIVEC_REGNO; ++r)
	    if ((mask & ALTIVEC_REG_BIT (r)) != 0)
	      count++;
	  diddle_return_value (is_altivec_return_reg, &yes);
	  if (yes)
	    count++;
	  p = rtvec_alloc (1 + count);
	  count = 0;
	  RTVEC_ELT (p, count++) = gen_mov_to_vrsave (reg);
	  for (r = FIRST_ALTIVEC_REGNO; r <= LAST_ALTIVEC_REGNO; ++r)
	    if ((mask & ALTIVEC_REG_BIT (r)) != 0)
	      RTVEC_ELT (p, count++) =
		gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (V4SImode, r));
	  if (yes)
	    RTVEC_ELT (p, count++) = 
	      gen_rtx_USE (VOIDmode, 
		gen_rtx_REG (V16QImode, ALTIVEC_ARG_RETURN));
	  emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
	}
    }
  /* APPLE LOCAL end AltiVec */
  /* Restore AltiVec registers if needed.  */
  /* APPLE LOCAL AltiVec */
#if 0 /* this messes up the "else if" below and causes double lr gets.  */
  if (0 /* TARGET_ALTIVEC_ABI && info->altivec_size != 0 */)
    {
      int i;

      for (i = info->first_altivec_reg_save; i <= LAST_ALTIVEC_REGNO; ++i)
	if (info->vrsave_mask & ALTIVEC_REG_BIT (i))
	  {
	    rtx addr, areg, mem;

	    areg = gen_rtx_REG (Pmode, 0);
	    emit_move_insn
	      (areg, GEN_INT (info->altivec_save_offset
			      + sp_offset
			      + 16 * (i - info->first_altivec_reg_save)));

	    /* AltiVec addressing mode is [reg+reg].  */
	    addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, areg);
	    mem = gen_rtx_MEM (V4SImode, addr);
	    set_mem_alias_set (mem, rs6000_sr_alias_set);

	    emit_move_insn (gen_rtx_REG (V4SImode, i), mem);
	  }
    }

  /* Restore VRSAVE if needed.  */
  /* APPLE LOCAL AltiVec */
  if (0 /* TARGET_ALTIVEC_ABI && info->vrsave_mask != 0 */)
    {
      rtx addr, mem, reg;

      addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			   GEN_INT (info->vrsave_save_offset + sp_offset));
      mem = gen_rtx_MEM (SImode, addr);
      set_mem_alias_set (mem, rs6000_sr_alias_set);
      reg = gen_rtx_REG (SImode, 12);
      emit_move_insn (reg, mem);

      emit_insn (generate_set_vrsave (reg, info, 1));
    }
#endif

  /* Get the old lr if we saved it.  */
  /* APPLE LOCAL AltiVec */
  else if (info->lr_save_p && vrsave != call && !dont_touch_lr)
    {
      rtx mem = gen_frame_mem_offset (Pmode, frame_reg_rtx,
				      /* APPLE LOCAL AltiVec */
				      info->lr_save_offset + lr_extra_offset + sp_offset);

      set_mem_alias_set (mem, rs6000_sr_alias_set);

      emit_move_insn (gen_rtx_REG (Pmode, 0), mem);
    }
  
  /* APPLE LOCAL begin AltiVec */
  if (lr_extra_offset)
    emit_move_insn (sp_reg_rtx, gen_rtx_PLUS (Pmode, sp_reg_rtx,
					      GEN_INT (info->total_size)));
  /* APPLE LOCAL end AltiVec */

  /* Get the old cr if we saved it.  */
  if (info->cr_save_p)
    {
      rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->cr_save_offset + sp_offset));
      rtx mem = gen_rtx_MEM (SImode, addr);

      set_mem_alias_set (mem, rs6000_sr_alias_set);

      /* APPLE LOCAL use R11 because of ObjC use of R12 in sibcall to CTR */
      emit_move_insn (gen_rtx_REG (SImode, 
	    DEFAULT_ABI == ABI_DARWIN ? 11 : 12), mem);
    }
  
  /* Set LR here to try to overlap restores below.  */
  /* APPLE LOCAL AltiVec */
  if (info->lr_save_p && vrsave != call && !dont_touch_lr)
    emit_move_insn (gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM),
		    gen_rtx_REG (Pmode, 0));
  
  /* Load exception handler data registers, if needed.  */
  if (current_function_calls_eh_return)
    {
      unsigned int i, regno;

      for (i = 0; ; ++i)
	{
	  rtx mem;

	  regno = EH_RETURN_DATA_REGNO (i);
	  if (regno == INVALID_REGNUM)
	    break;

	  mem = gen_frame_mem_offset (reg_mode, frame_reg_rtx,
				      info->ehrd_offset + sp_offset
				      + reg_size * (int) i);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  emit_move_insn (gen_rtx_REG (reg_mode, regno), mem);
	}
    }
  
  /* Restore GPRs.  This is done as a PARALLEL if we are using
     the load-multiple instructions.  */
  if (using_load_multiple)
    {
      rtvec p;
      p = rtvec_alloc (32 - info->first_gp_reg_save);
      for (i = 0; i < 32 - info->first_gp_reg_save; i++)
	{
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
				   GEN_INT (info->gp_save_offset 
					    + sp_offset 
					    + reg_size * i));
	  rtx mem = gen_rtx_MEM (reg_mode, addr);

	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, i) = 
	    gen_rtx_SET (VOIDmode,
			 gen_rtx_REG (reg_mode, info->first_gp_reg_save + i),
			 mem);
	}
      emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
    }
  else
    for (i = 0; i < 32 - info->first_gp_reg_save; i++)
      if ((regs_ever_live[info->first_gp_reg_save+i] 
	   && ! call_used_regs[info->first_gp_reg_save+i])
	  || (i+info->first_gp_reg_save == RS6000_PIC_OFFSET_TABLE_REGNUM
	      && ((DEFAULT_ABI == ABI_V4 && flag_pic == 1)
		  /* APPLE LOCAL begin darwin native */
		  || (DEFAULT_ABI == ABI_DARWIN && flag_pic
		      && current_function_uses_pic_offset_table
		      && cfun->machine->substitute_pic_base_reg == -1))))
		  /* APPLE LOCAL end darwin native */
	{
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, 
				   GEN_INT (info->gp_save_offset 
					    + sp_offset 
					    + reg_size * i));
	  rtx mem = gen_rtx_MEM (reg_mode, addr);

	  /* Restore 64-bit quantities for SPE.  */
	  if (TARGET_SPE_ABI)
	    {
	      int offset = info->spe_gp_save_offset + sp_offset + 8 * i;
	      rtx b;

	      if (!SPE_CONST_OFFSET_OK (offset))
		{
		  b = gen_rtx_REG (Pmode, FIXED_SCRATCH);
		  emit_move_insn (b, GEN_INT (offset));
		}
	      else
		b = GEN_INT (offset);

	      addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, b);
	      mem = gen_rtx_MEM (V2SImode, addr);
	    }

	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  emit_move_insn (gen_rtx_REG (reg_mode, 
				       info->first_gp_reg_save + i), mem);
	}

  /* Restore fpr's if we need to do it without calling a function.  */
  if (restoring_FPRs_inline)
    for (i = 0; i < 64 - info->first_fp_reg_save; i++)
      if ((regs_ever_live[info->first_fp_reg_save+i] 
	   && ! call_used_regs[info->first_fp_reg_save+i]))
	{
	  rtx addr, mem;
	  addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->fp_save_offset 
					+ sp_offset 
					+ 8 * i));
	  mem = gen_rtx_MEM (DFmode, addr);
	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  emit_move_insn (gen_rtx_REG (DFmode, 
				       info->first_fp_reg_save + i),
			  mem);
	}

  /* APPLE FIXME probably redundant now */
  /* Restore AltiVec registers if needed.  */
  if (TARGET_ALTIVEC_ABI && info->altivec_size != 0
      /* APPLE LOCAL AltiVec */
      && VECTOR_SAVE_INLINE (info->first_altivec_reg_save))
    {
      int i;

      for (i = info->first_altivec_reg_save; i <= LAST_ALTIVEC_REGNO; ++i)
	if (regs_ever_live[i] && ! call_used_regs[i])
	  {
	    rtx addr, areg, mem;

	    areg = gen_rtx_REG (Pmode, 0);
	    emit_move_insn
	      (areg, GEN_INT (info->altivec_save_offset
			      + sp_offset
			      + 16 * (i - info->first_altivec_reg_save)));

	    /* AltiVec addressing mode is [reg+reg].  */
	    addr = gen_rtx_PLUS (Pmode, frame_reg_rtx, areg);
	    mem = gen_rtx_MEM (V4SImode, addr);
	    set_mem_alias_set (mem, rs6000_sr_alias_set);

	    emit_move_insn (gen_rtx_REG (V4SImode, i), mem);
	  }
    }

  /* Restore VRSAVE if needed.  */
  /* APPLE LOCAL always false on Darwin */
  if (TARGET_ALTIVEC_ABI && info->vrsave_mask != 0)
    {
      rtx addr, mem, reg;

      addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			   GEN_INT (info->vrsave_save_offset + sp_offset));
      mem = gen_rtx_MEM (SImode, addr);
      set_mem_alias_set (mem, rs6000_sr_alias_set);
      reg = gen_rtx_REG (SImode, 12);
      emit_move_insn (reg, mem);

      emit_insn (generate_set_vrsave (reg, info, 1));
    }

  /* If we saved cr, restore it here.  Just those that were used.  */
  if (info->cr_save_p)
    {
      /* APPLE LOCAL use R11 because of ObjC use of R12 in sibcall to CTR */
      /* APPLE LOCAL silly name retained to minimize deviation from FSF */
      rtx r12_rtx = gen_rtx_REG (SImode, DEFAULT_ABI == ABI_DARWIN ? 11 : 12);
      int count = 0;
      
      if (using_mfcr_multiple)
	{
	  for (i = 0; i < 8; i++)
	    if (regs_ever_live[CR0_REGNO+i] && ! call_used_regs[CR0_REGNO+i])
	      count++;
	  if (count == 0)
	    abort ();
	}

      if (using_mfcr_multiple && count > 1)
	{
	  rtvec p;
	  int ndx;
	  
	  p = rtvec_alloc (count);

	  ndx = 0;
	  for (i = 0; i < 8; i++)
	    if (regs_ever_live[CR0_REGNO+i] && ! call_used_regs[CR0_REGNO+i])
	      {
		rtvec r = rtvec_alloc (2);
		RTVEC_ELT (r, 0) = r12_rtx;
		RTVEC_ELT (r, 1) = GEN_INT (1 << (7-i));
		RTVEC_ELT (p, ndx) =
		  gen_rtx_SET (VOIDmode, gen_rtx_REG (CCmode, CR0_REGNO+i), 
			       gen_rtx_UNSPEC (CCmode, r, 20));
		ndx++;
	      }
	  emit_insn (gen_rtx_PARALLEL (VOIDmode, p));
	  if (ndx != count)
	    abort ();
	}
      else
	for (i = 0; i < 8; i++)
	  if (regs_ever_live[CR0_REGNO+i] && ! call_used_regs[CR0_REGNO+i])
	    {
	      emit_insn (gen_movsi_to_cr_one (gen_rtx_REG (CCmode, 
							   CR0_REGNO+i),
					      r12_rtx));
	    }
    }

  /* If we have to restore more than two VECTOR registers,
     branch to the restore function.  */
  if (vrsave != none)
    {
      int i;
      char rname[30];
      const char *alloc_rname;
      int vregno = info->first_altivec_reg_save - FIRST_ALTIVEC_REGNO;
      rtvec p;

      rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
			       GEN_INT (info->altivec_save_offset +
					sp_offset + info->altivec_size));
      emit_move_insn (gen_rtx_REG (Pmode, 0), addr);

      p = rtvec_alloc (LAST_ALTIVEC_REGNO + 2 - info->first_altivec_reg_save
		       + ((vrsave == call) 
			   ? 4 
			   : ( 5 + 32 - info->first_gp_reg_save
			         + (restoring_FPRs_inline
				     ? (64 - info->first_fp_reg_save) : 0))));

#if TARGET_MACHO
	/* A Darwin extension to the vector restore routine is to have
	   a variant which sets the VRsave register to R10.  */
	sprintf (rname, "*restVEC%s%s%.0d ; restore v%d-v31",
		 (info->vrsave_save_p ? "_vr10" : ""),
		 (vregno == 20 ? "" : "+"), (vregno - 20) * 8, vregno);
#else
	sprintf (rname, "%s%d%s", RESTORE_VECTOR_PREFIX,
		 vregno, RESTORE_VECTOR_SUFFIX);
#endif /* TARGET_MACHO */
      alloc_rname = ggc_strdup (rname);

      j = 0;
      if (vrsave == branch)
	{
	  RTVEC_ELT (p, j++) = gen_rtx_RETURN (VOIDmode);
	  RTVEC_ELT (p, j++) =
	    gen_rtx_USE (VOIDmode, gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM));
	}
      else
	RTVEC_ELT (p, j++)
	  = gen_rtx_CLOBBER (VOIDmode,
			     gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM));
      RTVEC_ELT (p, j++)
	= gen_rtx_USE (VOIDmode, gen_rtx_SYMBOL_REF (Pmode, alloc_rname));
      RTVEC_ELT (p, j++)
	= gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (Pmode, 11));

      for (i = 0; info->first_altivec_reg_save + i <= LAST_ALTIVEC_REGNO; i++)
	{
	  rtx reg = gen_rtx_REG (V4SImode, info->first_altivec_reg_save + i);
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
				   GEN_INT (info->altivec_save_offset 
					    + sp_offset + 16*i));
	  rtx mem = gen_rtx_MEM (V4SImode, addr);

	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  RTVEC_ELT (p, j++) = gen_rtx_SET (VOIDmode, reg, mem);
	}
      if (vrsave == branch)
	{
	  /* If we restored any int or FP registers above, we must
	     mark those as used by this branch, otherwise scheduler
	     can move the stores below the branch (it doesn't know
	     it's a branch).  */
	  /* This may mark more regs used than were actually restored,
	     but that doesn't hurt anything, and simplifies the code a
	     lot.  */
	  for (i = 0; i < 32 - info->first_gp_reg_save; i++)
	    {
	      RTVEC_ELT (p, j++) = 
		gen_rtx_USE (VOIDmode,
			     gen_rtx_REG (reg_mode,
					  info->first_gp_reg_save + i));
	    }
	  if (restoring_FPRs_inline)
	    for (i = 0; i < 64 - info->first_fp_reg_save; i++)
	      {
		RTVEC_ELT (p, j++) =
		  gen_rtx_USE (VOIDmode,
			       gen_rtx_REG (DFmode,
					    info->first_fp_reg_save + i));
	      }
	}
      /* Prevent any attempt to delete the setting of R0 or R10!  */
      RTVEC_ELT (p, j++) = gen_rtx_USE (VOIDmode, gen_rtx_REG (reg_mode, 0));
      RTVEC_ELT (p, j++) = gen_rtx_USE (VOIDmode, gen_rtx_REG (reg_mode, 10));
      if (vrsave == branch)
        emit_jump_insn (gen_rtx_PARALLEL (VOIDmode, p));
      else
	emit_insn (gen_rtx_PARALLEL (VOIDmode, p));

      /* Get the old lr if we saved it.  */
      if (info->lr_save_p && vrsave == call)
	{
	  rtx addr = gen_rtx_PLUS (Pmode, frame_reg_rtx,
				   GEN_INT (info->lr_save_offset + sp_offset));
	  rtx mem = gen_rtx_MEM (Pmode, addr);

	  set_mem_alias_set (mem, rs6000_sr_alias_set);

	  emit_move_insn (gen_rtx_REG (reg_mode, 0), mem);
	  emit_move_insn (gen_rtx_REG (Pmode, LINK_REGISTER_REGNUM),
			  gen_rtx_REG (reg_mode, 0));
	}
    }
  /* APPLE LOCAL end tangentially AltiVec related */

  /* If this is V.4, unwind the stack pointer after all of the loads
     have been done.  We need to emit a block here so that sched
     doesn't decide to move the sp change before the register restores
     (which may not have any obvious dependency on the stack).  This
     doesn't hurt performance, because there is no scheduling that can
     be done after this point.  */
  /* APPLE LOCAL AltiVec */
  if (DEFAULT_ABI == ABI_V4 || info->vector_outside_red_zone_p)
    {
      if (frame_reg_rtx != sp_reg_rtx)
	  rs6000_emit_stack_tie ();

      if (use_backchain_to_restore_sp)
	{
	  emit_move_insn (sp_reg_rtx, frame_reg_rtx);
	}
      else if (sp_offset != 0)
	{
	  emit_insn (Pmode == SImode
		     ? gen_addsi3 (sp_reg_rtx, sp_reg_rtx,
				   GEN_INT (sp_offset))
		     : gen_adddi3 (sp_reg_rtx, sp_reg_rtx,
				   GEN_INT (sp_offset)));
	}
    }

  if (current_function_calls_eh_return)
    {
      rtx sa = EH_RETURN_STACKADJ_RTX;
      emit_insn (Pmode == SImode
		 ? gen_addsi3 (sp_reg_rtx, sp_reg_rtx, sa)
		 : gen_adddi3 (sp_reg_rtx, sp_reg_rtx, sa));
    }

  /* APPLE LOCAL Altivec related */
  if (!sibcall && vrsave != branch)
    {
      rtvec p;
      if (! restoring_FPRs_inline)
	p = rtvec_alloc (3 + 64 - info->first_fp_reg_save);
      else
	p = rtvec_alloc (2);

      RTVEC_ELT (p, 0) = gen_rtx_RETURN (VOIDmode);
      RTVEC_ELT (p, 1) = gen_rtx_USE (VOIDmode, 
				      gen_rtx_REG (Pmode, 
						   LINK_REGISTER_REGNUM));

      /* If we have to restore more than two FP registers, branch to the
	 restore function.  It will return to our caller.  */
      if (! restoring_FPRs_inline)
	{
	  int i;
	  char rname[30];
	  const char *alloc_rname;

	  /* APPLE LOCAL begin code size reduction / performance enhancement */
#if TARGET_MACHO
	  /* We have to calculate the offset into RESTFP to where we must
	     call (!!)  RESTFP also restores the caller's LR from 8(R1).
	     RESTFP should *never* be called to restore only F31.  */

	  if (info->lr_save_offset != 8 || info->first_fp_reg_save == 63)
	    abort ();

	  sprintf (rname, "*restFP%s%.0d ; restore f%d-f31",
		   (info->first_fp_reg_save - 32 == 14 ? "" : "+"),
		   (info->first_fp_reg_save - 46) * 4,
		   info->first_fp_reg_save - 32);
#else
	  /* APPLE LOCAL end code size reduction / performance enhancement */
	  sprintf (rname, "%s%d%s", RESTORE_FP_PREFIX, 
		   info->first_fp_reg_save - 32, RESTORE_FP_SUFFIX);
	  /* APPLE LOCAL code size reduction / performance enhancement */
#endif /* TARGET_MACHO */
	  alloc_rname = ggc_strdup (rname);
	  RTVEC_ELT (p, 2) = gen_rtx_USE (VOIDmode,
					  gen_rtx_SYMBOL_REF (Pmode,
							      alloc_rname));

	  for (i = 0; i < 64 - info->first_fp_reg_save; i++)
	    {
	      rtx addr, mem;
	      addr = gen_rtx_PLUS (Pmode, sp_reg_rtx,
				   GEN_INT (info->fp_save_offset + 8*i));
	      mem = gen_rtx_MEM (DFmode, addr);
	      set_mem_alias_set (mem, rs6000_sr_alias_set);

	      RTVEC_ELT (p, i+3) = 
		gen_rtx_SET (VOIDmode,
			     gen_rtx_REG (DFmode, info->first_fp_reg_save + i),
			     mem);
	    }
	}
      
      emit_jump_insn (gen_rtx_PARALLEL (VOIDmode, p));
    }
}

/* Write function epilogue.  */

static void
rs6000_output_function_epilogue (file, size)
     FILE *file;
     HOST_WIDE_INT size ATTRIBUTE_UNUSED;
{
  rs6000_stack_t *info = rs6000_stack_info ();

  if (! HAVE_epilogue)
    {
      rtx insn = get_last_insn ();
      /* If the last insn was a BARRIER, we don't have to write anything except
	 the trace table.  */
      if (GET_CODE (insn) == NOTE)
	insn = prev_nonnote_insn (insn);
      if (insn == 0 ||  GET_CODE (insn) != BARRIER)
	{
	  /* This is slightly ugly, but at least we don't have two
	     copies of the epilogue-emitting code.  */
	  start_sequence ();

	  /* A NOTE_INSN_DELETED is supposed to be at the start
	     and end of the "toplevel" insn chain.  */
	  emit_note (0, NOTE_INSN_DELETED);
	  rs6000_emit_epilogue (FALSE);
	  emit_note (0, NOTE_INSN_DELETED);

	  /* Expand INSN_ADDRESSES so final() doesn't crash. */
	  {
	    rtx insn;
	    unsigned addr = 0;
	    for (insn = get_insns (); insn != 0; insn = NEXT_INSN (insn))
	      {
		INSN_ADDRESSES_NEW (insn, addr);
		addr += 4;
	      }
	  }

	  if (TARGET_DEBUG_STACK)
	    debug_rtx_list (get_insns (), 100);
	  final (get_insns (), file, FALSE, FALSE);
	  end_sequence ();
	}
    }

  /* Output a traceback table here.  See /usr/include/sys/debug.h for info
     on its format.

     We don't output a traceback table if -finhibit-size-directive was
     used.  The documentation for -finhibit-size-directive reads
     ``don't output a @code{.size} assembler directive, or anything
     else that would cause trouble if the function is split in the
     middle, and the two halves are placed at locations far apart in
     memory.''  The traceback table has this property, since it
     includes the offset from the start of the function to the
     traceback table itself.

     System V.4 Powerpc's (and the embedded ABI derived from it) use a
     different traceback table.  */
  if (DEFAULT_ABI == ABI_AIX && ! flag_inhibit_size_directive
      && rs6000_traceback != traceback_none)
    {
      const char *fname = NULL;
      const char *language_string = lang_hooks.name;
      int fixed_parms = 0, float_parms = 0, parm_info = 0;
      int i;
      int optional_tbtab;

      if (rs6000_traceback == traceback_full)
	optional_tbtab = 1;
      else if (rs6000_traceback == traceback_part)
	optional_tbtab = 0;
      else
	optional_tbtab = !optimize_size && !TARGET_ELF;

      if (optional_tbtab)
	{
	  fname = XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0);
	  while (*fname == '.')	/* V.4 encodes . in the name */
	    fname++;

	  /* Need label immediately before tbtab, so we can compute
	     its offset from the function start.  */
	  ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LT");
	  ASM_OUTPUT_LABEL (file, fname);
	}

      /* The .tbtab pseudo-op can only be used for the first eight
	 expressions, since it can't handle the possibly variable
	 length fields that follow.  However, if you omit the optional
	 fields, the assembler outputs zeros for all optional fields
	 anyways, giving each variable length field is minimum length
	 (as defined in sys/debug.h).  Thus we can not use the .tbtab
	 pseudo-op at all.  */

      /* An all-zero word flags the start of the tbtab, for debuggers
	 that have to find it by searching forward from the entry
	 point or from the current pc.  */
      fputs ("\t.long 0\n", file);

      /* Tbtab format type.  Use format type 0.  */
      fputs ("\t.byte 0,", file);

      /* Language type.  Unfortunately, there doesn't seem to be any
	 official way to get this info, so we use language_string.  C
	 is 0.  C++ is 9.  No number defined for Obj-C, so use the
	 value for C for now.  There is no official value for Java,
         although IBM appears to be using 13.  There is no official value
	 for Chill, so we've chosen 44 pseudo-randomly.  */
      if (! strcmp (language_string, "GNU C")
	  || ! strcmp (language_string, "GNU Objective-C"))
	i = 0;
      else if (! strcmp (language_string, "GNU F77"))
	i = 1;
      else if (! strcmp (language_string, "GNU Ada"))
	i = 3;
      else if (! strcmp (language_string, "GNU Pascal"))
	i = 2;
      else if (! strcmp (language_string, "GNU C++"))
	i = 9;
      else if (! strcmp (language_string, "GNU Java"))
	i = 13;
      else if (! strcmp (language_string, "GNU CHILL"))
	i = 44;
      else
	abort ();
      fprintf (file, "%d,", i);

      /* 8 single bit fields: global linkage (not set for C extern linkage,
	 apparently a PL/I convention?), out-of-line epilogue/prologue, offset
	 from start of procedure stored in tbtab, internal function, function
	 has controlled storage, function has no toc, function uses fp,
	 function logs/aborts fp operations.  */
      /* Assume that fp operations are used if any fp reg must be saved.  */
      fprintf (file, "%d,",
	       (optional_tbtab << 5) | ((info->first_fp_reg_save != 64) << 1));

      /* 6 bitfields: function is interrupt handler, name present in
	 proc table, function calls alloca, on condition directives
	 (controls stack walks, 3 bits), saves condition reg, saves
	 link reg.  */
      /* The `function calls alloca' bit seems to be set whenever reg 31 is
	 set up as a frame pointer, even when there is no alloca call.  */
      fprintf (file, "%d,",
	       ((optional_tbtab << 6)
		| ((optional_tbtab & frame_pointer_needed) << 5)
		| (info->cr_save_p << 1)
		| (info->lr_save_p)));

      /* 3 bitfields: saves backchain, fixup code, number of fpr saved
	 (6 bits).  */
      fprintf (file, "%d,",
	       (info->push_p << 7) | (64 - info->first_fp_reg_save));

      /* 2 bitfields: spare bits (2 bits), number of gpr saved (6 bits).  */
      fprintf (file, "%d,", (32 - first_reg_to_save ()));

      if (optional_tbtab)
	{
	  /* Compute the parameter info from the function decl argument
	     list.  */
	  tree decl;
	  int next_parm_info_bit = 31;

	  for (decl = DECL_ARGUMENTS (current_function_decl);
	       decl; decl = TREE_CHAIN (decl))
	    {
	      rtx parameter = DECL_INCOMING_RTL (decl);
	      enum machine_mode mode = GET_MODE (parameter);

	      if (GET_CODE (parameter) == REG)
		{
		  if (GET_MODE_CLASS (mode) == MODE_FLOAT)
		    {
		      int bits;

		      float_parms++;

		      if (mode == SFmode)
			bits = 0x2;
		      else if (mode == DFmode || mode == TFmode)
			bits = 0x3;
		      else
			abort ();

		      /* If only one bit will fit, don't or in this entry.  */
		      if (next_parm_info_bit > 0)
			parm_info |= (bits << (next_parm_info_bit - 1));
		      next_parm_info_bit -= 2;
		    }
		  else
		    {
		      fixed_parms += ((GET_MODE_SIZE (mode)
				       + (ABI_UNITS_PER_WORD - 1))
				      / ABI_UNITS_PER_WORD);
		      next_parm_info_bit -= 1;
		    }
		}
	    }
	}

      /* Number of fixed point parameters.  */
      /* This is actually the number of words of fixed point parameters; thus
	 an 8 byte struct counts as 2; and thus the maximum value is 8.  */
      fprintf (file, "%d,", fixed_parms);

      /* 2 bitfields: number of floating point parameters (7 bits), parameters
	 all on stack.  */
      /* This is actually the number of fp registers that hold parameters;
	 and thus the maximum value is 13.  */
      /* Set parameters on stack bit if parameters are not in their original
	 registers, regardless of whether they are on the stack?  Xlc
	 seems to set the bit when not optimizing.  */
      fprintf (file, "%d\n", ((float_parms << 1) | (! optimize)));

      if (! optional_tbtab)
	return;

      /* Optional fields follow.  Some are variable length.  */

      /* Parameter types, left adjusted bit fields: 0 fixed, 10 single float,
	 11 double float.  */
      /* There is an entry for each parameter in a register, in the order that
	 they occur in the parameter list.  Any intervening arguments on the
	 stack are ignored.  If the list overflows a long (max possible length
	 34 bits) then completely leave off all elements that don't fit.  */
      /* Only emit this long if there was at least one parameter.  */
      if (fixed_parms || float_parms)
	fprintf (file, "\t.long %d\n", parm_info);

      /* Offset from start of code to tb table.  */
      fputs ("\t.long ", file);
      ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LT");
#if TARGET_AIX
      RS6000_OUTPUT_BASENAME (file, fname);
#else
      assemble_name (file, fname);
#endif
      fputs ("-.", file);
#if TARGET_AIX
      RS6000_OUTPUT_BASENAME (file, fname);
#else
      assemble_name (file, fname);
#endif
      putc ('\n', file);

      /* Interrupt handler mask.  */
      /* Omit this long, since we never set the interrupt handler bit
	 above.  */

      /* Number of CTL (controlled storage) anchors.  */
      /* Omit this long, since the has_ctl bit is never set above.  */

      /* Displacement into stack of each CTL anchor.  */
      /* Omit this list of longs, because there are no CTL anchors.  */

      /* Length of function name.  */
      if (*fname == '*')
	++fname;
      fprintf (file, "\t.short %d\n", (int) strlen (fname));

      /* Function name.  */
      assemble_string (fname, strlen (fname));

      /* Register for alloca automatic storage; this is always reg 31.
	 Only emit this if the alloca bit was set above.  */
      if (frame_pointer_needed)
	fputs ("\t.byte 31\n", file);

      fputs ("\t.align 2\n", file);
    }
}

/* A C compound statement that outputs the assembler code for a thunk
   function, used to implement C++ virtual function calls with
   multiple inheritance.  The thunk acts as a wrapper around a virtual
   function, adjusting the implicit object parameter before handing
   control off to the real function.

   First, emit code to add the integer DELTA to the location that
   contains the incoming first argument.  Assume that this argument
   contains a pointer, and is the one used to pass the `this' pointer
   in C++.  This is the incoming argument *before* the function
   prologue, e.g. `%o0' on a sparc.  The addition must preserve the
   values of all other incoming arguments.

   After the addition, emit code to jump to FUNCTION, which is a
   `FUNCTION_DECL'.  This is a direct pure jump, not a call, and does
   not touch the return address.  Hence returning from FUNCTION will
   return to whoever called the current `thunk'.

   The effect must be as if FUNCTION had been called directly with the
   adjusted first argument.  This macro is responsible for emitting
   all of the code for a thunk function; output_function_prologue()
   and output_function_epilogue() are not invoked.

   The THUNK_FNDECL is redundant.  (DELTA and FUNCTION have already
   been extracted from it.)  It might possibly be useful on some
   targets, but probably not.

   If you do not define this macro, the target-independent code in the
   C++ frontend will generate a less efficient heavyweight thunk that
   calls FUNCTION instead of jumping to it.  The generic approach does
   not support varargs.  */

static void
rs6000_output_mi_thunk (file, thunk_fndecl, delta, vcall_offset, function)
     FILE *file;
     tree thunk_fndecl ATTRIBUTE_UNUSED;
     HOST_WIDE_INT delta;
     HOST_WIDE_INT vcall_offset ATTRIBUTE_UNUSED;
     tree function;
{
  const char *this_reg =
    reg_names[ aggregate_value_p (TREE_TYPE (TREE_TYPE (function))) ? 4 : 3 ];
  const char *prefix;
  const char *fname;
  const char *r0	 = reg_names[0];
  const char *toc	 = reg_names[2];
  const char *schain	 = reg_names[11];
  const char *r12	 = reg_names[12];
  char buf[512];
  static int labelno = 0;

  /* Small constants that can be done by one add instruction.  */
  if (delta >= -32768 && delta <= 32767)
    {
      if (! TARGET_NEW_MNEMONICS)
	fprintf (file, "\tcal %s,%d(%s)\n", this_reg, (int) delta, this_reg);
      else
	fprintf (file, "\taddi %s,%s,%d\n", this_reg, this_reg, (int) delta);
    }

  /* 64-bit constants.  If "int" is 32 bits, we'll never hit this abort.  */
  else if (TARGET_64BIT && (delta < -2147483647 - 1 || delta > 2147483647))
    abort ();

  /* Large constants that can be done by one addis instruction.  */
  else if ((delta & 0xffff) == 0)
    asm_fprintf (file, "\t{cau|addis} %s,%s,%d\n", this_reg, this_reg,
		 (int) (delta >> 16));

  /* 32-bit constants that can be done by an add and addis instruction.  */
  else
    {
      /* Break into two pieces, propagating the sign bit from the low
	 word to the upper word.  */
      int delta_low  = ((delta & 0xffff) ^ 0x8000) - 0x8000;
      int delta_high = (delta - delta_low) >> 16;

      asm_fprintf (file, "\t{cau|addis} %s,%s,%d\n", this_reg, this_reg,
		   delta_high);

      if (! TARGET_NEW_MNEMONICS)
	fprintf (file, "\tcal %s,%d(%s)\n", this_reg, delta_low, this_reg);
      else
	fprintf (file, "\taddi %s,%s,%d\n", this_reg, this_reg, delta_low);
    }

  /* Get the prefix in front of the names.  */
  switch (DEFAULT_ABI)
    {
    default:
      abort ();

    case ABI_AIX:
      prefix = ".";
      break;
 
    case ABI_V4:
    case ABI_AIX_NODESC:
    case ABI_DARWIN:
      prefix = "";
      break;
    }

  /* If the function is compiled in this module, jump to it directly.
     Otherwise, load up its address and jump to it.  */

  fname = XSTR (XEXP (DECL_RTL (function), 0), 0);

  if (current_file_function_operand (XEXP (DECL_RTL (function), 0), VOIDmode)
      /* APPLE LOCAL long-branch */
      && ! TARGET_LONG_BRANCH
      && (! lookup_attribute ("longcall",
			      TYPE_ATTRIBUTES (TREE_TYPE (function)))
	  || lookup_attribute ("shortcall",
			       TYPE_ATTRIBUTES (TREE_TYPE (function)))))
    {
      fprintf (file, "\tb %s", prefix);
      assemble_name (file, fname);
      if (DEFAULT_ABI == ABI_V4 && flag_pic) fputs ("@local", file);
      putc ('\n', file);
    }

  else
    {
      switch (DEFAULT_ABI)
	{
	default:
	  abort ();

	case ABI_AIX:
	  /* Set up a TOC entry for the function.  */
	  ASM_GENERATE_INTERNAL_LABEL (buf, "Lthunk", labelno);
	  toc_section ();
	  ASM_OUTPUT_INTERNAL_LABEL (file, "Lthunk", labelno);
	  labelno++;

	  if (TARGET_MINIMAL_TOC)
	    fputs (TARGET_32BIT ? "\t.long " : DOUBLE_INT_ASM_OP, file);
	  else
	    {
	      fputs ("\t.tc ", file);
	      assemble_name (file, fname);
	      fputs ("[TC],", file);
	    }
	  assemble_name (file, fname);
	  putc ('\n', file);
	  function_section (current_function_decl);
	  if (TARGET_MINIMAL_TOC)
	    asm_fprintf (file, (TARGET_32BIT)
			 ? "\t{l|lwz} %s,%s(%s)\n" : "\tld %s,%s(%s)\n", r12,
			 TARGET_ELF ? ".LCTOC0@toc" : ".LCTOC..1", toc);
	  asm_fprintf (file, (TARGET_32BIT) ? "\t{l|lwz} %s," : "\tld %s,", r12);
	  assemble_name (file, buf);
	  if (TARGET_ELF && TARGET_MINIMAL_TOC)
	    fputs ("-(.LCTOC1)", file);
	  asm_fprintf (file, "(%s)\n", TARGET_MINIMAL_TOC ? r12 : toc);
	  asm_fprintf (file,
		       (TARGET_32BIT) ? "\t{l|lwz} %s,0(%s)\n" : "\tld %s,0(%s)\n",
		       r0, r12);

	  asm_fprintf (file,
		       (TARGET_32BIT) ? "\t{l|lwz} %s,4(%s)\n" : "\tld %s,8(%s)\n",
		       toc, r12);

	  asm_fprintf (file, "\tmtctr %s\n", r0);
	  asm_fprintf (file,
		       (TARGET_32BIT) ? "\t{l|lwz} %s,8(%s)\n" : "\tld %s,16(%s)\n",
		       schain, r12);

	  asm_fprintf (file, "\tbctr\n");
	  break;

	case ABI_AIX_NODESC:
	case ABI_V4:
	  fprintf (file, "\tb %s", prefix);
	  assemble_name (file, fname);
	  if (flag_pic) fputs ("@plt", file);
	  putc ('\n', file);
	  break;

#if TARGET_MACHO
	case ABI_DARWIN:
	  fprintf (file, "\tb %s", prefix);
	  /* APPLE LOCAL  dynamic-no-pic  */
	  if (MACHOPIC_INDIRECT && !machopic_name_defined_p (fname))
	    assemble_name (file, machopic_stub_name (fname));
	  else
	    assemble_name (file, fname);
	  putc ('\n', file);
	  break;
#endif
	}
    }
}

/* A quick summary of the various types of 'constant-pool tables'
   under PowerPC:

   Target	Flags		Name		One table per	
   AIX		(none)		AIX TOC		object file
   AIX		-mfull-toc	AIX TOC		object file
   AIX		-mminimal-toc	AIX minimal TOC	translation unit
   SVR4/EABI	(none)		SVR4 SDATA	object file
   SVR4/EABI	-fpic		SVR4 pic	object file
   SVR4/EABI	-fPIC		SVR4 PIC	translation unit
   SVR4/EABI	-mrelocatable	EABI TOC	function
   SVR4/EABI	-maix		AIX TOC		object file
   SVR4/EABI	-maix -mminimal-toc 
				AIX minimal TOC	translation unit

   Name			Reg.	Set by	entries	      contains:
					made by	 addrs?	fp?	sum?

   AIX TOC		2	crt0	as	 Y	option	option
   AIX minimal TOC	30	prolog	gcc	 Y	Y	option
   SVR4 SDATA		13	crt0	gcc	 N	Y	N
   SVR4 pic		30	prolog	ld	 Y	not yet	N
   SVR4 PIC		30	prolog	gcc	 Y	option	option
   EABI TOC		30	prolog	gcc	 Y	option	option

*/

/* Hash functions for the hash table.  */

static unsigned
rs6000_hash_constant (k)
     rtx k;
{
  enum rtx_code code = GET_CODE (k);
  enum machine_mode mode = GET_MODE (k);
  unsigned result = (code << 3) ^ mode;
  const char *format;
  int flen, fidx;
  
  format = GET_RTX_FORMAT (code);
  flen = strlen (format);
  fidx = 0;

  switch (code)
    {
    case LABEL_REF:
      return result * 1231 + (unsigned) INSN_UID (XEXP (k, 0));

    case CONST_DOUBLE:
      if (mode != VOIDmode)
	return real_hash (CONST_DOUBLE_REAL_VALUE (k)) * result;
      flen = 2;
      break;

    case CODE_LABEL:
      fidx = 3;
      break;

    default:
      break;
    }

  for (; fidx < flen; fidx++)
    switch (format[fidx])
      {
      case 's':
	{
	  unsigned i, len;
	  const char *str = XSTR (k, fidx);
	  len = strlen (str);
	  result = result * 613 + len;
	  for (i = 0; i < len; i++)
	    result = result * 613 + (unsigned) str[i];
	  break;
	}
      case 'u':
      case 'e':
	result = result * 1231 + rs6000_hash_constant (XEXP (k, fidx));
	break;
      case 'i':
      case 'n':
	result = result * 613 + (unsigned) XINT (k, fidx);
	break;
      case 'w':
	if (sizeof (unsigned) >= sizeof (HOST_WIDE_INT))
	  result = result * 613 + (unsigned) XWINT (k, fidx);
	else
	  {
	    size_t i;
	    for (i = 0; i < sizeof(HOST_WIDE_INT)/sizeof(unsigned); i++)
	      result = result * 613 + (unsigned) (XWINT (k, fidx)
						  >> CHAR_BIT * i);
	  }
	break;
      default:
	abort ();
      }

  return result;
}

static unsigned
toc_hash_function (hash_entry)
     const void * hash_entry;
{
  const struct toc_hash_struct *thc = 
    (const struct toc_hash_struct *) hash_entry;
  return rs6000_hash_constant (thc->key) ^ thc->key_mode;
}

/* Compare H1 and H2 for equivalence.  */

static int
toc_hash_eq (h1, h2)
     const void * h1;
     const void * h2;
{
  rtx r1 = ((const struct toc_hash_struct *) h1)->key;
  rtx r2 = ((const struct toc_hash_struct *) h2)->key;

  if (((const struct toc_hash_struct *) h1)->key_mode
      != ((const struct toc_hash_struct *) h2)->key_mode)
    return 0;

  return rtx_equal_p (r1, r2);
}

/* These are the names given by the C++ front-end to vtables, and
   vtable-like objects.  Ideally, this logic should not be here;
   instead, there should be some programmatic way of inquiring as
   to whether or not an object is a vtable.  */

#define VTABLE_NAME_P(NAME)				\
  (strncmp ("_vt.", name, strlen("_vt.")) == 0		\
  || strncmp ("_ZTV", name, strlen ("_ZTV")) == 0	\
  || strncmp ("_ZTT", name, strlen ("_ZTT")) == 0	\
  || strncmp ("_ZTC", name, strlen ("_ZTC")) == 0) 

void
rs6000_output_symbol_ref (file, x)
     FILE *file;
     rtx x;
{
  /* Currently C++ toc references to vtables can be emitted before it
     is decided whether the vtable is public or private.  If this is
     the case, then the linker will eventually complain that there is
     a reference to an unknown section.  Thus, for vtables only, 
     we emit the TOC reference to reference the symbol and not the
     section.  */
  const char *name = XSTR (x, 0);

  if (VTABLE_NAME_P (name)) 
    {
      RS6000_OUTPUT_BASENAME (file, name);
    }
  else
    assemble_name (file, name);
}

/* Output a TOC entry.  We derive the entry name from what is being
   written.  */

void
output_toc (file, x, labelno, mode)
     FILE *file;
     rtx x;
     int labelno;
     enum machine_mode mode;
{
  char buf[256];
  const char *name = buf;
  const char *real_name;
  rtx base = x;
  int offset = 0;

  if (TARGET_NO_TOC)
    abort ();

  /* When the linker won't eliminate them, don't output duplicate
     TOC entries (this happens on AIX if there is any kind of TOC,
     and on SVR4 under -fPIC or -mrelocatable).  Don't do this for
     CODE_LABELs.  */
  if (TARGET_TOC && GET_CODE (x) != LABEL_REF)
    {
      struct toc_hash_struct *h;
      void * * found;
      
      /* Create toc_hash_table.  This can't be done at OVERRIDE_OPTIONS
         time because GGC is not initialised at that point.  */
      if (toc_hash_table == NULL)
	toc_hash_table = htab_create_ggc (1021, toc_hash_function, 
					  toc_hash_eq, NULL);

      h = ggc_alloc (sizeof (*h));
      h->key = x;
      h->key_mode = mode;
      h->labelno = labelno;
      
      found = htab_find_slot (toc_hash_table, h, 1);
      if (*found == NULL)
	*found = h;
      else  /* This is indeed a duplicate.  
	       Set this label equal to that label.  */
	{
	  fputs ("\t.set ", file);
	  ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LC");
	  fprintf (file, "%d,", labelno);
	  ASM_OUTPUT_INTERNAL_LABEL_PREFIX (file, "LC");
	  fprintf (file, "%d\n", ((*(const struct toc_hash_struct **) 
					      found)->labelno));
	  return;
	}
    }

  /* If we're going to put a double constant in the TOC, make sure it's
     aligned properly when strict alignment is on.  */
  if (GET_CODE (x) == CONST_DOUBLE
      && STRICT_ALIGNMENT
      && GET_MODE_BITSIZE (mode) >= 64
      && ! (TARGET_NO_FP_IN_TOC && ! TARGET_MINIMAL_TOC)) {
    ASM_OUTPUT_ALIGN (file, 3);
  }

  ASM_OUTPUT_INTERNAL_LABEL (file, "LC", labelno);

  /* Handle FP constants specially.  Note that if we have a minimal
     TOC, things we put here aren't actually in the TOC, so we can allow
     FP constants.  */
  if (GET_CODE (x) == CONST_DOUBLE && GET_MODE (x) == TFmode)
    {
      REAL_VALUE_TYPE rv;
      long k[4];

      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_LONG_DOUBLE (rv, k);

      if (TARGET_64BIT)
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs (DOUBLE_INT_ASM_OP, file);
	  else
	    fprintf (file, "\t.tc FT_%lx_%lx_%lx_%lx[TC],",
		     k[0] & 0xffffffff, k[1] & 0xffffffff,
		     k[2] & 0xffffffff, k[3] & 0xffffffff);
	  fprintf (file, "0x%lx%08lx,0x%lx%08lx\n",
		   k[0] & 0xffffffff, k[1] & 0xffffffff,
		   k[2] & 0xffffffff, k[3] & 0xffffffff);
	  return;
	}
      else
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs ("\t.long ", file);
	  else
	    fprintf (file, "\t.tc FT_%lx_%lx_%lx_%lx[TC],",
		     k[0] & 0xffffffff, k[1] & 0xffffffff,
		     k[2] & 0xffffffff, k[3] & 0xffffffff);
	  fprintf (file, "0x%lx,0x%lx,0x%lx,0x%lx\n",
		   k[0] & 0xffffffff, k[1] & 0xffffffff,
		   k[2] & 0xffffffff, k[3] & 0xffffffff);
	  return;
	}
    }
  else if (GET_CODE (x) == CONST_DOUBLE && GET_MODE (x) == DFmode)
    {
      REAL_VALUE_TYPE rv;
      long k[2];

      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_DOUBLE (rv, k);

      if (TARGET_64BIT)
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs (DOUBLE_INT_ASM_OP, file);
	  else
	    fprintf (file, "\t.tc FD_%lx_%lx[TC],",
		     k[0] & 0xffffffff, k[1] & 0xffffffff);
	  fprintf (file, "0x%lx%08lx\n",
		   k[0] & 0xffffffff, k[1] & 0xffffffff);
	  return;
	}
      else
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs ("\t.long ", file);
	  else
	    fprintf (file, "\t.tc FD_%lx_%lx[TC],",
		     k[0] & 0xffffffff, k[1] & 0xffffffff);
	  fprintf (file, "0x%lx,0x%lx\n",
		   k[0] & 0xffffffff, k[1] & 0xffffffff);
	  return;
	}
    }
  else if (GET_CODE (x) == CONST_DOUBLE && GET_MODE (x) == SFmode)
    {
      REAL_VALUE_TYPE rv;
      long l;

      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_SINGLE (rv, l);

      if (TARGET_64BIT)
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs (DOUBLE_INT_ASM_OP, file);
	  else
	    fprintf (file, "\t.tc FS_%lx[TC],", l & 0xffffffff);
	  fprintf (file, "0x%lx00000000\n", l & 0xffffffff);
	  return;
	}
      else
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs ("\t.long ", file);
	  else
	    fprintf (file, "\t.tc FS_%lx[TC],", l & 0xffffffff);
	  fprintf (file, "0x%lx\n", l & 0xffffffff);
	  return;
	}
    }
  else if (GET_MODE (x) == VOIDmode
	   && (GET_CODE (x) == CONST_INT || GET_CODE (x) == CONST_DOUBLE))
    {
      unsigned HOST_WIDE_INT low;
      HOST_WIDE_INT high;

      if (GET_CODE (x) == CONST_DOUBLE)
	{
	  low = CONST_DOUBLE_LOW (x);
	  high = CONST_DOUBLE_HIGH (x);
	}
      else
#if HOST_BITS_PER_WIDE_INT == 32
	{
	  low = INTVAL (x);
	  high = (low & 0x80000000) ? ~0 : 0;
	}
#else
	{
          low = INTVAL (x) & 0xffffffff;
          high = (HOST_WIDE_INT) INTVAL (x) >> 32;
	}
#endif

      /* TOC entries are always Pmode-sized, but since this
	 is a bigendian machine then if we're putting smaller
	 integer constants in the TOC we have to pad them.
	 (This is still a win over putting the constants in
	 a separate constant pool, because then we'd have
	 to have both a TOC entry _and_ the actual constant.)

	 For a 32-bit target, CONST_INT values are loaded and shifted
	 entirely within `low' and can be stored in one TOC entry.  */

      if (TARGET_64BIT && POINTER_SIZE < GET_MODE_BITSIZE (mode))
	abort ();/* It would be easy to make this work, but it doesn't now.  */

      if (POINTER_SIZE > GET_MODE_BITSIZE (mode))
	{
#if HOST_BITS_PER_WIDE_INT == 32
	  lshift_double (low, high, POINTER_SIZE - GET_MODE_BITSIZE (mode),
			 POINTER_SIZE, &low, &high, 0);
#else
	  low |= high << 32;
	  low <<= POINTER_SIZE - GET_MODE_BITSIZE (mode);
	  high = (HOST_WIDE_INT) low >> 32;
	  low &= 0xffffffff;
#endif
	}

      if (TARGET_64BIT)
	{
	  if (TARGET_MINIMAL_TOC)
	    fputs (DOUBLE_INT_ASM_OP, file);
	  else
	    fprintf (file, "\t.tc ID_%lx_%lx[TC],",
		     (long) high & 0xffffffff, (long) low & 0xffffffff);
	  fprintf (file, "0x%lx%08lx\n",
		   (long) high & 0xffffffff, (long) low & 0xffffffff);
	  return;
	}
      else
	{
	  if (POINTER_SIZE < GET_MODE_BITSIZE (mode))
	    {
	      if (TARGET_MINIMAL_TOC)
		fputs ("\t.long ", file);
	      else
		fprintf (file, "\t.tc ID_%lx_%lx[TC],",
			 (long) high & 0xffffffff, (long) low & 0xffffffff);
	      fprintf (file, "0x%lx,0x%lx\n",
		       (long) high & 0xffffffff, (long) low & 0xffffffff);
	    }
	  else
	    {
	      if (TARGET_MINIMAL_TOC)
		fputs ("\t.long ", file);
	      else
		fprintf (file, "\t.tc IS_%lx[TC],", (long) low & 0xffffffff);
	      fprintf (file, "0x%lx\n", (long) low & 0xffffffff);
	    }
	  return;
	}
    }

  if (GET_CODE (x) == CONST)
    {
      if (GET_CODE (XEXP (x, 0)) != PLUS)
	abort ();

      base = XEXP (XEXP (x, 0), 0);
      offset = INTVAL (XEXP (XEXP (x, 0), 1));
    }
  
  if (GET_CODE (base) == SYMBOL_REF)
    name = XSTR (base, 0);
  else if (GET_CODE (base) == LABEL_REF)
    ASM_GENERATE_INTERNAL_LABEL (buf, "L", CODE_LABEL_NUMBER (XEXP (base, 0)));
  else if (GET_CODE (base) == CODE_LABEL)
    ASM_GENERATE_INTERNAL_LABEL (buf, "L", CODE_LABEL_NUMBER (base));
  else
    abort ();

  real_name = (*targetm.strip_name_encoding) (name);
  if (TARGET_MINIMAL_TOC)
    fputs (TARGET_32BIT ? "\t.long " : DOUBLE_INT_ASM_OP, file);
  else
    {
      fprintf (file, "\t.tc %s", real_name);

      if (offset < 0)
	fprintf (file, ".N%d", - offset);
      else if (offset)
	fprintf (file, ".P%d", offset);

      fputs ("[TC],", file);
    }

  /* Currently C++ toc references to vtables can be emitted before it
     is decided whether the vtable is public or private.  If this is
     the case, then the linker will eventually complain that there is
     a TOC reference to an unknown section.  Thus, for vtables only,
     we emit the TOC reference to reference the symbol and not the
     section.  */
  if (VTABLE_NAME_P (name))
    {
      RS6000_OUTPUT_BASENAME (file, name);
      if (offset < 0)
	fprintf (file, "%d", offset);
      else if (offset > 0)
	fprintf (file, "+%d", offset);
    }
  else
    output_addr_const (file, x);
  putc ('\n', file);
}

/* Output an assembler pseudo-op to write an ASCII string of N characters
   starting at P to FILE.

   On the RS/6000, we have to do this using the .byte operation and
   write out special characters outside the quoted string.
   Also, the assembler is broken; very long strings are truncated,
   so we must artificially break them up early.  */

void
output_ascii (file, p, n)
     FILE *file;
     const char *p;
     int n;
{
  char c;
  int i, count_string;
  const char *for_string = "\t.byte \"";
  const char *for_decimal = "\t.byte ";
  const char *to_close = NULL;

  count_string = 0;
  for (i = 0; i < n; i++)
    {
      c = *p++;
      if (c >= ' ' && c < 0177)
	{
	  if (for_string)
	    fputs (for_string, file);
	  putc (c, file);

	  /* Write two quotes to get one.  */
	  if (c == '"')
	    {
	      putc (c, file);
	      ++count_string;
	    }

	  for_string = NULL;
	  for_decimal = "\"\n\t.byte ";
	  to_close = "\"\n";
	  ++count_string;

	  if (count_string >= 512)
	    {
	      fputs (to_close, file);

	      for_string = "\t.byte \"";
	      for_decimal = "\t.byte ";
	      to_close = NULL;
	      count_string = 0;
	    }
	}
      else
	{
	  if (for_decimal)
	    fputs (for_decimal, file);
	  fprintf (file, "%d", c);

	  for_string = "\n\t.byte \"";
	  for_decimal = ", ";
	  to_close = "\n";
	  count_string = 0;
	}
    }

  /* Now close the string if we have written one.  Then end the line.  */
  if (to_close)
    fputs (to_close, file);
}

/* Generate a unique section name for FILENAME for a section type
   represented by SECTION_DESC.  Output goes into BUF.

   SECTION_DESC can be any string, as long as it is different for each
   possible section type.

   We name the section in the same manner as xlc.  The name begins with an
   underscore followed by the filename (after stripping any leading directory
   names) with the last period replaced by the string SECTION_DESC.  If
   FILENAME does not contain a period, SECTION_DESC is appended to the end of
   the name.  */

void
rs6000_gen_section_name (buf, filename, section_desc)
     char **buf;
     const char *filename;
     const char *section_desc;
{
  const char *q, *after_last_slash, *last_period = 0;
  char *p;
  int len;

  after_last_slash = filename;
  for (q = filename; *q; q++)
    {
      if (*q == '/')
	after_last_slash = q + 1;
      else if (*q == '.')
	last_period = q;
    }

  len = strlen (after_last_slash) + strlen (section_desc) + 2;
  *buf = (char *) xmalloc (len);

  p = *buf;
  *p++ = '_';

  for (q = after_last_slash; *q; q++)
    {
      if (q == last_period)
        {
	  strcpy (p, section_desc);
	  p += strlen (section_desc);
        }

      else if (ISALNUM (*q))
        *p++ = *q;
    }

  if (last_period == 0)
    strcpy (p, section_desc);
  else
    *p = '\0';
}

/* Emit profile function.  */

void
output_profile_hook (labelno)
     int labelno ATTRIBUTE_UNUSED;
{
  if (DEFAULT_ABI == ABI_AIX)
    {
#ifdef NO_PROFILE_COUNTERS
      emit_library_call (init_one_libfunc (RS6000_MCOUNT), 0, VOIDmode, 0);
#else
      char buf[30];
      const char *label_name;
      rtx fun;

      ASM_GENERATE_INTERNAL_LABEL (buf, "LP", labelno);
      label_name = (*targetm.strip_name_encoding) (ggc_strdup (buf));
      fun = gen_rtx_SYMBOL_REF (Pmode, label_name);

      emit_library_call (init_one_libfunc (RS6000_MCOUNT), 0, VOIDmode, 1,
                         fun, Pmode);
#endif
    }
  else if (DEFAULT_ABI == ABI_DARWIN)
    {
      const char *mcount_name = RS6000_MCOUNT;
      int caller_addr_regno = LINK_REGISTER_REGNUM;

      /* Be conservative and always set this, at least for now.  */
      current_function_uses_pic_offset_table = 1;

#if TARGET_MACHO
      /* For PIC code, set up a stub and collect the caller's address
	 from r0, which is where the prologue puts it.  */
      /* APPLE LOCAL  dynamic-no-pic  */
      if (MACHOPIC_INDIRECT)
	{
	  mcount_name = machopic_stub_name (mcount_name);
	  if (current_function_uses_pic_offset_table)
	    caller_addr_regno = 0;
	}
#endif
      emit_library_call (gen_rtx_SYMBOL_REF (Pmode, mcount_name),
			 0, VOIDmode, 1,
			 gen_rtx_REG (Pmode, caller_addr_regno), Pmode);
    }
}

/* Write function profiler code.  */

void
output_function_profiler (file, labelno)
  FILE *file;
  int labelno;
{
  char buf[100];
  int save_lr = 8;

  ASM_GENERATE_INTERNAL_LABEL (buf, "LP", labelno);
  switch (DEFAULT_ABI)
    {
    default:
      abort ();

    case ABI_V4:
      save_lr = 4;
      /* Fall through.  */

    case ABI_AIX_NODESC:
      if (!TARGET_32BIT)
	{
	  warning ("no profiling of 64-bit code for this ABI");
	  return;
	}
      fprintf (file, "\tmflr %s\n", reg_names[0]);
      if (flag_pic == 1)
	{
	  fputs ("\tbl _GLOBAL_OFFSET_TABLE_@local-4\n", file);
	  asm_fprintf (file, "\t{st|stw} %s,%d(%s)\n",
		       reg_names[0], save_lr, reg_names[1]);
	  asm_fprintf (file, "\tmflr %s\n", reg_names[12]);
	  asm_fprintf (file, "\t{l|lwz} %s,", reg_names[0]);
	  assemble_name (file, buf);
	  asm_fprintf (file, "@got(%s)\n", reg_names[12]);
	}
      else if (flag_pic > 1)
	{
	  asm_fprintf (file, "\t{st|stw} %s,%d(%s)\n",
		       reg_names[0], save_lr, reg_names[1]);
	  /* Now, we need to get the address of the label.  */
	  fputs ("\tbl 1f\n\t.long ", file);
	  assemble_name (file, buf);
	  fputs ("-.\n1:", file);
	  asm_fprintf (file, "\tmflr %s\n", reg_names[11]);
	  asm_fprintf (file, "\t{l|lwz} %s,0(%s)\n", 
		       reg_names[0], reg_names[11]);
	  asm_fprintf (file, "\t{cax|add} %s,%s,%s\n",
		       reg_names[0], reg_names[0], reg_names[11]);
	}
      else
	{
	  asm_fprintf (file, "\t{liu|lis} %s,", reg_names[12]);
	  assemble_name (file, buf);
	  fputs ("@ha\n", file);
	  asm_fprintf (file, "\t{st|stw} %s,%d(%s)\n",
		       reg_names[0], save_lr, reg_names[1]);
	  asm_fprintf (file, "\t{cal|la} %s,", reg_names[0]);
	  assemble_name (file, buf);
	  asm_fprintf (file, "@l(%s)\n", reg_names[12]);
	}

      if (current_function_needs_context && DEFAULT_ABI == ABI_AIX_NODESC)
	{
	  asm_fprintf (file, "\t{st|stw} %s,%d(%s)\n",
		       reg_names[STATIC_CHAIN_REGNUM],
		       12, reg_names[1]);
	  fprintf (file, "\tbl %s\n", RS6000_MCOUNT);
	  asm_fprintf (file, "\t{l|lwz} %s,%d(%s)\n",
		       reg_names[STATIC_CHAIN_REGNUM],
		       12, reg_names[1]);
	}
      else
	/* ABI_V4 saves the static chain reg with ASM_OUTPUT_REG_PUSH.  */
	fprintf (file, "\tbl %s\n", RS6000_MCOUNT);
      break;

    case ABI_AIX:
    case ABI_DARWIN:
      /* Don't do anything, done in output_profile_hook ().  */
      break;
    }
}


static int
rs6000_use_dfa_pipeline_interface ()
{
  return 1;
}

/* Power4 load update and store update instructions are cracked into a
   load or store and an integer insn which are executed in the same cycle.
   Branches have their own dispatch slot which does not count against the
   GCC issue rate, but it changes the program flow so there are no other
   instructions to issue in this cycle.  */

static int
rs6000_variable_issue (stream, verbose, insn, more)
  FILE *stream ATTRIBUTE_UNUSED;
  int verbose ATTRIBUTE_UNUSED;
  rtx insn;
  int more;
{
  if (GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
    return more;

  if (rs6000_cpu == PROCESSOR_POWER4)
    {
      enum attr_type type = get_attr_type (insn);
      if (type == TYPE_LOAD_EXT_U || type == TYPE_LOAD_EXT_UX
	  || type == TYPE_LOAD_UX || type == TYPE_STORE_UX
#ifdef MFCR_TYPE_IS_MICROCODED
          || type == TYPE_MFCR
#endif
	 )
	return 0;
      else if (type == TYPE_LOAD_U || type == TYPE_STORE_U
	       || type == TYPE_FPLOAD_U || type == TYPE_FPSTORE_U
	       || type == TYPE_FPLOAD_UX || type == TYPE_FPSTORE_UX
	       || type == TYPE_LOAD_EXT || type == TYPE_DELAYED_CR
	       || type == TYPE_COMPARE || type == TYPE_DELAYED_COMPARE
               || type == TYPE_IMUL_COMPARE || type == TYPE_LMUL_COMPARE
               || type == TYPE_IDIV || type == TYPE_LDIV)

	return more > 2 ? more - 2 : 0;
      else
	return more - 1;
    }
  else
    return more - 1;
}

/* DN begin */
#ifdef DN_SCHED_FINISH
static rtx
get_next_active_insn(just_before_next_insn, insn, tail)
     rtx *just_before_next_insn;
     rtx insn, tail;
{
  rtx next_insn;

  *just_before_next_insn = NULL_RTX;

  if (! insn || insn == NULL_RTX || insn == tail){
    return NULL_RTX;
  }

  *just_before_next_insn = insn;     
  next_insn = NEXT_INSN(insn);

  while(   next_insn
        && next_insn != tail
        && (GET_CODE(next_insn) == NOTE
            || GET_CODE (PATTERN (next_insn)) == USE
            || GET_CODE (PATTERN (next_insn)) == CLOBBER)){
    *just_before_next_insn = next_insn;
    next_insn = NEXT_INSN(next_insn);
  }

  if (! next_insn || next_insn == NULL_RTX || next_insn == tail){       
    return NULL_RTX;
  }

  return next_insn; 
}

static rtx
get_prev_active_insn(just_after_prev_insn, insn, head)
     rtx *just_after_prev_insn;
     rtx insn, head;
{
  rtx prev_insn;

  *just_after_prev_insn = NULL_RTX;

  if (! insn || insn == NULL_RTX || insn == head){
    return NULL_RTX;
  }

  prev_insn = PREV_INSN(insn);
  *just_after_prev_insn = prev_insn;

  while(   prev_insn
        && prev_insn != head
        && (GET_CODE(prev_insn) == NOTE
            || GET_CODE (PATTERN (prev_insn)) == USE
            || GET_CODE (PATTERN (prev_insn)) == CLOBBER)){
    prev_insn = PREV_INSN(prev_insn);
  }

  if (! prev_insn || prev_insn == NULL_RTX){
    return NULL_RTX;
  }

  return prev_insn;
}

static int
is_microcoded_insn(insn)
        rtx insn;
{
  if(! insn)
    return 0;

  if (GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
    return 0;

  if (rs6000_cpu == PROCESSOR_POWER4)
    {
      enum attr_type type = get_attr_type (insn);
      if (type == TYPE_LOAD_EXT_U || type == TYPE_LOAD_EXT_UX
          || type == TYPE_LOAD_UX || type == TYPE_STORE_UX
#ifdef MFCR_TYPE_IS_MICROCODED
          || type == TYPE_MFCR
#endif
	 ) 
        return 1;   
    }

  return 0;

}

static int
is_cracked_insn(insn)
        rtx insn;
{
  if(! insn)
    return 0;

  if (GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
    return 0;

  if (rs6000_cpu == PROCESSOR_POWER4)
    {
      enum attr_type type = get_attr_type (insn);
      if (type == TYPE_LOAD_U || type == TYPE_STORE_U
               || type == TYPE_FPLOAD_U || type == TYPE_FPSTORE_U
               || type == TYPE_FPLOAD_UX || type == TYPE_FPSTORE_UX
               || type == TYPE_LOAD_EXT || type == TYPE_DELAYED_CR
               || type == TYPE_COMPARE || type == TYPE_DELAYED_COMPARE
               || type == TYPE_IMUL_COMPARE || type == TYPE_LMUL_COMPARE
               || type == TYPE_IDIV || type == TYPE_LDIV)
        return 1;   
    }

  return 0;
}

static int
force_new_group(sched_verbose, dump, group_insns, next_insn)
	int sched_verbose;
	FILE *dump;
	rtx *group_insns;
	rtx next_insn;
{
  int i;
  rtx link;
  int cost;

  if (! next_insn)
    return 0;

  for (i = 0; i < GROUP_SIZE; i++){
    rtx insn = group_insns[i];
    if (!insn)
      continue;
    for (link = INSN_DEPEND (insn); link != 0; link = XEXP (link, 1))   
      {
        rtx next = XEXP (link, 0);
        if (next == next_insn) {
          cost = insn_cost (insn, link, next_insn);
#ifdef DN_DEBUG
	  if (sched_verbose > 6){
	    fprintf(dump,"insn depends on insn\n");
	    print_rtl_single(dump,insn);
	    fprintf(dump,"cost = %d\n",cost);
	  } 	
#endif
          if (rs6000_is_costly_dependence(insn, next_insn, link, cost))
            return 1; 
        }
      } 
  }

  return 0;
}

static int
redefine_groups(dump, sched_verbose, prev_head_insn, tail)
     FILE *dump;
     int sched_verbose;
     rtx prev_head_insn, tail;
{
  rtx insn, next_insn;
  rtx just_before_next_insn;
  rtx nop;
  int issue_rate;
  int can_issue_more;
  int prev_can_issue_more;
  int slot, i;
  enum attr_type type, next_type;
  int group_end;
  int group_count = 0;
  int force;
  rtx group_insns[4];

  if (rs6000_cpu != PROCESSOR_POWER4){
    return 0;
  }

  /* Initialize issue_rate.  */
  issue_rate = rs6000_issue_rate();      
  can_issue_more = issue_rate;

  if (sched_verbose > 6)
    fprintf (dump, "init group insns\n");

  slot = 0;
  for(i = 0; i < GROUP_SIZE; i++){
    group_insns[i] = 0;
  }

  insn = get_next_active_insn(&just_before_next_insn, prev_head_insn, tail);
  next_insn = get_next_active_insn(&just_before_next_insn, insn, tail);

  while(insn != NULL_RTX){

    type = get_attr_type (insn);  
    if(next_insn != NULL_RTX)
      next_type = get_attr_type (next_insn);

    prev_can_issue_more = can_issue_more;
    can_issue_more =
      rs6000_variable_issue(dump, sched_verbose, insn, can_issue_more);

    if (sched_verbose > 6){
      fprintf (dump, "\n///////// insn at slot %d\n",slot);
      print_rtl_single(dump,insn);
    }

    group_insns[slot] = insn;

    if(type == TYPE_BRANCH || type == TYPE_JMPREG || type == TYPE_BRINC)
	can_issue_more = 0;

    group_end = (next_insn == NULL_RTX || GET_MODE(next_insn) == TImode);

#ifdef ISSUE_RATE_INCLUDES_BU_SLOT
    if(can_issue_more == 1 && next_insn &&
       (next_type != TYPE_BRANCH && next_type != TYPE_JMPREG && next_type != TYPE_BRINC)){
        if (sched_verbose > 6)
          fprintf (dump, "mark group end\n");
	can_issue_more = 0;
	group_end = 1;
    }
    if(can_issue_more == 2 && next_insn && is_cracked_insn(next_insn)){ 
        if (sched_verbose > 6)
          fprintf (dump, "mark group end\n");
	can_issue_more = 0;
	group_end = 1;
    }
#else
    if(can_issue_more == 1 && next_insn && is_cracked_insn(next_insn)){ 
        if (sched_verbose > 6)
          fprintf (dump, "mark group end\n");
	can_issue_more = 0;
	group_end = 1;
    }
#endif

    force = force_new_group(sched_verbose, dump, group_insns, next_insn);
    if (sched_verbose > 6)
      fprintf (dump, "force =  %d\n",force);

    if (sched_verbose > 6)
      fprintf (dump, "group end =  %d, can issue more =  %d\n",force,can_issue_more);

    if ( !group_end && can_issue_more > 0 && next_insn && force &&
	 reload_completed && (insert_sched_nops == 2) ){

          if (sched_verbose >= 3)   
            fprintf (dump, "=== not end; force new group\n");  

#ifdef ISSUE_RATE_INCLUDES_BU_SLOT
          can_issue_more--; /* we don't want a nop in the branch slot */
#endif
          while(can_issue_more){
            if (sched_verbose > 6)
              fprintf (dump, "=== Add nop\n");
            nop = gen_nop ();
            if(next_insn != NULL_RTX){
             emit_insn_before (nop, next_insn);
            }
            else{
             sched_emit_insn_after (nop, just_before_next_insn, 0);
            }
            can_issue_more--;
          }

	  group_end = 1;
    }

    if (sched_verbose > 6)
      fprintf (dump, "group end =  %d, can issue more =  %d\n",force,can_issue_more);

    if ( (group_end || can_issue_more == 0) && 
	 reload_completed && (insert_sched_nops == 2) && next_insn){

       if (can_issue_more == 0){ /* make sure TI is set */
   	 if (GET_MODE(next_insn) != TImode)
	   PUT_MODE(next_insn, TImode);	
       }
       else if ( (can_issue_more > 0)
            && (!is_microcoded_insn(next_insn))
            && (!is_dispatch_slot_restricted(next_insn))
#ifdef ISSUE_RATE_INCLUDES_BU_SLOT
	    && (!is_cracked_insn(next_insn) || (can_issue_more > 2)) 
#else
	    && (!is_cracked_insn(next_insn) || (can_issue_more > 1)) 
#endif
	    && !force ){

   	 if (GET_MODE(next_insn) == TImode)
	   PUT_MODE(next_insn, VOIDmode);	
         	
	 group_end = 0;
       }
       else if ( (can_issue_more > 0)
            && (!is_microcoded_insn(next_insn))
            && (!is_dispatch_slot_restricted(next_insn))
	    && force ){

          if (sched_verbose >= 3)
            fprintf (dump, "=== force new group\n");
#ifdef ISSUE_RATE_INCLUDES_BU_SLOT
          can_issue_more--; /* we don't want a nop in the branch slot */
#endif
          while(can_issue_more){
            if (sched_verbose > 6)
              fprintf (dump, "=== Add nop\n");
            nop = gen_nop ();
            if(next_insn != NULL_RTX){
             emit_insn_before (nop, next_insn);
            }
            else{
             sched_emit_insn_after (nop, just_before_next_insn, 0);
            }
            can_issue_more--;
          }
       }
    }


    if(group_end){
      if (sched_verbose > 6)
        fprintf (dump, ">>> group end =  %d, can issue more =  %d\n",force,can_issue_more);

      for(i = 0; i < GROUP_SIZE; i++){
        group_insns[i] = 0;                              
      }
      slot = 0;
      can_issue_more = issue_rate;
      group_count++;
    }
    else{
      slot += (prev_can_issue_more - can_issue_more); 	
    }

    if( next_insn == NULL_RTX ){
       break;
    }

    insn = next_insn;
    next_insn = get_next_active_insn(&just_before_next_insn, insn, tail);
  }

  return group_count;       
}

static int
pad_groups(dump, sched_verbose, prev_head_insn, tail)
     FILE *dump;
     int sched_verbose;
     rtx prev_head_insn, tail;
{
  rtx insn, next_insn;
  rtx just_before_next_insn;
  rtx nop;
  int issue_rate;
  int can_issue_more;
  enum attr_type type, next_type;
  int group_end;
  int group_count = 0;

  if (rs6000_cpu != PROCESSOR_POWER4){
    return 0;
  }

  /* Initialize issue_rate.  */
  issue_rate = rs6000_issue_rate();
  can_issue_more = issue_rate;

  insn = get_next_active_insn(&just_before_next_insn, prev_head_insn, tail);
  next_insn = get_next_active_insn(&just_before_next_insn, insn, tail);

  while(insn != NULL_RTX){

    type = get_attr_type (insn);
    if(next_insn != NULL_RTX)
      next_type = get_attr_type (next_insn);

    can_issue_more =
      rs6000_variable_issue(dump, sched_verbose, insn, can_issue_more);

    group_end = (next_insn == NULL_RTX || GET_MODE(next_insn) == TImode);

    if (sched_verbose > 6 && group_end){
      fprintf (dump, "=== group end; can_issue_more = %d\n",can_issue_more);
      print_rtl_single(dump,insn);
    }

    /* No need to add nops in the following cases:
       - insn type is branch
       - next_insn type is branch
       - insn/next_insn is microcoded
       - next_insn is restricted to the first/second issue slot
     */
    if ( group_end ){
       if ( (can_issue_more > 0) 
            && reload_completed
            && (insert_sched_nops == 1)
            && (type != TYPE_BRANCH && type != TYPE_JMPREG && type != TYPE_BRINC)
            && ( (next_insn == NULL_RTX) ||
                 (next_type != TYPE_BRANCH && next_type != TYPE_JMPREG && next_type != TYPE_BRINC))
            && ! is_microcoded_insn(insn)
            && ( next_insn == NULL_RTX || !is_microcoded_insn(next_insn) )
            && ( next_insn == NULL_RTX || !is_dispatch_slot_restricted(next_insn) )
          ){
#ifdef ISSUE_RATE_INCLUDES_BU_SLOT
	  can_issue_more--; /* we don't want a nop in the branch slot */
#endif
          while(can_issue_more){
            if (sched_verbose > 6)
              fprintf (dump, "=== Add nop\n");
            nop = gen_nop ();
            if(next_insn != NULL_RTX){
             emit_insn_before (nop, next_insn);
            }
            else{
             sched_emit_insn_after (nop, just_before_next_insn, 0);
            }

            can_issue_more--;
          }
       }

      can_issue_more = issue_rate;      
      group_count++;
    }

    if( next_insn == NULL_RTX ){
       break;
    }

    insn = next_insn;
    next_insn = get_next_active_insn(&just_before_next_insn, insn, tail);
  }    

  return group_count;
}

enum FU_TYPE {NO_FU_TYPE = 0, IU_TYPE = 1, LSU_TYPE = 2, FPU_TYPE = 3};
#define N_FU_TYPES 3
#define FIRST_FU_TYPE 1
int fu_weight[N_FU_TYPES];
int fu_imbalance[N_FU_TYPES];

/* return true if the insn uses multiple functional units */
static bool
insn_uses_multiple_units(insn)  
     rtx insn;
{
  enum attr_type insn_type = get_attr_type (insn);

  if(insn_type == TYPE_FPSTORE_U ||
     insn_type == TYPE_FPSTORE_UX ||
     insn_type == TYPE_FPSTORE)
    return 1;

  return 0;
}

static enum FU_TYPE
get_fu_for_insn(insn, at)
     rtx insn;
     int at;
{
  enum attr_type insn_type = get_attr_type (insn);

  if(is_microcoded_insn(insn)){
    return NO_FU_TYPE;
  }

  if(insn_uses_multiple_units(insn) && at == 2 ){
    switch(insn_type){
    case TYPE_FPSTORE_UX:
    case TYPE_FPSTORE_U:
    case TYPE_FPSTORE:
      return FPU_TYPE;
    default:
      return NO_FU_TYPE;
    }
  }

  if(is_cracked_insn(insn) && at == 1){
    switch(insn_type){
    case TYPE_LOAD_U:
    case TYPE_STORE_U:
    case TYPE_FPLOAD_U:
    case TYPE_FPSTORE_U:
    case TYPE_FPLOAD_UX:
    case TYPE_FPSTORE_UX:
    case TYPE_LOAD_EXT:
    case TYPE_LMUL_COMPARE:
    case TYPE_IMUL_COMPARE:
    case TYPE_COMPARE:
        return IU_TYPE;
    default:
      /* TYPE_IDIV */
      /* TYPE_LDIV */
      return NO_FU_TYPE;
    }
  }

  if(at == 0){
    switch(insn_type){
    case TYPE_INTEGER:
    case TYPE_LMUL_COMPARE:
    case TYPE_IMUL_COMPARE:
    case TYPE_IMUL:
    case TYPE_IMUL2:
    case TYPE_IMUL3:
    case TYPE_LMUL:
    case TYPE_CMP:
    case TYPE_FAST_COMPARE:
    case TYPE_COMPARE:
#ifndef MFCR_TYPE_IS_MICROCODED
    case TYPE_MFCR:
#endif
    case TYPE_MFCRF:
    case TYPE_MTCR:
    case TYPE_MTJMPR:
    case TYPE_MFJMPR:
    case TYPE_IDIV:
    case TYPE_LDIV:
      return IU_TYPE;
    case TYPE_FP:
    case TYPE_DMUL:
    case TYPE_FPCOMPARE:
    case TYPE_SDIV:
    case TYPE_DDIV:
    case TYPE_SSQRT:
    case TYPE_DSQRT:
      return FPU_TYPE;
    case TYPE_LOAD_U:
    case TYPE_STORE_U:
    case TYPE_FPLOAD_U:
    case TYPE_FPSTORE_U:
    case TYPE_FPLOAD_UX:
    case TYPE_FPSTORE_UX:
    case TYPE_LOAD_EXT:
    case TYPE_LOAD:
    case TYPE_FPLOAD:
    case TYPE_STORE:
    case TYPE_FPSTORE:
      return LSU_TYPE;
    default:
      /* BRANCH */
      /* JMPREG */
      /* BRINC */
      /* NOP */
      /* MTJMPR */
      /* MFJMPR */
      /* DELAYED_CR */
      /* CR_LOGICAL */
      return NO_FU_TYPE;
    }
  }

  return NO_FU_TYPE;
}

enum QUEUE_TYPE {QUEUE_A = 0, QUEUE_B = 1, NO_Q_TYPE = 3};

#ifdef RELY_ON_GROUPING
#define du_queue(slot, insn)                                                             \
  ((get_attr_type (insn) == TYPE_IDIV || get_attr_type (insn) == TYPE_LDIV) ? QUEUE_B :  \
    ((slot == 0 || slot == 3) ? QUEUE_A :                                                \
     ((slot == 1 || slot == 2) ? QUEUE_B : NO_Q_TYPE)))
#endif

#define group_fu_info1(group_du_usage_info, fu)                        \
       (fu == IU_TYPE ? group_du_usage_info.iu :                       \
        (fu == LSU_TYPE ? group_du_usage_info.lsu : group_du_usage_info.fpu ))

#ifdef RELY_ON_GROUPING
#define group_du_info_for_fu1(group_du_usage_info, fu, queue_num)      \
       ((group_fu_info1(group_du_usage_info, fu))[queue_num])
#else
#define group_du_info_for_fu1(group_du_usage_info, fu, slot)           \
       ((group_fu_info1(group_du_usage_info, fu))[slot])
#endif

#define group_fu_info(group_du_usage_info, i, fu)                      \
     (fu == IU_TYPE ? group_du_usage_info[i].iu :                      \
      (fu == LSU_TYPE ? group_du_usage_info[i].lsu : group_du_usage_info[i].fpu ))

#ifdef RELY_ON_GROUPING
#define group_du_info_for_fu(group_du_usage_info, i, fu, queue_num)    \
       ((group_fu_info(group_du_usage_info, i, fu))[queue_num])
#else
#define group_du_info_for_fu(group_du_usage_info, i, fu, slot)         \
       ((group_fu_info(group_du_usage_info, i, fu))[slot])
#endif

#ifdef RELY_ON_GROUPING
#define total_fu_usage_per_group(group_fu_info)                        \
  ( group_fu_info[QUEUE_A] + group_fu_info[QUEUE_B] )
#else
static int
total_fu_usage_per_group(int *group_fu_info){
  int i;
  int sum = 0;
  for(i = 0; i < GROUP_SIZE; i++){
    sum += group_fu_info[i];
  }
  return sum;
}
#endif

static void
zero_group_info1(group_du_usage_info)
     struct group_usage_per_unit *group_du_usage_info;
{
  enum FU_TYPE fu = FIRST_FU_TYPE;
  int i;

  for( ; fu <= N_FU_TYPES; fu++){
#ifdef RELY_ON_GROUPING
    group_du_info_for_fu1((*group_du_usage_info), fu, QUEUE_A) = 0;
    group_du_info_for_fu1((*group_du_usage_info), fu, QUEUE_B) = 0;
#else
    for(i = 0; i < GROUP_SIZE; i++){
      group_du_info_for_fu1((*group_du_usage_info), fu, i) = 0;
    }
#endif
  }
}

static void
zero_group_info(group_du_usage_info, group_count)
     struct group_usage_per_unit *group_du_usage_info;
     int group_count;
{
  enum FU_TYPE fu = FIRST_FU_TYPE;
  int i;

  for( ; fu <= N_FU_TYPES; fu++){
#ifdef RELY_ON_GROUPING
    group_du_info_for_fu(group_du_usage_info, group_count, fu, QUEUE_A) = 0;
    group_du_info_for_fu(group_du_usage_info, group_count, fu, QUEUE_B) = 0;
#else
    for(i = 0; i < GROUP_SIZE; i++){
      group_du_info_for_fu(group_du_usage_info, group_count, fu, i) = 0;
    }
#endif
  }
}

#ifdef RELY_ON_GROUPING

#define diff_between_queues_for_fu(group_fu_info)                            \
  ( group_fu_info[QUEUE_A] - group_fu_info[QUEUE_B] )

#define abs_diff_between_queues_for_fu(group_fu_info)                        \
  ( abs(group_fu_info[QUEUE_A] - group_fu_info[QUEUE_B]) )

#else

static int
group_cost_for_fu(int *group_fu_info){
  int i;
  int cost = 0;
  for(i = 0; i < (GROUP_SIZE - 1); i++){
    cost += group_fu_info[i] & group_fu_info[i+1];
  }
  return cost;
}

#endif

static int
group_cost1(group_du_usage_info, scale)
     struct group_usage_per_unit group_du_usage_info;
     int scale;
{
  enum FU_TYPE i = FIRST_FU_TYPE;
  int cost = 0;
  int weight;
  int can_improve = 0;
  int *fu_info;
  int total = 0;

  for( ; i <= N_FU_TYPES; i++){
    weight = (scale ? fu_weight[i - FIRST_FU_TYPE] : 1);
    fu_info = group_fu_info1(group_du_usage_info, i);
    total = total_fu_usage_per_group(fu_info);
#ifdef RELY_ON_GROUPING
    can_improve = ((total > 1) && (total % 2));
    cost += (abs_diff_between_queues_for_fu(fu_info) * weight * can_improve);
#else
    can_improve = (total > 1);
    cost += (group_cost_for_fu(fu_info) * weight * can_improve);
#endif
  }

  return cost;
}

static int
group_cost(group_du_usage_info, group_count, scale)
     struct group_usage_per_unit *group_du_usage_info;
     int group_count;
     int scale;
{
  return group_cost1(group_du_usage_info[group_count], scale);
}

static int
calc_total_score(group_du_usage_info, from_group, to_group, scale, use_global)
     struct group_usage_per_unit *group_du_usage_info;
     int from_group, to_group;
     int scale;
     int use_global;
{
  struct group_usage_per_unit dummy;
  return calc_alt_total_score(group_du_usage_info, from_group, to_group, 
			      -1, dummy, scale, use_global);
}

static int
calc_alt_total_score(group_du_usage_info, from_group, to_group, without_group, alt_group, scale, use_global)
     struct group_usage_per_unit *group_du_usage_info;
     int from_group, to_group, without_group;
     struct group_usage_per_unit alt_group;
     int scale;
     int use_global;
{
  int i;
  int inter_group_score = 0;
  int intra_group_score = 0;
#ifdef RELY_ON_GROUPING
  int total_iu_imbalance = 0;
  int total_fpu_imbalance = 0;
  int total_lsu_imbalance = 0;
  int current_iu_imbalance = 0;
  int current_fpu_imbalance = 0;
  int current_lsu_imbalance = 0;
#endif

  for(i = from_group; i < to_group; i++){
    if(i == without_group){
      intra_group_score += group_cost1(alt_group, scale);
#ifdef RELY_ON_GROUPING
      current_iu_imbalance =
        diff_between_queues_for_fu(group_fu_info1(alt_group, IU_TYPE));
      current_lsu_imbalance =
        diff_between_queues_for_fu(group_fu_info1(alt_group, LSU_TYPE));
      current_fpu_imbalance =
        diff_between_queues_for_fu(group_fu_info1(alt_group, FPU_TYPE));
#endif
    }
    else{
      intra_group_score += group_cost(group_du_usage_info, i, scale);
#ifdef RELY_ON_GROUPING
      current_iu_imbalance =
        diff_between_queues_for_fu(group_fu_info(group_du_usage_info, i, IU_TYPE));
      current_lsu_imbalance =
        diff_between_queues_for_fu(group_fu_info(group_du_usage_info, i, LSU_TYPE));
      current_fpu_imbalance =
        diff_between_queues_for_fu(group_fu_info(group_du_usage_info, i, FPU_TYPE));
#endif
    }
#ifdef RELY_ON_GROUPING
    total_iu_imbalance += current_iu_imbalance;
    total_lsu_imbalance += current_lsu_imbalance;
    total_fpu_imbalance += current_fpu_imbalance;
#endif
  }

#ifdef RELY_ON_GROUPING
  if(scale)
    inter_group_score =
      abs(total_iu_imbalance) * fu_weight[IU_TYPE - FIRST_FU_TYPE] +
      abs(total_lsu_imbalance) * fu_weight[LSU_TYPE - FIRST_FU_TYPE] +
      abs(total_fpu_imbalance) * fu_weight[FPU_TYPE - FIRST_FU_TYPE];
  else
    inter_group_score =
      abs(total_iu_imbalance) +
      abs(total_lsu_imbalance) +
      abs(total_fpu_imbalance);
#endif

  if(use_global)
    return (intra_group_score + inter_group_score);
  else
    return intra_group_score;
}

static void
record_data1(dump, sched_verbose, group_du_usage, insn, slot)
     FILE *dump;
     int sched_verbose;
     struct group_usage_per_unit *group_du_usage;
     rtx insn;
     int slot;
{
  enum attr_type type;
  enum FU_TYPE fu;
  enum QUEUE_TYPE queue_num;

  type = get_attr_type (insn);

  if(!is_microcoded_insn(insn) &&
     !(type == TYPE_BRANCH || type == TYPE_JMPREG || type == TYPE_BRINC)){

    fu = get_fu_for_insn(insn, 0);
#ifdef RELY_ON_GROUPING
    queue_num = du_queue(slot, insn);

    if(sched_verbose >= 6)
      fprintf(dump,"fu %d, slot %d, queue_num %d\n",fu, slot, queue_num);
#endif

    if(is_dispatch_slot_restricted(insn) == 2) {/* div...*/
      group_du_usage->insn_at_slot[slot+1] = insn;
#ifdef RELY_ON_GROUPING
      queue_num = du_queue(slot+1, insn);
#endif
    }
    else{
      group_du_usage->insn_at_slot[slot] = insn;
    }

    if(fu != NO_FU_TYPE){
#ifdef RELY_ON_GROUPING
      group_du_info_for_fu1((*group_du_usage), fu, queue_num) ++;
#else
      group_du_info_for_fu1((*group_du_usage), fu, slot) ++;
#endif

#ifdef RELY_ON_GROUPING
      if(sched_verbose >= 6)
        fprintf(dump,"group_du_info after: %d\n",
                group_du_info_for_fu1((*group_du_usage), fu, queue_num));
#endif

      if(is_cracked_insn(insn)){
#ifdef RELY_ON_GROUPING
        queue_num = du_queue(slot+1, insn);
#endif
        fu = get_fu_for_insn(insn, 1);

#ifdef RELY_ON_GROUPING
        if(sched_verbose >= 6)
          fprintf(dump,"cracked: fu %d, slot %d, queue_num %d\n",fu, slot, queue_num);
#endif

        if(fu != NO_FU_TYPE){
#ifdef RELY_ON_GROUPING
          group_du_info_for_fu1((*group_du_usage), fu, queue_num) ++;
#else
          group_du_info_for_fu1((*group_du_usage), fu, slot) ++;
#endif

#ifdef RELY_ON_GROUPING
          if(sched_verbose >= 6)
            fprintf(dump,"group_du_info after: %d\n",
                    group_du_info_for_fu1((*group_du_usage), fu, queue_num));
#endif
        }
      }/* cracked */

      if(insn_uses_multiple_units(insn)){
#ifdef RELY_ON_GROUPING
        queue_num = du_queue(slot, insn);
#endif
        fu = get_fu_for_insn(insn, 2);

#ifdef RELY_ON_GROUPING
        if(sched_verbose >= 6)
          fprintf(dump,"multiple: fu %d, slot %d, queue_num %d\n",fu, slot, queue_num);
#endif

        if(fu != NO_FU_TYPE){
#ifdef RELY_ON_GROUPING
          group_du_info_for_fu1((*group_du_usage), fu, queue_num) ++;
#else
          group_du_info_for_fu1((*group_du_usage), fu, slot) ++;
#endif

#ifdef RELY_ON_GROUPING
          if(sched_verbose >= 6)
            fprintf(dump,"group_du_info after: %d\n",
                    group_du_info_for_fu1((*group_du_usage), fu, queue_num));
#endif
        }
      }/* multiple units */
    }
  }
}


static void
record_data(dump, sched_verbose, group_du_usage, group_count, insn, slot)
     FILE *dump;
     int sched_verbose;
     struct group_usage_per_unit *group_du_usage;
     int group_count;
     rtx insn;
     int slot;
{
  record_data1(dump, sched_verbose, &group_du_usage[group_count], insn, slot);
}


static void
init_group_data(dump, sched_verbose, group_du_usage, n_groups, prev_head_insn, tail)
     FILE *dump;
     int sched_verbose;
     struct group_usage_per_unit *group_du_usage;
     int n_groups;
     rtx prev_head_insn, tail;
{
  rtx dummy;
  rtx insn, next_insn;
  int issue_rate;
  int can_issue_more;
  int prev_can_issue_more;
  int group_end;
  int group_count = 0;
  int slot;
  int total_iu_usage = 0;
  int total_fpu_usage = 0;
  int total_lsu_usage = 0;
  int total_groups_cost = 0;
  int current_iu_usage = 0;
  int current_fpu_usage = 0;
  int current_lsu_usage = 0;
  int current_group_cost = 0;
#ifdef DN_DEBUG
#ifdef RELY_ON_GROUPING
  int total_iu_imbalance = 0;
  int total_fpu_imbalance = 0;
  int total_lsu_imbalance = 0;
  int current_iu_imbalance = 0;
  int current_fpu_imbalance = 0;
  int current_lsu_imbalance = 0;
#endif
#endif
  int min_usage = 1;
  enum attr_type type, next_type;
  int i;
 
  if (rs6000_cpu != PROCESSOR_POWER4){
    return;
  }

  if(sched_verbose >= 3)
    fprintf(dump,"\n\ninit dispatch slot info; ngroups = %d\n",n_groups);

  if(n_groups <= 0){
    return;
  }

  fu_weight[IU_TYPE - FIRST_FU_TYPE] = 1;
  fu_weight[LSU_TYPE - FIRST_FU_TYPE] = 1;
  fu_weight[FPU_TYPE - FIRST_FU_TYPE] = 1;

  issue_rate = rs6000_issue_rate();
  can_issue_more = issue_rate;

  insn = get_next_active_insn(&dummy, prev_head_insn, tail);
  next_insn = get_next_active_insn(&dummy, insn, tail);

  slot = 0;
  zero_group_info(group_du_usage, group_count);
  for(i = 0; i < GROUP_SIZE; i++){
    group_du_usage[group_count].insn_at_slot[i] = 0;
  }

  while(insn != NULL_RTX){

    type = get_attr_type (insn);
    if(next_insn != NULL_RTX)
      next_type = get_attr_type (next_insn);

    prev_can_issue_more = can_issue_more;
    can_issue_more =
      rs6000_variable_issue(dump, sched_verbose, insn, can_issue_more);
    if(type == TYPE_BRANCH || type == TYPE_JMPREG || type == TYPE_BRINC)
      can_issue_more = 0;

    if(sched_verbose >= 6){
      print_rtl_single(dump,insn);
      fprintf(dump,"can_issue_more after insn = %d\n",can_issue_more);
    }

    record_data(dump, sched_verbose, group_du_usage, group_count, insn, slot);

    group_end = (next_insn == NULL_RTX || GET_MODE(next_insn) == TImode);

    if(! can_issue_more && ! group_end ){
      if(sched_verbose)
        fprintf(dump,"error: can_issue_more is 0 but not group end\n");
      return; 
    }

    if ( group_end ){

      /* calc and print statistics */
      current_iu_usage =
        total_fu_usage_per_group(group_fu_info(group_du_usage, group_count, IU_TYPE));
      current_lsu_usage =
        total_fu_usage_per_group(group_fu_info(group_du_usage, group_count, LSU_TYPE));
      current_fpu_usage =
        total_fu_usage_per_group(group_fu_info(group_du_usage, group_count, FPU_TYPE));

      total_iu_usage += current_iu_usage;
      total_lsu_usage += current_lsu_usage;
      total_fpu_usage += current_fpu_usage;

#ifdef DN_DEBUG
#ifdef RELY_ON_GROUPING
      current_iu_imbalance =
        diff_between_queues_for_fu(group_fu_info(group_du_usage, group_count, IU_TYPE));
      current_lsu_imbalance =
        diff_between_queues_for_fu(group_fu_info(group_du_usage, group_count, LSU_TYPE));
      current_fpu_imbalance =
        diff_between_queues_for_fu(group_fu_info(group_du_usage, group_count, FPU_TYPE));

      total_iu_imbalance += current_iu_imbalance;
      total_lsu_imbalance += current_lsu_imbalance;
      total_fpu_imbalance += current_fpu_imbalance;

      current_group_cost = group_cost(group_du_usage, group_count, 0);
      total_groups_cost += current_group_cost;

      if(sched_verbose >= 3){
        fprintf(dump,
                ">>>> group %d flat cost = %d; unit usage:(%d,%d,%d) diffs per unit:(%d,%d,%d)\n",
                group_count,
                current_group_cost,
                current_iu_usage, current_lsu_usage, current_fpu_usage,
                current_iu_imbalance, current_lsu_imbalance, current_fpu_imbalance);
      }

      if(sched_verbose >= 6){
        int j;
        fprintf(dump,"//////// group insns:\n"); 
        for(j = 0; j < GROUP_SIZE; j++){
          rtx insn;
          insn = group_du_usage[group_count].insn_at_slot[j];
          print_rtl_single(dump, insn);
        }
      }	
#endif
#endif
      can_issue_more = issue_rate;
      group_count++;

      if(group_count >= n_groups){
        break;
      }

      zero_group_info(group_du_usage, group_count);
      for(i = 0; i < GROUP_SIZE; i++){
        group_du_usage[group_count].insn_at_slot[i] = 0;
      }
      slot = 0;
    }
    else{
      slot += (prev_can_issue_more - can_issue_more);
    }

    if( next_insn == NULL_RTX ){
      break;
    }

    insn = next_insn;
    next_insn = get_next_active_insn(&dummy, insn, tail);
  }

  /* find the mininum non zero usage count between the units */
  min_usage = total_iu_usage;
  if(total_lsu_usage > 0 && total_lsu_usage < min_usage)
    min_usage = total_lsu_usage;
  if(total_fpu_usage > 0 && total_fpu_usage < min_usage)
    min_usage = total_fpu_usage;
  if(min_usage == 0)
    min_usage = 1;

  /* calculate and record relative weights */
  fu_weight[IU_TYPE - FIRST_FU_TYPE] =
    (total_iu_usage == 0 ? 1 :
     total_iu_usage / min_usage + ((total_iu_usage % min_usage >= min_usage/2) ? 1 : 0));
  fu_weight[LSU_TYPE - FIRST_FU_TYPE] =
    (total_lsu_usage == 0 ? 1 :
     total_lsu_usage / min_usage + ((total_lsu_usage % min_usage >= min_usage/2) ? 1 : 0));
  fu_weight[FPU_TYPE - FIRST_FU_TYPE] =
    (total_fpu_usage == 0 ? 1 :
     total_fpu_usage / min_usage + ((total_fpu_usage % min_usage >= min_usage/2) ? 1 : 0));

#ifdef DN_DEBUG
#ifdef RELY_ON_GROUPING
  /* record unit imbalance info */
  fu_imbalance[IU_TYPE - FIRST_FU_TYPE] = total_iu_imbalance;
  fu_imbalance[LSU_TYPE - FIRST_FU_TYPE] = total_lsu_imbalance;
  fu_imbalance[FPU_TYPE - FIRST_FU_TYPE] = total_fpu_imbalance;

  /* calc and print statistics */
  if(sched_verbose >= 2){
    fprintf(dump,
            "====> total (flat) cost = %d, total fu usage (%d,%d,%d), total fu imbalance (%d,%d,%d)\n",
            total_groups_cost,
            total_iu_usage, total_lsu_usage, total_fpu_usage,
            total_iu_imbalance, total_lsu_imbalance, total_fpu_imbalance);
  }
#endif
#endif
}

static void
backward_reorder_insns_in_group(dump, sched_verbose, prev_group, current_insn, permuted_group, last_indx)
     FILE *dump;
     int sched_verbose;
     rtx prev_group, current_insn;
     struct group_usage_per_unit *permuted_group;
     int last_indx;
{
  rtx new_insn;
  rtx dummy;
  int new_i;

  if(sched_verbose >= 6)
    fprintf(dump,"apply permutation: backward traverse\n");

  for(new_i = last_indx ; new_i >= 0 ; new_i--){

    if(! current_insn){
      if(sched_verbose >= 6)
	fprintf(dump,"apply permutation: unexpected nil current_insn\n");
      abort();
    }
    
    new_insn = permuted_group->insn_at_slot[new_i];

    if(new_insn && new_insn == permuted_group->insn_at_slot[new_i - 1]){
      if(sched_verbose >= 6)
	fprintf(dump,"second slot of cracked op (new location: %d)\n",new_i);
      continue;
    }

#ifdef DN_DEBUG
    if(sched_verbose >= 6){
      fprintf(dump,"new_insn %d:\n",new_i);
      print_rtl_single(dump,new_insn);
      fprintf(dump,"curr_insn\n");
      print_rtl_single(dump,current_insn);
    }
#endif
    
    if(!new_insn){
      if(new_i == 0){
        rtx next_insn = permuted_group->insn_at_slot[1];
        if(next_insn && is_dispatch_slot_restricted(next_insn) == 2 /* div */){
          if(sched_verbose >= 6)
            fprintf(dump,"div. ignore the nil\n");
          continue;
        }
      }

      if(sched_verbose >= 6)
	fprintf(dump,"need to handle nil (new location: %d)\n",new_i);
      
      if(insert_sched_nops_for_ldb){
	/* add nop after current insn; current doesn't change */
	rtx nop = gen_nop();
	sched_emit_insn_after(nop, current_insn, 0);
	permuted_group->insn_at_slot[new_i] = NEXT_INSN(current_insn)/*nop*/;
      }
      else{
	if(sched_verbose >= 6)
	  fprintf(dump,"nop insertion disabled.\n");
      }
      continue;
    }
    
    if(new_insn == current_insn){
      /* skip to next non nil current op */
      current_insn = get_prev_active_insn(&dummy, current_insn, prev_group);
    }
    else{
      /* disconnect new_insn from it's current location in the insn chain,
	 and add it right after current_insn */
      rtx prev_new_insn = get_prev_active_insn(&dummy, new_insn, prev_group);
      sched_emit_insn_after(new_insn, current_insn, prev_new_insn);
    }
  }
}


static void
forward_reorder_insns_in_group(dump, sched_verbose, post_group, current_insn, permuted_group, last_indx)
     FILE *dump;
     int sched_verbose;
     rtx post_group, current_insn;
     struct group_usage_per_unit *permuted_group;
     int last_indx;
{
  rtx new_insn;
  rtx just_before_next_current = PREV_INSN(current_insn);
  int new_i;
  rtx prev_group = PREV_INSN(current_insn);

  if(sched_verbose >= 6)
    fprintf(dump,"apply permutation: forward traverse\n");

  for(new_i = 0; new_i < last_indx; new_i++){

    if(! current_insn){
      if(sched_verbose >= 6)
	fprintf(dump,"apply permutation: unexpected nil current_insn\n");
      abort();
    }
    
    new_insn = permuted_group->insn_at_slot[new_i];

    if(new_insn && new_insn == permuted_group->insn_at_slot[new_i - 1]){
      if(sched_verbose >= 6)
	fprintf(dump,"second slot of cracked op (new location: %d)\n",new_i);
      continue;
    }

#ifdef DN_DEBUG
    if(sched_verbose >= 6){
      fprintf(dump,"new_insn %d:\n",new_i);
      print_rtl_single(dump,new_insn);
      fprintf(dump,"curr_insn\n");
      print_rtl_single(dump,current_insn);
    }
#endif
    
    if(!new_insn){
      if(new_i == 0){
        rtx next_insn = permuted_group->insn_at_slot[1];	
        if(next_insn && is_dispatch_slot_restricted(next_insn) == 2 /* div */){	
	  if(sched_verbose >= 6)
            fprintf(dump,"div. ignore the nil\n");
	  continue;
	}
      }

      if(sched_verbose >= 6)
	fprintf(dump,"need to handle nil (new location: %d)\n",new_i);
      
      if(insert_sched_nops_for_ldb){
	/* add nop before current insn; current doesn't change */
	rtx nop = gen_nop();
#if 0
	sched_emit_insn_before(nop, current_insn, 1);
#else
	sched_emit_insn_after(nop, just_before_next_current, 0);
#endif
	permuted_group->insn_at_slot[new_i] = NEXT_INSN(just_before_next_current) /*nop*/;
      }
      else{
	if(sched_verbose >= 6)
	  fprintf(dump,"nop insertion disabled.\n");
      }
      continue;
    }
    
    if(new_insn == current_insn){
      /* skip to next non nil current op */
      current_insn = get_next_active_insn(&just_before_next_current, current_insn, post_group);
    }
    else{
#if 0
      sched_emit_insn_before(new_insn, current_insn, 0);
#else
      rtx dummy;
      rtx prev_new_insn = get_prev_active_insn(&dummy, new_insn, prev_group);
      sched_emit_insn_after(new_insn, just_before_next_current, prev_new_insn);
#endif
    }
  }
}

static int
apply_permute(dump, sched_verbose, groups_du_usage, group_no, permuted_group, from, to)
     FILE *dump;
     int sched_verbose;
     struct group_usage_per_unit *groups_du_usage;
     int group_no;
     struct group_usage_per_unit permuted_group;
     int from, to;
{
  struct group_usage_per_unit group_usage = groups_du_usage[group_no];
  rtx current_insn;
  rtx new_insn;
  rtx first_current_insn, last_current_insn;
  rtx prev, post;
  int new_i, curr_i;

  if(sched_verbose >= 6)
    fprintf(dump,"apply permutation!\n");

  /* 1) find insn that precedes this group */
  for(curr_i = 0; curr_i < GROUP_SIZE; curr_i++){
    current_insn = group_usage.insn_at_slot[curr_i];
    if(current_insn) break;
  }
  if(! current_insn){
    return 0;
  }
  first_current_insn = current_insn;
  prev = PREV_INSN(current_insn);

  if(! prev){
    if(sched_verbose >= 6)
      fprintf(dump,"no op before this group?!?!\n");
    return 0;
  }

  /* 2) find last insn in current group order */
  for(curr_i = GROUP_SIZE - 1; curr_i >= 0; curr_i--){
    current_insn = group_usage.insn_at_slot[curr_i];
    if(current_insn) break;
  }
  if(! current_insn){
    return 0;
  }
  last_current_insn = current_insn;
  post = NEXT_INSN(last_current_insn);
  
  /* 3) find last insn in new group order (skip 'nil's) */
  for(new_i = GROUP_SIZE - 1; new_i >= 0; new_i--){
    new_insn = permuted_group.insn_at_slot[new_i];
    if(new_insn) break;
  }
  if(!new_insn){
    return 0;
  }
  
  if(from > to)
    forward_reorder_insns_in_group(dump, sched_verbose, post, first_current_insn, &permuted_group, new_i);
  else
    backward_reorder_insns_in_group(dump, sched_verbose, prev, last_current_insn, &permuted_group, new_i);

  /** update usage data **/
  group_usage = permuted_group;


  /** update TI mode **/
  if(sched_verbose >= 6)
    fprintf(dump,"update TI mode\n");

  /* find index of first insn in new permutation */
  for( new_i = 0; new_i < GROUP_SIZE; new_i++){
    new_insn = permuted_group.insn_at_slot[new_i];
    if(new_insn) break;
  }

  if(sched_verbose >= 6)
	fprintf(dump,"index of first insn = %d\n",new_i);

  if(new_insn && GET_MODE(new_insn) != TImode){

    if(sched_verbose >= 6){
      fprintf(dump,"set TI mode to insn:\n");
      print_rtl_single(dump,new_insn);
    }

    PUT_MODE(new_insn, TImode);

    if(sched_verbose >= 6){
      fprintf(dump,"after:\n");
      print_rtl_single(dump,new_insn);
    }
  }

  new_i++;

  for( ; new_i < GROUP_SIZE; new_i++){
    new_insn = permuted_group.insn_at_slot[new_i];

    if(new_insn && GET_MODE(new_insn) == TImode){

      if(sched_verbose >= 6){
        fprintf(dump,"unset TI mode to insn:\n");
        print_rtl_single(dump,new_insn);
      }

      PUT_MODE(new_insn, VOIDmode);

      if(sched_verbose >= 6){
        fprintf(dump,"after:\n");
        print_rtl_single(dump,new_insn);
      }
    }
  }

  return 1;
}

static bool
try_permute(dump, sched_verbose, groups_du_usage, group_no, permute_no, new_group_du_usage)
     FILE *dump;
     int sched_verbose;
     struct group_usage_per_unit *groups_du_usage;
     int group_no;
     int permute_no;
     struct group_usage_per_unit *new_group_du_usage;
{
   struct group_usage_per_unit group_usage = groups_du_usage[group_no];
   rtx move_insn;
   rtx insn;
   rtx link;
   int slot;
   int is_cracked = 0;
   int from = alt_permute[permute_no].from_slot;
   int to = alt_permute[permute_no].to_slot;
   rtx first_insn, last_insn;
   enum attr_type type;

   move_insn = group_usage.insn_at_slot[ from ];

   if(!move_insn && !insert_sched_nops_for_ldb){
     if(sched_verbose >= 6)
       fprintf(dump,"try_permute: no insn to move!\n");
     return 0;
   }

   if(sched_verbose >= 6){
     fprintf(dump,"try_permute: move insn from slot %d to slot %d\n",from, to);
     print_rtl_single(dump, move_insn);
   }

   /* check disptch slot restrictions */
   if( (from == 0 || to == 0) &&
       (is_dispatch_slot_restricted(group_usage.insn_at_slot[0])) ){
     if(sched_verbose >= 6)
       fprintf(dump,"try_permute: restricted op at slot 0\n");
     return 0;
   }

   if( (from == 0 || to == 0 || from == 1 || to == 1) &&
       (is_dispatch_slot_restricted(group_usage.insn_at_slot[1])) /*div*/){
     if(sched_verbose >= 6)
       fprintf(dump,"try_permute: restricted op at slot 1\n");
     return 0;
   }

   if( ((to == 1 || from == 1) && is_cracked_insn(group_usage.insn_at_slot[0])) ||
       ((to == 2 || from == 2) && is_cracked_insn(group_usage.insn_at_slot[1])) ||
       ((to == 3 || from == 3) && is_cracked_insn(group_usage.insn_at_slot[2]))){
     if(sched_verbose >= 6)
       fprintf(dump,"try_permute: cracked op at slot 0,1 or 2\n");
     return 0;
   }

   if( is_cracked_insn(group_usage.insn_at_slot[from]) )
     is_cracked = 1;
   else
     is_cracked = 0;

   if( is_cracked && to == GROUP_SIZE - 1 ){
     if(sched_verbose >= 6)
       fprintf(dump,"try_permute: cracked op to last pos\n");
     return 0;
   }

   /* check dependencies */
   if(sched_verbose >= 6)
       fprintf(dump,"check dependences\n");

   if(move_insn){
     if(from > to){
       last_insn = group_usage.insn_at_slot[ from ];
       slot = to;	
       first_insn = group_usage.insn_at_slot[ slot ];
       while(!first_insn && slot <= from)
         first_insn = group_usage.insn_at_slot[ ++slot ];
 
       for(insn = first_insn; insn != last_insn; insn = NEXT_INSN(insn)){
	 if(insn){
	   if(GET_CODE(insn) == NOTE)
	     continue;
	   else if(GET_CODE (PATTERN (insn)) != USE &&
     		   GET_CODE (PATTERN (insn)) != CLOBBER){
  	     type = get_attr_type(insn);
	     if(type == TYPE_NOP)
	       continue;
	   }

           for (link = INSN_DEPEND (insn); link != 0; link = XEXP (link, 1)){
             rtx next = XEXP (link, 0);
             if(next == move_insn){
               if(sched_verbose >= 6){
                 fprintf(dump,"try_permute: move_insn depends on preceding insn:\n");
                 print_rtl_single(dump, next);
               }
               return 0;
             }
           }
	 }  
       }
     }
     else if(from < to){
       first_insn = group_usage.insn_at_slot[ from ];
       slot = to; 	
       last_insn = group_usage.insn_at_slot[ slot ];
       while(!last_insn && slot > from)
	 last_insn = group_usage.insn_at_slot[ --slot ]; 
       if(last_insn && last_insn != first_insn){
	 insn = first_insn;
         do{ 
	   insn = NEXT_INSN(insn);

	   if(GET_CODE(insn) == NOTE)
             continue;
	   else if(GET_CODE (PATTERN (insn)) != USE &&
                   GET_CODE (PATTERN (insn)) != CLOBBER){
             type = get_attr_type(insn);
             if(type == TYPE_NOP)
               continue;
           }

	   if(insn){
	     for (link = INSN_DEPEND (move_insn); link != 0; link = XEXP (link, 1)){
               rtx next = XEXP (link, 0);
               if(next == insn){
                 if(sched_verbose >= 6){
                   fprintf(dump,"try_permute: insn depends on move_insn:\n");
                   print_rtl_single(dump, next);
                 }
                 return 0;
               }
             }
	   } 
         }while(insn != last_insn);
       }/* if(last_insn) */
     }
   }

   /* record new usage data */
   zero_group_info1(new_group_du_usage);
   if(from > to){
     for(slot = 0; slot < GROUP_SIZE; slot++){
       if(sched_verbose >= 6)
         fprintf(dump,"new slot = %d\n",slot);

       if(slot < to || slot > (from + is_cracked))
         insn = group_usage.insn_at_slot[slot];
       else if(slot > (to + is_cracked) && slot <= (from + is_cracked))
         insn = group_usage.insn_at_slot[slot - (1 + is_cracked)];
       else if(slot == to)
         insn = group_usage.insn_at_slot[from];
       else if(is_cracked && slot == to + 1){
         insn = group_usage.insn_at_slot[from];
         (*new_group_du_usage).insn_at_slot[slot] = insn;
         continue;
       }

       (*new_group_du_usage).insn_at_slot[slot] = insn;

       if(insn){
         if(sched_verbose >= 6)
           print_rtl_single(dump, insn);
         record_data1(dump, sched_verbose, new_group_du_usage, insn, slot);
       }
     }
   }
   else if (from < to){
     if(is_cracked)
       to = to - 1;
     for(slot = 0; slot < GROUP_SIZE; slot++){
       if(sched_verbose >= 6)
         fprintf(dump,"new slot = %d\n",slot);

       if(slot < from || slot > (to + is_cracked))
         insn = group_usage.insn_at_slot[slot];
       else if(slot >= from && slot < to)
         insn = group_usage.insn_at_slot[slot + 1 + is_cracked];
       else if(slot == to)
         insn = group_usage.insn_at_slot[from];
       else if(is_cracked && slot == to + 1){
         insn = group_usage.insn_at_slot[from];
         (*new_group_du_usage).insn_at_slot[slot] = insn;
         continue;
       }

       (*new_group_du_usage).insn_at_slot[slot] = insn;

       if(insn){
         if(sched_verbose >= 6)
           print_rtl_single(dump, insn);
         record_data1(dump, sched_verbose, new_group_du_usage, insn, slot);
       }
     }
   }

   return 1;
}

static void
inter_group_load_balance(dump, sched_verbose, groups_du_usage, n_groups, scale, global)
  FILE *dump;
  int sched_verbose;
  struct group_usage_per_unit *groups_du_usage;
  int n_groups;
  int scale;
  int global;
{
  int j;
  int group_no;
  int permute_no;
  int min_cost;
  int best_move;
  int group_score;
  int permutation_score;
  struct group_usage_per_unit new_du_usage;
  struct group_usage_per_unit best_permuted_group;
  int empty_group;

  if (rs6000_cpu != PROCESSOR_POWER4){
    return;
  }

  for(group_no = 0; group_no < n_groups; group_no++){

    empty_group = 1;
    if(sched_verbose >= 6)
      fprintf(dump,">>>>>>> group insns:\n");
    for(j = 0; j < 4; j++){
      rtx insn;
      insn = groups_du_usage[group_no].insn_at_slot[j];
      if(insn) empty_group = 0;
      if(sched_verbose >= 6)
        print_rtl_single(dump, insn);
    }
    if(empty_group)
      continue;

    if(global)
      group_score = calc_total_score(groups_du_usage, 0, n_groups, scale, global);
    else
      group_score = group_cost(groups_du_usage, group_no, scale);

    if(sched_verbose >= 6)
      fprintf(dump,"======orig group score %d\n",group_score);

    if(group_score){ /* if there's room for improvement */
      min_cost = group_score;
      best_move = -1;

      for(permute_no = 0; permute_no < N_PERMUTATIONS; permute_no++){

        if(sched_verbose >= 6)
          fprintf(dump,"\ntry permutation %d:\n",permute_no);

        if(try_permute(dump, sched_verbose, groups_du_usage, group_no, permute_no, &new_du_usage)){

          if(global)
            permutation_score =
              calc_alt_total_score(groups_du_usage, 0, n_groups, group_no, new_du_usage, scale, global);
          else
            permutation_score = group_cost1(new_du_usage, scale);

          if(sched_verbose >= 6)
            fprintf(dump,"==========permutation score %d\n",permutation_score);

          if(permutation_score < min_cost){
            min_cost = permutation_score;
            best_move = permute_no;
            best_permuted_group = new_du_usage;
          }

          if(permutation_score == 0)
            break; /* no need to continue to try other options */
        }
      }/* for */

      if(best_move > -1){
        int from = alt_permute[best_move].from_slot;
        int to = alt_permute[best_move].to_slot;
        apply_permute(dump, sched_verbose, groups_du_usage, group_no, best_permuted_group,from,to);
      }
    }

  }

  return;
}

/* The following function is called at the end of scheduling BB.
   After reload, it inserts nops at insn group bundling.  */
static void
rs6000_sched_finish (dump, sched_verbose)
     FILE *dump;
     int sched_verbose;
{
  int n_groups;
  struct group_usage_per_unit *group_du_usage;
  int scale;
  int global;
 

  if (sched_verbose)
    fprintf (dump, "=== Finishing schedule.\n");

  if (!reload_completed)
    return;

  if(insert_sched_nops == 2)
     n_groups = redefine_groups(dump, sched_verbose, current_sched_info->prev_head,
  	                        current_sched_info->next_tail);
  else
     n_groups = pad_groups(dump, sched_verbose, current_sched_info->prev_head,
  	                   current_sched_info->next_tail);


  if(sched_verbose >= 6){
    fprintf(dump,"ngroups = %d\n",n_groups);
  }

  if(!sched_load_balance || n_groups <= 0){
    return;
  }

  group_du_usage =
    (struct group_usage_per_unit *)
    xcalloc (sizeof (struct group_usage_per_unit), n_groups);

  if(sched_load_balance_scale)
    scale = 1;
  else 
    scale = 0;

  global = flag_sched_ldb_global;
  if(global < 0)
    global = 0;

  if(sched_verbose >= 3){
    fprintf(dump,"scale = %d, global = %d\n",scale, global);
  }

  init_group_data(dump, sched_verbose,
                          group_du_usage, n_groups,
                          current_sched_info->prev_head,
                          current_sched_info->next_tail);

  inter_group_load_balance(dump, sched_verbose,
                           group_du_usage, n_groups, scale, global);

  free(group_du_usage);
}
#endif /* DN_SCHED_FINISH */

static int
is_dispatch_slot_restricted(insn)
  rtx insn;
{
  enum attr_type type; 
  int is_restricted;

  if (!insn
      || insn == NULL_RTX
      || GET_CODE(insn) == NOTE
      || GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
    return 0;

  type = get_attr_type(insn);

  switch(type){
#ifndef MFCR_TYPE_IS_MICROCODED
  case TYPE_MFCR:
#endif
  case TYPE_MFCRF:
  case TYPE_MTCR:
  case TYPE_DELAYED_CR:
  case TYPE_CR_LOGICAL:
  case TYPE_MTJMPR:
  case TYPE_MFJMPR:
    return 1;
  case TYPE_IDIV:
  case TYPE_LDIV:
    return 2;
  default:
    return 0;
  }

  return is_restricted;
}

static int
rs6000_prioritize(insn1, insn2)
  rtx insn1;
  rtx insn2;
{
  int is_du_restricted1;
  int is_du_restricted2;

  if(rs6000_cpu != PROCESSOR_POWER4){
    return 0;
  }

  is_du_restricted1 = is_dispatch_slot_restricted(insn1);
  is_du_restricted2 = is_dispatch_slot_restricted(insn2);

  if(is_du_restricted1 && is_du_restricted2)
    return 0;
  else if (is_du_restricted1)
    return 1;
  else if (is_du_restricted2)
    return -1;
  else
    return 0;

}

/* DN begin */
static bool
is_store_insn(rtx insn)
{
#if 1
  enum attr_type type;

  if (!insn
      || insn == NULL_RTX
      || GET_CODE(insn) == NOTE
      || GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
    return 0;

  type = get_attr_type(insn);

  switch (type){
	case TYPE_STORE:
	case TYPE_STORE_UX:
	case TYPE_STORE_U:
	case TYPE_FPSTORE:
	case TYPE_FPSTORE_UX:
	case TYPE_FPSTORE_U:
	case TYPE_VECSTORE:
	return 1;
 	default:
	return 0;	
  }

#else
  const char * fmt;
  int i, j;
  int ret = false;

  if (!insn)
    return false;

  if (GET_CODE (insn) == SET)
    insn = SET_DEST (insn);

  if (GET_CODE (insn) == MEM)
    return true;

  /* Recursively process the insn.  */
  fmt = GET_RTX_FORMAT (GET_CODE (insn));

  for (i = GET_RTX_LENGTH (GET_CODE (insn)) - 1; i >= 0 && !ret; i--)
    {
      if (fmt[i] == 'e')
        ret |= is_store_insn (XEXP (insn, i));
      else if (fmt[i] == 'E')
        for (j = XVECLEN (insn, i) - 1; j >= 0; j--)
          ret |= is_store_insn (XVECEXP (insn, i, j));
    }

  return ret;
#endif
}

static bool
is_load_insn(rtx insn)
{
#if 1
  enum attr_type type;

  if (!insn
      || insn == NULL_RTX
      || GET_CODE(insn) == NOTE
      || GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
    return 0;

  type = get_attr_type(insn);

  switch (type){
	case TYPE_LOAD:
	case TYPE_LOAD_EXT:
	case TYPE_LOAD_EXT_U:
	case TYPE_LOAD_EXT_UX:
	case TYPE_LOAD_UX:
	case TYPE_LOAD_U:
	case TYPE_FPLOAD:
	case TYPE_FPLOAD_UX:
	case TYPE_FPLOAD_U:
	case TYPE_VECLOAD:
	return 1;
	default:
	return 0;
  }  

#else
  const char * fmt;
  int i, j;
  int ret = false;

  if (!insn)
    return false;

  if (GET_CODE (insn) == SET)
    insn = SET_SRC (insn); 

  if (GET_CODE (insn) == MEM)
    return true;

  /* Recursively process the insn.  */
  fmt = GET_RTX_FORMAT (GET_CODE (insn));

  for (i = GET_RTX_LENGTH (GET_CODE (insn)) - 1; i >= 0 && !ret; i--)
    {
      if (fmt[i] == 'e')
        ret |= is_load_insn (XEXP (insn, i)); 
      else if (fmt[i] == 'E')
        for (j = XVECLEN (insn, i) - 1; j >= 0; j--)
          ret |= is_load_insn (XVECEXP (insn, i, j)); 
    }

  return ret;
#endif
}

static int 
rs6000_is_costly_dependence (insn, next, link, cost)
	rtx insn, next, link;
	int cost;
{
  /* if the flag is not enbled -
     allow all dependences in the same group - most aggressive option */
  if(!sched_costly_dep)
       return 0;

  /* if the flag is set to 1 -
     do not allow dependent instructions in the same group */
  if(sched_costly_dep == 1)
       return 1;

  /* if the flag is set to X > 4 -
     then dependences with latency >= X are considered costly, and will
     not be scheduled in the same group */
  if(sched_costly_dep > 4 && cost >= sched_costly_dep)
        return 1;

  /* if the flag is set to X == 2 or X > 4 -
     then only load-after-store is considered costly and are not
     scheduled in the same group */
  if (sched_costly_dep > 4 || sched_costly_dep == 2){
    /* prevent load after store in the same group */
    if (is_load_insn(next) && is_store_insn(insn))
      return 1;
  } 
#if 1
  else if (sched_costly_dep > 4 || sched_costly_dep == 3){
    /* prevent load after store in the same group if it is a true dependence */
    if (is_load_insn(next) && is_store_insn(insn) &&
        (!link || (int) REG_NOTE_KIND (link) == 0))
      return 1;
  } 
#else
  else if (sched_costly_dep > 4 || sched_costly_dep == 3){
    /* prevent load after load in the same group */
    if (is_load_insn(next) && (is_store_insn(insn) || is_load_insn(insn))) 
      return 1;  
  } 
  else if (sched_costly_dep > 4 || sched_costly_dep == 4){ 
    /* prevent any insn after load in the same group */ 
    if (is_load_insn(insn) || (is_store_insn(insn) && is_load_insn(next))) 
      return 1;
  } 
#endif

  return 0;
}
/* DN end */


/* Adjust the cost of a scheduling dependency.  Return the new cost of
   a dependency LINK or INSN on DEP_INSN.  COST is the current cost.  */

static int
rs6000_adjust_cost (insn, link, dep_insn, cost)
     rtx insn;
     rtx link;
     rtx dep_insn ATTRIBUTE_UNUSED;
     int cost;
{
  if (! recog_memoized (insn))
    return 0;

  if (REG_NOTE_KIND (link) != 0)
    return 0;

  if (REG_NOTE_KIND (link) == 0)
    {
      /* Data dependency; DEP_INSN writes a register that INSN reads
	 some cycles later.  */
      switch (get_attr_type (insn))
	{
	case TYPE_JMPREG:
	  /* Tell the first scheduling pass about the latency between
	     a mtctr and bctr (and mtlr and br/blr).  The first
	     scheduling pass will not know about this latency since
	     the mtctr instruction, which has the latency associated
	     to it, will be generated by reload.  */
	  return TARGET_POWER ? 5 : 4;
	case TYPE_BRANCH:
	  /* Leave some extra cycles between a compare and its
	     dependent branch, to inhibit expensive mispredicts.  */
	  if ((rs6000_cpu_attr == CPU_PPC603
	       || rs6000_cpu_attr == CPU_PPC604
	       || rs6000_cpu_attr == CPU_PPC604E
	       || rs6000_cpu_attr == CPU_PPC620
	       || rs6000_cpu_attr == CPU_PPC630
	       || rs6000_cpu_attr == CPU_PPC750
	       || rs6000_cpu_attr == CPU_PPC7400
	       || rs6000_cpu_attr == CPU_PPC7450
	       || rs6000_cpu_attr == CPU_POWER4)
	      && recog_memoized (dep_insn)
	      && (INSN_CODE (dep_insn) >= 0)
	      && (get_attr_type (dep_insn) == TYPE_CMP
		  || get_attr_type (dep_insn) == TYPE_COMPARE
		  || get_attr_type (dep_insn) == TYPE_DELAYED_COMPARE
		  || get_attr_type (dep_insn) == TYPE_IMUL_COMPARE
		  || get_attr_type (dep_insn) == TYPE_LMUL_COMPARE
		  || get_attr_type (dep_insn) == TYPE_FPCOMPARE
		  || get_attr_type (dep_insn) == TYPE_CR_LOGICAL
		  || get_attr_type (dep_insn) == TYPE_DELAYED_CR))
	    return cost + 2;
	default:
	  break;
	}
      /* Fall out to return default cost.  */
    }

  return cost;
}

/* A C statement (sans semicolon) to update the integer scheduling
   priority INSN_PRIORITY (INSN).  Reduce the priority to execute the
   INSN earlier, increase the priority to execute INSN later.  Do not
   define this macro if you do not need to adjust the scheduling
   priorities of insns.  */

static int
rs6000_adjust_priority (insn, priority)
     rtx insn ATTRIBUTE_UNUSED;
     int priority;
{
  /* On machines (like the 750) which have asymmetric integer units,
     where one integer unit can do multiply and divides and the other
     can't, reduce the priority of multiply/divide so it is scheduled
     before other integer operations.  */

#if 0
  if (! INSN_P (insn))
    return priority;

  if (GET_CODE (PATTERN (insn)) == USE)
    return priority;

  switch (rs6000_cpu_attr) {
  case CPU_PPC750:
    switch (get_attr_type (insn))
      {
      default:
	break;

      case TYPE_IMUL:
      case TYPE_IDIV:
	fprintf (stderr, "priority was %#x (%d) before adjustment\n",
		 priority, priority);
	if (priority >= 0 && priority < 0x01000000)
	  priority >>= 3;
	break;
      }
  }
#endif

  return priority;
}

/* Return how many instructions the machine can issue per cycle.  */

static int
rs6000_issue_rate ()
{
  /* Use issue rate of 1 for first scheduling pass to decrease degradation.  */
  if (!reload_completed)
    return 1;

  switch (rs6000_cpu_attr) {
  case CPU_RIOS1:  /* ? */
  case CPU_RS64A:
  case CPU_PPC601: /* ? */
  case CPU_PPC7450:
    return 3;
  case CPU_PPC440:
  case CPU_PPC603:
  case CPU_PPC750:
  case CPU_PPC7400:
    return 2; 
  case CPU_RIOS2:
  case CPU_PPC604:
  case CPU_PPC604E:
  case CPU_PPC620:
  case CPU_PPC630:
    return 4;
/* DN begin */
  case CPU_POWER4:
#ifdef ISSUE_RATE_INCLUDES_BU_SLOT
    return 5; 
#else
    return 4;
#endif
/* DN end */
  default:
    return 1;
  }
}


/* Length in units of the trampoline for entering a nested function.  */

int
rs6000_trampoline_size ()
{
  int ret = 0;

  switch (DEFAULT_ABI)
    {
    default:
      abort ();

    case ABI_AIX:
      ret = (TARGET_32BIT) ? 12 : 24;
      break;

    case ABI_DARWIN:
    case ABI_V4:
    case ABI_AIX_NODESC:
      ret = (TARGET_32BIT) ? 40 : 48;
      break;
    }

  return ret;
}

/* Emit RTL insns to initialize the variable parts of a trampoline.
   FNADDR is an RTX for the address of the function's pure code.
   CXT is an RTX for the static chain value for the function.  */

void
rs6000_initialize_trampoline (addr, fnaddr, cxt)
     rtx addr;
     rtx fnaddr;
     rtx cxt;
{
  enum machine_mode pmode = Pmode;
  int regsize = (TARGET_32BIT) ? 4 : 8;
  rtx ctx_reg = force_reg (pmode, cxt);

  switch (DEFAULT_ABI)
    {
    default:
      abort ();

/* Macros to shorten the code expansions below.  */
#define MEM_DEREF(addr) gen_rtx_MEM (pmode, memory_address (pmode, addr))
#define MEM_PLUS(addr,offset) \
  gen_rtx_MEM (pmode, memory_address (pmode, plus_constant (addr, offset)))

    /* Under AIX, just build the 3 word function descriptor */
    case ABI_AIX:
      {
	rtx fn_reg = gen_reg_rtx (pmode);
	rtx toc_reg = gen_reg_rtx (pmode);
	emit_move_insn (fn_reg, MEM_DEREF (fnaddr));
	emit_move_insn (toc_reg, MEM_PLUS (fnaddr, regsize));
	emit_move_insn (MEM_DEREF (addr), fn_reg);
	emit_move_insn (MEM_PLUS (addr, regsize), toc_reg);
	emit_move_insn (MEM_PLUS (addr, 2*regsize), ctx_reg);
      }
      break;

    /* Under V.4/eabi/darwin, __trampoline_setup does the real work.  */
    case ABI_DARWIN:
    case ABI_V4:
    case ABI_AIX_NODESC:
      emit_library_call (gen_rtx_SYMBOL_REF (SImode, "__trampoline_setup"),
			 FALSE, VOIDmode, 4,
			 addr, pmode,
			 GEN_INT (rs6000_trampoline_size ()), SImode,
			 fnaddr, pmode,
			 ctx_reg, pmode);
      break;
    }

  return;
}


/* Table of valid machine attributes.  */

const struct attribute_spec rs6000_attribute_table[] =
{
  /* { name, min_len, max_len, decl_req, type_req, fn_type_req, handler } */
  /* APPLE LOCAL begin double destructor */
#ifdef SUBTARGET_ATTRIBUTE_TABLE
  SUBTARGET_ATTRIBUTE_TABLE
#endif
  /* APPLE LOCAL end double destructor */
  { "longcall",  0, 0, false, true,  true,  rs6000_handle_longcall_attribute },
  { "shortcall", 0, 0, false, true,  true,  rs6000_handle_longcall_attribute },
  { NULL,        0, 0, false, false, false, NULL }
};

/* Handle a "longcall" or "shortcall" attribute; arguments as in
   struct attribute_spec.handler.  */

static tree
rs6000_handle_longcall_attribute (node, name, args, flags, no_add_attrs)
     tree *node;
     tree name;
     tree args ATTRIBUTE_UNUSED;
     int flags ATTRIBUTE_UNUSED;
     bool *no_add_attrs;
{
  if (TREE_CODE (*node) != FUNCTION_TYPE
      && TREE_CODE (*node) != FIELD_DECL
      && TREE_CODE (*node) != TYPE_DECL)
    {
      warning ("`%s' attribute only applies to functions",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Set longcall attributes on all functions declared when
   rs6000_default_long_calls is true.  */
static void
rs6000_set_default_type_attributes (type)
     tree type;
{
  if (rs6000_default_long_calls
      && (TREE_CODE (type) == FUNCTION_TYPE
	  || TREE_CODE (type) == METHOD_TYPE))
    TYPE_ATTRIBUTES (type) = tree_cons (get_identifier ("longcall"),
					NULL_TREE,
					TYPE_ATTRIBUTES (type));
}

/* Return a reference suitable for calling a function with the
   longcall attribute.  */

struct rtx_def *
rs6000_longcall_ref (call_ref)
     rtx call_ref;
{
  const char *call_name;
  tree node;

  if (GET_CODE (call_ref) != SYMBOL_REF)
    return call_ref;

  /* System V adds '.' to the internal name, so skip them.  */
  call_name = XSTR (call_ref, 0);
  if (*call_name == '.')
    {
      while (*call_name == '.')
	call_name++;

      node = get_identifier (call_name);
      call_ref = gen_rtx_SYMBOL_REF (VOIDmode, IDENTIFIER_POINTER (node));
    }

  return force_reg (Pmode, call_ref);
}


#ifdef USING_ELFOS_H

/* A C statement or statements to switch to the appropriate section
   for output of RTX in mode MODE.  You can assume that RTX is some
   kind of constant in RTL.  The argument MODE is redundant except in
   the case of a `const_int' rtx.  Select the section by calling
   `text_section' or one of the alternatives for other sections.

   Do not define this macro if you put all constants in the read-only
   data section.  */

static void
rs6000_elf_select_rtx_section (mode, x, align)
     enum machine_mode mode;
     rtx x;
     unsigned HOST_WIDE_INT align;
{
  if (ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (x, mode))
    toc_section ();
  else
    default_elf_select_rtx_section (mode, x, align);
}

/* A C statement or statements to switch to the appropriate
   section for output of DECL.  DECL is either a `VAR_DECL' node
   or a constant of some sort.  RELOC indicates whether forming
   the initial value of DECL requires link-time relocations.  */

static void
rs6000_elf_select_section (decl, reloc, align)
     tree decl;
     int reloc;
     unsigned HOST_WIDE_INT align;
{
  default_elf_select_section_1 (decl, reloc, align,
				flag_pic || DEFAULT_ABI == ABI_AIX);
}

/* A C statement to build up a unique section name, expressed as a
   STRING_CST node, and assign it to DECL_SECTION_NAME (decl).
   RELOC indicates whether the initial value of EXP requires
   link-time relocations.  If you do not define this macro, GCC will use
   the symbol name prefixed by `.' as the section name.  Note - this
   macro can now be called for uninitialized data items as well as
   initialized data and functions.  */

static void
rs6000_elf_unique_section (decl, reloc)
     tree decl;
     int reloc;
{
  default_unique_section_1 (decl, reloc,
			    flag_pic || DEFAULT_ABI == ABI_AIX);
}


/* If we are referencing a function that is static or is known to be
   in this file, make the SYMBOL_REF special.  We can use this to indicate
   that we can branch to this function without emitting a no-op after the
   call.  For real AIX calling sequences, we also replace the
   function name with the real name (1 or 2 leading .'s), rather than
   the function descriptor name.  This saves a lot of overriding code
   to read the prefixes.  */

static void
rs6000_elf_encode_section_info (decl, first)
     tree decl;
     int first;
{
  if (!first)
    return;

  if (TREE_CODE (decl) == FUNCTION_DECL)
    {
      rtx sym_ref = XEXP (DECL_RTL (decl), 0);
      if ((*targetm.binds_local_p) (decl))
	SYMBOL_REF_FLAG (sym_ref) = 1;

      if (DEFAULT_ABI == ABI_AIX)
	{
	  size_t len1 = (DEFAULT_ABI == ABI_AIX) ? 1 : 2;
	  size_t len2 = strlen (XSTR (sym_ref, 0));
	  char *str = alloca (len1 + len2 + 1);
	  str[0] = '.';
	  str[1] = '.';
	  memcpy (str + len1, XSTR (sym_ref, 0), len2 + 1);

	  XSTR (sym_ref, 0) = ggc_alloc_string (str, len1 + len2);
	}
    }
  else if (rs6000_sdata != SDATA_NONE
	   && DEFAULT_ABI == ABI_V4
	   && TREE_CODE (decl) == VAR_DECL)
    {
      rtx sym_ref = XEXP (DECL_RTL (decl), 0);
      int size = int_size_in_bytes (TREE_TYPE (decl));
      tree section_name = DECL_SECTION_NAME (decl);
      const char *name = (char *)0;
      int len = 0;

      if ((*targetm.binds_local_p) (decl))
	SYMBOL_REF_FLAG (sym_ref) = 1;

      if (section_name)
	{
	  if (TREE_CODE (section_name) == STRING_CST)
	    {
	      name = TREE_STRING_POINTER (section_name);
	      len = TREE_STRING_LENGTH (section_name);
	    }
	  else
	    abort ();
	}

      if ((size > 0 && size <= g_switch_value)
	  || (name
	      && ((len == sizeof (".sdata") - 1
		   && strcmp (name, ".sdata") == 0)
		  || (len == sizeof (".sdata2") - 1
		      && strcmp (name, ".sdata2") == 0)
		  || (len == sizeof (".sbss") - 1
		      && strcmp (name, ".sbss") == 0)
		  || (len == sizeof (".sbss2") - 1
		      && strcmp (name, ".sbss2") == 0)
		  || (len == sizeof (".PPC.EMB.sdata0") - 1
		      && strcmp (name, ".PPC.EMB.sdata0") == 0)
		  || (len == sizeof (".PPC.EMB.sbss0") - 1
		      && strcmp (name, ".PPC.EMB.sbss0") == 0))))
	{
	  size_t len = strlen (XSTR (sym_ref, 0));
	  char *str = alloca (len + 2);

	  str[0] = '@';
	  memcpy (str + 1, XSTR (sym_ref, 0), len + 1);
	  XSTR (sym_ref, 0) = ggc_alloc_string (str, len + 1);
	}
    }
}

static const char *
rs6000_elf_strip_name_encoding (str)
     const char *str;
{
  while (*str == '*' || *str == '@')
    str++;
  return str;
}

static bool
rs6000_elf_in_small_data_p (decl)
     tree decl;
{
  if (rs6000_sdata == SDATA_NONE)
    return false;

  if (TREE_CODE (decl) == VAR_DECL && DECL_SECTION_NAME (decl))
    {
      const char *section = TREE_STRING_POINTER (DECL_SECTION_NAME (decl));
      if (strcmp (section, ".sdata") == 0
	  || strcmp (section, ".sdata2") == 0
	  || strcmp (section, ".sbss") == 0)
	return true;
    }
  else
    {
      HOST_WIDE_INT size = int_size_in_bytes (TREE_TYPE (decl));

      if (size > 0
	  && size <= g_switch_value
	  && (rs6000_sdata != SDATA_DATA || TREE_PUBLIC (decl)))
	return true;
    }

  return false;
}

#endif /* USING_ELFOS_H */


/* Return a REG that occurs in ADDR with coefficient 1.
   ADDR can be effectively incremented by incrementing REG.

   r0 is special and we must not select it as an address
   register by this routine since our caller will try to
   increment the returned register via an "la" instruction.  */

struct rtx_def *
find_addr_reg (addr)
     rtx addr;
{
  while (GET_CODE (addr) == PLUS)
    {
      if (GET_CODE (XEXP (addr, 0)) == REG
	  && REGNO (XEXP (addr, 0)) != 0)
	addr = XEXP (addr, 0);
      else if (GET_CODE (XEXP (addr, 1)) == REG
	       && REGNO (XEXP (addr, 1)) != 0)
	addr = XEXP (addr, 1);
      else if (CONSTANT_P (XEXP (addr, 0)))
	addr = XEXP (addr, 1);
      else if (CONSTANT_P (XEXP (addr, 1)))
	addr = XEXP (addr, 0);
      else
	abort ();
    }
  if (GET_CODE (addr) == REG && REGNO (addr) != 0)
    return addr;
  abort ();
}

void
rs6000_fatal_bad_address (op)
  rtx op;
{
  fatal_insn ("bad address", op);
}

#if TARGET_MACHO

#if 0
/* Returns 1 if OP is either a symbol reference or a sum of a symbol
   reference and a constant.  */

int
symbolic_operand (op)
     rtx op;
{
  switch (GET_CODE (op))
    {
    case SYMBOL_REF:
    case LABEL_REF:
      return 1;
    case CONST:
      op = XEXP (op, 0);
      return (GET_CODE (op) == SYMBOL_REF ||
	      (GET_CODE (XEXP (op, 0)) == SYMBOL_REF
	       || GET_CODE (XEXP (op, 0)) == LABEL_REF)
	      && GET_CODE (XEXP (op, 1)) == CONST_INT);
    default:
      return 0;
    }
}
#endif

#define GEN_LOCAL_LABEL_FOR_SYMBOL(BUF,SYMBOL,LENGTH,N)		\
  do {								\
    const char *const symbol_ = (SYMBOL);			\
    char *buffer_ = (BUF);					\
    if (symbol_[0] == '"')					\
      {								\
        sprintf(buffer_, "\"L%d$%s", (N), symbol_+1);		\
      }								\
    else if (name_needs_quotes(symbol_))			\
      {								\
        sprintf(buffer_, "\"L%d$%s\"", (N), symbol_);		\
      }								\
    else							\
      {								\
        sprintf(buffer_, "L%d$%s", (N), symbol_);		\
      }								\
  } while (0)

#ifdef RS6000_LONG_BRANCH

static tree stub_list = 0;
static int local_label_unique_number = 0;

/* ADD_COMPILER_STUB adds the compiler generated stub for handling 
   procedure calls to the linked list.  */

void 
add_compiler_stub (label_name, function_name, line_number)
     tree label_name;
     tree function_name;
     int line_number;
{
  tree stub = build_tree_list (function_name, label_name);
  TREE_TYPE (stub) = build_int_2 (line_number, 0);
  TREE_CHAIN (stub) = stub_list;
  stub_list = stub;
}

#define STUB_LABEL_NAME(STUB)     TREE_VALUE (STUB)
#define STUB_FUNCTION_NAME(STUB)  TREE_PURPOSE (STUB)

/* OUTPUT_COMPILER_STUB outputs the compiler generated stub for
   handling procedure calls from the linked list and initializes the
   linked list.  */

void
output_compiler_stub ()
{
  tree stub;
  const char *name;
  char *local_label_0;
  const char *non_lazy_pointer_name, *unencoded_non_lazy_pointer_name;
  int length;

  for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
    {
      fprintf (asm_out_file,
	       "%s:\n", IDENTIFIER_POINTER(STUB_LABEL_NAME(stub)));

      /* APPLE LOCAL begin structor thunks */
      name = IDENTIFIER_POINTER (STUB_FUNCTION_NAME (stub));

      /* If PIC and the callee has no stub, do indirect call through a
	 non-lazy-pointer.  'save_world' expects a parameter in R11;
	 the dyld_stub_binding_helper (part of the Mach-O stub
	 interface) expects a different parameter in R11.  This is
	 effectively a "non-lazy stub."  By-the-way, a
	 "non-lazy-pointer" is a .long that gets coalesced with others
	 of the same value, so one NLP suffices for an entire
	 application.  */
      if (flag_pic && (machopic_classify_ident (get_identifier (name)) == MACHOPIC_UNDEFINED))
	{
	  /* This is the address of the non-lazy pointer; load from it
	     to get the address we want.  */
	  non_lazy_pointer_name = machopic_non_lazy_ptr_name (name);
	  machopic_validate_stub_or_non_lazy_ptr (non_lazy_pointer_name,
						  /* non-lazy-pointer */0);
	  unencoded_non_lazy_pointer_name =
	    (*targetm.strip_name_encoding) (non_lazy_pointer_name);
	  length = strlen (name);
	  local_label_0 = alloca (length + 32);
	  GEN_LOCAL_LABEL_FOR_SYMBOL (local_label_0, name, length,
				      local_label_unique_number);
	  local_label_unique_number++;
	  fprintf (asm_out_file, "\tmflr r0\n");
	  fprintf (asm_out_file, "\tbcl 20,31,%s\n", local_label_0);
	  fprintf (asm_out_file, "%s:\n", local_label_0);
	  fprintf (asm_out_file, "\tmflr r12\n");
	  fprintf (asm_out_file, "\taddis r12,r12,ha16(");
	  assemble_name (asm_out_file, non_lazy_pointer_name);
	  fprintf (asm_out_file, "-%s)\n", local_label_0);
	  fprintf (asm_out_file, "\tlwz r12,lo16(");
	  assemble_name (asm_out_file, non_lazy_pointer_name);
	  fprintf (asm_out_file, "-%s)(r12)\n", local_label_0);
	  fprintf (asm_out_file, "\tmtlr r0\n");
	  fprintf (asm_out_file, "\tmtctr r12\n");
	  fprintf (asm_out_file, "\tbctr\n");
	}
      else if (flag_pic)	/* Far call to a stub.  */
	{
	  fputs ("\tmflr r0\n", asm_out_file);
	  fprintf (asm_out_file, "\tbcl 20,31,%s_pic\n",
	       IDENTIFIER_POINTER(STUB_LABEL_NAME(stub)));
	  fprintf (asm_out_file, "%s_pic:\n",
		   IDENTIFIER_POINTER(STUB_LABEL_NAME(stub)));
	  fputs ("\tmflr r12\n", asm_out_file);

	  fputs ("\taddis r12,r12,ha16(", asm_out_file);
	  ASM_OUTPUT_LABELREF (asm_out_file, name);
	  fprintf (asm_out_file, " - %s_pic)\n", IDENTIFIER_POINTER(STUB_LABEL_NAME(stub)));
		   
	  fputs ("\tmtlr r0\n", asm_out_file);

	  fputs ("\taddi r12,r12,lo16(", asm_out_file);
	  ASM_OUTPUT_LABELREF (asm_out_file, name);
	  fprintf (asm_out_file, " - %s_pic)\n", IDENTIFIER_POINTER(STUB_LABEL_NAME(stub)));

	  fputs ("\tmtctr r12\n", asm_out_file);
	  fputs ("\tbctr\n", asm_out_file);
	}
      else
	{
	  fputs ("\tlis r12,hi16(", asm_out_file);
	  ASM_OUTPUT_LABELREF (asm_out_file, name);
	  fputs (")\n\tori r12,r12,lo16(", asm_out_file);
	  ASM_OUTPUT_LABELREF (asm_out_file, name);
	  fputs (")\n\tmtctr r12\n\tbctr\n", asm_out_file);
	}
      /* APPLE LOCAL end structor thunks */
    }

  stub_list = 0;
}

/* NO_PREVIOUS_DEF checks in the link list whether the function name is
   already there or not.  */

int
no_previous_def (function_name)
     tree function_name;
{
  tree stub;
  for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
    if (function_name == STUB_FUNCTION_NAME (stub))
      return 0;
  return 1;
}

/* GET_PREV_LABEL gets the label name from the previous definition of
   the function.  */

tree
get_prev_label (function_name)
     tree function_name;
{
  tree stub;
  for (stub = stub_list; stub; stub = TREE_CHAIN (stub))
    if (function_name == STUB_FUNCTION_NAME (stub))
      return STUB_LABEL_NAME (stub);
  return 0;
}

/* INSN is either a function call or a millicode call.  It may have an
   unconditional jump in its delay slot.  

   CALL_DEST is the routine we are calling.  */

char *
output_call (insn, call_dest, operand_number, suffix)
     rtx insn;
     rtx call_dest;
     int operand_number;
     char *suffix;
{
  static char buf[256];
  const char *far_call_instr_str=NULL, *near_call_instr_str=NULL;
  rtx pattern;

  switch (GET_CODE (insn))
    {
    case CALL_INSN:
      far_call_instr_str = "jbsr";
      near_call_instr_str = "bl";
      pattern = NULL_RTX;
      break;
    case JUMP_INSN:
      far_call_instr_str = "jmp";
      near_call_instr_str = "b";
      pattern = NULL_RTX;
      break;
    case INSN:
      pattern = PATTERN (insn);
      break;
    default:
      abort();
      break;
    }

  if (GET_CODE (call_dest) == SYMBOL_REF && TARGET_LONG_BRANCH)
    {
      tree labelname;
      tree funname = get_identifier (XSTR (call_dest, 0));
      
      {
	static int warned = 0;
	if (flag_reorder_blocks_and_partition && !warned)
	  {
	    error ("-mlongcall and -freorder-blocks-and-partition not supported simultaneously");
	    warned = 1;
	  }
      }
	  
      /* This insn represents a prologue or epilogue.  */
      if ((pattern != NULL_RTX) && GET_CODE (pattern) == PARALLEL)
	{
	  rtx parallel_first_op = XVECEXP (pattern, 0, 0);
	  switch (GET_CODE (parallel_first_op))
	    {
	    case CLOBBER:	/* Prologue: a call to save_world.  */
	      far_call_instr_str = "jbsr";
	      near_call_instr_str = "bl";
	      break;
	    case RETURN:	/* Epilogue: a call to rest_world.  */
	      far_call_instr_str = "jmp";
	      near_call_instr_str = "b";
	      break;
	    default:
	      abort();
	      break;
	    }
	}

      if (no_previous_def (funname))
	{
	  int line_number = 0;
	  rtx label_rtx = gen_label_rtx ();
	  char *label_buf, temp_buf[256];
	  ASM_GENERATE_INTERNAL_LABEL (temp_buf, "L",
				       CODE_LABEL_NUMBER (label_rtx));
	  label_buf = temp_buf[0] == '*' ? temp_buf + 1 : temp_buf;
	  labelname = get_identifier (label_buf);
	  for (; insn && GET_CODE (insn) != NOTE; insn = PREV_INSN (insn));
	  if (insn)
	    line_number = NOTE_LINE_NUMBER (insn);
	  add_compiler_stub (labelname, funname, line_number);
	}
      else
	labelname = get_prev_label (funname);

      sprintf (buf, "%s %%z%d,%.246s", far_call_instr_str,
	       operand_number, IDENTIFIER_POINTER (labelname));
    }
  else
    sprintf (buf, "%s %%z%d", near_call_instr_str, operand_number);
  strcat (buf, suffix);
  return buf;
}

#endif /* RS6000_LONG_BRANCH */

/* Generate PIC and indirect symbol stubs.  */

void
machopic_output_stub (file, symb, stub)
     FILE *file;
     const char *symb, *stub;
{
  unsigned int length;
  char *symbol_name, *lazy_ptr_name;
  char *local_label_0;
  static int label = 0;

  /* Lose our funky encoding stuff so it doesn't contaminate the stub.  */
  symb = (*targetm.strip_name_encoding) (symb);

  label += 1;

  length = strlen (symb);
  symbol_name = alloca (length + 32);
  GEN_SYMBOL_NAME_FOR_SYMBOL (symbol_name, symb, length);

  lazy_ptr_name = alloca (length + 32);
  GEN_LAZY_PTR_NAME_FOR_SYMBOL (lazy_ptr_name, symb, length);

  local_label_0 = alloca (length + 32);
  GEN_LOCAL_LABEL_FOR_SYMBOL (local_label_0, symb, length,
			      local_label_unique_number);
  local_label_unique_number++;

  if (flag_pic == 2)
    machopic_picsymbol_stub1_section ();
  else
    machopic_symbol_stub1_section ();
  fprintf (file, "\t.align 2\n");

  fprintf (file, "%s:\n", stub);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);

  if (flag_pic == 2)
    {
      fprintf (file, "\tmflr r0\n");
      fprintf (file, "\tbcl 20,31,%s\n", local_label_0);
      fprintf (file, "%s:\n\tmflr r11\n", local_label_0);
      fprintf (file, "\taddis r11,r11,ha16(%s-%s)\n",
	       lazy_ptr_name, local_label_0);
      fprintf (file, "\tmtlr r0\n");
      fprintf (file, "\tlwzu r12,lo16(%s-%s)(r11)\n",
	       lazy_ptr_name, local_label_0);
      fprintf (file, "\tmtctr r12\n");
      fprintf (file, "\tbctr\n");
    }
  else
    /* APPLE LOCAL begin dynamic-no-pic  */
    {
      fprintf (file, "\tlis r11,ha16(%s)\n", lazy_ptr_name);
      fprintf (file, "\tlwzu r12,lo16(%s)(r11)\n", lazy_ptr_name);
      fprintf (file, "\tmtctr r12\n");
      fprintf (file, "\tbctr\n");
    }
    /* APPLE LOCAL end dynamic-no-pic  */
 
  machopic_lazy_symbol_ptr_section ();
  fprintf (file, "%s:\n", lazy_ptr_name);
  fprintf (file, "\t.indirect_symbol %s\n", symbol_name);
  fprintf (file, "\t.long dyld_stub_binding_helper\n");
}

/* Legitimize PIC addresses.  If the address is already
   position-independent, we return ORIG.  Newly generated
   position-independent addresses go into a reg.  This is REG if non
   zero, otherwise we allocate register(s) as necessary.  */

#define SMALL_INT(X) ((unsigned) (INTVAL(X) + 0x8000) < 0x10000)

rtx
rs6000_machopic_legitimize_pic_address (orig, mode, reg)
     rtx orig;
     enum machine_mode mode;
     rtx reg;
{
  rtx base, offset;

  if (reg == NULL && ! reload_in_progress && ! reload_completed)
    reg = gen_reg_rtx (Pmode);

  if (GET_CODE (orig) == CONST)
    {
      if (GET_CODE (XEXP (orig, 0)) == PLUS
	  && XEXP (XEXP (orig, 0), 0) == pic_offset_table_rtx)
	return orig;

      /* This pattern can arise when reloading labels with -mdynamic-no-pic. */
      if (MACHO_DYNAMIC_NO_PIC_P ()
	  && GET_CODE (XEXP (orig, 0)) == MINUS
	  && XEXP (XEXP (orig, 0), 1) == CONST0_RTX (Pmode)
	  && (GET_CODE (XEXP (XEXP (orig, 0), 0)) == SYMBOL_REF
	      || GET_CODE (XEXP (XEXP (orig, 0), 0)) == LABEL_REF))
	return machopic_legitimize_pic_address (XEXP (XEXP (orig, 0), 0), 
						mode, reg);

      if (GET_CODE (XEXP (orig, 0)) == PLUS)
	{
	  /* APPLE LOCAL use reg2 */
	  rtx reg2 = reg;
	  /* Use a different reg for the intermediate value, as
	     it will be marked UNCHANGING. */
	  if (!reload_in_progress && !reload_completed)
	    reg2 = gen_reg_rtx (Pmode);

	  base =
	    rs6000_machopic_legitimize_pic_address (XEXP (XEXP (orig, 0), 0),
						    Pmode, reg2);
	  offset =
	    rs6000_machopic_legitimize_pic_address (XEXP (XEXP (orig, 0), 1),
						    Pmode, reg);
	}
      else
	abort ();

      if (GET_CODE (offset) == CONST_INT)
	{
	  if (SMALL_INT (offset))
	    return plus_constant (base, INTVAL (offset));
	  else if (! reload_in_progress && ! reload_completed)
	    offset = force_reg (Pmode, offset);
	  else
	    {
 	      rtx mem = force_const_mem (Pmode, orig);
	      return machopic_legitimize_pic_address (mem, Pmode, reg);
	    }
	}
      return gen_rtx (PLUS, Pmode, base, offset);
    }

  /* Fall back on generic machopic code.  */
  return machopic_legitimize_pic_address (orig, mode, reg);
}

/* This is just a placeholder to make linking work without having to
   add this to the generic Darwin EXTRA_SECTIONS.  If -mcall-aix is
   ever needed for Darwin (not too likely!) this would have to get a
   real definition.  */

void
toc_section ()
{
}

#endif /* TARGET_MACHO */

/* APPLE LOCAL begin AltiVec */
int
decl_target_overloaded_intrinsic_p (NODE) 
     tree NODE;
{
  return (DECL_BUILT_IN(NODE)
	  && DECL_FUNCTION_CODE(NODE) >= BUILT_IN_FIRST_TARGET_OVERLOADED_INTRINSIC
	  && DECL_FUNCTION_CODE(NODE) <= BUILT_IN_LAST_TARGET_OVERLOADED_INTRINSIC);
}
/* APPLE LOCAL end AltiVec */

/* APPLE LOCAL begin Macintosh alignment 2002-1-22 ff */
/* Return the alignment of a struct based on the Macintosh PowerPC
   alignment rules.  In general the alignment of a struct is
   determined by the greatest alignment of its elements.  However, the
   PowerPC rules cause the alignment of a struct to peg at word
   alignment except when the first field has greater than word
   (32-bit) alignment, in which case the alignment is determined by
   the alignment of the first field.  */

unsigned
round_type_align (the_struct, computed, specified)
     tree the_struct;
     unsigned computed;
     unsigned specified;
{
  if (flag_altivec && TREE_CODE (the_struct) == VECTOR_TYPE)
    {
      /* All vectors are (at least) 16-byte aligned.  A struct or
	 union with a vector element is also 16-byte aligned.  */
      return MAX (RS6000_VECTOR_ALIGNMENT, MAX (computed, specified));
    }
  
  if (TREE_CODE (the_struct) == RECORD_TYPE
      || TREE_CODE (the_struct) == UNION_TYPE
      || TREE_CODE (the_struct) == QUAL_UNION_TYPE)
    {
      tree first_field = TYPE_FIELDS (the_struct);

      /* Skip past static fields, enums, and constant fields that are
         not really a part of the record layout.  */
      while ((first_field != 0)
             && (TREE_CODE (first_field) != FIELD_DECL))
	first_field = TREE_CHAIN (first_field);

      if (first_field != 0)
        {
	  /* If other-than-default alignment (which includes mac68k
	     mode) is in effect, then no adjustments to the alignment
	     should be necessary.  Ditto if the struct has the 
	     __packed__ attribute.  */
	  if (TYPE_PACKED (the_struct) || TARGET_ALIGN_MAC68K
	      || TARGET_ALIGN_NATURAL || maximum_field_alignment != 0)
	    /* Do nothing  */ ;
	  else
            {
              /* The following code handles Macintosh PowerPC
                 alignment.  The implementation is complicated by the
                 fact that BIGGEST_ALIGNMENT is 128 when AltiVec is
                 enabled and 32 when it is not.  So when AltiVec is
                 not enabled, alignment is generally limited to word
                 alignment.  Consequently, the alignment of unions has
                 to be recalculated if AltiVec is not enabled.
                 
                 Below we explicitly test for fields with greater than
                 word alignment: doubles, long longs, and structs and
                 arrays with greater than word alignment.  */
              unsigned val;
              tree field_type;

              val = MAX (computed, specified);

              if (TREE_CODE (the_struct) == UNION_TYPE && ! flag_altivec)
                {
                  tree field = first_field;
                  
                  while (field != 0)
                    {
                      /* Don't consider statics, enums and constant fields
                         which are not really a part of the record.  */
                      if (TREE_CODE (field) != FIELD_DECL)
                        {
                          field = TREE_CHAIN (field);
                          continue;
                        }
                      field_type = TREE_TYPE(field);
                      if (TREE_CODE (TREE_TYPE (field)) == ARRAY_TYPE)
                        field_type = get_inner_array_type (field);
                      else
                        field_type = TREE_TYPE (field);
                      val = MAX (TYPE_ALIGN (field_type), val);
                      if (FLOAT_TYPE_P (field_type)
				&& TYPE_MODE (field_type) == DFmode)
                        val = MAX (RS6000_DOUBLE_ALIGNMENT, val);
                      else if (INTEGRAL_TYPE_P (field_type)
				&& TYPE_MODE (field_type) == DImode)
                        val = MAX (RS6000_LONGLONG_ALIGNMENT, val);
                      field = TREE_CHAIN (field);
                    }
                }
              else
                {
                  if (TREE_CODE (TREE_TYPE (first_field)) == ARRAY_TYPE)
                    field_type = get_inner_array_type (first_field);
                  else
                    field_type = TREE_TYPE (first_field);
                  val = MAX (TYPE_ALIGN (field_type), val);

                  if (FLOAT_TYPE_P (field_type)
				&& TYPE_MODE (field_type) == DFmode)
                    val = MAX (RS6000_DOUBLE_ALIGNMENT, val);
                  else if (INTEGRAL_TYPE_P (field_type)
				&& TYPE_MODE (field_type) == DImode)
                    val = MAX (RS6000_LONGLONG_ALIGNMENT, val);
                }
              
	      return val;
            }
        }					/* first_field != 0  */

      /* Ensure all MAC68K structs are at least 16-bit aligned.
	 Unless the struct has __attribute__ ((packed)).  */

      if (TARGET_ALIGN_MAC68K && ! TYPE_PACKED (the_struct))
	{
	  if (computed < 16) 
	    computed = 16;
	}
    }						/* RECORD_TYPE, etc  */

  return (MAX (computed, specified));
}
/* APPLE LOCAL end Macintosh alignment 2002-1-22 ff */

#if TARGET_ELF
static unsigned int
rs6000_elf_section_type_flags (decl, name, reloc)
     tree decl;
     const char *name;
     int reloc;
{
  unsigned int flags
    = default_section_type_flags_1 (decl, name, reloc,
				    flag_pic || DEFAULT_ABI == ABI_AIX);

  if (TARGET_RELOCATABLE)
    flags |= SECTION_WRITE;

  return flags;
}

/* Record an element in the table of global constructors.  SYMBOL is
   a SYMBOL_REF of the function to be called; PRIORITY is a number
   between 0 and MAX_INIT_PRIORITY.

   This differs from default_named_section_asm_out_constructor in
   that we have special handling for -mrelocatable.  */

static void
rs6000_elf_asm_out_constructor (symbol, priority)
     rtx symbol;
     int priority;
{
  const char *section = ".ctors";
  char buf[16];

  if (priority != DEFAULT_INIT_PRIORITY)
    {
      sprintf (buf, ".ctors.%.5u",
               /* Invert the numbering so the linker puts us in the proper
                  order; constructors are run from right to left, and the
                  linker sorts in increasing order.  */
               MAX_INIT_PRIORITY - priority);
      section = buf;
    }

  named_section_flags (section, SECTION_WRITE);
  assemble_align (POINTER_SIZE);

  if (TARGET_RELOCATABLE)
    {
      fputs ("\t.long (", asm_out_file);
      output_addr_const (asm_out_file, symbol);
      fputs (")@fixup\n", asm_out_file);
    }
  else
    assemble_integer (symbol, POINTER_SIZE / BITS_PER_UNIT, POINTER_SIZE, 1);
}

static void
rs6000_elf_asm_out_destructor (symbol, priority)
     rtx symbol;
     int priority;
{
  const char *section = ".dtors";
  char buf[16];

  if (priority != DEFAULT_INIT_PRIORITY)
    {
      sprintf (buf, ".dtors.%.5u",
               /* Invert the numbering so the linker puts us in the proper
                  order; constructors are run from right to left, and the
                  linker sorts in increasing order.  */
               MAX_INIT_PRIORITY - priority);
      section = buf;
    }

  named_section_flags (section, SECTION_WRITE);
  assemble_align (POINTER_SIZE);

  if (TARGET_RELOCATABLE)
    {
      fputs ("\t.long (", asm_out_file);
      output_addr_const (asm_out_file, symbol);
      fputs (")@fixup\n", asm_out_file);
    }
  else
    assemble_integer (symbol, POINTER_SIZE / BITS_PER_UNIT, POINTER_SIZE, 1);
}
#endif

#if TARGET_XCOFF
static void
rs6000_xcoff_asm_globalize_label (stream, name)
     FILE *stream;
     const char *name;
{
  fputs (GLOBAL_ASM_OP, stream);
  RS6000_OUTPUT_BASENAME (stream, name);
  putc ('\n', stream);
}

static void
rs6000_xcoff_asm_named_section (name, flags)
     const char *name;
     unsigned int flags;
{
  int smclass;
  static const char * const suffix[3] = { "PR", "RO", "RW" };

  if (flags & SECTION_CODE)
    smclass = 0;
  else if (flags & SECTION_WRITE)
    smclass = 2;
  else
    smclass = 1;

  fprintf (asm_out_file, "\t.csect %s%s[%s],%u\n",
	   (flags & SECTION_CODE) ? "." : "",
	   name, suffix[smclass], flags & SECTION_ENTSIZE);
}

static void
rs6000_xcoff_select_section (decl, reloc, align)
     tree decl;
     int reloc;
     unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED;
{
  if (decl_readonly_section_1 (decl, reloc, 1))
    {
      if (TREE_PUBLIC (decl))
        read_only_data_section ();
      else
        read_only_private_data_section ();
    }
  else
    {
      if (TREE_PUBLIC (decl))
        data_section ();
      else
        private_data_section ();
    }
}

static void
rs6000_xcoff_unique_section (decl, reloc)
     tree decl;
     int reloc ATTRIBUTE_UNUSED;
{
  const char *name;

  /* Use select_section for private and uninitialized data.  */
  if (!TREE_PUBLIC (decl)
      || DECL_COMMON (decl)
      || DECL_INITIAL (decl) == NULL_TREE
      || DECL_INITIAL (decl) == error_mark_node
      || (flag_zero_initialized_in_bss
	  && initializer_zerop (DECL_INITIAL (decl))))
    return;

  name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
  name = (*targetm.strip_name_encoding) (name);
  DECL_SECTION_NAME (decl) = build_string (strlen (name), name);
}

/* Select section for constant in constant pool.

   On RS/6000, all constants are in the private read-only data area.
   However, if this is being placed in the TOC it must be output as a
   toc entry.  */

static void
rs6000_xcoff_select_rtx_section (mode, x, align)
     enum machine_mode mode;
     rtx x;
     unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED;
{
  if (ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (x, mode))
    toc_section ();
  else
    read_only_private_data_section ();
}

/* Remove any trailing [DS] or the like from the symbol name.  */

static const char *
rs6000_xcoff_strip_name_encoding (name)
     const char *name;
{
  size_t len;
  if (*name == '*')
    name++;
  len = strlen (name);
  if (name[len - 1] == ']')
    return ggc_alloc_string (name, len - 4);
  else
    return name;
}

/* Section attributes.  AIX is always PIC.  */

static unsigned int
rs6000_xcoff_section_type_flags (decl, name, reloc)
     tree decl;
     const char *name;
     int reloc;
{
  unsigned int align;
  unsigned int flags = default_section_type_flags_1 (decl, name, reloc, 1);

  /* Align to at least UNIT size.  */
  if (flags & SECTION_CODE)
    align = MIN_UNITS_PER_WORD;
  else
    /* Increase alignment of large objects if not already stricter.  */
    align = MAX ((DECL_ALIGN (decl) / BITS_PER_UNIT),
		 int_size_in_bytes (TREE_TYPE (decl)) > MIN_UNITS_PER_WORD
		 ? UNITS_PER_FP_WORD : MIN_UNITS_PER_WORD);

  return flags | (exact_log2 (align) & SECTION_ENTSIZE);
}

#endif /* TARGET_XCOFF */

/* Note that this is also used for PPC64 Linux.  */

static void
rs6000_xcoff_encode_section_info (decl, first)
     tree decl;
     int first ATTRIBUTE_UNUSED;
{
  if (TREE_CODE (decl) == FUNCTION_DECL
      && (*targetm.binds_local_p) (decl))
    SYMBOL_REF_FLAG (XEXP (DECL_RTL (decl), 0)) = 1;
}

/* Cross-module name binding.  For AIX and PPC64 Linux, which always are
   PIC, use private copy of flag_pic.  Darwin does not support overriding
   functions at dynamic-link time.  */

static bool
rs6000_binds_local_p (decl)
     tree decl;
{
  /* APPLE LOCAL FSF3.4 patch */
  return default_binds_local_p_1 (decl, 
	DEFAULT_ABI == ABI_DARWIN ? 0 : flag_pic || rs6000_flag_pic);
}

/* A C expression returning the cost of moving data from a register of class
   CLASS1 to one of CLASS2.  */

int
rs6000_register_move_cost (mode, from, to)
     enum machine_mode mode;
     enum reg_class from, to;
{
  /*  Moves from/to GENERAL_REGS.  */
  if (reg_classes_intersect_p (to, GENERAL_REGS)
      || reg_classes_intersect_p (from, GENERAL_REGS))
    {
      if (! reg_classes_intersect_p (to, GENERAL_REGS))
	from = to;

      if (from == FLOAT_REGS || from == ALTIVEC_REGS)
	return (rs6000_memory_move_cost (mode, from, 0)
		+ rs6000_memory_move_cost (mode, GENERAL_REGS, 0));

/* It's more expensive to move CR_REGS than CR0_REGS because of the shift...*/
      else if (from == CR_REGS)
	return 4;

      else
/* A move will cost one instruction per GPR moved.  */
	return 2 * HARD_REGNO_NREGS (0, mode);
    }

/* Moving between two similar registers is just one instruction.  */
  else if (reg_classes_intersect_p (to, from))
    return mode == TFmode ? 4 : 2;

/* Everything else has to go through GENERAL_REGS.  */
  else
    return (rs6000_register_move_cost (mode, GENERAL_REGS, to) 
	    + rs6000_register_move_cost (mode, from, GENERAL_REGS));
}

/* A C expressions returning the cost of moving data of MODE from a register to
   or from memory.  */

int
rs6000_memory_move_cost (mode, class, in)
  enum machine_mode mode;
  enum reg_class class;
  int in ATTRIBUTE_UNUSED;
{
  if (reg_classes_intersect_p (class, GENERAL_REGS))
    return 4 * HARD_REGNO_NREGS (0, mode);
  else if (reg_classes_intersect_p (class, FLOAT_REGS))
    return 4 * HARD_REGNO_NREGS (32, mode);
  else if (reg_classes_intersect_p (class, ALTIVEC_REGS))
    return 4 * HARD_REGNO_NREGS (FIRST_ALTIVEC_REGNO, mode);
  else
    return 4 + rs6000_register_move_cost (mode, class, GENERAL_REGS);
}

/* APPLE LOCAL begin CW asm blocks */
char *
rs6000_cw_asm_register_name (regname, buf)
     char *regname, *buf;
{
  /* SP is a valid reg name, but asm doesn't like it yet, so translate.  */
  if (strcmp (regname, "sp") == 0)
    return "r1";
  if (decode_reg_name (regname) >= 0)
    return regname;
  /* Change "gpr0" to "r0".  */
  if (regname[0] == 'g'
      && regname[1] == 'p'
      && decode_reg_name (regname + 2) >= 0)
    return regname + 2;
  /* Change "fp0" to "f0".  */
  if (regname[0] == 'f' && regname[1] == 'p')
    {
      buf[0] = 'f';
      strcpy (buf + 1, regname + 2);
      if (decode_reg_name (buf) >= 0)
	return buf;
    }
  if (regname[0] == 's'
      && regname[1] == 'p'
      && regname[2] == 'r'
      )
    /* Temp hack, return it as a number.  */
    return regname + 3;
  if (strcmp (regname, "RTOC") == 0)
    return "r2";
  return NULL;
}
/* APPLE LOCAL end CW asm blocks */

/* APPLE LOCAL begin inline floor */

/* The following function finds calls to "floor" in the instruction 
   sequence, and replaces the call instruction with a sequence of instructions
   that implements the following version of floor, effectively inlining
   the floor calls:

   floor (double x) {

     double t, y, f1, f2;

     __asm__("fctidz %0, %1":"=f"(t):"f"(x));
     __asm__("fcfid %0, %1":"=f"(y):"f"(t));
     if (x >= 0.0) {
       f2 = y - 1.0;
       f1 = x - y;
       --asm__("fsel %0, %1, %2, %3":"=f"(y):"f"(f1):"f"(y):"f"(f2));
     }
     return y;
   }

   NOTE: Because this code contains instructions specific to the 970
   architecture, this function is only called when we are running on
   that architecture.

*/

void
rs6000_inline_floor_calls ()
{
  rtx cur_insn;
  basic_block call_bb;
  basic_block cur_bb;
  rtx cur_bb_insn;
  rtx sub1;
  rtx stmt;
  rtx return_stmt;
  rtx x;
  rtx b;
  rtx c;
  rtx d;
  rtx e;
  rtx g;
  rtx h;
  rtx t;
  rtx out_reg;
  rtx twoTo52;
  rtx negTwoTo52;
  rtx one;
  rtx zero;
  int i;
  int call_has_return = 0;
  REAL_VALUE_TYPE dconst_twoTo52;
  REAL_VALUE_TYPE dconst_negTwoTo52;
  rtx asm_body3;
  rtx input_op;
  rtx asm_stmt3;
  rtx reg_2to52;
  rtx reg_neg2to52;

  typedef union big_int {
    long long whole_int;
    struct hi_lo {
      int high_part;
      int low_part;
    } hi_lo;
  } big_int;

  big_int twoTo52_const;
  big_int negTwoTo52_const;

  twoTo52_const.whole_int = 4503599627370496LL;
  negTwoTo52_const.whole_int = -4503599627370496LL;

  if (TARGET_POWERPC64)
    {
      /* Go through evey instruction in the current function, looking for
	 call instructions. */

      for (cur_insn = get_insns(); cur_insn; cur_insn = NEXT_INSN (cur_insn))
	if (GET_CODE (cur_insn) == CALL_INSN)
	  {
	    /* Pull apart call instruction to find name of function being 
	       called. */
	    
	    rtx body;
	    rtx call_rtx = NULL_RTX;
	    
	    body = PATTERN (cur_insn);
	    
	    if (GET_CODE (body) == CALL_PLACEHOLDER)
	      {
		rtx cp = body;
		rtx exp0 = XEXP(cp, 0);
		rtx exp1 = XEXP(cp, 1);
		rtx exp2 = XEXP(cp, 2);
		
		if (exp0 && (GET_CODE (exp0) == CALL_INSN))
		  body = exp0;
		else if (exp1 && (GET_CODE (exp1) == CALL_INSN))
		  body = exp1;
		else if (exp2 && (GET_CODE (exp2) == CALL_INSN))
		  body = exp2;
	      }
	    
	    if (GET_CODE (body) == SET
		&& GET_CODE (SET_SRC (body)) == CALL)
	      call_rtx = SET_SRC (body);
	    else if (GET_CODE (body) == PARALLEL
		     && GET_CODE (XVECEXP (body, 0, 0)) == SET
		     && GET_CODE (SET_SRC (XVECEXP (body, 0, 0))) == CALL)
	      {
		call_rtx = SET_SRC (XVECEXP (body, 0, 0));
		for (i = 0; i < XVECLEN (body, 0); i++)
		  {
		    if (GET_CODE (XVECEXP (body, 0, i)) == RETURN)
		      call_has_return = 1;
		  }
	      }
	    
	    if (call_rtx)
	      {
		/* Check to see if function being called in "floor".  If
		   so, proceed with inlining of call. */
		
		if ((GET_CODE (XEXP (call_rtx, 0)) == MEM)
		    && (GET_CODE (XEXP (XEXP (call_rtx, 0), 0))  == SYMBOL_REF)
		    && (strcmp (XSTR (XEXP (XEXP (call_rtx, 0), 0), 0),
				"&L_floor$stub") == 0))
		  {
		    /* Find the basic block containing this call. */

		    call_bb = NULL;
		    FOR_EACH_BB (cur_bb)
		      {
			for (cur_bb_insn = cur_bb->head; 
			     cur_bb_insn != cur_bb->end; 
			     cur_bb_insn = NEXT_INSN (cur_bb_insn))
			  if (cur_bb_insn == cur_insn)
			      call_bb = cur_bb;

			/* check last insn in bb */
			
			if (cur_bb_insn == cur_insn)
			  call_bb = cur_bb;

			if (call_bb)
			  break;
		      }

		    /* Generate registers to act as "local variables" */
		    
		    x = gen_rtx_REG (DFmode, 33);
		    b = gen_reg_rtx (DFmode);
		    c = gen_reg_rtx (DFmode);
		    d = gen_reg_rtx (DFmode);
		    e = gen_reg_rtx (DFmode);
		    g = gen_reg_rtx (DFmode);
		    h = gen_reg_rtx (DFmode);
		    t = gen_reg_rtx (DFmode);
		    reg_2to52 = gen_reg_rtx (DFmode);
		    reg_neg2to52 = gen_reg_rtx (DFmode);
		    out_reg = gen_rtx_REG (DFmode, 33);

		    /* Set up constants */

		    REAL_VALUE_FROM_INT (dconst_twoTo52, 
					 twoTo52_const.hi_lo.low_part,
					 twoTo52_const.hi_lo.high_part, DFmode);
		    REAL_VALUE_FROM_INT (dconst_negTwoTo52, 
					 negTwoTo52_const.hi_lo.low_part,
					 negTwoTo52_const.hi_lo.high_part,
					 DFmode);

		    twoTo52 = const_double_from_real_value (dconst_twoTo52,
							    DFmode);
		    negTwoTo52 = const_double_from_real_value (dconst_negTwoTo52,
							       DFmode);
		    one = const_double_from_real_value (dconst1, DFmode);
		    zero = const_double_from_real_value (dconst0, DFmode);


		    /* Put the large double constants into registers.  */

		    stmt = emit_insn_before (gen_rtx_SET (VOIDmode, reg_2to52,
							  twoTo52),
					     cur_insn);
		    stmt = emit_insn_after (gen_rtx_SET (VOIDmode, reg_neg2to52,
							 negTwoTo52),
					    stmt);

		    /*  c = fsel (x, -twoTo52, twoTo52)  */
		    
		    asm_body3 = gen_rtx_fmt_ssiEEsi (ASM_OPERANDS, DFmode,
						     "fsel %0, %1, %2, %3", "=f", 
						     0,
						     rtvec_alloc(3),
						     rtvec_alloc(3),
						     input_filename, lineno);
		    
		    input_op = gen_rtx_fmt_s (ASM_INPUT, DFmode, "f");
		    ASM_OPERANDS_INPUT_CONSTRAINT_EXP (asm_body3, 0) = input_op;
		    
		    input_op = gen_rtx_fmt_s (ASM_INPUT, DFmode, "f");
		    ASM_OPERANDS_INPUT_CONSTRAINT_EXP (asm_body3, 1) = input_op;
		    
		    input_op = gen_rtx_fmt_s (ASM_INPUT, DFmode, "f");
		    ASM_OPERANDS_INPUT_CONSTRAINT_EXP (asm_body3, 2) = input_op;
		    
		    ASM_OPERANDS_INPUT (asm_body3, 0) = x;
		    ASM_OPERANDS_INPUT (asm_body3, 1) = reg_neg2to52;
		    ASM_OPERANDS_INPUT (asm_body3, 2) = reg_2to52;
		    
		    asm_stmt3 = emit_insn_after (gen_rtx_SET (VOIDmode, c, 
							       asm_body3),
						 stmt);

		    /* b = fabs (x) */

		    stmt = emit_insn_after (gen_rtx_SET (VOIDmode, b, 
							 gen_rtx_ABS (DFmode, x)),
					    asm_stmt3);

		    /* d = (x - c) + c */


		    sub1 = emit_insn_after (gen_rtx_SET (VOIDmode, d,
							 gen_rtx_MINUS (DFmode, x,
									c)),
					    stmt);

		    stmt = emit_insn_after (gen_rtx_SET (VOIDmode, d,
							 gen_rtx_PLUS (DFmode, d,
								       c)), 
					    sub1);

		    /* e = b - twoTo52 */

		    stmt = emit_insn_after (gen_rtx_SET (VOIDmode, e,
							 gen_rtx_MINUS (DFmode, b,
									reg_2to52)),
					    stmt);

		    /* g = x - d */

		    stmt = emit_insn_after (gen_rtx_SET (VOIDmode, g,
							 gen_rtx_MINUS (DFmode, x,
									d)),
					    stmt);


		    /* h = fsel (g, 0.0, 1.0) */
		    
		    asm_body3 = gen_rtx_fmt_ssiEEsi (ASM_OPERANDS, DFmode,
						     "fsel %0, %1, %2, %3", "=f", 
						     0,
						     rtvec_alloc(3),
						     rtvec_alloc(3),
						     input_filename, lineno);
		    
		    input_op = gen_rtx_fmt_s (ASM_INPUT, DFmode, "f");
		    ASM_OPERANDS_INPUT_CONSTRAINT_EXP (asm_body3, 0) = input_op;
		    
		    input_op = gen_rtx_fmt_s (ASM_INPUT, DFmode, "f");
		    ASM_OPERANDS_INPUT_CONSTRAINT_EXP (asm_body3, 1) = input_op;
		    
		    input_op = gen_rtx_fmt_s (ASM_INPUT, DFmode, "f");
		    ASM_OPERANDS_INPUT_CONSTRAINT_EXP (asm_body3, 2) = input_op;
		    
		    ASM_OPERANDS_INPUT (asm_body3, 0) = g;
		    ASM_OPERANDS_INPUT (asm_body3, 1) = zero;
		    ASM_OPERANDS_INPUT (asm_body3, 2) = one;
		    
		    asm_stmt3 = emit_insn_after (gen_rtx_SET (VOIDmode, h, 
							       asm_body3),
						  stmt);

		    /* t = d - h */

		    stmt = emit_insn_after (gen_rtx_SET (VOIDmode, t,
							 gen_rtx_MINUS (DFmode, d,
									h)),
					    asm_stmt3);

		    /* out_reg = fsel (e, f, t) */
		    /* return out_reg  */

		    asm_body3 = gen_rtx_fmt_ssiEEsi (ASM_OPERANDS, DFmode,
						     "fsel %0, %1, %2, %3", "=f", 
						     0,
						     rtvec_alloc(3),
						     rtvec_alloc(3),
						     input_filename, lineno);
		    
		    input_op = gen_rtx_fmt_s (ASM_INPUT, DFmode, "f");
		    ASM_OPERANDS_INPUT_CONSTRAINT_EXP (asm_body3, 0) = input_op;
		    
		    input_op = gen_rtx_fmt_s (ASM_INPUT, DFmode, "f");
		    ASM_OPERANDS_INPUT_CONSTRAINT_EXP (asm_body3, 1) = input_op;
		    
		    input_op = gen_rtx_fmt_s (ASM_INPUT, DFmode, "f");
		    ASM_OPERANDS_INPUT_CONSTRAINT_EXP (asm_body3, 2) = input_op;
		    
		    ASM_OPERANDS_INPUT (asm_body3, 0) = e;
		    ASM_OPERANDS_INPUT (asm_body3, 1) = x;
		    ASM_OPERANDS_INPUT (asm_body3, 2) = t;
		    
		    return_stmt = emit_insn_after (gen_rtx_SET (VOIDmode, out_reg,
								asm_body3),
						  stmt);
		    
		    if (call_has_return)
		      emit_insn_after (gen_rtx_USE (VOIDmode, out_reg),
				       return_stmt);

		    /* Now all the inlined statements are in the
		       instruction chain; all that's left to do is to
		       remove the call to floor (cur_insn). */
		    
		    delete_insn (cur_insn);

		  }
	      }
	  }
    }
}
/* APPLE LOCAL end inline floor */

#include "gt-rs6000.h"
