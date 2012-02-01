/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#include "lib.h"
#include "str.h"
#include "str-sanitize.h"
#include "array.h"
#include "buffer.h"
#include "mempool.h"
#include "hash.h"

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-script.h"
#include "sieve-ast.h"
#include "sieve-commands.h"
#include "sieve-validator.h"

#include "sieve-comparators.h"
#include "sieve-address-parts.h"

/*
 * Forward declarations
 */
 
static void sieve_validator_register_core_commands
	(struct sieve_validator *valdtr);
static void sieve_validator_register_core_tests
	(struct sieve_validator *valdtr);
	
/*
 * Types
 */
 
/* Tag registration */

struct sieve_tag_registration {
	const struct sieve_argument_def *tag_def;
	const struct sieve_extension *ext;

	const char *identifier;	
	int id_code;
};

/* Command registration */

struct sieve_command_registration {
	const struct sieve_command_def *cmd_def;
	const struct sieve_extension *ext;
	
	ARRAY_DEFINE(normal_tags, struct sieve_tag_registration *); 
	ARRAY_DEFINE(instanced_tags, struct sieve_tag_registration *); 
	ARRAY_DEFINE(persistent_tags, struct sieve_tag_registration *); 
};
 
/* Default (literal) arguments */

struct sieve_default_argument {
	const struct sieve_argument_def *arg_def;
	const struct sieve_extension *ext;

	struct sieve_default_argument *overrides;
};

/* 
 * Validator extension
 */

struct sieve_validator_extension_reg {
	const struct sieve_validator_extension *valext;
	const struct sieve_extension *ext;
	struct sieve_ast_argument *arg;
	void *context;

	bool loaded;
};

/* 
 * Validator
 */

struct sieve_validator {
	pool_t pool;

	struct sieve_instance *svinst;
	struct sieve_ast *ast;
	struct sieve_script *script;
	
	struct sieve_error_handler *ehandler;

	bool finished_require;
	
	/* Registries */
	
	struct hash_table *commands;
	
	ARRAY_DEFINE(extensions, struct sieve_validator_extension_reg);
	
	/* This is currently a wee bit ugly and needs more thought */
	struct sieve_default_argument default_arguments[SAT_COUNT];

	/* Default argument processing state (FIXME: ugly) */
	struct sieve_default_argument *current_defarg;
	enum sieve_argument_type current_defarg_type;
	bool current_defarg_constant;
};

/* 
 * Error handling 
 */

void sieve_validator_warning
(struct sieve_validator *valdtr, unsigned int source_line, 
	const char *fmt, ...) 
{ 
	va_list args;
	
	va_start(args, fmt);
	sieve_vwarning(valdtr->ehandler, 
		sieve_error_script_location(valdtr->script, source_line),
		fmt, args);
	va_end(args);
	
}
 
void sieve_validator_error
(struct sieve_validator *valdtr, unsigned int source_line, 
	const char *fmt, ...) 
{
	va_list args;
	
	va_start(args, fmt);
	sieve_verror(valdtr->ehandler, 
		sieve_error_script_location(valdtr->script, source_line),
		fmt, args);
	va_end(args);
}

/* 
 * Validator object 
 */

struct sieve_validator *sieve_validator_create
(struct sieve_ast *ast, struct sieve_error_handler *ehandler) 
{
	pool_t pool;
	struct sieve_validator *valdtr;
	const struct sieve_extension *const *ext_preloaded;
	unsigned int i, ext_count;
	
	pool = pool_alloconly_create("sieve_validator", 8192);	
	valdtr = p_new(pool, struct sieve_validator, 1);
	valdtr->pool = pool;
	
	valdtr->ehandler = ehandler;
	sieve_error_handler_ref(ehandler);
	
	valdtr->ast = ast;	
	sieve_ast_ref(ast);

	valdtr->script = sieve_ast_script(ast);
	valdtr->svinst = sieve_script_svinst(valdtr->script);

	/* Setup default arguments */
	valdtr->default_arguments[SAT_NUMBER].arg_def = &number_argument;
	valdtr->default_arguments[SAT_NUMBER].ext = NULL;
	valdtr->default_arguments[SAT_VAR_STRING].arg_def = &string_argument;
	valdtr->default_arguments[SAT_VAR_STRING].ext = NULL;
	valdtr->default_arguments[SAT_CONST_STRING].arg_def = &string_argument;
	valdtr->default_arguments[SAT_CONST_STRING].ext = NULL;
	valdtr->default_arguments[SAT_STRING_LIST].arg_def = &string_list_argument;
	valdtr->default_arguments[SAT_STRING_LIST].ext = NULL;

	/* Setup storage for extension contexts */		
	p_array_init(&valdtr->extensions, pool, 
		sieve_extensions_get_count(valdtr->svinst));
		
	/* Setup command registry */
	valdtr->commands = hash_table_create
		(default_pool, pool, 0, strcase_hash, (hash_cmp_callback_t *)strcasecmp);
	sieve_validator_register_core_commands(valdtr);
	sieve_validator_register_core_tests(valdtr);
		
	/* Pre-load core language features implemented as 'extensions' */
	ext_preloaded = sieve_extensions_get_preloaded(valdtr->svinst, &ext_count); 
	for ( i = 0; i < ext_count; i++ ) {
		const struct sieve_extension_def *ext_def = ext_preloaded[i]->def;

		if ( ext_def != NULL && ext_def->validator_load != NULL )
			(void)ext_def->validator_load(ext_preloaded[i], valdtr);		
	}

	return valdtr;
}

void sieve_validator_free(struct sieve_validator **valdtr) 
{
	const struct sieve_validator_extension_reg *extrs;
	unsigned int ext_count, i;

	hash_table_destroy(&(*valdtr)->commands);
	sieve_ast_unref(&(*valdtr)->ast);

	sieve_error_handler_unref(&(*valdtr)->ehandler);

	/* Signal registered extensions that the validator is being destroyed */
	extrs = array_get(&(*valdtr)->extensions, &ext_count);
	for ( i = 0; i < ext_count; i++ ) {
		if ( extrs[i].valext != NULL && extrs[i].valext->free != NULL )
			extrs[i].valext->free(extrs[i].ext, *valdtr, extrs[i].context);
	}

	pool_unref(&(*valdtr)->pool);

	*valdtr = NULL;
}

