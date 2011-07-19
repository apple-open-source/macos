/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#include "lib.h"
#include "compat.h"
#include "mempool.h"
#include "hash.h"
#include "array.h"
#include "str-sanitize.h"

#include "sieve-extensions.h"
#include "sieve-code.h"
#include "sieve-address.h"
#include "sieve-commands.h"
#include "sieve-binary.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-match.h"

#include "sieve-address-parts.h"

#include <string.h>

/* 
 * Default address parts
 */

const struct sieve_address_part_def *sieve_core_address_parts[] = {
	&all_address_part, &local_address_part, &domain_address_part
};

const unsigned int sieve_core_address_parts_count = 
	N_ELEMENTS(sieve_core_address_parts);

/* 
 * Address-part 'extension' 
 */

static bool addrp_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *valdtr);

const struct sieve_extension_def address_part_extension = {
	"@address-parts",
	NULL, NULL,
	addrp_validator_load,
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_NO_OPERATIONS,
	SIEVE_EXT_DEFINE_NO_OPERANDS /* Defined as core operand */
};
	
/* 
 * Validator context:
 *   name-based address-part registry. 
 */

static struct sieve_validator_object_registry *_get_object_registry
(struct sieve_validator *valdtr)
{
	struct sieve_instance *svinst;
	const struct sieve_extension *adrp_ext;

	svinst = sieve_validator_svinst(valdtr);
	adrp_ext = sieve_get_address_part_extension(svinst);
	return sieve_validator_object_registry_get(valdtr, adrp_ext);
}
 
void sieve_address_part_register
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	const struct sieve_address_part_def *addrp_def) 
{
	struct sieve_validator_object_registry *regs = _get_object_registry(valdtr);
	
	sieve_validator_object_registry_add(regs, ext, &addrp_def->obj_def);
}

static bool sieve_address_part_exists
(struct sieve_validator *valdtr, const char *identifier) 
{
	struct sieve_validator_object_registry *regs = _get_object_registry(valdtr);

	return sieve_validator_object_registry_find(regs, identifier, NULL);
}

static const struct sieve_address_part *sieve_address_part_create_instance
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	const char *identifier) 
{
	struct sieve_validator_object_registry *regs = _get_object_registry(valdtr);
	struct sieve_object object;
	struct sieve_address_part *addrp;

	if ( !sieve_validator_object_registry_find(regs, identifier, &object) )
		return NULL;

	addrp = p_new(sieve_command_pool(cmd), struct sieve_address_part, 1);
	addrp->object = object;
	addrp->def = (const struct sieve_address_part_def *) object.def;

  return addrp;
}

static bool addrp_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	struct sieve_validator_object_registry *regs = 
		sieve_validator_object_registry_init(valdtr, ext);
	unsigned int i;

	/* Register core address-parts */
	for ( i = 0; i < sieve_core_address_parts_count; i++ ) {
		sieve_validator_object_registry_add
			(regs, NULL, &(sieve_core_address_parts[i]->obj_def));
	}

	return TRUE;
}

void sieve_address_parts_link_tags
(struct sieve_validator *valdtr, struct sieve_command_registration *cmd_reg,
	int id_code) 
{	
	struct sieve_instance *svinst;
	const struct sieve_extension *adrp_ext;

	svinst = sieve_validator_svinst(valdtr);
	adrp_ext = sieve_get_address_part_extension(svinst);

	sieve_validator_register_tag
		(valdtr, cmd_reg, adrp_ext, &address_part_tag, id_code); 	
}

/* 
 * Address-part tagged argument 
 */
 
/* Forward declarations */

static bool tag_address_part_is_instance_of
	(struct sieve_validator *valdtr, struct sieve_command *cmd,
		const struct sieve_extension *ext, const char *identifier, void **data);
static bool tag_address_part_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);
static bool tag_address_part_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
		struct sieve_command *cmd);

/* Argument object */

const struct sieve_argument_def address_part_tag = { 
	"ADDRESS-PART",
	tag_address_part_is_instance_of, 
	tag_address_part_validate,
	NULL, NULL,
	tag_address_part_generate 
};

/* Argument implementation */
  
static bool tag_address_part_is_instance_of
(struct sieve_validator *valdtr, struct sieve_command *cmd,
	const struct sieve_extension *ext ATTR_UNUSED, const char *identifier, 
	void **data)
{
	const struct sieve_address_part *addrp;

	if ( data == NULL )
		return sieve_address_part_exists(valdtr, identifier);

	if ( (addrp=sieve_address_part_create_instance
		(valdtr, cmd, identifier)) == NULL )
		return FALSE;
	
	*data = (void *) addrp;
	return TRUE;
}
 
