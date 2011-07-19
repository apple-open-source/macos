/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_BINARY_H
#define __SIEVE_BINARY_H

#include "lib.h"

#include "sieve-common.h"

/*
 * Config
 */
 
#define SIEVE_BINARY_VERSION_MAJOR     0
#define SIEVE_BINARY_VERSION_MINOR     3

/*
 * Binary object
 */
 
struct sieve_binary;

struct sieve_binary *sieve_binary_create_new(struct sieve_script *script);
void sieve_binary_ref(struct sieve_binary *sbin);
void sieve_binary_unref(struct sieve_binary **sbin);

/*
 * Accessors
 */

pool_t sieve_binary_pool(struct sieve_binary *sbin);
struct sieve_instance *sieve_binary_svinst(struct sieve_binary *sbin);
const char *sieve_binary_path(struct sieve_binary *sbin);
struct sieve_script *sieve_binary_script(struct sieve_binary *sbin);

bool sieve_binary_script_newer
	(struct sieve_binary *sbin, struct sieve_script *script);
const char *sieve_binary_script_name(struct sieve_binary *sbin);
const char *sieve_binary_script_path(struct sieve_binary *sbin);

const char *sieve_binary_source(struct sieve_binary *sbin);
bool sieve_binary_loaded(struct sieve_binary *sbin);
bool sieve_binary_saved(struct sieve_binary *sbin);

/*
 * Activation after code generation
 */ 
 
void sieve_binary_activate(struct sieve_binary *sbin);

/* 
 * Saving the binary
 */
 
int sieve_binary_save
(struct sieve_binary *sbin, const char *path, bool update,
	enum sieve_error *error_r);
	
/* 
 * Loading the binary
 */ 
	
struct sieve_binary *sieve_binary_open
	(struct sieve_instance *svinst, const char *path, 
		struct sieve_script *script, enum sieve_error *error_r);
bool sieve_binary_up_to_date(struct sieve_binary *sbin);
	
/* 
 * Block management 
 */
 
enum sieve_binary_system_block {
	SBIN_SYSBLOCK_EXTENSIONS,
	SBIN_SYSBLOCK_MAIN_PROGRAM,
	SBIN_SYSBLOCK_LAST
};

struct sieve_binary_block *sieve_binary_block_create
	(struct sieve_binary *sbin);

unsigned int sieve_binary_block_count
	(struct sieve_binary *sbin);

struct sieve_binary_block *sieve_binary_block_get
	(struct sieve_binary *sbin, unsigned int id);

void sieve_binary_block_clear
	(struct sieve_binary_block *sblock);

size_t sieve_binary_block_get_size
	(const struct sieve_binary_block *sblock);

struct sieve_binary *sieve_binary_block_get_binary
	(const struct sieve_binary_block *sblock);

unsigned int sieve_binary_block_get_id
	(const struct sieve_binary_block *sblock);

/* 
 * Extension support 
 */
 
struct sieve_binary_extension {
	const struct sieve_extension_def *extension;

	bool (*binary_save)
		(const struct sieve_extension *ext, struct sieve_binary *sbin,
			void *context);
	bool (*binary_open)
		(const struct sieve_extension *ext, struct sieve_binary *sbin,
			void *context);	

	void (*binary_free)
		(const struct sieve_extension *ext, struct sieve_binary *sbin,
			void *context);	
	
	bool (*binary_up_to_date)
		(const struct sieve_extension *ext, struct sieve_binary *sbin,
			void *context);	
};
 
void sieve_binary_extension_set_context
	(struct sieve_binary *sbin, const struct sieve_extension *ext, void *context);
const void *sieve_binary_extension_get_context
	(struct sieve_binary *sbin, const struct sieve_extension *ext);
	
void sieve_binary_extension_set
	(struct sieve_binary *sbin, const struct sieve_extension *ext,
		const struct sieve_binary_extension *bext, void *context);

struct sieve_binary_block *sieve_binary_extension_create_block
	(struct sieve_binary *sbin, const struct sieve_extension *ext);
struct sieve_binary_block *sieve_binary_extension_get_block
(struct sieve_binary *sbin, const struct sieve_extension *ext);

int sieve_binary_extension_link
	(struct sieve_binary *sbin, const struct sieve_extension *ext);
