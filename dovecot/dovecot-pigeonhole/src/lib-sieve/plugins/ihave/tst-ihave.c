/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"

#include "ext-ihave-common.h"

/* 
 * Ihave test 
 *
 * Syntax:
 *   ihave <capabilities: string-list>
 */

static bool tst_ihave_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool tst_ihave_validate_const
	(struct sieve_validator *valdtr, struct sieve_command *tst,
		int *const_current, int const_next);

const struct sieve_command_def ihave_test = { 
	"ihave", 
	SCT_TEST, 
	1, 0, FALSE, FALSE,
	NULL, NULL,
	tst_ihave_validate,
	tst_ihave_validate_const,
	NULL, NULL
};

/*
 * Code validation
 */

static bool tst_ihave_validate
(struct sieve_validator *valdtr, struct sieve_command *tst)
{ 		
	struct _capability { 
		const struct sieve_extension *ext; 
		struct sieve_ast_argument *arg;
	};

	struct sieve_ast_argument *arg = tst->first_positional;
	struct sieve_ast_argument *stritem;
	ARRAY_DEFINE(capabilities, struct _capability);
	struct _capability capability;
	const struct _capability *caps;
	unsigned int i, count;
	bool all_known = TRUE;

	t_array_init(&capabilities, 64);

	tst->data = (void *) FALSE;

	/* Check stringlist argument */
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "capabilities", 1, SAAT_STRING_LIST) ) {
		return FALSE;
	}

	switch ( sieve_ast_argument_type(arg) ) {
	case SAAT_STRING:
		/* Single string */
		capability.arg = arg;
		capability.ext = sieve_extension_get_by_name
			(tst->ext->svinst, sieve_ast_argument_strc(arg));
		array_append(&capabilities, &capability, 1);

		if ( capability.ext == NULL ) {
			all_known = FALSE;

			ext_ihave_ast_add_missing_extension
				(tst->ext, tst->ast_node->ast, sieve_ast_argument_strc(arg));
		}

		break;

	case SAAT_STRING_LIST:
		/* String list */
		stritem = sieve_ast_strlist_first(arg);
		
		while ( stritem != NULL ) {
			capability.arg = stritem;
			capability.ext = sieve_extension_get_by_name
				(tst->ext->svinst, sieve_ast_argument_strc(stritem));
			array_append(&capabilities, &capability, 1);

			if ( capability.ext == NULL ) {
				all_known = FALSE;

				ext_ihave_ast_add_missing_extension
					(tst->ext, tst->ast_node->ast, sieve_ast_argument_strc(stritem));
			}
	
			stritem = sieve_ast_strlist_next(stritem);
		}

		break;
	default:
		i_unreached();
	}

	if ( !all_known )
		return TRUE;

	/* RFC 5463, Section 4, page 4:
	 *
	 * The "ihave" extension is designed to be used with other extensions
	 * that add tests, actions, comparators, or arguments.  Implementations
	 * MUST NOT allow it to be used with extensions that change the
	 * underlying Sieve grammar, or extensions like encoded-character
	 * [RFC5228], or variables [RFC5229] that change how the content of
	 * Sieve scripts are interpreted.  The test MUST fail and the extension
	 * MUST NOT be enabled if such usage is attempted.
	 *
	 * FIXME: current implementation of this restriction is hardcoded and
	 * therefore highly inflexible
	 */
	caps = array_get(&capabilities, &count);
	for ( i = 0; i < count; i++ ) {
		if ( sieve_extension_name_is(caps[i].ext, "variables") ||
			sieve_extension_name_is(caps[i].ext, "encoded-character") )
			return TRUE;
	}

	/* Load all extensions */
	caps = array_get(&capabilities, &count);
	for ( i = 0; i < count; i++ ) {
		if ( !sieve_validator_extension_load
			(valdtr, tst, caps[i].arg, caps[i].ext) )
			return FALSE;
	}

	tst->data = (void *) TRUE;
	return TRUE;	
}

static bool tst_ihave_validate_const
(struct sieve_validator *valdtr ATTR_UNUSED, struct sieve_command *tst,
	int *const_current, int const_next ATTR_UNUSED)
{
	if ( (bool)tst->data == TRUE )
		*const_current = 1;
	else
		*const_current = 0;
	return TRUE;
}
