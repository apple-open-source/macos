/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * variable.h - handle jam multi-element variables
 *
 * 08/23/94 (seiwald) - Support for '+=' (append to variable)
 */

LIST *var_get();
void var_defines();
void var_set();
LIST *var_swap();
LIST *var_list();
int var_string();
void var_done();

/*
 * Defines for var_set().
 */

# define VAR_SET	0	/* override previous value */
# define VAR_APPEND	1	/* append to previous value */
# define VAR_DEFAULT	2	/* set only if no previous value */

#ifdef APPLE_EXTENSIONS

void var_export();
void var_setenv_all_exported_variables();

void var_set_deferred();
void var_commit_all_deferred_assignments();

#endif
