/* APPLE LOCAL PFE */
/* Persistent Front End (PFE) for the GNU compiler.
   Copyright (C) 2001
   Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

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

#ifndef GCC_PFE_HEADER_H
#define GCC_PFE_HEADER_H

#ifdef PFE

/* The GENERATOR_FILE is used to suppress some includes in config.h
   such as insn-flags.h and insn-flags.h which we don't want nor 
   need for the files that use pfe-header.h.  Unfortunately this
   macro also controls an additional emum value in machmode.def
   which is used by machmode.h.  That enum determines the value
   of MAX_MACHINE_MODE which we reference in the pfe_compiler_state.
   Since most everything is built using the default build rules
   determined by the Makefile, and since those default rules do
   NOT define GENERATOR_FILE, then we need to keep machmode.h
   from generating that additional enum value and thus making
   MAX_MACHINE_MODE inconsistant.
   
   By undefining GENERATOR_FILE before we do out includes we
   suppress the extra enum value.  The machmode.h header is
   included by tree.h, rtl.h, and varray.h.  */
   
#undef GENERATOR_FILE

#include <stdio.h>
#include "tree.h"
#include "c-common.h"
#include "cpplib.h"
#include "cpphash.h"
#include "function.h"
#include "rtl.h"

/* Structs referenced as pointers.  */
struct eh_status;		/* except.c		*/
struct varasm_status;		/* varasm .c		*/
struct initial_value_struct;	/* except.c		*/
struct answer;			/* cpplib.c		*/
struct cpp_macro;		/* cppmacro.c		*/
struct cpp_hashnode;		/* cpplib.h		*/
struct cpp_token;		/* cpplib.h		*/
struct dbxout_stabs_t;		/* dbxout.c		*/
struct pfe_lang_compiler_state;	/* function of language */

/* List of all predefined names, i.e., global_binding_level->names
   before any source has been parsed.  This is set during a dump
   by compile_file() and saved in the dump file.  It is restored
   by a load and used to temporarily replace global_binding_level->names
   during compile_file()'s initialization.  That way the names list
   looks the same as it did during that time when it was dumped.
   If we didn't do this then global_binding_level->names would
   be a list of ALL globals (predefined plust the globals created
   for the dump file) since the previous dump was done at the end of
   compilation after all symbols have been seen.  */
extern tree pfe_predefined_global_names;

/* Possible languages supported by PFE.  This is needed in order
   to ensure that a loaded PFE header is for the expected language.
   Note that we cannot use enum c_lang because (a) it doesn't
   distinguish between objc and objc++ and it is not determined
   until a reader is created which is too late for our purposes.  */
enum pfe_lang {PFE_LANG_UNKNOWN = 0, PFE_LANG_C, PFE_LANG_CXX,
               PFE_LANG_OBJC, PFE_LANG_OBJCXX};

/* PFE state header.  This contains global variables whose state
   needs to be saved during a dump and restored during a load.
   These variables include pointers to the "roots" of the data
   structures used by the compiler.  */
struct pfe_compiler_state {
  /* The magic number helps to validate that a pre-compiled header file 
     is legitimate when performing a load.  */
  long magic_number;
  
  /* The format version number identifies the version of the compiler 
     and PFE code creating and reading a pre-compiled header file.  The 
     format number should be changed whenever a format incompatibility is 
     introduced relative to the last format.  */
  long format_version;
  
  /* Compiler version string. Use __APPLE_CC__ value but GNU GCC version
     number can also be used.  */
  char compiler_version[16];

#if PFE_NO_THAW_LOAD
  /* The default mode of operation is to assume that the pre-compiled
     header information will be loaded into an arbitrary location.
     Alternately, on some systems it may be possible to do the load
     at a specified address; if so, frozen pointers can be biased
     relative to this address, allowing the header information to be
     loaded without having to thaw the pointers.  The 
     pfe_assumed_load_address field will be zero in the normal case, 
     and non-zero when a particular load address has been assumed
     and used as a bias for pointer offsets in the load area.  */
  unsigned long pfe_assumed_load_address;
#endif

  /* The language we are preprocessing.  */
  enum pfe_lang lang;

