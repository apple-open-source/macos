/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension envelope
 * ------------------
 *
 * Authors: Stephan Bosch
 * Specification: RFC 5228
 * Implementation: full
 * Status: testing
 *
 */

#include "lib.h"
#include "str-sanitize.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-address.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-address-parts.h"
#include "sieve-message.h"

#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-match.h"

/*
 * Forward declarations
 */

static const struct sieve_command_def envelope_test;
const struct sieve_operation_def envelope_operation;
const struct sieve_extension_def envelope_extension;

/* 
 * Extension 
 */

static bool ext_envelope_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr);

const struct sieve_extension_def envelope_extension = { 
	"envelope", 
	NULL, NULL,
	ext_envelope_validator_load, 
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_OPERATION(envelope_operation), 
	SIEVE_EXT_DEFINE_NO_OPERANDS 
};

static bool ext_envelope_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	/* Register new test */
	sieve_validator_register_command(valdtr, ext, &envelope_test);

	return TRUE;
}

/* 
 * Envelope test 
 *
 * Syntax
 *   envelope [COMPARATOR] [ADDRESS-PART] [MATCH-TYPE]
 *     <envelope-part: string-list> <key-list: string-list>   
 */

static bool tst_envelope_registered
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		struct sieve_command_registration *cmd_reg);
static bool tst_envelope_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool tst_envelope_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);

static const struct sieve_command_def envelope_test = { 
	"envelope", 
	SCT_TEST, 
	2, 0, FALSE, FALSE,
	tst_envelope_registered, 
	NULL,
	tst_envelope_validate,
	NULL, 
	tst_envelope_generate, 
	NULL 
};

/* 
 * Envelope operation 
 */

static bool ext_envelope_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int ext_envelope_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def envelope_operation = { 
	"ENVELOPE",
	&envelope_extension,
	0,
	ext_envelope_operation_dump, 
	ext_envelope_operation_execute 
};

/*
 * Envelope parts
 *
 * FIXME: not available to extensions
 */

struct sieve_envelope_part {
	const char *identifier;

	const struct sieve_address *const *(*get_addresses)
		(const struct sieve_runtime_env *renv);
	const char * const *(*get_values)
		(const struct sieve_runtime_env *renv);
};

static const struct sieve_address *const *_from_part_get_addresses
	(const struct sieve_runtime_env *renv);
static const char *const *_from_part_get_values
	(const struct sieve_runtime_env *renv);
static const struct sieve_address *const *_to_part_get_addresses
	(const struct sieve_runtime_env *renv);
static const char *const *_to_part_get_values
	(const struct sieve_runtime_env *renv);
static const char *const *_auth_part_get_values
	(const struct sieve_runtime_env *renv);

static const struct sieve_envelope_part _from_part = {
	"from",
	_from_part_get_addresses,
	_from_part_get_values,
};

static const struct sieve_envelope_part _to_part = {
	"to",
	_to_part_get_addresses,
	_to_part_get_values,
};	

static const struct sieve_envelope_part _auth_part = {
	"auth",
	NULL,
	_auth_part_get_values,
};	

static const struct sieve_envelope_part *_envelope_parts[] = {
	/* Required */
	&_from_part, &_to_part, 

	/* Non-standard */
	&_auth_part
};

static unsigned int _envelope_part_count = N_ELEMENTS(_envelope_parts);

static const struct sieve_envelope_part *_envelope_part_find
(const char *identifier)
{
	unsigned int i;

	for ( i = 0; i < _envelope_part_count; i++ ) {
		if ( strcasecmp( _envelope_parts[i]->identifier, identifier ) == 0 ) {
			return _envelope_parts[i];
        }
	}
	
	return NULL;
}

/* Envelope parts implementation */

static const struct sieve_address *const *_from_part_get_addresses
(const struct sieve_runtime_env *renv)
{
	ARRAY_DEFINE(envelope_values, const struct sieve_address *);
	const struct sieve_address *address =
		sieve_message_get_sender_address(renv->msgctx);
	
	if ( address != NULL ) {
		t_array_init(&envelope_values, 2);

        array_append(&envelope_values, &address, 1);

	    (void)array_append_space(&envelope_values);
    	return array_idx(&envelope_values, 0);
	} 

	return NULL;
}