static bool tag_address_part_validate
(struct sieve_validator *valdtr ATTR_UNUSED, struct sieve_ast_argument **arg, 
	struct sieve_command *cmd ATTR_UNUSED)
{
	/* NOTE: Currenly trivial, but might need to allow for further validation for
	 * future extensions.
	 */

	/* Syntax:
	 *   ":localpart" / ":domain" / ":all" (subject to extension)
   	 */

	/* Skip tag */
	*arg = sieve_ast_argument_next(*arg);

	return TRUE;
}

static bool tag_address_part_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *cmd ATTR_UNUSED)
{
	struct sieve_address_part *addrp =
		(struct sieve_address_part *) arg->argument->data;
		
	sieve_opr_address_part_emit(cgenv->sblock, addrp); 
		
	return TRUE;
}

/*
 * Address-part operand
 */
 
const struct sieve_operand_class sieve_address_part_operand_class = 
	{ "address part" };

static const struct sieve_extension_objects core_address_parts =
	SIEVE_EXT_DEFINE_MATCH_TYPES(sieve_core_address_parts);

const struct sieve_operand_def address_part_operand = { 
	"address-part", 
	NULL, SIEVE_OPERAND_ADDRESS_PART,
	&sieve_address_part_operand_class,
	&core_address_parts
};

/*
 * Address-part string list
 */

static int sieve_address_part_stringlist_next_item
	(struct sieve_stringlist *_strlist, string_t **str_r);
static void sieve_address_part_stringlist_reset
	(struct sieve_stringlist *_strlist);
static int sieve_address_part_stringlist_get_length
	(struct sieve_stringlist *_strlist);
static void sieve_address_part_stringlist_set_trace
(struct sieve_stringlist *_strlist, bool trace);

struct sieve_address_part_stringlist {
	struct sieve_stringlist strlist;

	const struct sieve_address_part *addrp;
	struct sieve_address_list *addresses;
};

struct sieve_stringlist *sieve_address_part_stringlist_create
(const struct sieve_runtime_env *renv, const struct sieve_address_part *addrp,
	struct sieve_address_list *addresses)
{
	struct sieve_address_part_stringlist *strlist;

	strlist = t_new(struct sieve_address_part_stringlist, 1);
	strlist->strlist.runenv = renv;
	strlist->strlist.next_item = sieve_address_part_stringlist_next_item;
	strlist->strlist.reset = sieve_address_part_stringlist_reset;
	strlist->strlist.get_length = sieve_address_part_stringlist_get_length;
	strlist->strlist.set_trace = sieve_address_part_stringlist_set_trace;

	strlist->addrp = addrp;
	strlist->addresses = addresses;

	return &strlist->strlist;
}

static int sieve_address_part_stringlist_next_item
	(struct sieve_stringlist *_strlist, string_t **str_r)
{
	struct sieve_address_part_stringlist *strlist = 
		(struct sieve_address_part_stringlist *)_strlist;
	struct sieve_address item;
	string_t *item_unparsed;
	int ret;

	*str_r = NULL;

	while ( *str_r == NULL ) {
		if ( (ret=sieve_address_list_next_item
			(strlist->addresses, &item, &item_unparsed)) <= 0 )
			return ret;
		
		if ( item.local_part == NULL ) {
			if ( item_unparsed != NULL ) {
				if ( _strlist->trace ) {
					sieve_runtime_trace(_strlist->runenv, 0,
						"extracting `%s' part from non-address value `%s'",
						sieve_address_part_name(strlist->addrp),
						str_sanitize(str_c(item_unparsed), 80));
				}

				if ( str_len(item_unparsed) == 0 ||
					sieve_address_part_is(strlist->addrp, all_address_part) )
					*str_r = item_unparsed;
			}
		} else {
			const struct sieve_address_part *addrp = strlist->addrp;
			const char *part = NULL;

			if ( _strlist->trace ) {
				sieve_runtime_trace(_strlist->runenv, 0,
					"extracting `%s' part from address `%s'",
					sieve_address_part_name(strlist->addrp),
					str_sanitize(sieve_address_to_string(&item), 80));
			}

			if ( addrp->def != NULL && addrp->def->extract_from ) 
				part = addrp->def->extract_from(addrp, &item);

			if ( part != NULL )
				*str_r = t_str_new_const(part, strlen(part));
		}
	}
		
	return 1;
}

static void sieve_address_part_stringlist_reset
	(struct sieve_stringlist *_strlist)
{
	struct sieve_address_part_stringlist *strlist = 
		(struct sieve_address_part_stringlist *)_strlist;

	sieve_address_list_reset(strlist->addresses);
}

