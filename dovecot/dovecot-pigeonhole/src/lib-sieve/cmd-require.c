/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-extensions.h"
#include "sieve-validator.h" 
#include "sieve-generator.h"

/* 
 * Require command
 *
 * Syntax 
 *   Syntax: require <capabilities: string-list>
 */

static bool cmd_require_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);

const struct sieve_command_def cmd_require = { 
	"require", 
	SCT_COMMAND, 
	1, 0, FALSE, FALSE,
	NULL, NULL, 
	cmd_require_validate, 
	NULL, NULL, NULL
};
 
/* 
 * Validation 
 */

static bool cmd_require_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd) 
{
	bool result = TRUE;
	struct sieve_ast_argument *arg;
	struct sieve_command *prev = sieve_command_prev(cmd);
	
	/* Check valid command placement */
	if ( !sieve_command_is_toplevel(cmd) ||
		( !sieve_command_is_first(cmd) && prev != NULL &&
			!sieve_command_is(prev, cmd_require) ) ) 
	{	
		sieve_command_validate_error(valdtr, cmd, 
			"require commands can only be placed at top level "
			"at the beginning of the file");
		return FALSE;
	}
	
	/* Check argument and load specified extension(s) */

	arg = cmd->first_positional;
	if ( sieve_ast_argument_type(arg) == SAAT_STRING ) {
		/* Single string */
		const struct sieve_extension *ext = sieve_validator_extension_load_by_name
			(valdtr, cmd, arg, sieve_ast_argument_strc(arg));	

		if ( ext == NULL ) result = FALSE;
		
	} else if ( sieve_ast_argument_type(arg) == SAAT_STRING_LIST ) {
		/* String list */
		struct sieve_ast_argument *stritem = sieve_ast_strlist_first(arg);
		
		while ( stritem != NULL ) {
			const struct sieve_extension *ext = sieve_validator_extension_load_by_name
				(valdtr, cmd, stritem, sieve_ast_strlist_strc(stritem));

			if ( ext == NULL ) result = FALSE;
	
			stritem = sieve_ast_strlist_next(stritem);
		}
	} else {
		/* Something else */
		sieve_argument_validate_error(valdtr, arg, 
			"the require command accepts a single string or string list argument, "
			"but %s was found", 
			sieve_ast_argument_name(arg));
		return FALSE;
	}
	 
	return result;
}
