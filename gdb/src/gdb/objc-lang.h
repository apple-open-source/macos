/* Objective-C language support definitions for GDB, the GNU debugger.

   Copyright 1992, 2005 Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#if !defined(OBJC_LANG_H)
#define OBJC_LANG_H

struct stoken;

struct value;
struct block;

extern int objc_parse (void);		/* Defined in c-exp.y */

extern void objc_error (char *);	/* Defined in c-exp.y */

extern CORE_ADDR lookup_objc_class     (char *classname);
extern CORE_ADDR lookup_child_selector (char *methodname);

extern char *objc_demangle (const char *mangled, int options);

extern int find_objc_msgcall (CORE_ADDR pc, CORE_ADDR *new_pc);

void tell_objc_msgsend_cacher_objfile_changed (struct objfile *);

void objc_clear_caches ();

CORE_ADDR find_implementation (CORE_ADDR object, CORE_ADDR sel, int stret);

extern char *parse_selector (char *method, char **selector);

extern char *parse_method (char *method, char *type, 
			   char **class, char **category, 
			   char **selector);

extern char *find_imps (struct symtab *symtab, struct block *block,
			char *method, struct symbol **syms, 
			unsigned int *nsym, unsigned int *ndebug);

extern struct value *value_nsstring (char *ptr, int len);

/* APPLE LOCAL: I needed this bit to decorate up the gc-roots command output.  */
extern struct type *objc_target_type_from_object (CORE_ADDR isa_addr,
						  struct block *block, int addrsize,
						  char **class_name);

extern struct type *value_objc_target_type (struct value *, struct block *, char **);
int should_lookup_objc_class ();

/* for parsing Objective C */
extern void start_msglist (void);
extern void add_msglist (struct stoken *str, int addcolon);
extern int end_msglist (void);

struct symbol *lookup_struct_typedef (char *name, struct block *block,
				      int noerr);

/* APPLE LOCAL: We need to control this down in macosx/ when debugging
   translated programs.  */
extern int lookup_objc_class_p;

extern int objc_handle_update (CORE_ADDR stop_addr);
void objc_init_trampoline_observer ();
int pc_in_objc_trampoline_p (CORE_ADDR pc, uint32_t *flags);

void objc_invalidate_objc_class (struct type *type);

/* APPLE LOCAL Disable breakpoints while updating data formatters.  */
extern int is_objc_exception_throw_breakpoint (struct breakpoint *);

enum objc_debugger_mode_result
  {
    objc_debugger_mode_unknown,
    objc_debugger_mode_success,
    objc_debugger_mode_fail_unable_to_enter_debug_mode,
    objc_debugger_mode_fail_spinlock_held,
    objc_debugger_mode_fail_malloc_lock_held,
    objc_debugger_mode_fail_objc_api_unavailable
  };

enum objc_debugger_mode_result make_cleanup_set_restore_debugger_mode (struct cleanup **cleanup, int level);

enum objc_handcall_fail_reasons
  {
    objc_no_fail,
    objc_exception_thrown,
    objc_debugger_mode_fail
  };

enum objc_handcall_fail_reasons objc_pc_at_fail_point (CORE_ADDR pc);
struct cleanup *make_cleanup_init_objc_exception_catcher (void);
void reinitialize_objc ();
int get_objc_runtime_check_level ();
#endif