/*
 * Accessors
 */

pool_t sieve_validator_pool(struct sieve_validator *valdtr)
{
	return valdtr->pool;
}

struct sieve_error_handler *sieve_validator_error_handler
(struct sieve_validator *valdtr)
{
	return valdtr->ehandler;
}

struct sieve_ast *sieve_validator_ast
(struct sieve_validator *valdtr)
{
	return valdtr->ast;
}

struct sieve_script *sieve_validator_script
(struct sieve_validator *valdtr)
{
	return valdtr->script;
}

struct sieve_instance *sieve_validator_svinst
(struct sieve_validator *valdtr)
{
	return valdtr->svinst;
}

/* 
 * Command registry 
 */

/* Dummy command object to mark unknown commands in the registry */

static bool _cmd_unknown_validate
(struct sieve_validator *valdtr ATTR_UNUSED, 
	struct sieve_command *cmd ATTR_UNUSED) 
{
	i_unreached();
	return FALSE;
}

static const struct sieve_command_def unknown_command = { 
	"", SCT_NONE, 0, 0, FALSE, FALSE , 
	NULL, NULL, _cmd_unknown_validate, NULL, NULL, NULL 
};

/* Registration of the core commands of the language */

static void sieve_validator_register_core_tests
(struct sieve_validator *valdtr) 
{
	unsigned int i;
	
	for ( i = 0; i < sieve_core_tests_count; i++ ) {
		sieve_validator_register_command(valdtr, NULL, sieve_core_tests[i]); 
	}
}

static void sieve_validator_register_core_commands
(struct sieve_validator *valdtr) 
{
	unsigned int i;
	
	for ( i = 0; i < sieve_core_commands_count; i++ ) {
		sieve_validator_register_command(valdtr, NULL, sieve_core_commands[i]); 
	}
}

/* Registry functions */

static struct sieve_command_registration *
sieve_validator_find_command_registration
(struct sieve_validator *valdtr, const char *command) 
{
	return (struct sieve_command_registration *) 
		hash_table_lookup(valdtr->commands, command);
}

static struct sieve_command_registration *_sieve_validator_register_command
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	const struct sieve_command_def *cmd_def, const char *identifier) 
{
	struct sieve_command_registration *cmd_reg = 
		p_new(valdtr->pool, struct sieve_command_registration, 1);

	cmd_reg->cmd_def = cmd_def;
	cmd_reg->ext = ext;
	
	hash_table_insert(valdtr->commands, (void *) identifier, (void *) cmd_reg);
		
	return cmd_reg;
}

void sieve_validator_register_command
(struct sieve_validator *valdtr, const struct sieve_extension *ext, 
	const struct sieve_command_def *cmd_def) 
{
	struct sieve_command_registration *cmd_reg =
		sieve_validator_find_command_registration(valdtr, cmd_def->identifier);
		
	if ( cmd_reg == NULL ) 
		cmd_reg = _sieve_validator_register_command
			(valdtr, ext, cmd_def, cmd_def->identifier);
	else {
		cmd_reg->cmd_def = cmd_def;
		cmd_reg->ext = ext;
	}
	
	if ( cmd_def->registered != NULL ) 
		cmd_def->registered(valdtr, ext, cmd_reg);
}

static void sieve_validator_register_unknown_command
(struct sieve_validator *valdtr, const char *command) 
{
	(void)_sieve_validator_register_command
		(valdtr, NULL, &unknown_command, command);		
}

/*const struct sieve_command *sieve_validator_find_command
(struct sieve_validator *valdtr, const char *command) 
{
  struct sieve_command_registration *cmd_reg = 
  	sieve_validator_find_command_registration(valdtr, command);
  
  return ( record == NULL ? NULL : record->command );
}*/

/* 
 * Per-command tagged argument registry 
 */

/* Dummy argument object to mark unknown arguments in the registry */

static bool _unknown_tag_validate
(struct sieve_validator *valdtr ATTR_UNUSED, 
	struct sieve_ast_argument **arg ATTR_UNUSED, 
	struct sieve_command *tst ATTR_UNUSED)
{
	i_unreached();
	return FALSE;
}

static const struct sieve_argument_def _unknown_tag = { 
	"", 
	NULL, 
	_unknown_tag_validate, 
	NULL, NULL, NULL 
};

static inline bool _tag_registration_is_unknown
(struct sieve_tag_registration *tag_reg) 
{
	return ( tag_reg != NULL && tag_reg->tag_def == &_unknown_tag );
}

/* Registry functions */

static void _sieve_validator_register_tag
(struct sieve_validator *valdtr, struct sieve_command_registration *cmd_reg, 
	const struct sieve_extension *ext, const struct sieve_argument_def *tag_def, 
	const char *identifier, int id_code) 
{
	struct sieve_tag_registration *reg;

	reg = p_new(valdtr->pool, struct sieve_tag_registration, 1);
	reg->ext = ext;
	reg->tag_def = tag_def;
	reg->id_code = id_code;
	if ( identifier == NULL )
		reg->identifier = tag_def->identifier;
	else
		reg->identifier = p_strdup(valdtr->pool, identifier);
	
	if ( !array_is_created(&cmd_reg->normal_tags) )
		p_array_init(&cmd_reg->normal_tags, valdtr->pool, 4);

	array_append(&cmd_reg->normal_tags, &reg, 1);
}

