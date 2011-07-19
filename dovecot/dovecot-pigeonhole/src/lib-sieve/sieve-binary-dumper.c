/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "ostream.h"
#include "array.h"
#include "buffer.h"

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-dump.h"

#include "sieve-binary-private.h"

/*
 * Binary dumper object
 */ 
 
struct sieve_binary_dumper {
	pool_t pool;
	
	/* Dumptime environment */
	struct sieve_dumptime_env dumpenv; 
};

struct sieve_binary_dumper *sieve_binary_dumper_create
(struct sieve_binary *sbin) 
{
	pool_t pool;
	struct sieve_binary_dumper *dumper;
	
	pool = pool_alloconly_create("sieve_binary_dumper", 4096);	
	dumper = p_new(pool, struct sieve_binary_dumper, 1);
	dumper->pool = pool;
	dumper->dumpenv.dumper = dumper;
	
	dumper->dumpenv.sbin = sbin;
	sieve_binary_ref(sbin);
	
	dumper->dumpenv.svinst = sieve_binary_svinst(sbin);

	return dumper;
}

void sieve_binary_dumper_free(struct sieve_binary_dumper **dumper) 
{
	sieve_binary_unref(&(*dumper)->dumpenv.sbin);
	pool_unref(&((*dumper)->pool));
	
	*dumper = NULL;
}

pool_t sieve_binary_dumper_pool(struct sieve_binary_dumper *dumper)
{
	return dumper->pool;
}

/* 
 * Formatted output 
 */

void sieve_binary_dumpf
(const struct sieve_dumptime_env *denv, const char *fmt, ...)
{ 
	string_t *outbuf = t_str_new(128);
	va_list args;
	
	va_start(args, fmt);			
	str_vprintfa(outbuf, fmt, args);
	va_end(args);
	
	o_stream_send(denv->stream, str_data(outbuf), str_len(outbuf));
}

void sieve_binary_dump_sectionf
(const struct sieve_dumptime_env *denv, const char *fmt, ...)
{
	string_t *outbuf = t_str_new(128);
	va_list args;
	
	va_start(args, fmt);			
	str_printfa(outbuf, "\n* ");
	str_vprintfa(outbuf, fmt, args);
	str_printfa(outbuf, ":\n\n");
	va_end(args);
	
	o_stream_send(denv->stream, str_data(outbuf), str_len(outbuf));
}

/* 
 * Dumping the binary
 */

bool sieve_binary_dumper_run
(struct sieve_binary_dumper *dumper, struct ostream *stream, bool verbose) 
{	
	struct sieve_binary *sbin = dumper->dumpenv.sbin;
	struct sieve_dumptime_env *denv = &(dumper->dumpenv);
	int count, i;
	
	dumper->dumpenv.stream = stream;

	/* Dump list of binary blocks */
	if ( verbose ) {
		count = sieve_binary_block_count(sbin);

		sieve_binary_dump_sectionf
			(denv, "Binary blocks (count: %d)", count);

		for ( i = 0; i < count; i++ ) {
			struct sieve_binary_block *sblock = sieve_binary_block_get(sbin, i);

			sieve_binary_dumpf(denv, 
				"%3d: size: %"PRIuSIZE_T" bytes\n", i, 
				sieve_binary_block_get_size(sblock));
		}
	}
	
	/* Dump list of used extensions */
	
	count = sieve_binary_extensions_count(sbin);
	if ( count > 0 ) {
		sieve_binary_dump_sectionf
			(denv, "Required extensions (block: %d)", SBIN_SYSBLOCK_EXTENSIONS);
	
		for ( i = 0; i < count; i++ ) {
			const struct sieve_extension *ext = sieve_binary_extension_get_by_index
				(sbin, i);

			struct sieve_binary_block *sblock = sieve_binary_extension_get_block
				(sbin, ext);
	
			if ( sblock == NULL ) {
				sieve_binary_dumpf(denv, "%3d: %s (id: %d)\n", 
					i, sieve_extension_name(ext), ext->id);
			} else {
				sieve_binary_dumpf(denv, "%3d: %s (id: %d; block: %d)\n", 
					i, sieve_extension_name(ext), ext->id, 
					sieve_binary_block_get_id(sblock));
			}
		}
	}

	/* Dump extension-specific elements of the binary */
	
	count = sieve_binary_extensions_count(sbin);
	if ( count > 0 ) {	
		for ( i = 0; i < count; i++ ) {
			bool success = TRUE;

			T_BEGIN { 
				const struct sieve_extension *ext = sieve_binary_extension_get_by_index
					(sbin, i);
	
				if ( ext->def != NULL && ext->def->binary_dump != NULL ) {	
					success = ext->def->binary_dump(ext, denv);
				}
			} T_END;

			if ( !success ) return FALSE;
		}
	}
	
	/* Dump main program */
	
	sieve_binary_dump_sectionf
		(denv, "Main program (block: %d)", SBIN_SYSBLOCK_MAIN_PROGRAM);

	dumper->dumpenv.sblock = 
		sieve_binary_block_get(sbin, SBIN_SYSBLOCK_MAIN_PROGRAM);
	dumper->dumpenv.cdumper = sieve_code_dumper_create(&(dumper->dumpenv));

	if ( dumper->dumpenv.cdumper != NULL ) {
		sieve_code_dumper_run(dumper->dumpenv.cdumper);
		
		sieve_code_dumper_free(&dumper->dumpenv.cdumper);
	}
	
	/* Finish with empty line */
	sieve_binary_dumpf(denv, "\n");

	return TRUE;
}

