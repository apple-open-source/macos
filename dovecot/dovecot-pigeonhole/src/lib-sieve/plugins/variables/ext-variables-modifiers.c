/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "unichar.h"

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-binary.h"

#include "ext-variables-common.h"
#include "ext-variables-modifiers.h"

#include <ctype.h>

/*
 * Core modifiers
 */
 
extern const struct sieve_variables_modifier_def lower_modifier;
extern const struct sieve_variables_modifier_def upper_modifier;
extern const struct sieve_variables_modifier_def lowerfirst_modifier;
extern const struct sieve_variables_modifier_def upperfirst_modifier;
extern const struct sieve_variables_modifier_def quotewildcard_modifier;
extern const struct sieve_variables_modifier_def length_modifier;

enum ext_variables_modifier_code {
    EXT_VARIABLES_MODIFIER_LOWER,
    EXT_VARIABLES_MODIFIER_UPPER,
    EXT_VARIABLES_MODIFIER_LOWERFIRST,
    EXT_VARIABLES_MODIFIER_UPPERFIRST,
    EXT_VARIABLES_MODIFIER_QUOTEWILDCARD,
    EXT_VARIABLES_MODIFIER_LENGTH
};

const struct sieve_variables_modifier_def *ext_variables_core_modifiers[] = {
	&lower_modifier,
	&upper_modifier,
	&lowerfirst_modifier,
	&upperfirst_modifier,
	&quotewildcard_modifier,
	&length_modifier
};

const unsigned int ext_variables_core_modifiers_count =
    N_ELEMENTS(ext_variables_core_modifiers);

#define ext_variables_modifier_name(modf) \
	(modf)->object->def->name
#define ext_variables_modifiers_equal(modf1, modf2) \
	( (modf1)->def == (modf2)->def )
#define ext_variables_modifiers_equal_precedence(modf1, modf2) \
	( (modf1)->def->precedence == (modf2)->def->precendence )

/*
 * Modifier registry
 */

void sieve_variables_modifier_register
(const struct sieve_extension *var_ext, struct sieve_validator *valdtr,
	const struct sieve_extension *ext,
	const struct sieve_variables_modifier_def *smodf_def) 
{
	struct ext_variables_validator_context *ctx = 
		ext_variables_validator_context_get(var_ext, valdtr);
	
	sieve_validator_object_registry_add(ctx->modifiers, ext, &smodf_def->obj_def);
}

bool ext_variables_modifier_exists
(const struct sieve_extension *var_ext, struct sieve_validator *valdtr,
	const char *identifier) 
{
	struct ext_variables_validator_context *ctx = 
		ext_variables_validator_context_get(var_ext, valdtr);

	return sieve_validator_object_registry_find(ctx->modifiers, identifier, NULL);
}

const struct sieve_variables_modifier *ext_variables_modifier_create_instance
(const struct sieve_extension *var_ext, struct sieve_validator *valdtr,
	struct sieve_command *cmd, const char *identifier) 
{
	struct ext_variables_validator_context *ctx = 
		ext_variables_validator_context_get(var_ext, valdtr);
	struct sieve_object object;
	struct sieve_variables_modifier *modf;
	pool_t pool;

	if ( !sieve_validator_object_registry_find
		(ctx->modifiers, identifier, &object) )
		return NULL;

	pool = sieve_command_pool(cmd);
	modf = p_new(pool, struct sieve_variables_modifier, 1);
	modf->object = object;
	modf->def = (const struct sieve_variables_modifier_def *) object.def;

  return modf;
}

void ext_variables_register_core_modifiers
(const struct sieve_extension *ext, struct ext_variables_validator_context *ctx)
{
	unsigned int i;
	
	/* Register core modifiers*/
	for ( i = 0; i < ext_variables_core_modifiers_count; i++ ) {
		sieve_validator_object_registry_add
			(ctx->modifiers, ext, &(ext_variables_core_modifiers[i]->obj_def));
	}
}

/*
 * Modifier coding
 */
 
const struct sieve_operand_class sieve_variables_modifier_operand_class = 
	{ "modifier" };
	
