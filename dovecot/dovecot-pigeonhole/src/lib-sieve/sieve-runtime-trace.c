/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "ostream.h"

#include "sieve-common.h"
#include "sieve-script.h"
#include "sieve-binary.h"
#include "sieve-code.h"
#include "sieve-interpreter.h"
#include "sieve-runtime.h"
#include "sieve-runtime-trace.h"

static inline string_t *_trace_line_new
(const struct sieve_runtime_env *renv, sieve_size_t address, unsigned int cmd_line)
{
	string_t *trline;
	unsigned int i;
	
	trline = t_str_new(128);
	if ( (renv->trace->config.flags & SIEVE_TRFLG_ADDRESSES) > 0 )
		str_printfa(trline, "%08llx: ", (unsigned long long) address);
	if ( cmd_line > 0 )	
		str_printfa(trline, "%4d: ", cmd_line); 
	else
		str_append(trline, "      "); 

	for ( i = 0; i < renv->trace->indent; i++ )
		str_append(trline, "  "); 

	return trline;
}

static inline void _trace_line_print
(string_t *trline, const struct sieve_runtime_env *renv)
{
	str_append_c(trline, '\n');

	o_stream_send(renv->trace->stream, str_data(trline), str_len(trline));
}

static inline void _trace_line_print_empty
(const struct sieve_runtime_env *renv)
{
	o_stream_send_str(renv->trace->stream, "\n");
}

/*
 * Trace errors
 */

void _sieve_runtime_trace_error
(const struct sieve_runtime_env *renv, const char *fmt, va_list args)
{
	string_t *trline = _trace_line_new(renv, renv->pc, 0);

	str_printfa(trline, "%s: #ERROR#: ", sieve_operation_mnemonic(renv->oprtn));
	str_vprintfa(trline, fmt, args);

	_trace_line_print(trline, renv);
}

void _sieve_runtime_trace_operand_error
(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd,
	const char *fmt, va_list args)
{
	string_t *trline = _trace_line_new(renv, oprnd->address,
		sieve_runtime_get_source_location(renv, oprnd->address));

	str_printfa(trline, "%s: #ERROR#: ", sieve_operation_mnemonic(renv->oprtn));

	if ( oprnd->field_name != NULL )
		str_printfa(trline, "%s: ", oprnd->field_name);

	str_vprintfa(trline, fmt, args);

	_trace_line_print(trline, renv);
}

/*
 * Trace info
 */

static inline void _sieve_runtime_trace_vprintf
(const struct sieve_runtime_env *renv, sieve_size_t address,
	unsigned int cmd_line, const char *fmt, va_list args)
{	
	string_t *trline = _trace_line_new(renv, address, cmd_line);
		
	str_vprintfa(trline, fmt, args); 
	
	_trace_line_print(trline, renv);
}

static inline void _sieve_runtime_trace_printf
(const struct sieve_runtime_env *renv, sieve_size_t address,
	unsigned int cmd_line, const char *fmt, ...)
{
	va_list args;
	
	va_start(args, fmt);
	_sieve_runtime_trace_vprintf(renv, address, cmd_line, fmt, args); 
	va_end(args);
}

void _sieve_runtime_trace
(const struct sieve_runtime_env *renv, const char *fmt, va_list args)
{	
	_sieve_runtime_trace_vprintf
		(renv, renv->oprtn->address, sieve_runtime_get_command_location(renv),
			fmt, args); 
}

void _sieve_runtime_trace_address
(const struct sieve_runtime_env *renv, sieve_size_t address, 
	const char *fmt, va_list args)
{	
	_sieve_runtime_trace_vprintf
		(renv, address, sieve_runtime_get_source_location(renv, address), fmt,
			args); 
}

/* 
 * Trace boundaries
 */

void _sieve_runtime_trace_begin(const struct sieve_runtime_env *renv)
{
	const char *script_name = ( renv->script != NULL ? 
		sieve_script_name(renv->script) : sieve_binary_path(renv->sbin) );

	_trace_line_print_empty(renv);
	_sieve_runtime_trace_printf(renv, renv->pc, 0, 
		"## Started executing script '%s'", script_name);
}

void _sieve_runtime_trace_end(const struct sieve_runtime_env *renv)
{
	const char *script_name = ( renv->script != NULL ? 
		sieve_script_name(renv->script) : sieve_binary_path(renv->sbin) );

	_sieve_runtime_trace_printf(renv, renv->pc, 0, 
		"## Finished executing script '%s'", script_name);
	_trace_line_print_empty(renv);
}

void _sieve_runtime_trace_sep(const struct sieve_runtime_env *renv)
{
	_trace_line_print_empty(renv);	
}