/*
 * Hexdump production
 */

void sieve_binary_dumper_hexdump
(struct sieve_binary_dumper *dumper, struct ostream *stream)
{
	struct sieve_binary *sbin = dumper->dumpenv.sbin;
	struct sieve_dumptime_env *denv = &(dumper->dumpenv);
	int count, i;
	
	dumper->dumpenv.stream = stream;

	count = sieve_binary_block_count(sbin);

	/* Block overview */

	sieve_binary_dump_sectionf
		(denv, "Binary blocks (count: %d)", count);

	for ( i = 0; i < count; i++ ) {
		struct sieve_binary_block *sblock = sieve_binary_block_get(sbin, i);

		sieve_binary_dumpf(denv, 
			"%3d: size: %"PRIuSIZE_T" bytes\n", i, 
			sieve_binary_block_get_size(sblock));
	}

	/* Hexdump for each block */

	for ( i = 0; i < count; i++ ) {
		struct sieve_binary_block *sblock = sieve_binary_block_get(sbin, i);
		buffer_t *blockbuf = sieve_binary_block_get_buffer(sblock);
		string_t *line;
		size_t data_size;
		const char *data;
		size_t offset;

		data = (const char *) buffer_get_data(blockbuf, &data_size);
		
		sieve_binary_dump_sectionf
			(denv, "Block %d (%"PRIuSIZE_T" bytes, file offset %08llx)", i, 
				data_size, sblock->offset + 8 /* header size (yuck) */);

		line = t_str_new(128);
		offset = 0;
		while ( offset < data_size ) {
			size_t len = ( data_size - offset >= 16 ? 16 : data_size - offset );
			size_t b;

			str_printfa(line, "%08llx  ", (unsigned long long) offset);

			for ( b = 0; b < len; b++ ) {
				str_printfa(line, "%02x ", (unsigned int) data[offset+b]);	
				if ( b == 7 ) str_append_c(line, ' ');
			}

			if ( len < 16 ) {
				if ( len <= 7 ) str_append_c(line, ' ');

				for ( b = len; b < 16; b++ ) {
					str_append(line, "   ");
				}
			}

			str_append(line, " |");

			for ( b = 0; b < len; b++ ) {
				const char c = data[offset+b];

				if ( c >= 32 && c <= 126 )
					str_append_c(line, c);
				else
					str_append_c(line, '.');
			}

			str_append(line, "|\n");
			o_stream_send(stream, str_data(line), str_len(line));
			str_truncate(line, 0);
			offset += len;
		}
		
		str_printfa(line, "%08llx\n", (unsigned long long) offset);
		o_stream_send(stream, str_data(line), str_len(line));
	}
} 

