/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str-sanitize.h"

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-message.h"
#include "sieve-address.h"
#include "sieve-address-parts.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-match.h"

#include <stdio.h>

/* 
 * Address test
 *
 * Syntax:
 *    address [ADDRESS-PART] [COMPARATOR] [MATCH-TYPE]
 *       <header-list: string-list> <key-list: string-list>
 */

static bool tst_address_registered
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		struct sieve_command_registration *cmd_reg);
static bool tst_address_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool tst_address_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);

const struct sieve_command_def tst_address = { 
	"address", 
	SCT_TEST, 
	2, 0, FALSE, FALSE,
	tst_address_registered,
	NULL, 
	tst_address_validate,
	NULL, 
	tst_address_generate, 
	NULL 
};

/* 
 * Address operation 
 */

static bool tst_address_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int tst_address_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def tst_address_operation = { 
	"ADDRESS",
	NULL,
	SIEVE_OPERATION_ADDRESS,
	tst_address_operation_dump, 
	tst_address_operation_execute 
};

/* 
 * Test registration 
 */

static bool tst_address_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext ATTR_UNUSED,
	struct sieve_command_registration *cmd_reg) 
{
	/* The order of these is not significant */
	sieve_comparators_link_tag(valdtr, cmd_reg, SIEVE_AM_OPT_COMPARATOR );
	sieve_address_parts_link_tags(valdtr, cmd_reg, SIEVE_AM_OPT_ADDRESS_PART);
	sieve_match_types_link_tags(valdtr, cmd_reg, SIEVE_AM_OPT_MATCH_TYPE);

	return TRUE;
}

/* 
 * Validation 
 */
 
/* List of valid headers:
 *   Implementations MUST restrict the address test to headers that
 *   contain addresses, but MUST include at least From, To, Cc, Bcc,
 *   Sender, Resent-From, and Resent-To, and it SHOULD include any other
 *   header that utilizes an "address-list" structured header body.
 *
 * This list explicitly does not contain the envelope-to and return-path 
 * headers. The envelope test must be used to test against these addresses.
 *
 * FIXME: this restriction is somewhat odd. Sieve list advises to allow 
 *        any other header as long as its content matches the address-list
 *        grammar.
 */
static const char * const _allowed_headers[] = {
	/* Required */
	"from", "to", "cc", "bcc", "sender", "resent-from", "resent-to",

	/* Additional (RFC 822 / RFC 2822) */
	"reply-to", "resent-reply-to", "resent-sender", "resent-cc", "resent-bcc",  

	/* Non-standard (RFC 2076, draft-palme-mailext-headers-08.txt) */
	"for-approval", "for-handling", "for-comment", "apparently-to", "errors-to", 
	"delivered-to", "return-receipt-to", "x-admin", "read-receipt-to", 
	"x-confirm-reading-to", "return-receipt-requested", 
	"registered-mail-reply-requested-by", "mail-followup-to", "mail-reply-to",
	"abuse-reports-to", "x-complaints-to", "x-report-abuse-to",
	
	/* Undocumented */
	"x-beenthere",
	
	NULL  
};

static int _header_is_allowed
(void *context ATTR_UNUSED, struct sieve_ast_argument *arg)
{
	if ( sieve_argument_is_string_literal(arg) ) {
		const char *header = sieve_ast_strlist_strc(arg);

		const char * const *hdsp = _allowed_headers;
		while ( *hdsp != NULL ) {
			if ( strcasecmp( *hdsp, header ) == 0 ) 
				return TRUE;

			hdsp++;
		}
		
		return FALSE;
	}
	
	return TRUE;
}

static bool tst_address_validate
(struct sieve_validator *valdtr, struct sieve_command *tst) 
{
	struct sieve_ast_argument *arg = tst->first_positional;
	struct sieve_ast_argument *header;
	struct sieve_comparator cmp_default = 
		SIEVE_COMPARATOR_DEFAULT(i_ascii_casemap_comparator);
	struct sieve_match_type mcht_default = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
		
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "header list", 1, SAAT_STRING_LIST) ) {
		return FALSE;
	}
	
	if ( !sieve_validator_argument_activate(valdtr, tst, arg, FALSE) )
		return FALSE;

	if ( !sieve_command_verify_headers_argument(valdtr, arg) )
        return FALSE;

	/* Check if supplied header names are allowed
	 *   FIXME: verify dynamic header names at runtime 
	 */
	header = arg;
	if ( !sieve_ast_stringlist_map(&header, NULL, _header_is_allowed) ) {		
		sieve_argument_validate_error(valdtr, header, 
			"specified header '%s' is not allowed for the address test", 
			str_sanitize(sieve_ast_strlist_strc(header), 64));
		return FALSE;
	}

	/* Check key list */
	
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

static bool tst_address_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *tst) 
{
	sieve_operation_emit(cgenv->sblock, NULL, &tst_address_operation);
	
	/* Generate arguments */  	
	return sieve_generate_arguments(cgenv, tst, NULL);
}

/* 
 * Code dump 
 */

static bool tst_address_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "ADDRESS");
	sieve_code_descend(denv);
	
	/* Handle any optional arguments */
	if ( sieve_addrmatch_opr_optional_dump(denv, address, NULL) != 0 )
		return FALSE;

	return
		sieve_opr_stringlist_dump(denv, address, "header list") &&
		sieve_opr_stringlist_dump(denv, address, "key list");
}

/* 
 * Code execution 
 */

static int tst_address_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{	
	struct sieve_comparator cmp = 
		SIEVE_COMPARATOR_DEFAULT(i_ascii_casemap_comparator);
	struct sieve_match_type mcht = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	struct sieve_address_part addrp = 
		SIEVE_ADDRESS_PART_DEFAULT(all_address_part);
	struct sieve_stringlist *hdr_list, *hdr_value_list, *value_list, *key_list;
	struct sieve_address_list *addr_list;
	int match, ret;
	
	/* Read optional operands */
	if ( sieve_addrmatch_opr_optional_read
		(renv, address, NULL, &ret, &addrp, &mcht, &cmp) < 0 ) 
		return ret;
		
	/* Read header-list */
	if ( (ret=sieve_opr_stringlist_read(renv, address, "header-list", &hdr_list))
		<= 0 )
		return ret;

	/* Read key-list */
	if ( (ret=sieve_opr_stringlist_read(renv, address, "key-list", &key_list))
		<= 0 )
		return ret;

	sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, "address test");

	/* Create value stringlist */
	hdr_value_list = sieve_message_header_stringlist_create(renv, hdr_list, FALSE);
	addr_list = sieve_header_address_list_create(renv, hdr_value_list);
	value_list = sieve_address_part_stringlist_create(renv, &addrp, addr_list);

	/* Perform match */
	if ( (match=sieve_match(renv, &mcht, &cmp, value_list, key_list, &ret)) < 0 )	
		return ret;

	/* Set test result for subsequent conditional jump */
	sieve_interpreter_set_test_result(renv->interp, match > 0);
	return SIEVE_EXEC_OK;
}