  /* Name of the program which generated this PFE database. For example,
     'cc1', 'cc1plus' etc...  */
  const char *progname;
  
  /* List of predefind global names.  */
  tree pfe_predefined_global_names;

  /* root for the all the decls and other stuff...  */
  struct binding_level *global_binding_level;

  /* Idenfiter hash table */
  struct ht *ident_hash;

  /* Include header files  */
  struct ht *include_hash;
  
  /* Global trees (tree.c) */
  tree global_trees[TI_MAX];
  
  /* Global tree nodes to represent various integer types. (tree.c) */
  tree integer_types[itk_none];

  /* Global trees from c-common.h  */ 
  tree c_global_trees[CTI_MAX];
  
  /* Built in sizetype table.  */
  tree sizetype_tab[TYPE_KIND_LAST];
  
  /* __builtin_xxxx's from builtins.def  */
  /*const char *built_in_names[END_BUILTINS];*/
  tree built_in_decls[END_BUILTINS];
  
  /* rtl globals from rtl.h.  */
  rtx global_rtl[GR_MAX];
  rtx const_int_rtx[MAX_SAVED_CONST_INT * 2 + 1];
  rtx const_tiny_rtx[3][(int) MAX_MACHINE_MODE];
  rtx const_true_rtx;
  rtx return_address_pointer_rtx;
  rtx struct_value_rtx;
  rtx struct_value_incoming_rtx;
  rtx static_chain_rtx;
  rtx static_chain_incoming_rtx;
  rtx pic_offset_table_rtx;
  
  /* Globals from varasm.c.  */
  int const_labelno;
  int var_labelno;

  /* Globals from emit-rtl.c.  */
  int label_num;
  
  /* Garbage collector globals in ggc-page.c.  */
  /* Use anonomous pointer here.               */
  void *ggc_globals;
  
  /* Debug globals from dbxout.c.  */
  struct dbxout_stabs_t *dbxout_data_array;
  int dbxout_data_array_size;
  int dbxout_data_array_next;
  
  /* Is, or points to, target-specific additions to the pfe header.
     This is handled through the PFE_TARGET_ADDITIONS macro which,
     if defined, points to a target function that allocates the
     space this pointer points to and also freeze/thaw's the stuff
     it allocates.  The convention is when this field is NULL the
     PFE_TARGET_ADDITIONS function allocates the space for a dump.
     When it's not null it should freeze/thaw its data.  */
  void *pfe_target_additions;
 
  int cmd_ln_macro_count;
  /* Points to a structure whose details depend on the language in use.  */
  struct pfe_lang_compiler_state *lang_specific;
};

typedef struct pfe_compiler_state pfe_compiler_state;

/* PFE include header information.  This contains header name and 
   timestamp. It is used to validate pre-compiled header against
   the actual header.  */
struct pfe_include_header {
  struct ht_identifier include_name;	/* full pathname of include file.   */
  					/* Note: this must be 1st field.    */
  ino_t inode;				/* file's inode.		    */
  time_t timestamp;              	/* time of last data modification.  */
};

typedef struct pfe_include_header pfe_include_header;

/* Determine whether the include/import file has already been included
   in a pre-compiled header.  If it has already been load return 1, 
   else return 0.  */
extern int pfe_check_header             	 PARAMS ((const char *, time_t, ino_t));

/* Add the include/import in the list of headers included in current
   precompiled header.  */
extern void pfe_add_header_name         	 PARAMS ((const char *, time_t, ino_t));

/* Set the language for validating the load file.  */
extern void pfe_set_lang			 PARAMS ((enum pfe_lang));

/* Set the language for validating the load file.  */
extern void pfe_check_lang			 PARAMS ((enum pfe_lang));

/* Check the compiler settings to see if they are compatible with the current
   settings of the compiler.  */
extern void pfe_check_compiler			 PARAMS ((void));

/* Check command line macros.  */
extern void pfe_check_cmd_ln_macros        PARAMS ((void));

/* Freeze/thaw a pointer to and a cpp_token struct (defined in 
   cpplib.h).  */
extern void pfe_freeze_thaw_cpp_token 	 	 PARAMS ((struct cpp_token **));

