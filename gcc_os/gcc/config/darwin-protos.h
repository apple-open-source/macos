/* Prototypes.
   Copyright (C) 2001 Free Software Foundation, Inc.

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

extern int name_needs_quotes PARAMS ((const char *));

extern void machopic_validate_stub_or_non_lazy_ptr PARAMS ((const char *, int));

/* APPLE LOCAL begin fix prototypes  */
extern const char *machopic_function_base_name PARAMS ((void));
extern const char *machopic_non_lazy_ptr_name PARAMS ((const char*));
extern const char *machopic_stub_name PARAMS ((const char*)); 
extern int machopic_var_referred_to_p PARAMS ((const char*)); 
/* APPLE LOCAL end fix prototypes  */

extern void machopic_add_gc_roots PARAMS ((void));

extern void machopic_picsymbol_stub_section PARAMS ((void));
extern void machopic_symbol_stub_section PARAMS ((void));
extern void machopic_lazy_symbol_ptr_section PARAMS ((void));
extern void machopic_nl_symbol_ptr_section PARAMS ((void));

extern void constructor_section PARAMS ((void));
extern void destructor_section PARAMS ((void));
extern void mod_init_section PARAMS ((void));
extern void mod_term_section PARAMS ((void));

#ifdef RTX_CODE

extern int machopic_operand_p PARAMS ((rtx));
extern enum machopic_addr_class machopic_classify_name PARAMS ((const char*));

extern rtx machopic_indirect_call_target PARAMS ((rtx));
extern rtx machopic_legitimize_pic_address PARAMS ((rtx, enum machine_mode, rtx));

extern void machopic_asm_out_constructor PARAMS ((rtx, int));
extern void machopic_asm_out_destructor PARAMS ((rtx, int));

extern rtx machopic_indirect_data_reference PARAMS ((rtx, rtx));
#endif /* RTX_CODE */

#ifdef TREE_CODE

extern enum machopic_addr_class machopic_classify_ident PARAMS ((tree));
extern void machopic_define_ident PARAMS ((tree));
extern void machopic_define_name PARAMS ((const char*));
extern int machopic_name_defined_p PARAMS ((const char*));
extern int machopic_ident_defined_p PARAMS ((tree));
extern void darwin_encode_section_info PARAMS ((tree));

#endif /* TREE_CODE */

extern void machopic_finish PARAMS ((FILE *));

/* APPLE LOCAL C++ EH */
extern void darwin_asm_output_dwarf_delta PARAMS ((FILE *file, int size,
						   const char *lab1,
						   const char *lab2,
						   int force_reloc));

extern void darwin_exception_section PARAMS ((void));
extern void darwin_eh_frame_section PARAMS ((void));

#ifdef GCC_C_PRAGMA_H
extern void darwin_pragma_ignore PARAMS ((cpp_reader *));
extern void darwin_pragma_options PARAMS ((cpp_reader *));
/* APPLE LOCAL begin Macintosh alignment 2002-1-22 ff */
extern void darwin_pragma_pack PARAMS ((cpp_reader *));
/* APPLE LOCAL end Macintosh alignment 2002-1-22 ff */
extern void darwin_pragma_unused PARAMS ((cpp_reader *));
/* APPLE LOCAL begin CALL_ON_LOAD/CALL_ON_UNLOAD pragmas  20020202 turly  */
extern void darwin_pragma_call_on_load PARAMS ((cpp_reader *));
extern void darwin_pragma_call_on_unload PARAMS ((cpp_reader *));
/* APPLE LOCAL end CALL_ON_LOAD/CALL_ON_UNLOAD pragmas  20020202 turly  */
/* APPLE LOCAL begin CALL_ON_MODULE_BIND deprecated 2002-4-10 ff */
extern void darwin_pragma_call_on_module_bind PARAMS ((cpp_reader *));
/* APPLE LOCAL end CALL_ON_MODULE_BIND deprecated 2002-4-10 ff */
/* APPLE LOCAL begin temporary pragmas 2001-07-05 sts */
extern void darwin_pragma_cc_no_mach_text_sections PARAMS ((cpp_reader *));
extern void darwin_pragma_cc_opt_off PARAMS ((cpp_reader *));
extern void darwin_pragma_cc_opt_on PARAMS ((cpp_reader *));
extern void darwin_pragma_cc_opt_restore PARAMS ((cpp_reader *));
extern void darwin_pragma_cc_writable_strings PARAMS ((cpp_reader *));
extern void darwin_pragma_cc_non_writable_strings PARAMS ((cpp_reader *));
/* APPLE LOCAL end temporary pragmas 2001-07-05 sts */
#endif

/* APPLE LOCAL  named sections  */
extern void darwin_asm_named_section PARAMS ((const char *, unsigned int));
extern unsigned int darwin_section_type_flags PARAMS ((tree, const char *,
                                                           int));
extern int darwin_set_section_for_var_p PARAMS ((tree, int, int));

/* APPLE LOCAL  double destructor  */
extern tree darwin_handle_odd_attribute (tree *, tree, tree, int, bool *);

/* APPLE LOCAL PFE  */
#ifdef PFE
/* This is called to allocate or freeze/thaw target-specific additions to the
   pfe header.  The argument is a pointer to a target-defined field in the pfe
   header.  When the value in the field is NULL then, by definition, this is for
   creating the dump file.  The function should allocate space for any data
   (using pfe_malloc or pfe_calloc), do any appropriate initialization to this
   space, and save the pointer in *pp.  If the argument is not NULL the function
   is expected to freeze/thaw (depending on pfe_operation) the data pointed to
   by the argument as well as *pp itself (*pp is never NULL in this case).  Of
   course during thawing *pp will be the pointer set when the function was
   originally called with a NULL argument.  */
extern void darwin_pfe_freeze_thaw_target_additions PARAMS ((void *pp));

/* This is called by pfe_savestring() to determine whether strings should be
   treated  specially and not placed in PFE memory by pfe_savestring().  For
   darwin there are actually some strings in this category.  See comments
   for darwin_pfe_maybe_savestring() for details.  */
extern int  darwin_pfe_maybe_savestring PARAMS ((char *));

/* Called to check for consistent target-specific switches in pfe files.  */
extern void darwin_pfe_check_target_settings PARAMS ((void));
#endif
