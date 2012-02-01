/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-extensions.h"

#include "sieve-code.h"
#include "sieve-ast.h"
#include "sieve-commands.h"
#include "sieve-binary.h"

#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "ext-variables-common.h"
#include "ext-variables-limits.h"
#include "ext-variables-modifiers.h"

/* 
 * Set command 
 *	
 * Syntax: 
 *    set [MODIFIER] <name: string> <value: string>
 */

static bool cmd_set_registered
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		struct sieve_command_registration *cmd_reg);
static bool cmd_set_pre_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_set_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_set_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);

const struct sieve_command_def cmd_set = { 
	"set",
	SCT_COMMAND, 
	2, 0, FALSE, FALSE, 
	cmd_set_registered,
	cmd_set_pre_validate,  
	cmd_set_validate,
	NULL, 
	cmd_set_generate, 
	NULL 
};

/* 
 * Set operation 
 */

static bool cmd_set_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_set_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def cmd_set_operation = { 
	"SET",
	&variables_extension,
	EXT_VARIABLES_OPERATION_SET,
	cmd_set_operation_dump, 
	cmd_set_operation_execute
};

/* 
 * Compiler context 
 */

struct cmd_set_context {
	ARRAY_DEFINE(modifiers, const struct sieve_variables_modifier *);
};

/* 
 * Set modifier tag
 *
 * [MODIFIER]:
 *   ":lower" / ":upper" / ":lowerfirst" / ":upperfirst" /
 *             ":quotewildcard" / ":length"
 */

/* Forward declarations */
 
static bool tag_modifier_is_instance_of
	(struct sieve_validator *valdtr, struct sieve_command *cmd,
		const struct sieve_extension *ext, const char *identifier, void **context);
static bool tag_modifier_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);

/* Modifier tag object */

const struct sieve_argument_def modifier_tag = { 
	"MODIFIER",
	tag_modifier_is_instance_of, 
	tag_modifier_validate, 
	NULL, NULL, NULL
};
 
/* Modifier tag implementation */ 
 
static bool tag_modifier_is_instance_of
(struct sieve_validator *valdtr, struct sieve_command *cmd,
	const struct sieve_extension *ext, const char *identifier, void **data)
{	
	const struct sieve_variables_modifier *modf;

	if ( data == NULL ) {
		return ext_variables_modifier_exists(ext, valdtr, identifier); 
	}

	if ( (modf=ext_variables_modifier_create_instance
		(ext, valdtr, cmd, identifier)) == NULL )
		return FALSE;

	*data = (void *) modf;
		
	return TRUE;
}

static bool tag_modifier_validate
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *cmd)
{
	unsigned int i, modf_count;
	bool inserted;
	const struct sieve_variables_modifier *modf = 
		(const struct sieve_variables_modifier *) (*arg)->argument->data;
	const struct sieve_variables_modifier *const *modfs; 
	struct cmd_set_context *sctx = (struct cmd_set_context *) cmd->data;
	
	inserted = FALSE;
	modfs = array_get(&sctx->modifiers, &modf_count);
	for ( i = 0; i < modf_count && !inserted; i++ ) {
	
		if ( modfs[i]->def->precedence == modf->def->precedence ) {
			sieve_argument_validate_error(valdtr, *arg, 
				"modifiers :%s and :%s specified for the set command conflict "
				"having equal precedence", 
				modfs[i]->def->obj_def.identifier, modf->def->obj_def.identifier);
			return FALSE;
		}
			
		if ( modfs[i]->def->precedence < modf->def->precedence ) {
			array_insert(&sctx->modifiers, i, &modf, 1);
			inserted = TRUE;
		}
	}
	
	if ( !inserted )
		array_append(&sctx->modifiers, &modf, 1);
	
	/* Added to modifier list; self-destruct to prevent duplicate generation */
	*arg = sieve_ast_arguments_detach(*arg, 1);
	
	return TRUE;
}

/* Command registration */

static bool cmd_set_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	struct sieve_command_registration *cmd_reg) 
{
	sieve_validator_register_tag(valdtr, cmd_reg, ext, &modifier_tag, 0); 	

	return TRUE;
}

/* 
 * Command validation 
 */

static bool cmd_set_pre_validate
(struct sieve_validator *valdtr ATTR_UNUSED, 
	struct sieve_command *cmd)
{
	pool_t pool = sieve_command_pool(cmd);
	struct cmd_set_context *sctx = p_new(pool, struct cmd_set_context, 1);
	
	/* Create an array for the sorted list of modifiers */
	p_array_init(&sctx->modifiers, pool, 2);

	cmd->data = (void *) sctx;
	
	return TRUE;
} 

static bool cmd_set_validate(struct sieve_validator *valdtr, 
	struct sieve_command *cmd) 
{ 
	const struct sieve_extension *this_ext = cmd->ext;
	struct sieve_ast_argument *arg = cmd->first_positional;
		
	if ( !sieve_validate_positional_argument
		(valdtr, cmd, arg, "name", 1, SAAT_STRING) ) {
		return FALSE;
	}
	
	if ( !sieve_variable_argument_activate(this_ext, valdtr, cmd, arg, TRUE) ) {
		return FALSE;
	}

	arg = sieve_ast_argument_next(arg);
	