/* Freeze/thaw a pointer to and a cpp_hashnode struct (defined in 
   cpplib.h).  */
extern void pfe_freeze_thaw_cpp_hashnode 	 PARAMS ((struct cpp_hashnode **));

/* The following routines are used for freeze/thawing specific structs.
   Some or all of these structs are defined privately, i.e., locally in
   the files that access them.  So these freeze/thaw routines are
   located in those files too.  */

/* cppmacro.c - freeze/thaw the a preprocessor macro.  */
extern void pfe_freeze_thaw_cpp_macro		 PARAMS ((struct cpp_macro **));

/* except.c - freeze/thaw eh_status as part of a struct function.  */
extern void pfe_freeze_thaw_eh_status            PARAMS ((struct eh_status **));

/* varasm.c - freeze/thaw varasm_status as part of a struct function.  */
extern void pfe_freeze_thaw_varasm_status        PARAMS ((struct varasm_status **));

/* integrate.c - freeze/thaw initial_value_struct as part of a struct function  */
extern void pfe_freeze_thaw_initial_value_struct PARAMS ((struct initial_value_struct **));

/* config/??? - freeze/thaw machine_function as part of a struct function.  */
extern void (*pfe_freeze_thaw_machine_status)    PARAMS ((struct machine_function **));

/* cpplib.c - freeze/thaw machine_function as part of a struct function.  */
extern void pfe_freeze_thaw_answer		 PARAMS ((struct answer **));

/* c-decl.c, cp/decl.c - set current_binding_level->names.  */
extern void pfe_setdecls			 PARAMS ((tree));

/* emit-rtl.c - freeze/thaw all globals.  */
extern void pfe_freeze_thaw_emitrtl		 PARAMS ((pfe_compiler_state *));

/* ggc-state.c - freeze/thaw GC globals.  */
extern void pfe_freeze_thaw_ggc			 PARAMS ((void *));

/* dbxout.c - freeze/thaw all debug data in the header.  */
extern void pfe_freeze_thaw_dbxout		 PARAMS ((pfe_compiler_state *));

/*-------------------------------------------------------------------*/
 
/* The following declarations are used to allow us to control a sizeof
   check for each struct we freeze/thaw.  We do this by a call to 
   pfe_check_all_struct_sizes() when -fpfedbg=check-structs is
   specified.  The reason for this is to attempt to verify that no
   fields of these structs are deleted or new ones added when each new
   merge is done with the fsf.
   
   Deleted fields probably will result in an compilation error if its a
   field we are freeze/thawing.  Additional fields could cause us to
   miss something that now needs freezeing/thawing if the addional
   field are pointers.  The sizeof check will pick these up.
   
   Of course if the size doesn't change, there still no guarantee
   hasn't changed.  But hopefully it will again cause a compile error
   if we reference the changed field in it's old form.
   
   There is a language hook for the language-dependent counterparts
   to pfe_check_all_struct_sizes().  There is one of these in each of
   our X-freeze-thaw.c files (X is 'c', "cp', or 'objc').  It controls
   checking of structs we only freeze/thaw for that languge.
   
   DEFINE_CHECK_STRUCT_FUNCTION is used to define each sizeof check
   function (each does a call to pfe_check_struct_size()).  They are
   placed in the same files that freeze/thaw the private structs they
   check.  Structs in accessible headers are generally done from
   DEFINE_CHECK_STRUCT_FUNCTION calls in the X-freeze-thaw.c files.  */
   
extern void pfe_check_all_struct_sizes PARAMS ((void));
extern void pfe_check_struct_size      PARAMS ((int, int, const char *));

#define DEFINE_CHECK_STRUCT_FUNCTION(name) 					\
void CONCAT2(check_struct_, name) PARAMS ((int));				\
void CONCAT2(check_struct_, name) (assumed_size)				\
     int assumed_size;								\
{										\
  pfe_check_struct_size (sizeof (struct name), assumed_size, STRINGX (name));	\
}
  
typedef void (*pfe_check_struct_t) PARAMS ((int));

/*-------------------------------------------------------------------*/

#endif /* PFE */
#endif /* GCC_PFE_HEADER_H */
