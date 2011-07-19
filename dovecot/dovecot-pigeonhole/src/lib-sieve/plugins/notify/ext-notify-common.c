/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "rfc822-parser.h"
#include "message-parser.h"
#include "message-decoder.h"

#include "sieve-common.h"
#include "sieve-code.h"
#include "sieve-message.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-actions.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-result.h"

#include "ext-notify-common.h"

#include <ctype.h>

/*
 * Importance argument
 */

static bool tag_importance_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg,
		struct sieve_command *cmd);

static const struct sieve_argument_def importance_low_tag = {
	"low",
	NULL,
	tag_importance_validate,
	NULL, NULL, NULL
};

static const struct sieve_argument_def importance_normal_tag = {
	"normal",
	NULL, 
	tag_importance_validate,
	NULL, NULL, NULL
};

static const struct sieve_argument_def importance_high_tag = {
	"high",
	NULL,
	tag_importance_validate,
	NULL, NULL, NULL
};

static bool tag_importance_validate
(struct sieve_validator *valdtr ATTR_UNUSED, struct sieve_ast_argument **arg,
	struct sieve_command *cmd ATTR_UNUSED)
{
	struct sieve_ast_argument *tag = *arg;

	if ( sieve_argument_is(tag, importance_low_tag) )
		sieve_ast_argument_number_substitute(tag, 3);
	else if ( sieve_argument_is(tag, importance_normal_tag) )
		sieve_ast_argument_number_substitute(tag, 2);
	else
		sieve_ast_argument_number_substitute(tag, 1);

	tag->argument = sieve_argument_create
		(tag->ast, &number_argument, tag->argument->ext, tag->argument->id_code);

	/* Skip parameter */
	*arg = sieve_ast_argument_next(*arg);

	return TRUE;
}

void ext_notify_register_importance_tags
(struct sieve_validator *valdtr, struct sieve_command_registration *cmd_reg, 
	const struct sieve_extension *ext, unsigned int id_code)
{
	sieve_validator_register_tag(valdtr, cmd_reg, ext, &importance_low_tag, id_code);
	sieve_validator_register_tag(valdtr, cmd_reg, ext, &importance_normal_tag, id_code);
	sieve_validator_register_tag(valdtr, cmd_reg, ext, &importance_high_tag, id_code);
}

/*
 * Body extraction
 */

/* FIXME: overlaps somewhat with body extension */

struct ext_notify_message_context {
	pool_t pool;
	buffer_t *body_text;
};

static struct ext_notify_message_context *ext_notify_get_message_context
(const struct sieve_extension *this_ext, struct sieve_message_context *msgctx)
{
	struct ext_notify_message_context *ctx;
	
	/* Get message context (contains cached message body information) */
	ctx = (struct ext_notify_message_context *)
		sieve_message_context_extension_get(msgctx, this_ext);
	
	/* Create it if it does not exist already */
	if ( ctx == NULL ) {
		pool_t pool = sieve_message_context_pool(msgctx);
		ctx = p_new(pool, struct ext_notify_message_context, 1);	
		ctx->pool = pool;
		ctx->body_text = NULL;

		/* Register context */
		sieve_message_context_extension_set
			(msgctx, this_ext, (void *) ctx);
	}
	
	return ctx;
}

static bool _is_text_content(const struct message_header_line *hdr)
{
	struct rfc822_parser_context parser;
	string_t *content_type;
	const char *data;

	/* Initialize parsing */
	rfc822_parser_init(&parser, hdr->full_value, hdr->full_value_len, NULL);
	(void)rfc822_skip_lwsp(&parser);

	/* Parse content type */
	content_type = t_str_new(64);
	if (rfc822_parse_content_type(&parser, content_type) < 0)
		return FALSE;

	/* Content-type value must end here, otherwise it is invalid after all */
	(void)rfc822_skip_lwsp(&parser);
	if ( parser.data != parser.end && *parser.data != ';' )
		return FALSE;

	/* Success */
	data = str_c(content_type);
	if ( strncmp(data, "text", 4) == 0 && data[4] == '/' ) {
		return TRUE;
	}

	return FALSE;
}

