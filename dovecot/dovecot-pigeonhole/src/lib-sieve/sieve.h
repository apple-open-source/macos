/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

#ifndef __SIEVE_H
#define __SIEVE_H

struct sieve_script;
struct sieve_binary;

#include "sieve-config.h"
#include "sieve-types.h"
#include "sieve-error.h"

/*
 * Main Sieve library interface
 */

/* sieve_init(): 
 *   Initializes the sieve engine. Must be called before any sieve functionality
 *   is used.
 */
struct sieve_instance *sieve_init
	(const struct sieve_environment *env, void *context, bool debug);

/* sieve_deinit():
 *   Frees all memory allocated by the sieve engine. 
 */
void sieve_deinit(struct sieve_instance **svinst);

/* sieve_get_capabilities():
 *
 */
const char *sieve_get_capabilities
	(struct sieve_instance *svinst, const char *name);

/* sieve_set_extensions():
 *
 */
void sieve_set_extensions
	(struct sieve_instance *svinst, const char *extensions);

/*
 * Script compilation
 */

/* sieve_compile_script:
 */
struct sieve_binary *sieve_compile_script
	(struct sieve_script *script, struct sieve_error_handler *ehandler,
		enum sieve_error *error_r);

/* sieve_compile:
 *
 *   Compiles the script into a binary.
 */
struct sieve_binary *sieve_compile
	(struct sieve_instance *svinst, const char *script_path, 
		const char *script_name, struct sieve_error_handler *ehandler,
		enum sieve_error *error_r);

/* 
 * Reading/writing Sieve binaries
 */

/* sieve_load:
 *
 *  Loads the sieve binary indicated by the provided path.
 */
struct sieve_binary *sieve_load
	(struct sieve_instance *svinst, const char *bin_path,
		enum sieve_error *error_r);

/* sieve_open:
 *
 *   First tries to open the binary version of the specified script and if it 
 *   does not exist or if it contains errors, the script is (re-)compiled. Note 
 *   that errors in the bytecode are caught only at runtime.
 */
struct sieve_binary *sieve_open
	(struct sieve_instance *svinst, const char *script_path, 
		const char *script_name, struct sieve_error_handler *ehandler, 
		enum sieve_error *error_r);

/* sieve_save:
 *
 *  Saves the binary as the file indicated by the path parameter. If 
 *  path is NULL, it chooses the default path relative to the original
 *  script. This function will not write the binary to disk when it was
 *  loaded from the indicated bin_path, unless update is TRUE. 
 */
int sieve_save
    (struct sieve_binary *sbin, const char *bin_path, bool update,
			enum sieve_error *error_r);

/* sieve_close:
 *
 *   Closes a compiled/opened sieve binary.
 */
void sieve_close(struct sieve_binary **sbin);

/* sieve_get_source:
 *
 *   Obtains the path the binary was compiled or loaded from
 */
const char *sieve_get_source(struct sieve_binary *sbin);

/*
 * sieve_is_loeded:
 *
 *   Indicates whether the binary was loaded from a pre-compiled file. 
 */
bool sieve_is_loaded(struct sieve_binary *sbin);

/*
 * Debugging
 */

/* sieve_dump:
 *
 *   Dumps the byte code in human-readable form to the specified ostream.
 */
void sieve_dump
	(struct sieve_binary *sbin, struct ostream *stream, bool verbose);

/* sieve_hexdump:
 *
 *   Dumps the byte code in hexdump form to the specified ostream.
 */

void sieve_hexdump
	(struct sieve_binary *sbin, struct ostream *stream);

/* sieve_test:
 *
 *   Executes the bytecode, but only prints the result to the given stream.
 */ 
int sieve_test
	(struct sieve_binary *sbin, const struct sieve_message_data *msgdata, 
		const struct sieve_script_env *senv, struct sieve_error_handler *ehandler, 
		struct ostream *stream, bool *keep);

/*
 * Script execution
 */

/* sieve_execute:
 *
 *   Executes the binary, including the result.  
 */
int sieve_execute
	(struct sieve_binary *sbin, const struct sieve_message_data *msgdata,
		const struct sieve_script_env *senv, struct sieve_error_handler *ehandler,
		bool *keep);
		
/*
 * Multiscript support
 */
 
struct sieve_multiscript;
 
struct sieve_multiscript *sieve_multiscript_start_execute
	(struct sieve_instance *svinst, const struct sieve_message_data *msgdata,
		const struct sieve_script_env *senv);
struct sieve_multiscript *sieve_multiscript_start_test
	(struct sieve_instance *svinst, const struct sieve_message_data *msgdata,
		const struct sieve_script_env *senv, struct ostream *stream);

bool sieve_multiscript_run
	(struct sieve_multiscript *mscript, struct sieve_binary *sbin, 
		struct sieve_error_handler *ehandler, bool final);

int sieve_multiscript_status(struct sieve_multiscript *mscript);

int sieve_multiscript_finish
	(struct sieve_multiscript **mscript, struct sieve_error_handler *ehandler,
		bool *keep);

/*
 * Configured limits
 */

unsigned int sieve_max_redirects(struct sieve_instance *svinst);
unsigned int sieve_max_actions(struct sieve_instance *svinst);
size_t sieve_max_script_size(struct sieve_instance *svinst);

/*
 * Script directory
 */

struct sieve_directory;

struct sieve_directory *sieve_directory_open
	(struct sieve_instance *svinst, const char *path, enum sieve_error *error_r);
const char *sieve_directory_get_scriptfile(struct sieve_directory *sdir);
void sieve_directory_close(struct sieve_directory **sdir);

#endif