void sieve_validator_register_persistent_tag
(struct sieve_validator *valdtr, const char *command,
	const struct sieve_extension *ext, const struct sieve_argument_def *tag_def)
{		
	/* Add the tag to the persistent tags list if necessary */
	if ( tag_def->validate_persistent != NULL ) {
		struct sieve_command_registration *cmd_reg = 
			sieve_validator_find_command_registration(valdtr, command);
	
		if ( cmd_reg == NULL ) {
			cmd_reg = _sieve_validator_register_command(valdtr, NULL, NULL, command);
		}	

		struct sieve_tag_registration *reg;
 
		reg = p_new(valdtr->pool, struct sieve_tag_registration, 1);
		reg->ext = ext;
		reg->tag_def = tag_def;
		reg->id_code = -1;

		if ( !array_is_created(&cmd_reg->persistent_tags) ) 
			p_array_init(&cmd_reg->persistent_tags, valdtr->pool, 4);
				
		array_append(&cmd_reg->persistent_tags, &reg, 1);
	}
}

void sieve_validator_register_external_tag
(struct sieve_validator *valdtr, const char *command, 
	const struct sieve_extension *ext, const struct sieve_argument_def *tag_def,
	int id_code) 
{
	struct sieve_command_registration *cmd_reg = 
		sieve_validator_find_command_registration(valdtr, command);
		
	if ( cmd_reg == NULL ) {
		cmd_reg = _sieve_validator_register_command(valdtr, NULL, NULL, command);
	}
	
	_sieve_validator_register_tag
		(valdtr, cmd_reg, ext, tag_def, NULL, id_code);
}

void sieve_validator_register_tag
(struct sieve_validator *valdtr, struct sieve_command_registration *cmd_reg, 
	const struct sieve_extension *ext, const struct sieve_argument_def *tag_def, 
	int id_code) 
{
	if ( tag_def->is_instance_of == NULL )
		_sieve_validator_register_tag(valdtr, cmd_reg, ext, tag_def, NULL, id_code);
	else {
		struct sieve_tag_registration *reg = 
			p_new(valdtr->pool, struct sieve_tag_registration, 1);
		reg->ext = ext;		
		reg->tag_def = tag_def;
		reg->id_code = id_code;

		if ( !array_is_created(&cmd_reg->instanced_tags) ) 
				p_array_init(&cmd_reg->instanced_tags, valdtr->pool, 4);
				
		array_append(&cmd_reg->instanced_tags, &reg, 1);
	}
}

static void sieve_validator_register_unknown_tag
(struct sieve_validator *valdtr, struct sieve_command_registration *cmd_reg, 
	const char *tag) 
{
	_sieve_validator_register_tag(valdtr, cmd_reg, NULL, &_unknown_tag, tag, 0);
}

static struct sieve_tag_registration *_sieve_validator_command_tag_get
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	const char *tag, void **data)
{
	struct sieve_command_registration *cmd_reg = cmd->reg;
	struct sieve_tag_registration * const *regs;
	unsigned int i, reg_count;
				
	/* First check normal tags */
	if ( array_is_created(&cmd_reg->normal_tags) ) {
		regs = array_get(&cmd_reg->normal_tags, &reg_count);

		for ( i = 0; i < reg_count; i++ ) {
			if ( regs[i]->tag_def != NULL && 
				strcasecmp(regs[i]->identifier, tag) == 0) {

				return regs[i];
			}
		}
	}	
  
	/* Not found so far, try the instanced tags */
	if ( array_is_created(&cmd_reg->instanced_tags) ) {
		regs = array_get(&cmd_reg->instanced_tags, &reg_count);

		for ( i = 0; i < reg_count; i++ ) {
			if ( regs[i]->tag_def != NULL ) {
				if ( regs[i]->tag_def->is_instance_of
					(valdtr, cmd, regs[i]->ext, tag, data) )
					return regs[i];
			}
		}
	}
	
	return NULL;
}

static bool sieve_validator_command_tag_exists
(struct sieve_validator *valdtr, struct sieve_command *cmd, const char *tag) 
{
	return ( _sieve_validator_command_tag_get(valdtr, cmd, tag, NULL) != NULL );  
}

static struct sieve_tag_registration *sieve_validator_command_tag_get
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	struct sieve_ast_argument *arg, void **data)
{
	const char *tag = sieve_ast_argument_tag(arg);

	return _sieve_validator_command_tag_get(valdtr, cmd, tag, data);
}

/* 
 * Extension support 
 */

bool sieve_validator_extension_load
(struct sieve_validator *valdtr, struct sieve_command *cmd,
	struct sieve_ast_argument *ext_arg, const struct sieve_extension *ext)
{
	const struct sieve_extension_def *extdef = ext->def;
	struct sieve_validator_extension_reg *reg;

	if ( !sieve_ast_extension_link(valdtr->ast, ext) ) {
		/*if ( cmd != NULL && ext_arg != NULL ) {
			sieve_argument_validate_warning(valdtr, ext_arg, 
				"%s %s: sieve capability `%s' already loaded",
				sieve_command_identifier(cmd), sieve_command_type_name(cmd),
				sieve_extension_name(ext));
		}*/
	} else {
		if ( extdef->validator_load != NULL && 
			!extdef->validator_load(ext, valdtr) ) {
			if ( cmd != NULL && ext_arg != NULL ) {
				sieve_argument_validate_error(valdtr, ext_arg, 
					"%s %s: failed to load Sieve capability '%s'",
					sieve_command_identifier(cmd), sieve_command_type_name(cmd),
					sieve_extension_name(ext));
			}
			return FALSE;
		}
	}

	/* Register extension no matter what and store the AST argument registering it 
	 */
	if ( ext->id >= 0 ) {
		reg = array_idx_modifiable(&valdtr->extensions, (unsigned int) ext->id);
		if ( reg->arg == NULL )
			reg->arg = ext_arg;
		reg->loaded = TRUE;
	}

	return TRUE;
}

const struct sieve_extension *sieve_validator_extension_load_by_name
(struct sieve_validator *valdtr, struct sieve_command *cmd,
	struct sieve_ast_argument *ext_arg, const char *ext_name) 
{
	const struct sieve_extension *ext;

