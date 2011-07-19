/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "string.h"
#include "ostream.h"
#include "hash.h"
#include "mail-storage.h"

#include "sieve.h"
#include "sieve-code.h"
#include "sieve-commands.h"
#include "sieve-extensions.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-binary.h"
#include "sieve-dump.h"

#include "testsuite-common.h"
#include "testsuite-objects.h"
#include "testsuite-message.h"
 
/* 
 * Testsuite core objects
 */
 
enum testsuite_object_code {
	TESTSUITE_OBJECT_MESSAGE,
	TESTSUITE_OBJECT_ENVELOPE
};

const struct testsuite_object_def *testsuite_core_objects[] = {
	&message_testsuite_object, &envelope_testsuite_object
};

const unsigned int testsuite_core_objects_count =
	N_ELEMENTS(testsuite_core_objects);

/* 
 * Testsuite object registry
 */

static inline struct sieve_validator_object_registry *_get_object_registry
(struct sieve_validator *valdtr)
{
	struct testsuite_validator_context *ctx = 
		testsuite_validator_context_get(valdtr);

	return ctx->object_registrations;
}
 
void testsuite_object_register
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	const struct testsuite_object_def *tobj_def) 
{
	struct sieve_validator_object_registry *regs = _get_object_registry(valdtr);
	
	sieve_validator_object_registry_add(regs, ext, &tobj_def->obj_def);
}

static const struct testsuite_object *testsuite_object_create
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	const char *identifier) 
{
	struct sieve_validator_object_registry *regs = _get_object_registry(valdtr);
	struct sieve_object object;
	struct testsuite_object *tobj;

	if ( !sieve_validator_object_registry_find(regs, identifier, &object) )
		return NULL;

	tobj = p_new(sieve_command_pool(cmd), struct testsuite_object, 1);
	tobj->object = object;
	tobj->def = (const struct testsuite_object_def *) object.def;

  return tobj;
}

void testsuite_register_core_objects
(struct testsuite_validator_context *ctx)
{
	struct sieve_validator_object_registry *regs = ctx->object_registrations;
	unsigned int i;
	
	/* Register core testsuite objects */
	for ( i = 0; i < testsuite_core_objects_count; i++ ) {
		const struct testsuite_object_def *tobj_def = testsuite_core_objects[i];

		sieve_validator_object_registry_add
			(regs, testsuite_ext, &tobj_def->obj_def);
	}
}
 
/* 
 * Testsuite object code
 */ 
 
const struct sieve_operand_class sieve_testsuite_object_operand_class = 
	{ "testsuite object" };

static const struct sieve_extension_objects core_testsuite_objects =
	SIEVE_EXT_DEFINE_OBJECTS(testsuite_core_objects);

const struct sieve_operand_def testsuite_object_operand = { 
	"testsuite-object",
	&testsuite_extension, 
	TESTSUITE_OPERAND_OBJECT, 
	&sieve_testsuite_object_operand_class,
	&core_testsuite_objects
};

static void testsuite_object_emit
(struct sieve_binary_block *sblock, const struct testsuite_object *tobj,
	int member_id)
{ 
	sieve_opr_object_emit(sblock, tobj->object.ext, tobj->object.def);
	
	if ( tobj->def != NULL && tobj->def->get_member_id != NULL ) {
		(void) sieve_binary_emit_byte(sblock, (unsigned char) member_id);
	}
}

bool testsuite_object_read
(struct sieve_binary_block *sblock, sieve_size_t *address, 
	struct testsuite_object *tobj)
{
	struct sieve_operand oprnd;

	if ( !sieve_operand_read(sblock, address, NULL, &oprnd) )
		return FALSE;
	
	if ( !sieve_opr_object_read_data
		(sblock, &oprnd, &sieve_testsuite_object_operand_class, address,
			&tobj->object) )
		return FALSE;

	tobj->def = (const struct testsuite_object_def *) tobj->object.def;

	return TRUE;
}

bool testsuite_object_read_member
(struct sieve_binary_block *sblock, sieve_size_t *address, 
	struct testsuite_object *tobj, int *member_id_r)
{		
	if ( !testsuite_object_read(sblock, address, tobj) )
		return FALSE;
		
	*member_id_r = -1;
	if ( tobj->def != NULL && tobj->def->get_member_id != NULL ) {
		if ( !sieve_binary_read_code(sblock, address, member_id_r) ) 
			return FALSE;
	}
	
	return TRUE;
}

const char *testsuite_object_member_name
(const struct testsuite_object *object, int member_id)
{
	const struct testsuite_object_def *obj_def = object->def;
	const char *member = NULL;

	if ( obj_def->get_member_id != NULL ) {
		if ( obj_def->get_member_name != NULL )
			member = obj_def->get_member_name(member_id);
	} else 
		return obj_def->obj_def.identifier;
		
	if ( member == NULL )	
		return t_strdup_printf("%s.%d", obj_def->obj_def.identifier, member_id);
	
	return t_strdup_printf("%s.%s", obj_def->obj_def.identifier, member);
}

bool testsuite_object_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	struct testsuite_object object;
	int member_id;

	sieve_code_mark(denv);
		
	if ( !testsuite_object_read_member
		(denv->sblock, address, &object, &member_id) )
		return FALSE;
	
	sieve_code_dumpf(denv, "%s: %s",
		sieve_testsuite_object_operand_class.name, 
		testsuite_object_member_name(&object, member_id));
	
	return TRUE;
}