static const char *const *_from_part_get_values
(const struct sieve_runtime_env *renv)
{
	ARRAY_DEFINE(envelope_values, const char *);

	t_array_init(&envelope_values, 2);

	if ( renv->msgdata->return_path != NULL ) {
        array_append(&envelope_values, &renv->msgdata->return_path, 1);
	}

	(void)array_append_space(&envelope_values);

	return array_idx(&envelope_values, 0);
}

static const struct sieve_address *const *_to_part_get_addresses
(const struct sieve_runtime_env *renv)
{
	ARRAY_DEFINE(envelope_values, const struct sieve_address *);
	const struct sieve_address *address = 
		sieve_message_get_orig_recipient_address(renv->msgctx);	

	if ( address != NULL && address->local_part != NULL ) {
		t_array_init(&envelope_values, 2);

		array_append(&envelope_values, &address, 1);

		(void)array_append_space(&envelope_values);
		return array_idx(&envelope_values, 0);
	}

	return NULL;
}

static const char *const *_to_part_get_values
(const struct sieve_runtime_env *renv)
{
	ARRAY_DEFINE(envelope_values, const char *);

	t_array_init(&envelope_values, 2);

	if ( renv->msgdata->orig_envelope_to != NULL ) {
        array_append(&envelope_values, &renv->msgdata->orig_envelope_to, 1);
	}

	(void)array_append_space(&envelope_values);

	return array_idx(&envelope_values, 0);
}

static const char *const *_auth_part_get_values
(const struct sieve_runtime_env *renv)
{
	ARRAY_DEFINE(envelope_values, const char *);

	t_array_init(&envelope_values, 2);

	if ( renv->msgdata->auth_user != NULL )
        array_append(&envelope_values, &renv->msgdata->auth_user, 1);

	(void)array_append_space(&envelope_values);

	return array_idx(&envelope_values, 0);
}

/*
 * Envelope address list
 */

/* Forward declarations */

static int sieve_envelope_address_list_next_string_item
	(struct sieve_stringlist *_strlist, string_t **str_r);
static int sieve_envelope_address_list_next_item
	(struct sieve_address_list *_addrlist, struct sieve_address *addr_r, 
		string_t **unparsed_r);
static void sieve_envelope_address_list_reset
	(struct sieve_stringlist *_strlist);

/* Stringlist object */

struct sieve_envelope_address_list {
	struct sieve_address_list addrlist;

	struct sieve_stringlist *env_parts;

	const struct sieve_address *const *cur_addresses;
	const char * const *cur_values;

	int value_index; 
};

static struct sieve_address_list *sieve_envelope_address_list_create
(const struct sieve_runtime_env *renv, struct sieve_stringlist *env_parts)
{
	struct sieve_envelope_address_list *addrlist;
	    
	addrlist = t_new(struct sieve_envelope_address_list, 1);
	addrlist->addrlist.strlist.runenv = renv;
	addrlist->addrlist.strlist.exec_status = SIEVE_EXEC_OK;
	addrlist->addrlist.strlist.next_item = 
		sieve_envelope_address_list_next_string_item;
	addrlist->addrlist.strlist.reset = sieve_envelope_address_list_reset;
	addrlist->addrlist.next_item = sieve_envelope_address_list_next_item;
	addrlist->env_parts = env_parts;
  
	return &addrlist->addrlist;
}