	ext = sieve_extension_get_by_name(valdtr->svinst, ext_name); 

	if ( ext == NULL || ext->def == NULL || !ext->enabled ) {
		unsigned int i;
		bool core_test = FALSE;
		bool core_command = FALSE;

		for ( i = 0; !core_command && i < sieve_core_commands_count; i++ ) {
			if ( strcasecmp(sieve_core_commands[i]->identifier, ext_name) == 0 )
				core_command = TRUE;
		}

		for ( i = 0; !core_test && i < sieve_core_tests_count; i++ ) {
			if ( strcasecmp(sieve_core_tests[i]->identifier, ext_name) == 0 )
				core_test = TRUE;
		}

		if ( core_test || core_command ) {
			sieve_argument_validate_error(valdtr, ext_arg,
				"%s %s: '%s' is not known as a Sieve capability, "
				"but it is known as a Sieve %s that is always available",
				sieve_command_identifier(cmd), sieve_command_type_name(cmd),
				str_sanitize(ext_name, 128), ( core_test ? "test" : "command" ));
		} else {
			sieve_argument_validate_error(valdtr, ext_arg,
				"%s %s: unknown Sieve capability '%s'", 
				sieve_command_identifier(cmd), sieve_command_type_name(cmd),
				str_sanitize(ext_name, 128));
		}
		return NULL;
	}

	if ( !sieve_validator_extension_load(valdtr, cmd, ext_arg, ext) )
		return NULL;

	return ext;
}

const struct sieve_extension *sieve_validator_extension_load_implicit
(struct sieve_validator *valdtr, const char *ext_name)
{
	const struct sieve_extension *ext;

	ext = sieve_extension_get_by_name(valdtr->svinst, ext_name); 

	if ( ext == NULL || ext->def == NULL )
		return NULL;

	if ( !sieve_validator_extension_load(valdtr, NULL, NULL, ext) )
		return NULL;

	return ext;
}

void sieve_validator_extension_register
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	const struct sieve_validator_extension *valext, void *context)
{
	struct sieve_validator_extension_reg *reg;

	if ( ext->id < 0 ) return;
	
	reg = array_idx_modifiable(&valdtr->extensions, (unsigned int) ext->id);
	reg->valext = valext;
	reg->ext = ext;
	reg->context = context;
}

bool sieve_validator_extension_loaded
(struct sieve_validator *valdtr, const struct sieve_extension *ext)
{
	const struct sieve_validator_extension_reg *reg;

	if ( ext->id < 0 || ext->id >= (int) array_count(&valdtr->extensions))
		return FALSE;

	reg = array_idx(&valdtr->extensions, (unsigned int) ext->id);

	return ( reg->loaded );
}

void sieve_validator_extension_set_context
(struct sieve_validator *valdtr, const struct sieve_extension *ext, 
	void *context)
{
	struct sieve_validator_extension_reg *reg;

	if ( ext->id < 0 ) return;
	
	reg = array_idx_modifiable(&valdtr->extensions, (unsigned int) ext->id);
	reg->context = context;
}

void *sieve_validator_extension_get_context
(struct sieve_validator *valdtr, const struct sieve_extension *ext) 
{
	const struct sieve_validator_extension_reg *reg;

	if  ( ext->id < 0 || ext->id >= (int) array_count(&valdtr->extensions) )
		return NULL;
	
	reg = array_idx(&valdtr->extensions, (unsigned int) ext->id);		

	return reg->context;
}

/* 
 * Overriding the default literal arguments
 */

void sieve_validator_argument_override
(struct sieve_validator *valdtr, enum sieve_argument_type type, 
	const struct sieve_extension *ext, const struct sieve_argument_def *arg_def)
{
	struct sieve_default_argument *arg;
	
	if ( valdtr->default_arguments[type].arg_def != NULL ) {
		arg = p_new(valdtr->pool, struct sieve_default_argument, 1);
		*arg = valdtr->default_arguments[type];	
		
		valdtr->default_arguments[type].overrides = arg;
	}
	
	valdtr->default_arguments[type].arg_def = arg_def;
	valdtr->default_arguments[type].ext = ext;
}

static bool sieve_validator_argument_default_activate
(struct sieve_validator *valdtr, struct sieve_command *cmd,
	struct sieve_default_argument *defarg, struct sieve_ast_argument *arg)
{
	bool result = TRUE;
	struct sieve_default_argument *prev_defarg;
	
	prev_defarg = valdtr->current_defarg;
	valdtr->current_defarg = defarg;
	
	if ( arg->argument == NULL ) {
		arg->argument = sieve_argument_create
			(arg->ast, defarg->arg_def, defarg->ext, 0);
	} else { 
		arg->argument->def = defarg->arg_def;
		arg->argument->ext = defarg->ext;
	}

	if (defarg->arg_def != NULL && defarg->arg_def->validate != NULL )
		result = defarg->arg_def->validate(valdtr, &arg, cmd); 
		
	valdtr->current_defarg = prev_defarg;	
		
	return result;
}

bool sieve_validator_argument_activate_super
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	struct sieve_ast_argument *arg, bool constant ATTR_UNUSED)
{
	struct sieve_default_argument *defarg;

	if ( valdtr->current_defarg == NULL ||	
		valdtr->current_defarg->overrides == NULL )
		return FALSE;
	
	if ( valdtr->current_defarg->overrides->arg_def == &string_argument ) {
		switch ( valdtr->current_defarg_type) {
		case SAT_CONST_STRING:
			if ( !valdtr->current_defarg_constant ) {
				valdtr->current_defarg_type = SAT_VAR_STRING;
				defarg = &valdtr->default_arguments[SAT_VAR_STRING];
			} else
				defarg = valdtr->current_defarg->overrides;
			break;
		case SAT_VAR_STRING:
			defarg = valdtr->current_defarg->overrides;
			break;
		default:
			return FALSE;
		}
	} else
		defarg = valdtr->current_defarg->overrides;
	
	return sieve_validator_argument_default_activate
		(valdtr, cmd, defarg, arg);
}