static const struct sieve_extension_objects core_modifiers =
	SIEVE_VARIABLES_DEFINE_MODIFIERS(ext_variables_core_modifiers);

const struct sieve_operand_def modifier_operand = { 
	"modifier", 
	&variables_extension,
	EXT_VARIABLES_OPERAND_MODIFIER, 
	&sieve_variables_modifier_operand_class,
	&core_modifiers
};

/* 
 * Core modifiers 
 */
 
/* Forward declarations */

bool mod_lower_modify(string_t *in, string_t **result);
bool mod_upper_modify(string_t *in, string_t **result);
bool mod_lowerfirst_modify(string_t *in, string_t **result);
bool mod_upperfirst_modify(string_t *in, string_t **result);
bool mod_length_modify(string_t *in, string_t **result);
bool mod_quotewildcard_modify(string_t *in, string_t **result);

/* Modifier objects */

const struct sieve_variables_modifier_def lower_modifier = {
	SIEVE_OBJECT("lower", &modifier_operand, EXT_VARIABLES_MODIFIER_LOWER),
	40,
	mod_lower_modify
};

const struct sieve_variables_modifier_def upper_modifier = {
	SIEVE_OBJECT("upper", &modifier_operand, EXT_VARIABLES_MODIFIER_UPPER),
	40,
	mod_upper_modify
};

const struct sieve_variables_modifier_def lowerfirst_modifier = {
	SIEVE_OBJECT
		("lowerfirst", &modifier_operand, EXT_VARIABLES_MODIFIER_LOWERFIRST),
	30,
	mod_lowerfirst_modify
};

const struct sieve_variables_modifier_def upperfirst_modifier = {
	SIEVE_OBJECT
		("upperfirst", &modifier_operand,	EXT_VARIABLES_MODIFIER_UPPERFIRST),
	30,
	mod_upperfirst_modify
};

const struct sieve_variables_modifier_def quotewildcard_modifier = {
	SIEVE_OBJECT
		("quotewildcard", &modifier_operand, EXT_VARIABLES_MODIFIER_QUOTEWILDCARD),
	20,
	mod_quotewildcard_modify
};

const struct sieve_variables_modifier_def length_modifier = {
	SIEVE_OBJECT("length", &modifier_operand, EXT_VARIABLES_MODIFIER_LENGTH),
	10,
	mod_length_modify
};

/* Modifier implementations */

bool mod_upperfirst_modify(string_t *in, string_t **result)
{
	char *content;
	
	*result = t_str_new(str_len(in));
	str_append_str(*result, in);
		
	content = str_c_modifiable(*result);
	content[0] = i_toupper(content[0]);

	return TRUE;
}

bool mod_lowerfirst_modify(string_t *in, string_t **result)
{
	char *content;
	
	*result = t_str_new(str_len(in));
	str_append_str(*result, in);
		
	content = str_c_modifiable(*result);
	content[0] = i_tolower(content[0]);

	return TRUE;
}

bool mod_upper_modify(string_t *in, string_t **result)
{
	char *content;
	
	*result = t_str_new(str_len(in));
	str_append_str(*result, in);

	content = str_c_modifiable(*result);
	content = str_ucase(content);
	
	return TRUE;
}

bool mod_lower_modify(string_t *in, string_t **result)
{
	char *content;
	
	*result = t_str_new(str_len(in));
	str_append_str(*result, in);

	content = str_c_modifiable(*result);
	content = str_lcase(content);

	return TRUE;
}

bool mod_length_modify(string_t *in, string_t **result)
{
	*result = t_str_new(64);
	str_printfa(*result, "%llu", (unsigned long long) 
		uni_utf8_strlen_n(str_data(in), str_len(in)));
	return TRUE;
}

bool mod_quotewildcard_modify(string_t *in, string_t **result)
{
	unsigned int i;
	const char *content;
	
	*result = t_str_new(str_len(in) * 2);
	content = (const char *) str_data(in);
	
	for ( i = 0; i < str_len(in); i++ ) {
		if ( content[i] == '*' || content[i] == '?' || content[i] == '\\' ) {
			str_append_c(*result, '\\');
		}
		str_append_c(*result, content[i]);
	}
	
	return TRUE;
}