static buffer_t *cmd_notify_extract_body_text
(const struct sieve_runtime_env *renv)
{ 
	const struct sieve_extension *this_ext = renv->oprtn->ext;
	struct ext_notify_message_context *mctx;
	struct message_parser_ctx *parser;
	struct message_decoder_context *decoder;
	struct message_part *parts;
	struct message_block block, decoded;
	struct istream *input;
	bool is_text, save_body;
	int ret;
	
	/* Return cached result if available */
	mctx = ext_notify_get_message_context(this_ext, renv->msgctx);
	if ( mctx->body_text != NULL ) {
		return mctx->body_text;	
	}

	/* Create buffer */
	mctx->body_text = buffer_create_dynamic(mctx->pool, 1024*64);

	/* Get the message stream */
	if ( mail_get_stream(renv->msgdata->mail, NULL, NULL, &input) < 0 )
		return FALSE;
			
	/* Initialize body decoder */
	decoder = message_decoder_init(FALSE);
	
	parser = message_parser_init(mctx->pool, input, 0, 0);
	is_text = TRUE;
	save_body = FALSE;
	while ( (ret = message_parser_parse_next_block(parser, &block)) > 0 ) {		
		if ( block.hdr != NULL || block.size == 0 ) {
			/* Decode block */
			(void)message_decoder_decode_next_block(decoder, &block, &decoded);

			/* Check for end of headers */
			if ( block.hdr == NULL ) {
				save_body = is_text;
				continue;
			}
							
			/* We're interested of only Content-Type: header */
			if ( strcasecmp(block.hdr->name, "Content-Type" ) != 0)
				continue;

			/* Header can have folding whitespace. Acquire the full value before 
			 * continuing
			 */
			if ( block.hdr->continues ) {
				block.hdr->use_full_value = TRUE;
				continue;
			}
		
			/* Is it a text part? */
			T_BEGIN {
				is_text = _is_text_content(block.hdr);
			} T_END;

			continue;
		}

		/* Read text body */
		if ( save_body ) {
			(void)message_decoder_decode_next_block(decoder, &block, &decoded);
			buffer_append(mctx->body_text, decoded.data, decoded.size);
			is_text = TRUE;			
		}
	}

	/* Cleanup */
	(void)message_parser_deinit(&parser, &parts);
	message_decoder_deinit(&decoder);
	
	/* Return status */
	return mctx->body_text;
}

void ext_notify_construct_message
(const struct sieve_runtime_env *renv, const char *msg_format, 
	string_t *out_msg)
{
	const struct sieve_message_data *msgdata = renv->msgdata;
	const char *p;

	if ( msg_format == NULL )
		msg_format = "$from$: $subject$";
 
	/* Scan message for substitutions */
	p = msg_format;
	while ( *p != '\0' ) {
		const char *const *header;

		if ( strncasecmp(p, "$from$", 6) == 0 ) {
			p += 6;
		
			/* Fetch sender from oriinal message */
			if ( mail_get_headers_utf8(msgdata->mail, "from", &header) >= 0 )
				 str_append(out_msg, header[0]); 

		} else if ( strncasecmp(p, "$env-from$", 10) == 0 ) {
			p += 10;

			if ( msgdata->return_path != NULL ) 
				str_append(out_msg, msgdata->return_path);

		} else if ( strncasecmp(p, "$subject$", 9) == 0 ) {	
			p += 9;

			/* Fetch sender from oriinal message */
			if ( mail_get_headers_utf8(msgdata->mail, "subject", &header) >= 0 )
				 str_append(out_msg, header[0]); 
			
		} else if ( strncasecmp(p, "$text", 5) == 0 
			&& (p[5] == '[' || p[5] == '$') ) {
			size_t num = 0;
			const char *begin = p;
			bool valid = TRUE;

			p += 5;
			if ( *p == '[' ) {
				p += 1;

				while ( i_isdigit(*p) ) {
					num = num * 10 + (*p - '0');
					p++;
				}

				if ( *p++ != ']' || *p++ != '$' ) {
					str_append_n(out_msg, begin, p-begin);
					valid = FALSE;										
				}	    	
			} else {
				p += 1;			
			}

			if ( valid ) {
				size_t body_size;
				const char *body_text = (const char *)
					buffer_get_data(cmd_notify_extract_body_text(renv), &body_size);

				if ( num > 0 && num < body_size) 
					str_append_n(out_msg, body_text, num);
				else						
					str_append_n(out_msg, body_text, body_size);
			}
		} else {
			size_t len;

			/* Find next substitution */
			len = strcspn(p + 1, "$") + 1; 

			/* Copy normal text */
			str_append_n(out_msg, p, len);
			p += len;
		}
  }
}