static int sieve_address_part_stringlist_get_length
	(struct sieve_stringlist *_strlist)
{
	struct sieve_address_part_stringlist *strlist = 
		(struct sieve_address_part_stringlist *)_strlist;

	return sieve_address_list_get_length(strlist->addresses);
}

static void sieve_address_part_stringlist_set_trace
(struct sieve_stringlist *_strlist, bool trace)
{
	struct sieve_address_part_stringlist *strlist = 
		(struct sieve_address_part_stringlist *)_strlist;

	sieve_address_list_set_trace(strlist->addresses, trace);
}

/* 
 * Default ADDRESS-PART, MATCH-TYPE, COMPARATOR access
 */
 
int sieve_addrmatch_opr_optional_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address, 
	signed int *opt_code) 
{
	signed int _opt_code = 0;
	bool final = FALSE, opok = TRUE;

	if ( opt_code == NULL ) {
		opt_code = &_opt_code;
		final = TRUE;
	}

	while ( opok ) {
		int opt;

		if ( (opt=sieve_opr_optional_dump(denv, address, opt_code)) <= 0 )
			return opt;

		switch ( *opt_code ) {
		case SIEVE_AM_OPT_COMPARATOR:
			opok = sieve_opr_comparator_dump(denv, address);
			break;
		case SIEVE_AM_OPT_MATCH_TYPE:
			opok = sieve_opr_match_type_dump(denv, address);
			break;
		case SIEVE_AM_OPT_ADDRESS_PART:
			opok = sieve_opr_address_part_dump(denv, address);
			break;
		default:
			return ( final ? -1 : 1 );
		}
	}

	return -1;
}

int sieve_addrmatch_opr_optional_read
(const struct sieve_runtime_env *renv, sieve_size_t *address,
	signed int *opt_code, int *exec_status, struct sieve_address_part *addrp,
	struct sieve_match_type *mtch, struct sieve_comparator *cmp) 
{
	signed int _opt_code = 0;
	bool final = FALSE;
	int status = SIEVE_EXEC_OK;

	if ( opt_code == NULL ) {
		opt_code = &_opt_code;
		final = TRUE;
	}

	if ( exec_status != NULL )
		*exec_status = SIEVE_EXEC_OK;			

	while ( status == SIEVE_EXEC_OK ) {
		int opt;

		if ( (opt=sieve_opr_optional_read(renv, address, opt_code)) <= 0 ){
			if ( opt < 0 && exec_status != NULL )
				*exec_status = SIEVE_EXEC_BIN_CORRUPT;				
			return opt;
		}

		switch ( *opt_code ) {
		case SIEVE_AM_OPT_COMPARATOR:
			status = sieve_opr_comparator_read(renv, address, cmp);
			break;
		case SIEVE_AM_OPT_MATCH_TYPE:
			status = sieve_opr_match_type_read(renv, address, mtch);
			break;
		case SIEVE_AM_OPT_ADDRESS_PART:
			status = sieve_opr_address_part_read(renv, address, addrp);
			break;
		default:
			if ( final ) {
				sieve_runtime_trace_error(renv, "invalid optional operand");
				if ( exec_status != NULL )
					*exec_status = SIEVE_EXEC_BIN_CORRUPT;
				return -1;
			}
			return 1;
		}
	}

	if ( exec_status != NULL )
		*exec_status = status;
	return -1;
}

/* 
 * Core address-part modifiers
 */
 
static const char *addrp_all_extract_from
(const struct sieve_address_part *addrp ATTR_UNUSED, 
	const struct sieve_address *address)
{
	const char *local_part = address->local_part;
	const char *domain = address->domain;

	if ( domain == NULL ) {
		return local_part;
	}

	if ( local_part == NULL ) {
		return NULL;
	}

	return t_strconcat(local_part, "@", domain, NULL);
}

static const char *addrp_domain_extract_from
(const struct sieve_address_part *addrp ATTR_UNUSED,
	const struct sieve_address *address)
{
	return address->domain;
}

static const char *addrp_localpart_extract_from
(const struct sieve_address_part *addrp ATTR_UNUSED,
	const struct sieve_address *address)
{
	return address->local_part;
}

const struct sieve_address_part_def all_address_part = {
	SIEVE_OBJECT("all", &address_part_operand, SIEVE_ADDRESS_PART_ALL),
	addrp_all_extract_from
};

const struct sieve_address_part_def local_address_part = {
	SIEVE_OBJECT("localpart", &address_part_operand, SIEVE_ADDRESS_PART_LOCAL),
	addrp_localpart_extract_from
};

const struct sieve_address_part_def domain_address_part = {
	SIEVE_OBJECT("domain", &address_part_operand,	SIEVE_ADDRESS_PART_DOMAIN),
	addrp_domain_extract_from
};