static int sieve_envelope_address_list_next_item
(struct sieve_address_list *_addrlist, struct sieve_address *addr_r, 
	string_t **unparsed_r)
{
	struct sieve_envelope_address_list *addrlist = 
		(struct sieve_envelope_address_list *) _addrlist;	
	const struct sieve_runtime_env *renv = _addrlist->strlist.runenv;

	if ( addr_r != NULL ) addr_r->local_part = NULL;
	if ( unparsed_r != NULL ) *unparsed_r = NULL;

	while ( addrlist->cur_addresses == NULL && addrlist->cur_values == NULL ) {
		const struct sieve_envelope_part *epart;
		string_t *envp_item = NULL;
		int ret;

		/* Read next header value from source list */
		if ( (ret=sieve_stringlist_next_item(addrlist->env_parts, &envp_item)) 
			<= 0 )
			return ret;

		if ( _addrlist->strlist.trace ) {
			sieve_runtime_trace(_addrlist->strlist.runenv, 0,
				"getting `%s' part from message envelope",
				str_sanitize(str_c(envp_item), 80));
		}
			
		if ( (epart=_envelope_part_find(str_c(envp_item))) != NULL ) {
			addrlist->value_index = 0;

			if ( epart->get_addresses != NULL ) {
				/* Field contains addresses */
				addrlist->cur_addresses = epart->get_addresses(renv);

				/* Drop empty list */
				if ( addrlist->cur_addresses != NULL &&
					addrlist->cur_addresses[0] == NULL )
					addrlist->cur_addresses = NULL;
			}  

			if ( addrlist->cur_addresses == NULL && epart->get_values != NULL ) {
				/* Field contains something else */
				addrlist->cur_values = epart->get_values(renv);

				/* Drop empty list */
				if ( addrlist->cur_values != NULL && addrlist->cur_values[0] == NULL )
					addrlist->cur_values = NULL;
			}
		}
	}
	
	/* Return next item */
	if ( addrlist->cur_addresses != NULL ) {
		const struct sieve_address *addr = 
			addrlist->cur_addresses[addrlist->value_index];

		if ( addr->local_part == NULL ) {
			/* Null path <> */
			if ( unparsed_r != NULL ) 
				*unparsed_r = t_str_new_const("", 0);
		} else {
			if ( addr_r != NULL )
				*addr_r = *addr;
		}

		/* Advance to next value */
		addrlist->value_index++;
		if ( addrlist->cur_addresses[addrlist->value_index] == NULL ) {
			addrlist->cur_addresses = NULL;
			addrlist->value_index = 0;
		}
	} else {
		if ( unparsed_r != NULL ) {
			const char *value = addrlist->cur_values[addrlist->value_index];

			*unparsed_r = t_str_new_const(value, strlen(value));
		}

		/* Advance to next value */
		addrlist->value_index++;
		if ( addrlist->cur_values[addrlist->value_index] == NULL ) {
			addrlist->cur_values = NULL;
			addrlist->value_index = 0;
		}
	}

	return 1;
}

static int sieve_envelope_address_list_next_string_item
(struct sieve_stringlist *_strlist, string_t **str_r)
{
	struct sieve_address_list *addrlist = (struct sieve_address_list *)_strlist;
	struct sieve_address addr;
	int ret;

	if ( (ret=sieve_envelope_address_list_next_item
		(addrlist, &addr, str_r)) <= 0 )
		return ret;

	if ( addr.local_part != NULL ) {
		const char *addr_str = sieve_address_to_string(&addr);
		*str_r = t_str_new_const(addr_str, strlen(addr_str));
	}

	return 1;
}

static void sieve_envelope_address_list_reset
(struct sieve_stringlist *_strlist)
{
	struct sieve_envelope_address_list *addrlist = 
		(struct sieve_envelope_address_list *)_strlist;

	sieve_stringlist_reset(addrlist->env_parts);
	addrlist->cur_addresses = NULL;
	addrlist->cur_values = NULL;
	addrlist->value_index = 0;
}

/* 
 * Command Registration 
 */

static bool tst_envelope_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext ATTR_UNUSED,
	struct sieve_command_registration *cmd_reg) 
{
	/* The order of these is not significant */
	sieve_comparators_link_tag(valdtr, cmd_reg, SIEVE_AM_OPT_COMPARATOR);
	sieve_address_parts_link_tags(valdtr, cmd_reg, SIEVE_AM_OPT_ADDRESS_PART);
	sieve_match_types_link_tags(valdtr, cmd_reg, SIEVE_AM_OPT_MATCH_TYPE);
	
	return TRUE;
}

/* 
 * Validation 
 */
 
static int _envelope_part_is_supported
(void *context, struct sieve_ast_argument *arg)
{
	const struct sieve_envelope_part **not_address =
		(const struct sieve_envelope_part **) context;

	if ( sieve_argument_is_string_literal(arg) ) {
		const struct sieve_envelope_part *epart;

		if ( (epart=_envelope_part_find(sieve_ast_strlist_strc(arg))) != NULL ) {
			if ( epart->get_addresses == NULL ) {
				if ( *not_address == NULL )
					*not_address = epart;
			}
					
			return TRUE;
		}
		
		return FALSE;
	} 
	
	return TRUE; /* Can't check at compile time */
}