	if ( !sieve_validate_positional_argument
		(valdtr, cmd, arg, "value", 2, SAAT_STRING) ) {
		return FALSE;
	}
	
	return sieve_validator_argument_activate(valdtr, cmd, arg, FALSE);	
}

/*
 * Code generation
 */
 
static bool cmd_set_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{
	const struct sieve_extension *this_ext = cmd->ext;
	struct sieve_binary_block *sblock = cgenv->sblock;
	struct cmd_set_context *sctx = (struct cmd_set_context *) cmd->data;
	const struct sieve_variables_modifier *const *modfs;
	unsigned int i, modf_count;	

	sieve_operation_emit(sblock, this_ext, &cmd_set_operation); 

	/* Generate arguments */
	if ( !sieve_generate_arguments(cgenv, cmd, NULL) )
		return FALSE;	
		
	/* Generate modifiers (already sorted during validation) */
	sieve_binary_emit_byte(sblock, array_count(&sctx->modifiers));

	modfs = array_get(&sctx->modifiers, &modf_count); 
	for ( i = 0; i < modf_count; i++ ) {
		ext_variables_opr_modifier_emit(sblock, modfs[i]->object.ext, modfs[i]->def);
	}

	return TRUE;
}

/* 
 * Code dump
 */
 
static bool cmd_set_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{	
	unsigned int mdfs, i;
	
	sieve_code_dumpf(denv, "SET");
	sieve_code_descend(denv);
	
	/* Print both variable name and string value */
	if ( !sieve_opr_string_dump(denv, address, "variable") ||
		!sieve_opr_string_dump(denv, address, "value") )
		return FALSE;
	
	/* Read the number of applied modifiers we need to read */
	if ( !sieve_binary_read_byte(denv->sblock, address, &mdfs) ) 
		return FALSE;
	
	/* Print all modifiers (sorted during code generation already) */
	for ( i = 0; i < mdfs; i++ ) {
		if ( !ext_variables_opr_modifier_dump(denv, address) )
			return FALSE;
	}
	
	return TRUE;
}

/* 
 * Code execution
 */
 
static int cmd_set_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{	
	struct sieve_variable_storage *storage;
	unsigned int var_index, mdfs, i;
	string_t *value;
	int ret = SIEVE_EXEC_OK;

	/*
	 * Read the normal operands
	 */
		
	if ( (ret=sieve_variable_operand_read
		(renv, address, "variable", &storage, &var_index)) <= 0 )
		return ret;
		
	if ( (ret=sieve_opr_string_read(renv, address, "string", &value)) <= 0 )
		return ret;

	if ( !sieve_binary_read_byte(renv->sblock, address, &mdfs) ) {
		sieve_runtime_trace_error(renv, "invalid modifier count");
		return SIEVE_EXEC_BIN_CORRUPT;
	}
	
	/* 
	 * Determine and assign the value 
	 */

	sieve_runtime_trace(renv, SIEVE_TRLVL_COMMANDS, "set command");
	sieve_runtime_trace_descend(renv);

	/* Hold value within limits */
	if ( str_len(value) > EXT_VARIABLES_MAX_VARIABLE_SIZE )
		str_truncate(value, EXT_VARIABLES_MAX_VARIABLE_SIZE);

	T_BEGIN {
		/* Apply modifiers if necessary (sorted during code generation already) */
		if ( str_len(value) > 0 ) {
			for ( i = 0; i < mdfs; i++ ) {
				string_t *new_value;
				struct sieve_variables_modifier modf;
					
				if ( !ext_variables_opr_modifier_read(renv, address, &modf) ) {
					value = NULL;
					ret = SIEVE_EXEC_BIN_CORRUPT;
					break;
				}
				
				if ( modf.def != NULL && modf.def->modify != NULL ) {
					if ( !modf.def->modify(value, &new_value) ) {
						value = NULL;
						ret = SIEVE_EXEC_FAILURE;
						break;
					}

					sieve_runtime_trace_here
						(renv, SIEVE_TRLVL_COMMANDS, "modify :%s \"%s\" => \"%s\"",
							sieve_variables_modifier_name(&modf), str_c(value), str_c(new_value));

					value = new_value;
					if ( value == NULL )
						break;

					/* Hold value within limits */
					if ( str_len(value) > EXT_VARIABLES_MAX_VARIABLE_SIZE )
						str_truncate(value, EXT_VARIABLES_MAX_VARIABLE_SIZE);
				}
			}
		}	
		
		/* Actually assign the value if all is well */
		if ( value != NULL ) {
			if ( !sieve_variable_assign(storage, var_index, value) )
				ret = SIEVE_EXEC_BIN_CORRUPT;
			else {
				if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_COMMANDS) ) {
					const char *var_name, *var_id;

					(void)sieve_variable_get_identifier(storage, var_index, &var_name);
					var_id = sieve_variable_get_varid(storage, var_index);

					sieve_runtime_trace_here(renv, 0, "assign `%s' [%s] = \"%s\"",
						var_name, var_id, str_c(value));
				}
			}
		}
	} T_END;
			
	if ( ret <= 0 ) return ret;		
	if ( value == NULL ) return SIEVE_EXEC_FAILURE;

	return SIEVE_EXEC_OK;
}






