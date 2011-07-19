/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"

#include "sieve-common.h"
#include "sieve-error.h"
#include "sieve-code.h"

#include "sieve-binary-private.h"

/* Quick 'n dirty debug */
#if 0
#define debug_printf(...) printf ("lineinfo: " __VA_ARGS__)
#else
#define debug_printf(...) 
#endif

/*
 * Opcodes
 */

enum {
	LINPROG_OP_COPY,
	LINPROG_OP_ADVANCE_PC,
	LINPROG_OP_ADVANCE_LINE,
	LINPROG_OP_SET_COLUMN,
	LINPROG_OP_SPECIAL_BASE
};

#define LINPROG_LINE_BASE   0
#define LINPROG_LINE_RANGE  4

/*
 * Lineinfo writer
 */

struct sieve_binary_debug_writer {
	struct sieve_binary_block *sblock;

	sieve_size_t address;
	unsigned long int line;
	unsigned long int column;
};

struct sieve_binary_debug_writer *sieve_binary_debug_writer_init
(struct sieve_binary_block *sblock)
{
	struct sieve_binary_debug_writer *dwriter;

	dwriter = i_new(struct sieve_binary_debug_writer, 1);
	dwriter->sblock = sblock;

	return dwriter;
}

void sieve_binary_debug_writer_deinit
(struct sieve_binary_debug_writer **dwriter)
{
	i_free(*dwriter);
	*dwriter = NULL;
}

void sieve_binary_debug_emit
(struct sieve_binary_debug_writer *dwriter, sieve_size_t code_address, 
	unsigned int code_line, unsigned int code_column)
{
	struct sieve_binary_block *sblock = dwriter->sblock;
	sieve_size_t address_inc = code_address - dwriter->address;
	unsigned int line_inc = code_line - dwriter->line;
	unsigned int sp_opcode = 0;
	
	/* Check for applicability of special opcode */
	if ( (LINPROG_LINE_BASE + LINPROG_LINE_RANGE - 1) >= line_inc ) {
		sp_opcode = LINPROG_OP_SPECIAL_BASE + (line_inc - LINPROG_LINE_BASE) + 
			(LINPROG_LINE_RANGE * address_inc);
		
		if ( sp_opcode > 255 )
			sp_opcode = 0;
	}

	/* Update line and address */
	if ( sp_opcode == 0 ) {
		if ( line_inc > 0 ) {
			(void)sieve_binary_emit_byte(sblock, LINPROG_OP_ADVANCE_LINE);
			(void)sieve_binary_emit_unsigned(sblock, line_inc);
		}

		if ( address_inc > 0 ) {
			(void)sieve_binary_emit_byte(sblock, LINPROG_OP_ADVANCE_PC);
			(void)sieve_binary_emit_unsigned(sblock, address_inc);
		}
	} else {
		(void)sieve_binary_emit_byte(sblock, sp_opcode); 	
	}

	/* Set column */
	if ( dwriter->column != code_column ) {
		(void)sieve_binary_emit_byte(sblock, LINPROG_OP_SET_COLUMN);
		(void)sieve_binary_emit_unsigned(sblock, code_column);
	}

	/* Generate matrix row */
	(void)sieve_binary_emit_byte(sblock, LINPROG_OP_COPY);

	dwriter->address = code_address;
	dwriter->line = code_line;
	dwriter->column = code_column;
}

/*
 * Debug reader
 */

struct sieve_binary_debug_reader {
	struct sieve_binary_block *sblock;

	sieve_size_t address, last_address;
	unsigned long int line, last_line;

	unsigned long int column;

	sieve_size_t state;
};

struct sieve_binary_debug_reader *sieve_binary_debug_reader_init
(struct sieve_binary_block *sblock)
{
	struct sieve_binary_debug_reader *dreader;

	dreader = i_new(struct sieve_binary_debug_reader, 1);
	dreader->sblock = sblock;

	return dreader;
}