const struct sieve_extension *sieve_binary_extension_get_by_index
	(struct sieve_binary *sbin, int index);
int sieve_binary_extension_get_index
	(struct sieve_binary *sbin, const struct sieve_extension *ext);
int sieve_binary_extensions_count(struct sieve_binary *sbin);

	
/* 
 * Code emission 
 */
 
/* Low-level emission functions */

sieve_size_t sieve_binary_emit_data
	(struct sieve_binary_block *sblock, const void *data, sieve_size_t size);
sieve_size_t sieve_binary_emit_byte
	(struct sieve_binary_block *sblock, uint8_t byte);
void sieve_binary_update_data
	(struct sieve_binary_block *sblock, sieve_size_t address, const void *data, 
		sieve_size_t size);

/* Offset emission functions */

sieve_size_t sieve_binary_emit_offset
	(struct sieve_binary_block *sblock, sieve_offset_t offset);
void sieve_binary_resolve_offset
	(struct sieve_binary_block *sblock, sieve_size_t address);

/* Literal emission functions */

sieve_size_t sieve_binary_emit_integer
	(struct sieve_binary_block *sblock, sieve_number_t integer);
sieve_size_t sieve_binary_emit_string
	(struct sieve_binary_block *sblock, const string_t *str);
sieve_size_t sieve_binary_emit_cstring
	(struct sieve_binary_block *sblock, const char *str);

static inline sieve_size_t sieve_binary_emit_unsigned
	(struct sieve_binary_block *sblock, unsigned int count)
{
	return sieve_binary_emit_integer(sblock, count);
}

/* Extension emission functions */

sieve_size_t sieve_binary_emit_extension
	(struct sieve_binary_block *sblock, const struct sieve_extension *ext,
		unsigned int offset);
void sieve_binary_emit_extension_object
	(struct sieve_binary_block *sblock, 
		const struct sieve_extension_objects *objs, unsigned int code);

/* 
 * Code retrieval 
 */

/* Literals */

bool sieve_binary_read_byte
	(struct sieve_binary_block *sblock, sieve_size_t *address, 
		unsigned int *byte_r);
bool sieve_binary_read_code
	(struct sieve_binary_block *sblock, sieve_size_t *address, 
		signed int *code_r);
bool sieve_binary_read_offset
	(struct sieve_binary_block *sblock, sieve_size_t *address,
		sieve_offset_t *offset_r);
bool sieve_binary_read_integer
  (struct sieve_binary_block *sblock, sieve_size_t *address, 
		sieve_number_t *int_r); 
bool sieve_binary_read_string
  (struct sieve_binary_block *sblock, sieve_size_t *address, 
		string_t **str_r);

static inline bool sieve_binary_read_unsigned
(struct sieve_binary_block *sblock, sieve_size_t *address, 
	unsigned int *count_r)
{
	sieve_number_t integer;

	if ( !sieve_binary_read_integer(sblock, address, &integer) )
		return FALSE;

	*count_r = integer;

	return TRUE;
}

/* Extensions */

bool sieve_binary_read_extension
	(struct sieve_binary_block *sblock, sieve_size_t *address,
		unsigned int *offset_r, const struct sieve_extension **ext_r);
const void *sieve_binary_read_extension_object
	(struct sieve_binary_block *sblock, sieve_size_t *address,
    const struct sieve_extension_objects *objs);

/*
 * Debug info
 */

/* Writer */

struct sieve_binary_debug_writer;

struct sieve_binary_debug_writer *sieve_binary_debug_writer_init
	(struct sieve_binary_block *sblock);
void sieve_binary_debug_writer_deinit
	(struct sieve_binary_debug_writer **dwriter);

void sieve_binary_debug_emit
	(struct sieve_binary_debug_writer *dwriter, sieve_size_t code_address, 
		unsigned int code_line, unsigned int code_column);

/* Reader */

struct sieve_binary_debug_reader *sieve_binary_debug_reader_init
	(struct sieve_binary_block *sblock);
void sieve_binary_debug_reader_deinit
	(struct sieve_binary_debug_reader **dreader);

void sieve_binary_debug_reader_reset
	(struct sieve_binary_debug_reader *dreader);

unsigned int sieve_binary_debug_read_line
	(struct sieve_binary_debug_reader *dreader, sieve_size_t code_address);

#endif