/* 
 * Argument Validation API 
 */

bool sieve_validator_argument_activate
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	struct sieve_ast_argument *arg, bool constant)
{
	struct sieve_default_argument *defarg;
	
	switch ( sieve_ast_argument_type(arg) ) {
	case SAAT_NUMBER:	
		valdtr->current_defarg_type = SAT_NUMBER;
		break;
	case SAAT_STRING:
		valdtr->current_defarg_type = SAT_CONST_STRING;
		break;
	case SAAT_STRING_LIST:
		valdtr->current_defarg_type = SAT_STRING_LIST;
		break;
	default:
		return FALSE;
	}

	valdtr->current_defarg_constant = constant;
	defarg = &valdtr->default_arguments[valdtr->current_defarg_type];

	if ( !constant && defarg->arg_def == &string_argument ) {
		valdtr->current_defarg_type = SAT_VAR_STRING;
		defarg = &valdtr->default_arguments[SAT_VAR_STRING];
	}
	
	return sieve_validator_argument_default_activate(valdtr, cmd, defarg, arg);
}

bool sieve_validate_positional_argument
(struct sieve_validator *valdtr, struct sieve_command *cmd,
	struct sieve_ast_argument *arg, const char *arg_name, unsigned int arg_pos,
	enum sieve_ast_argument_type req_type)
{
	if ( sieve_ast_argument_type(arg) != req_type && 
		(sieve_ast_argument_type(arg) != SAAT_STRING || 
			req_type != SAAT_STRING_LIST) ) 
	{
		sieve_argument_validate_error(valdtr, arg, 
			"the %s %s expects %s as argument %d (%s), but %s was found", 
			sieve_command_identifier(cmd), sieve_command_type_name(cmd), 
			sieve_ast_argument_type_name(req_type),
			arg_pos, arg_name, sieve_ast_argument_name(arg));
		return FALSE; 
	}
	
	return TRUE;
}

bool sieve_validate_tag_parameter
(struct sieve_validator *valdtr, struct sieve_command *cmd,
	struct sieve_ast_argument *tag, struct sieve_ast_argument *param,
	const char *arg_name, unsigned int arg_pos,
	enum sieve_ast_argument_type req_type, bool constant)
{
	if ( param == NULL ) {
		const char *position = ( arg_pos == 0 ? "" :
			t_strdup_printf(" %d (%s)", arg_pos, arg_name) );
 
		sieve_argument_validate_error(valdtr, tag, 
			"the :%s tag for the %s %s requires %s as parameter %s, "
			"but no parameters were found", sieve_ast_argument_tag(tag), 
			sieve_command_identifier(cmd), sieve_command_type_name(cmd),
			sieve_ast_argument_type_name(req_type), position);
		return FALSE;	
	}

	if ( sieve_ast_argument_type(param) != req_type && 
		(sieve_ast_argument_type(param) != SAAT_STRING || 
			req_type != SAAT_STRING_LIST) ) 
	{
		const char *position = ( arg_pos == 0 ? "" :
			t_strdup_printf(" %d (%s)", arg_pos, arg_name) );

		sieve_argument_validate_error(valdtr, param, 
			"the :%s tag for the %s %s requires %s as parameter%s, "
			"but %s was found", sieve_ast_argument_tag(tag), 
			sieve_command_identifier(cmd), sieve_command_type_name(cmd),
			sieve_ast_argument_type_name(req_type),	position,
			sieve_ast_argument_name(param));
		return FALSE;
	}

	if ( !sieve_validator_argument_activate(valdtr, cmd, param, constant) )
		return FALSE;

	param->argument->id_code = tag->argument->id_code;

	return TRUE;
}

/* 
 * Command argument validation 
 */