static bool tst_envelope_validate
(struct sieve_validator *valdtr, struct sieve_command *tst) 
{ 		
	struct sieve_ast_argument *arg = tst->first_positional;
	struct sieve_ast_argument *epart;
	struct sieve_comparator cmp_default = 
		SIEVE_COMPARATOR_DEFAULT(i_ascii_casemap_comparator);
	struct sieve_match_type mcht_default = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	const struct sieve_envelope_part *not_address = NULL;
				
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "envelope part", 1, SAAT_STRING_LIST) ) {
		return FALSE;
	}
	
	if ( !sieve_validator_argument_activate(valdtr, tst, arg, FALSE) )
		return FALSE;
		
	/* Check whether supplied envelope parts are supported
	 *   FIXME: verify dynamic envelope parts at runtime 
	 */
	epart = arg;
	if ( !sieve_ast_stringlist_map(&epart, (void *) &not_address, 
		_envelope_part_is_supported) ) {		
		
		sieve_argument_validate_error(valdtr, epart, 
			"specified envelope part '%s' is not supported by the envelope test", 
				str_sanitize(sieve_ast_strlist_strc(epart), 64));
		return FALSE;
	}

	if ( not_address != NULL ) {
		struct sieve_ast_argument *addrp_arg = 
			sieve_command_find_argument(tst, &address_part_tag);

		if ( addrp_arg != NULL ) {
			sieve_argument_validate_error(valdtr, addrp_arg,
				"address part ':%s' specified while non-address envelope part '%s' "
				"is tested with the envelope test",
                sieve_ast_argument_tag(addrp_arg), not_address->identifier);
	        return FALSE;
		}
	}
	
	arg = sieve_ast_argument_next(arg);
	
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "key list", 2, SAAT_STRING_LIST) ) {
		return FALSE;
	}
	
	if ( !sieve_validator_argument_activate(valdtr, tst, arg, FALSE) )
		return FALSE;

	/* Validate the key argument to a specified match type */
	return sieve_match_type_validate
		(valdtr, tst, arg, &mcht_default, &cmp_default);
}

/*
 * Code generation
 */
 
static bool tst_envelope_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{
	(void)sieve_operation_emit(cgenv->sblock, cmd->ext, &envelope_operation);

	/* Generate arguments */
	if ( !sieve_generate_arguments(cgenv, cmd, NULL) )
		return FALSE;

	return TRUE;
}

/* 
 * Code dump 
 */
 
static bool ext_envelope_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "ENVELOPE");
	sieve_code_descend(denv);

	/* Handle any optional arguments */
	if ( sieve_addrmatch_opr_optional_dump(denv, address, NULL) != 0 )
		return FALSE;

	return
		sieve_opr_stringlist_dump(denv, address, "envelope part") &&
		sieve_opr_stringlist_dump(denv, address, "key list");
}

/*
 * Interpretation
 */

static int ext_envelope_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	struct sieve_comparator cmp = 
		SIEVE_COMPARATOR_DEFAULT(i_ascii_casemap_comparator);
	struct sieve_match_type mcht = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	struct sieve_address_part addrp = 
		SIEVE_ADDRESS_PART_DEFAULT(all_address_part);
	struct sieve_stringlist *env_part_list, *value_list, *key_list;
	struct sieve_address_list *addr_list;
	int match, ret;

	/*
	 * Read operands
	 */

	/* Read optional operands */
	if ( sieve_addrmatch_opr_optional_read
		(renv, address, NULL, &ret, &addrp, &mcht, &cmp) < 0 )
		return ret;

	/* Read envelope-part */
	if ( (ret=sieve_opr_stringlist_read
		(renv, address, "envelope-part", &env_part_list)) <= 0 )
		return ret;

	/* Read key-list */
	if ( (ret=sieve_opr_stringlist_read(renv, address, "key-list", &key_list))
		<= 0 )
		return ret;

	/* 
	 * Perform test
	 */

	sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, "envelope test");

	/* Create value stringlist */
	addr_list = sieve_envelope_address_list_create(renv, env_part_list);
	value_list = sieve_address_part_stringlist_create(renv, &addrp, addr_list);

	/* Perform match */
	if ( (match=sieve_match(renv, &mcht, &cmp, value_list, key_list, &ret)) < 0 )
		return ret;

	/* Set test result for subsequent conditional jump */
	sieve_interpreter_set_test_result(renv->interp, match > 0);
	return SIEVE_EXEC_OK;
}