/* 
 * Testsuite object argument
 */
 
static bool arg_testsuite_object_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
		struct sieve_command *cmd);

const struct sieve_argument_def testsuite_object_argument = { 
	"testsuite-object", 
	NULL, NULL, NULL, NULL,
	arg_testsuite_object_generate 
};
 
struct testsuite_object_argctx {
	const struct testsuite_object *object;
	int member;
};

bool testsuite_object_argument_activate
(struct sieve_validator *valdtr, struct sieve_ast_argument *arg,
	struct sieve_command *cmd) 
{
	const char *objname = sieve_ast_argument_strc(arg);
	const struct testsuite_object *tobj;
	int member_id;
	const char *member;
	struct testsuite_object_argctx *ctx;
	
	/* Parse the object specifier */
	
	member = strchr(objname, '.');
	if ( member != NULL ) {
		objname = t_strdup_until(objname, member);
		member++;
	}
	
	/* Find the object */
	
	tobj = testsuite_object_create(valdtr, cmd, objname);
	if ( tobj == NULL ) {
		sieve_argument_validate_error(valdtr, arg, 
			"unknown testsuite object '%s'", objname);
		return FALSE;
	}
	
	/* Find the object member */
	
	member_id = -1;
	if ( member != NULL ) {
		if ( tobj->def == NULL || tobj->def->get_member_id == NULL || 
			(member_id=tobj->def->get_member_id(member)) == -1 ) {
			sieve_argument_validate_error(valdtr, arg, 
				"member '%s' does not exist for testsuite object '%s'", member, objname);
			return FALSE;
		}
	}
	
	/* Assign argument context */
	
	ctx = p_new(sieve_command_pool(cmd), struct testsuite_object_argctx, 1);
	ctx->object = tobj;
	ctx->member = member_id;
	
	arg->argument = sieve_argument_create
		(arg->ast, &testsuite_object_argument, testsuite_ext, 0);
	arg->argument->data = (void *) ctx;
	
	return TRUE;
}

static bool arg_testsuite_object_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *cmd ATTR_UNUSED)
{
	struct testsuite_object_argctx *ctx = 
		(struct testsuite_object_argctx *) arg->argument->data;
	
	testsuite_object_emit(cgenv->sblock, ctx->object, ctx->member);
		
	return TRUE;
}

/* 
 * Testsuite core object implementation
 */
 
static bool tsto_message_set_member
	(const struct sieve_runtime_env *renv, int id, string_t *value);

static int tsto_envelope_get_member_id(const char *identifier);
static const char *tsto_envelope_get_member_name(int id);
static bool tsto_envelope_set_member
	(const struct sieve_runtime_env *renv, int id, string_t *value);

const struct testsuite_object_def message_testsuite_object = { 
	SIEVE_OBJECT("message",	&testsuite_object_operand, TESTSUITE_OBJECT_MESSAGE),
	NULL, NULL, 
	tsto_message_set_member, 
	NULL
};

const struct testsuite_object_def envelope_testsuite_object = { 
	SIEVE_OBJECT("envelope", &testsuite_object_operand, TESTSUITE_OBJECT_ENVELOPE),
	tsto_envelope_get_member_id, 
	tsto_envelope_get_member_name,
	tsto_envelope_set_member, 
	NULL
};

enum testsuite_object_envelope_field {
	TESTSUITE_OBJECT_ENVELOPE_FROM,
	TESTSUITE_OBJECT_ENVELOPE_TO,
	TESTSUITE_OBJECT_ENVELOPE_AUTH_USER
};

static bool tsto_message_set_member
(const struct sieve_runtime_env *renv, int id, string_t *value) 
{
	if ( id != -1 ) return FALSE;
	
	testsuite_message_set_string(renv, value);
	
	return TRUE;
}

static int tsto_envelope_get_member_id(const char *identifier)
{
	if ( strcasecmp(identifier, "from") == 0 )
		return TESTSUITE_OBJECT_ENVELOPE_FROM;
	if ( strcasecmp(identifier, "to") == 0 )
		return TESTSUITE_OBJECT_ENVELOPE_TO;
	if ( strcasecmp(identifier, "auth") == 0 )
		return TESTSUITE_OBJECT_ENVELOPE_AUTH_USER;	
	
	return -1;
}

static const char *tsto_envelope_get_member_name(int id) 
{
	switch ( id ) {
	case TESTSUITE_OBJECT_ENVELOPE_FROM: 
		return "from";
	case TESTSUITE_OBJECT_ENVELOPE_TO: 
		return "to";
	case TESTSUITE_OBJECT_ENVELOPE_AUTH_USER: 
		return "auth";
	}
	
	return NULL;
}

static bool tsto_envelope_set_member
(const struct sieve_runtime_env *renv, int id, string_t *value)
{
	switch ( id ) {
	case TESTSUITE_OBJECT_ENVELOPE_FROM: 
		testsuite_envelope_set_sender(renv, str_c(value));
		return TRUE;
	case TESTSUITE_OBJECT_ENVELOPE_TO:
		testsuite_envelope_set_recipient(renv, str_c(value));
		return TRUE;
	case TESTSUITE_OBJECT_ENVELOPE_AUTH_USER: 
		testsuite_envelope_set_auth_user(renv, str_c(value));
		return TRUE;
	}
	
	return FALSE;
}