void sieve_binary_debug_reader_deinit
(struct sieve_binary_debug_reader **dreader)
{
	i_free(*dreader);
	*dreader = NULL;
}

void sieve_binary_debug_reader_reset
(struct sieve_binary_debug_reader *dreader)
{
	dreader->address = 0;
	dreader->line = 0;
	dreader->column = 0;
	dreader->state = 0;
}

unsigned int sieve_binary_debug_read_line
(struct sieve_binary_debug_reader *dreader, sieve_size_t code_address)
{
	size_t linprog_size; 
	sieve_size_t address;
	unsigned long int line;

	if ( code_address < dreader->last_address )
		sieve_binary_debug_reader_reset(dreader);

	if ( code_address >= dreader->last_address && 
		code_address < dreader->address ) {
		debug_printf("%08llx: NOOP [%08llx]\n", 
			(unsigned long long) dreader->state, (unsigned long long) code_address);
		return dreader->last_line;
	}

	address = dreader->address;
	line = dreader->line;

	debug_printf("%08llx: READ [%08llx]\n", 
		(unsigned long long) dreader->state, (unsigned long long) code_address);

	linprog_size = sieve_binary_block_get_size(dreader->sblock);
	while ( dreader->state < linprog_size ) {
		unsigned int opcode;
		unsigned int value;

		if ( sieve_binary_read_byte(dreader->sblock, &dreader->state, &opcode) ) {
			switch ( opcode ) {

			case LINPROG_OP_COPY:
				debug_printf("%08llx: COPY ==> %08llx: %ld\n", 
					(unsigned long long) dreader->state, (unsigned long long) address, 
					line);
			
				dreader->last_address = dreader->address;				
				dreader->last_line = dreader->line;				

				dreader->address = address;
				dreader->line = line;

				if ( code_address < address ) {
					return dreader->last_line;
				} else if ( code_address == address ) {
					return dreader->line;
				}
				break;

			case LINPROG_OP_ADVANCE_PC:
				debug_printf("%08llx: ADV_PC\n", (unsigned long long) dreader->state);
				if ( !sieve_binary_read_unsigned
					(dreader->sblock, &dreader->state, &value) ) {
					sieve_binary_debug_reader_reset(dreader);
					return 0;
				}
				debug_printf("        : + %d\n", value);
				address += value;
				break;

			case LINPROG_OP_ADVANCE_LINE:
				debug_printf("%08llx: ADV_LINE\n", (unsigned long long) dreader->state);
				if ( !sieve_binary_read_unsigned
					(dreader->sblock, &dreader->state, &value) ) {
					sieve_binary_debug_reader_reset(dreader);
					return 0;
				}
				debug_printf("        : + %d\n", value);
				line += value;
				break;

			case LINPROG_OP_SET_COLUMN:
				debug_printf("%08llx: SET_COL\n", (unsigned long long) dreader->state);
				if ( !sieve_binary_read_unsigned
					(dreader->sblock, &dreader->state, &value) ) {
					sieve_binary_debug_reader_reset(dreader);
					return 0;
				}
				debug_printf("        : = %d\n", value);
				dreader->column = value;
				break;			

			default:
				opcode -= LINPROG_OP_SPECIAL_BASE;

				address += (opcode / LINPROG_LINE_RANGE);
				line += LINPROG_LINE_BASE + (opcode % LINPROG_LINE_RANGE);

				debug_printf("%08llx: SPECIAL\n", (unsigned long long) dreader->state);
				debug_printf("        :  +A %d +L %d\n", (opcode / LINPROG_LINE_RANGE),
					LINPROG_LINE_BASE + (opcode % LINPROG_LINE_RANGE));				
				break;
			}
		} else {
			debug_printf("OPCODE READ FAILED\n");
			sieve_binary_debug_reader_reset(dreader);
			return 0;
		}
	}

	return dreader->line;
}