static bool sieve_validate_command_arguments
(struct sieve_validator *valdtr, struct sieve_command *cmd) 
{
	int arg_count = cmd->def->positional_arguments;
	int real_count = 0;
	struct sieve_ast_argument *arg;
	struct sieve_command_registration *cmd_reg = cmd->reg;

	/* Validate any tags that might be present */
	arg = sieve_ast_argument_first(cmd->ast_node);
		
	/* Visit tagged and optional arguments */
	while ( sieve_ast_argument_type(arg) == SAAT_TAG ) {
		struct sieve_ast_argument *parg;
		void *arg_data = NULL; 
		struct sieve_tag_registration *tag_reg = 
			sieve_validator_command_tag_get(valdtr, cmd, arg, &arg_data);
		const struct sieve_argument_def *tag_def;
		
		if ( tag_reg == NULL ) {
			sieve_argument_validate_error(valdtr, arg, 
				"unknown tagged argument ':%s' for the %s %s "
				"(reported only once at first occurence)",
				sieve_ast_argument_tag(arg), sieve_command_identifier(cmd), 
				sieve_command_type_name(cmd));
			sieve_validator_register_unknown_tag
				(valdtr, cmd_reg, sieve_ast_argument_tag(arg));
			return FALSE;					
		}
		
		/* Check whether previously tagged as unknown */
		if ( _tag_registration_is_unknown(tag_reg) ) 
			return FALSE;
		
		tag_def = tag_reg->tag_def;

		/* Assign the tagged argument type to the ast for later reference */
		arg->argument = sieve_argument_create
			(arg->ast, tag_def, tag_reg->ext, tag_reg->id_code);
		arg->argument->data = arg_data;

		/* Scan backwards for any duplicates */
		parg = sieve_ast_argument_prev(arg);
		while ( parg != NULL ) {
			if ( (sieve_ast_argument_type(parg) == SAAT_TAG && 
					parg->argument->def == tag_reg->tag_def) 
				|| (tag_reg->id_code > 0 && parg->argument != NULL && 
					parg->argument->id_code == tag_reg->id_code) ) 
			{
				const char *tag_id = sieve_ast_argument_tag(arg);
				const char *tag_desc =
					strcmp(tag_def->identifier, tag_id) != 0 ?
					t_strdup_printf("%s argument (:%s)", tag_def->identifier, tag_id) : 
					t_strdup_printf(":%s argument", tag_def->identifier); 	 
				
				sieve_argument_validate_error(valdtr, arg, 
					"encountered duplicate %s for the %s %s",
					tag_desc, sieve_command_identifier(cmd), 
					sieve_command_type_name(cmd));
					
				return FALSE;	
			}
			
			parg = sieve_ast_argument_prev(parg);
		}
		
		/* Call the validation function for the tag (if present)
		 *   Fail if the validation fails:
		 *     Let's not whine multiple	times about a single command having multiple 
		 *     bad arguments...
		 */ 
		if ( tag_def->validate != NULL ) {
			if ( !tag_def->validate(valdtr, &arg, cmd) ) 
				return FALSE;
		} else {
			arg = sieve_ast_argument_next(arg);
		}
	} 
	
	/* Remaining arguments should be positional (tags are not allowed here) */
	cmd->first_positional = arg;
	
	while ( arg != NULL ) {
		if ( sieve_ast_argument_type(arg) == SAAT_TAG ) {
			sieve_argument_validate_error(valdtr, arg, 
				"encountered an unexpected tagged argument ':%s' "
				"while validating positional arguments for the %s %s",
				sieve_ast_argument_tag(arg), sieve_command_identifier(cmd), 
				sieve_command_type_name(cmd));
			return FALSE;
		}
		
		real_count++;
	 
		arg = sieve_ast_argument_next(arg);
	}
	
	/* Check the required count versus the real number of arguments */
	if ( arg_count >= 0 && real_count != arg_count ) {
		sieve_command_validate_error(valdtr, cmd, 
			"the %s %s requires %d positional argument(s), but %d is/are specified",
			sieve_command_identifier(cmd), sieve_command_type_name(cmd), 
			arg_count, real_count);
		return FALSE;
	}
	
	/* Call initial validation for persistent arguments */
	if ( array_is_created(&cmd_reg->persistent_tags) ) {
		struct sieve_tag_registration * const *regs;
		unsigned int i, reg_count;
		
  	regs = array_get(&cmd_reg->persistent_tags, &reg_count); 
		for ( i = 0; i < reg_count; i++ ) {
	  		
			const struct sieve_argument_def *tag_def = regs[i]->tag_def;
  
			if ( tag_def != NULL && tag_def->validate_persistent != NULL ) { 
				/* To be sure */
				if ( !tag_def->validate_persistent(valdtr, cmd, regs[i]->ext) )
	  				return FALSE;
			}
		}
	}

	return TRUE;
}

static bool sieve_validate_arguments_context
(struct sieve_validator *valdtr, struct sieve_command *cmd)
{ 
	struct sieve_ast_argument *arg = 
		sieve_command_first_argument(cmd);
	
	while ( arg != NULL ) {
		const struct sieve_argument *argument = arg->argument;
		
		if ( argument != NULL && argument->def != NULL && 
			argument->def->validate_context != NULL ) {
 
			if ( !argument->def->validate_context(valdtr, arg, cmd) ) 
				return FALSE;
		}
		
		arg = sieve_ast_argument_next(arg);
	}

	return TRUE;
}
 
/* 
 * Command Validation API 
 */ 
                 
static bool sieve_validate_command_subtests
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	const unsigned int count) 
{
	switch ( count ) {
	
	case 0:
	 	if ( sieve_ast_test_count(cmd->ast_node) > 0 ) {
			/* Unexpected command specified */
			enum sieve_command_type ctype = SCT_NONE;
			struct sieve_command_registration *cmd_reg;
			struct sieve_ast_node *test = sieve_ast_test_first(cmd->ast_node);

			cmd_reg = sieve_validator_find_command_registration
				(valdtr, test->identifier);
	
			/* First check what we are dealing with */
			if ( cmd_reg != NULL && cmd_reg->cmd_def != NULL )
				ctype = cmd_reg->cmd_def->type;

			switch ( ctype ) {
			case SCT_TEST: /* Spurious test */
			case SCT_HYBRID:
				sieve_command_validate_error(valdtr, cmd, 
					"the %s %s accepts no sub-tests, but tests are specified", 
					sieve_command_identifier(cmd), sieve_command_type_name(cmd));
				break;

			case SCT_NONE: /* Unknown command */

				/* Is it perhaps a tag for which the ':' was omitted ? */
				if ( sieve_validator_command_tag_exists
					(valdtr, cmd, test->identifier) ) {
					sieve_command_validate_error(valdtr, cmd, 
						"missing colon ':' before ':%s' tag in %s %s", test->identifier, 
						sieve_command_identifier(cmd), sieve_command_type_name(cmd));
					break;
				} 
				/* Fall through */
			
			case SCT_COMMAND:
				sieve_command_validate_error(valdtr, cmd, 
					"missing semicolon ';' after %s %s", 
					sieve_command_identifier(cmd), sieve_command_type_name(cmd));
				break;
			}
			return FALSE;
		}
		break;
	case 1:
		if ( sieve_ast_test_count(cmd->ast_node) == 0 ) {
			sieve_command_validate_error(valdtr, cmd, 
				"the %s %s requires one sub-test, but none is specified", 
				sieve_command_identifier(cmd), sieve_command_type_name(cmd));
				
			return FALSE;
			
		} else if ( sieve_ast_test_count(cmd->ast_node) > 1 || 
			cmd->ast_node->test_list ) {
			
			sieve_command_validate_error(valdtr, cmd, 
				"the %s %s requires one sub-test, but a list of tests is specified", 
				sieve_command_identifier(cmd), sieve_command_type_name(cmd));
				
			return FALSE;
		}
		break;
		
	default:
		if ( sieve_ast_test_count(cmd->ast_node) == 0 ) {
			sieve_command_validate_error(valdtr, cmd, 
				"the %s %s requires a list of sub-tests, but none is specified", 
				sieve_command_identifier(cmd), sieve_command_type_name(cmd));
			
			return FALSE;
			
		} else if ( sieve_ast_test_count(cmd->ast_node) == 1 && 
			!cmd->ast_node->test_list ) {
			
			sieve_command_validate_error(valdtr, cmd, 
				"the %s %s requires a list of sub-tests, "
				"but a single test is specified", 
				sieve_command_identifier(cmd), sieve_command_type_name(cmd) );
			
			return FALSE;
		}
		break;		
	}

	return TRUE;
}

