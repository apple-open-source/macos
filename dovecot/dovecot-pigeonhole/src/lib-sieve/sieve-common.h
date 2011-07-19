/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_COMMON_H
#define __SIEVE_COMMON_H

#include "lib.h"

#include "sieve-config.h"
#include "sieve-types.h"

#include <sys/types.h>
#include <stdlib.h>

/* 
 * Types
 */

typedef size_t sieve_size_t; 
typedef uint32_t sieve_offset_t;
typedef uint32_t sieve_number_t;

#define SIEVE_MAX_NUMBER ((sieve_number_t) -1)

/*
 * Forward declarations
 */

/* sieve-error.h */
struct sieve_error_handler;

/* sieve-ast.h */
enum sieve_ast_argument_type;

struct sieve_ast;
struct sieve_ast_node;
struct sieve_ast_argument;

/* sieve-commands.h */
struct sieve_argument;
struct sieve_argument_def;
struct sieve_command;
struct sieve_command_def;
struct sieve_command_context;
struct sieve_command_registration;

/* sieve-stringlist.h */
struct sieve_stringlist;

/* sieve-code.h */
struct sieve_operation_extension;

/* sieve-lexer.h */
struct sieve_lexer;

/* sieve-parser.h */
struct sieve_parser;

/* sieve-validator.h */
struct sieve_validator;

/* sieve-generator.h */
struct sieve_jumplist;
struct sieve_generator;
struct sieve_codegen_env;

/* sieve-runtime.h */
struct sieve_runtime_env;

/* sieve-interpreter.h */
struct sieve_interpreter;

/* sieve-dump.h */
struct sieve_dumptime_env;

/* sieve-binary-dumper.h */
struct sieve_binary_dumper;

/* sieve-code-dumper.h */
struct sieve_code_dumper;

/* sieve-extension.h */
struct sieve_extension;
struct sieve_extension_def;
struct sieve_extension_objects;

/* sieve-code.h */
struct sieve_operand;
struct sieve_operand_def;
struct sieve_operand_class;
struct sieve_operation;
struct sieve_coded_stringlist;

/* sieve-binary.h */
struct sieve_binary;
struct sieve_binary_block;
struct sieve_binary_debug_writer;
struct sieve_binary_debug_reader;

/* sieve-objects.h */
struct sieve_object_def;
struct sieve_object;

/* sieve-comparator.h */
struct sieve_comparator;

/* sieve-match-types.h */
struct sieve_match_type;

/* sieve-match.h */
struct sieve_match_context;

/* sieve-address.h */
struct sieve_address;
struct sieve_address_list;

/* sieve-address-parts.h */
struct sieve_address_part_def;
struct sieve_address_part;

/* sieve-result.h */
struct sieve_result;
struct sieve_side_effects_list;
struct sieve_result_print_env;

/* sieve-actions.h */
struct sieve_action_exec_env;
struct sieve_action;
struct sieve_action_def;
struct sieve_side_effect;
struct sieve_side_effect_def;

/* sieve-script.h */
struct sieve_script;

/* sieve-message.h */
struct sieve_message_context;

/* sieve-plugins.h */
struct sieve_plugin;

/* sieve.c */
struct sieve_ast *sieve_parse
	(struct sieve_script *script, struct sieve_error_handler *ehandler, 
		enum sieve_error *error_r);
bool sieve_validate
	(struct sieve_ast *ast, struct sieve_error_handler *ehandler,
		enum sieve_error *error_r);	

/*
 * Sieve engine instance
 */

struct sieve_instance {
	/* Main engine pool */
	pool_t pool;

	/* Callbacks */
	const struct sieve_environment *env;
	void *context;

	/* Engine debug */
	bool debug;

	/* Extension registry */
	struct sieve_extension_registry *ext_reg;

	/* System error handler */
	struct sieve_error_handler *system_ehandler;

	/* Plugin modules */
	struct sieve_plugin *plugins;

	/* Limits */
	size_t max_script_size;
	unsigned int max_actions;
	unsigned int max_redirects;
};

#endif /* __SIEVE_COMMON_H */