static bool sieve_validate_command_block
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	bool block_allowed, bool block_required) 
{
	i_assert( cmd->ast_node->type == SAT_COMMAND );
	
	if ( block_required ) {
		if ( !cmd->ast_node->block ) {
			sieve_command_validate_error(valdtr, cmd, 
				"the %s command requires a command block, but it is missing", 
				sieve_command_identifier(cmd));
			
			return FALSE;
		}
	} else if ( !block_allowed && cmd->ast_node->block ) {
		sieve_command_validate_error(valdtr, cmd, 
			"the %s command does not accept a command block, "
			"but one is specified anyway", 
			sieve_command_identifier(cmd) );
		
		return FALSE;
	}
	
	return TRUE;
} 

/* 
 * AST Validation 
 */

static bool sieve_validate_test_list
	(struct sieve_validator *valdtr, struct sieve_ast_node *test_list,
		int *const_r); 
static bool sieve_validate_block
	(struct sieve_validator *valdtr, struct sieve_ast_node *block);
static bool sieve_validate_command
	(struct sieve_validator *valdtr, struct sieve_ast_node *cmd_node,
		int *const_r);
	
static bool sieve_validate_command_context
(struct sieve_validator *valdtr, struct sieve_ast_node *cmd_node) 
{
	enum sieve_ast_type ast_type = sieve_ast_node_type(cmd_node);
	struct sieve_command_registration *cmd_reg;
	
	i_assert( ast_type == SAT_TEST || ast_type == SAT_COMMAND );
	
	/* Verify the command specified by this node */
	cmd_reg = sieve_validator_find_command_registration
		(valdtr, cmd_node->identifier);
	
	if ( cmd_reg != NULL && cmd_reg->cmd_def != NULL ) {
		const struct sieve_command_def *cmd_def = cmd_reg->cmd_def;

		/* Identifier = "" when the command was previously marked as unknown */
		if ( *(cmd_def->identifier) != '\0' ) {
			struct sieve_command *cmd;

			if ( (cmd_def->type == SCT_COMMAND && ast_type == SAT_TEST)
				|| (cmd_def->type == SCT_TEST && ast_type == SAT_COMMAND) ) {
				sieve_validator_error(
					valdtr, cmd_node->source_line, "attempted to use %s '%s' as %s", 
					sieve_command_def_type_name(cmd_def), cmd_node->identifier,
					sieve_ast_type_name(ast_type));
			
			 	return FALSE;
			} 
			 
			cmd_node->command = cmd = 
				sieve_command_create(cmd_node, cmd_reg->ext, cmd_def, cmd_reg); 
		} else {
			return FALSE;
		}

	}	else {
		sieve_validator_error(
			valdtr, cmd_node->source_line, 
			"unknown %s '%s' (only reported once at first occurence)", 
			sieve_ast_type_name(ast_type), cmd_node->identifier);
			
		sieve_validator_register_unknown_command(valdtr, cmd_node->identifier);
		
		return FALSE;
	}

	return TRUE;
}

static bool sieve_validate_command
(struct sieve_validator *valdtr, struct sieve_ast_node *cmd_node, int *const_r) 
{
	enum sieve_ast_type ast_type = sieve_ast_node_type(cmd_node);
	struct sieve_command *cmd = cmd_node->command;
	const struct sieve_command_def *cmd_def = ( cmd != NULL ? cmd->def : NULL );
	bool result = TRUE;
	
	i_assert( ast_type == SAT_TEST || ast_type == SAT_COMMAND );

	if ( cmd_def != NULL && *(cmd_def->identifier) != '\0' ) {
		
		if ( cmd_def->pre_validate == NULL 
			|| cmd_def->pre_validate(valdtr, cmd) ) {
	
			/* Check argument syntax */
			if ( !sieve_validate_command_arguments(valdtr, cmd) ) {
				result = FALSE;	

				/* A missing ':' causes a tag to become a test. This can be the cause
				 * of the arguments validation failing. Therefore we must produce an
				 * error for the sub-tests as well if appropriate.
				 */
				(void)sieve_validate_command_subtests(valdtr, cmd, cmd_def->subtests);

			} else if (
				!sieve_validate_command_subtests(valdtr, cmd, cmd_def->subtests) || 
				(ast_type == SAT_COMMAND && !sieve_validate_command_block
					(valdtr, cmd, cmd_def->block_allowed, cmd_def->block_required)) ) {

				result = FALSE;

			} else {
				/* Call command validation function if specified */
				if ( cmd_def->validate != NULL )
					result = cmd_def->validate(valdtr, cmd) && result;
			}
		} else {
			/* If pre-validation fails, don't bother to validate further 
			 * as context might be missing and doing so is not very useful for 
			 * further error reporting anyway
			 */
			return FALSE;
		}
			
		result = result && sieve_validate_arguments_context(valdtr, cmd);								
	}

	/*  
	 * Descend further into the AST 
	 */
	
	if ( cmd_def != NULL ) {
		/* Tests */
		if ( cmd_def->subtests > 0 ) { 
			if ( result || sieve_errors_more_allowed(valdtr->ehandler) ) {
				result = sieve_validate_test_list(valdtr, cmd_node, const_r) && result;
			} 
		} else if ( result ) {
			if ( cmd_def->validate_const != NULL ) {
				(void)cmd_def->validate_const(valdtr, cmd, const_r, -1);
			} else {
				*const_r = -1;
			}
		}

		/* Skip block if result of test is const FALSEconst struct sieve_extension *sieve_validator_extension_load
(struct sieve_validator *valdtr, struct sieve_command *cmd,
	struct sieve_ast_argument *ext_arg, string_t *ext_name)  */
		if ( result && *const_r == 0 )
			return TRUE;

		/* Command block */
		if ( cmd_def->block_allowed && ast_type == SAT_COMMAND && 
			(result || sieve_errors_more_allowed(valdtr->ehandler)) ) {
			result = sieve_validate_block(valdtr, cmd_node) && result;
		}
	}
	
	return result;
}

static bool sieve_validate_test_list
(struct sieve_validator *valdtr, struct sieve_ast_node *test_node,
	int *const_r) 
{
	struct sieve_command *tst = test_node->command;
	const struct sieve_command_def *tst_def = ( tst != NULL ? tst->def : NULL );
	struct sieve_ast_node *test;
	bool result = TRUE;

	if ( tst_def != NULL && tst_def->validate_const != NULL ) {
		if ( !tst_def->validate_const(valdtr, tst, const_r, -2) ) 
			return TRUE;
	}

	test = sieve_ast_test_first(test_node);
	while ( test != NULL 
		&& (result || sieve_errors_more_allowed(valdtr->ehandler)) ) {
		int const_value = -2;
	
		result = 
			sieve_validate_command_context(valdtr, test) && 
			sieve_validate_command(valdtr, test, &const_value) &&
			result;

		if ( result ) {
			if ( tst_def != NULL && tst_def->validate_const != NULL ) {
				if ( !tst_def->validate_const(valdtr, tst, const_r, const_value) ) 
					return TRUE;
			} else {
				*const_r = -1;
			}
		}
		
		if ( result && const_value >= 0 )
			test = sieve_ast_node_detach(test);
		else
			test = sieve_ast_test_next(test);
	}		
	
	return result;
}

static bool sieve_validate_block
(struct sieve_validator *valdtr, struct sieve_ast_node *block) 
{
	bool result = TRUE, fatal = FALSE;
	struct sieve_ast_node *cmd_node, *next;

	T_BEGIN {	
		cmd_node = sieve_ast_command_first(block);
		while ( !fatal && cmd_node != NULL
			&& (result || sieve_errors_more_allowed(valdtr->ehandler)) ) {	
			bool command_success;
			int const_value = -2;

			next = sieve_ast_command_next(cmd_node);
			command_success = sieve_validate_command_context(valdtr, cmd_node);
			result = command_success && result;	

	 		/* Check if this is the first non-require command */
			if ( command_success && sieve_ast_node_type(block) == SAT_ROOT
				&& !valdtr->finished_require && cmd_node->command != NULL
				&& !sieve_command_is(cmd_node->command, cmd_require) ) {
				const struct sieve_validator_extension_reg *extrs;
				unsigned int ext_count, i;

				valdtr->finished_require = TRUE;

				/* Validate all 'require'd extensions */
				extrs = array_get(&valdtr->extensions, &ext_count);
				for ( i = 0; i < ext_count; i++ ) {
					if ( extrs[i].valext != NULL 
						&& extrs[i].valext->validate != NULL ) {

						if ( !extrs[i].valext->validate
							(extrs[i].ext, valdtr, extrs[i].context, extrs[i].arg) )
						fatal = TRUE;
						break;
					} 
				}
			}

			result = !fatal && sieve_validate_command(valdtr, cmd_node, &const_value)
				&& result;
			
			cmd_node = next;
		}		
	} T_END;
	
	return result && !fatal;
}

bool sieve_validator_run(struct sieve_validator *valdtr) 
{	
	return sieve_validate_block(valdtr, sieve_ast_root(valdtr->ast));
}

/*
 * Validator object registry
 */

struct sieve_validator_object_reg {
	const struct sieve_object_def *obj_def;
	const struct sieve_extension *ext;
};

struct sieve_validator_object_registry {
	struct sieve_validator *valdtr;
	ARRAY_DEFINE(registrations, struct sieve_validator_object_reg);
};

struct sieve_validator_object_registry *sieve_validator_object_registry_get
(struct sieve_validator *valdtr, const struct sieve_extension *ext)
{
	return (struct sieve_validator_object_registry *) 
		sieve_validator_extension_get_context(valdtr, ext);
}

void sieve_validator_object_registry_add
(struct sieve_validator_object_registry *regs,
	const struct sieve_extension *ext, const struct sieve_object_def *obj_def) 
{
	struct sieve_validator_object_reg *reg;

	reg = array_append_space(&regs->registrations);
	reg->ext = ext;
	reg->obj_def = obj_def;
}

bool sieve_validator_object_registry_find
(struct sieve_validator_object_registry *regs, const char *identifier,
	struct sieve_object *obj) 
{
	unsigned int i;

	for ( i = 0; i < array_count(&regs->registrations); i++ ) {
		const struct sieve_validator_object_reg *reg = 
			array_idx(&regs->registrations, i);

		if ( strcasecmp(reg->obj_def->identifier, identifier) == 0) {
			if ( obj != NULL ) {
				obj->def = reg->obj_def;
				obj->ext = reg->ext;
			}
			return TRUE;
		}
	}

	return FALSE;
}

struct sieve_validator_object_registry *sieve_validator_object_registry_create
(struct sieve_validator *valdtr)
{
	pool_t pool = valdtr->pool;
	struct sieve_validator_object_registry *regs = 
		p_new(pool, struct sieve_validator_object_registry, 1);
	
	/* Setup registry */        
	p_array_init(&regs->registrations, valdtr->pool, 4);

	regs->valdtr = valdtr;

	return regs;
}

struct sieve_validator_object_registry *sieve_validator_object_registry_init
(struct sieve_validator *valdtr, const struct sieve_extension *ext)
{
	struct sieve_validator_object_registry *regs = 
		sieve_validator_object_registry_create(valdtr);
	
	sieve_validator_extension_set_context(valdtr, ext, regs);
	return regs;
}


