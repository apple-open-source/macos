/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "ioloop.h"
#include "mempool.h"
#include "array.h"
#include "str.h"
#include "str-sanitize.h"
#include "mail-storage.h"

#include "sieve-common.h"
#include "sieve-stringlist.h"
#include "sieve-error.h"
#include "sieve-extensions.h"
#include "sieve-runtime.h"
#include "sieve-runtime-trace.h"
#include "sieve-address.h"

#include "sieve-message.h"

/*
 * Message transmission
 */
 
const char *sieve_message_get_new_id
(const struct sieve_script_env *senv)
{
	static int count = 0;
	
	return t_strdup_printf("<dovecot-sieve-%s-%s-%d@%s>",
		dec2str(ioloop_timeval.tv_sec), dec2str(ioloop_timeval.tv_usec),
    count++, senv->hostname);
}

/* 
 * Message context 
 */

struct sieve_message_context {
	pool_t pool;
	int refcount;

	struct sieve_instance *svinst; 

	const struct sieve_message_data *msgdata;

	/* Normalized envelope addresses */

	bool envelope_parsed;

	const struct sieve_address *envelope_sender;
	const struct sieve_address *envelope_orig_recipient;
	const struct sieve_address *envelope_final_recipient;

	
	/* Context data for extensions */
	ARRAY_DEFINE(ext_contexts, void *); 
};

struct sieve_message_context *sieve_message_context_create
(struct sieve_instance *svinst, const struct sieve_message_data *msgdata)
{
	struct sieve_message_context *msgctx;
	
	msgctx = i_new(struct sieve_message_context, 1);
	msgctx->refcount = 1;
	msgctx->svinst = svinst;

	msgctx->msgdata = msgdata;
		
	sieve_message_context_flush(msgctx);

	return msgctx;
}

void sieve_message_context_ref(struct sieve_message_context *msgctx)
{
	msgctx->refcount++;
}

void sieve_message_context_unref(struct sieve_message_context **msgctx)
{
	i_assert((*msgctx)->refcount > 0);

	if (--(*msgctx)->refcount != 0)
		return;
	
	pool_unref(&((*msgctx)->pool));
		
	i_free(*msgctx);
	*msgctx = NULL;
}

void sieve_message_context_flush(struct sieve_message_context *msgctx)
{
	pool_t pool;

	if ( msgctx->pool != NULL ) {
		pool_unref(&msgctx->pool);
	}

	pool = pool_alloconly_create("sieve_message_context", 1024);
	msgctx->pool = pool;

	msgctx->envelope_orig_recipient = NULL;
	msgctx->envelope_final_recipient = NULL;
	msgctx->envelope_sender = NULL;
	msgctx->envelope_parsed = FALSE;

	p_array_init(&msgctx->ext_contexts, pool, 
		sieve_extensions_get_count(msgctx->svinst));
}

pool_t sieve_message_context_pool(struct sieve_message_context *msgctx)
{
	return msgctx->pool;
}

/* Extension support */

void sieve_message_context_extension_set
(struct sieve_message_context *msgctx, const struct sieve_extension *ext, 
	void *context)
{
	if ( ext->id < 0 ) return;

	array_idx_set(&msgctx->ext_contexts, (unsigned int) ext->id, &context);	
}

const void *sieve_message_context_extension_get
(struct sieve_message_context *msgctx, const struct sieve_extension *ext) 
{
	void * const *ctx;

	if  ( ext->id < 0 || ext->id >= (int) array_count(&msgctx->ext_contexts) )
		return NULL;
	
	ctx = array_idx(&msgctx->ext_contexts, (unsigned int) ext->id);		

	return *ctx;
}

/* Envelope */

static void sieve_message_envelope_parse(struct sieve_message_context *msgctx)
{
	const struct sieve_message_data *msgdata = msgctx->msgdata;
	struct sieve_instance *svinst = msgctx->svinst;

	/* FIXME: log parse problems properly; logs only 'failure' now */

	msgctx->envelope_orig_recipient = sieve_address_parse_envelope_path
		(msgctx->pool, msgdata->orig_envelope_to);	

	if ( msgctx->envelope_orig_recipient == NULL ) {
		sieve_sys_error(svinst,
			"original envelope recipient address '%s' is unparsable",
			msgdata->orig_envelope_to); 
	} else if ( msgctx->envelope_orig_recipient->local_part == NULL ) {
		sieve_sys_error(svinst,
			"original envelope recipient address '%s' is a null path",
			msgdata->orig_envelope_to);
	} 

	msgctx->envelope_final_recipient = sieve_address_parse_envelope_path
		(msgctx->pool, msgdata->final_envelope_to);	
	
	if ( msgctx->envelope_final_recipient == NULL ) {
		if ( msgctx->envelope_orig_recipient != NULL ) {
			sieve_sys_error(svinst,
				"final envelope recipient address '%s' is unparsable",
				msgdata->final_envelope_to);
		} 
	} else if ( msgctx->envelope_final_recipient->local_part == NULL ) {
		if ( strcmp(msgdata->orig_envelope_to, msgdata->final_envelope_to) != 0 ) {
			sieve_sys_error(svinst,
				"final envelope recipient address '%s' is a null path",
				msgdata->final_envelope_to);
		}
	}

	msgctx->envelope_sender = sieve_address_parse_envelope_path
		(msgctx->pool, msgdata->return_path);	

	if ( msgctx->envelope_sender == NULL ) {
		sieve_sys_error(svinst, 
			"envelope sender address '%s' is unparsable", 
			msgdata->return_path);
	}

	msgctx->envelope_parsed = TRUE;
}

const struct sieve_address *sieve_message_get_orig_recipient_address
(struct sieve_message_context *msgctx)
{
	if ( !msgctx->envelope_parsed ) 
		sieve_message_envelope_parse(msgctx);

	return msgctx->envelope_orig_recipient;
}

const struct sieve_address *sieve_message_get_final_recipient_address
(struct sieve_message_context *msgctx)
{
	if ( !msgctx->envelope_parsed ) 
		sieve_message_envelope_parse(msgctx);

	return msgctx->envelope_final_recipient;
}

const struct sieve_address *sieve_message_get_sender_address
(struct sieve_message_context *msgctx)
{
	if ( !msgctx->envelope_parsed ) 
		sieve_message_envelope_parse(msgctx);

	return msgctx->envelope_sender;	
} 

const char *sieve_message_get_orig_recipient
(struct sieve_message_context *msgctx)
{
	if ( !msgctx->envelope_parsed ) 
		sieve_message_envelope_parse(msgctx);

	return sieve_address_to_string(msgctx->envelope_orig_recipient);
}

const char *sieve_message_get_final_recipient
(struct sieve_message_context *msgctx)
{
	if ( !msgctx->envelope_parsed ) 
		sieve_message_envelope_parse(msgctx);

	return sieve_address_to_string(msgctx->envelope_final_recipient);
}

const char *sieve_message_get_sender
(struct sieve_message_context *msgctx)
{
	if ( !msgctx->envelope_parsed ) 
		sieve_message_envelope_parse(msgctx);

	return sieve_address_to_string(msgctx->envelope_sender);
} 

/*
 * Header stringlist
 */

/* Forward declarations */

static int sieve_message_header_stringlist_next_item
	(struct sieve_stringlist *_strlist, string_t **str_r);
static void sieve_message_header_stringlist_reset
	(struct sieve_stringlist *_strlist);

/* String list object */

struct sieve_message_header_stringlist {
	struct sieve_stringlist strlist;

	struct sieve_stringlist *field_names;

	const char *const *headers;
	int headers_index;

	unsigned int mime_decode:1;
};

struct sieve_stringlist *sieve_message_header_stringlist_create
(const struct sieve_runtime_env *renv, struct sieve_stringlist *field_names,
	bool mime_decode)
{
	struct sieve_message_header_stringlist *strlist;

	strlist = t_new(struct sieve_message_header_stringlist, 1);
	strlist->strlist.runenv = renv;
	strlist->strlist.exec_status = SIEVE_EXEC_OK;
	strlist->strlist.next_item = sieve_message_header_stringlist_next_item;
	strlist->strlist.reset = sieve_message_header_stringlist_reset;
	strlist->field_names = field_names;
	strlist->mime_decode = mime_decode;

	return &strlist->strlist;
}

static inline string_t *_header_right_trim(const char *raw) 
{
	string_t *result;
	int i;
	
	for ( i = strlen(raw)-1; i >= 0; i-- ) {
		if ( raw[i] != ' ' && raw[i] != '\t' ) break;
	}
	
	result = t_str_new(i+1);
	str_append_n(result, raw, i + 1);
	return result;
}

/* String list implementation */

static int sieve_message_header_stringlist_next_item
(struct sieve_stringlist *_strlist, string_t **str_r)
{
	struct sieve_message_header_stringlist *strlist = 
		(struct sieve_message_header_stringlist *) _strlist;
	const struct sieve_runtime_env *renv = _strlist->runenv;
	struct mail *mail = renv->msgdata->mail;

	*str_r = NULL;

	/* Check for end of current header list */
	if ( strlist->headers == NULL ) {
		strlist->headers_index = 0;
 	} else if ( strlist->headers[strlist->headers_index] == NULL ) {
		strlist->headers = NULL;
		strlist->headers_index = 0;
	}

	/* Fetch next header */
	while ( strlist->headers == NULL ) {
		string_t *hdr_item = NULL;
		int ret;

		/* Read next header name from source list */
		if ( (ret=sieve_stringlist_next_item(strlist->field_names, &hdr_item)) 
			<= 0 )
			return ret;

		if ( _strlist->trace ) {
			sieve_runtime_trace(renv, 0,
				"extracting `%s' headers from message",
				str_sanitize(str_c(hdr_item), 80));
		}

		/* Fetch all matching headers from the e-mail */
		if ( strlist->mime_decode ) {
			if ( mail_get_headers_utf8(mail, str_c(hdr_item), &strlist->headers) < 0 ||
				( strlist->headers != NULL && strlist->headers[0] == NULL ) ) {
				/* Try next item when this fails somehow */
				strlist->headers = NULL;
				continue;
			}
		} else {
			if ( mail_get_headers(mail, str_c(hdr_item), &strlist->headers) < 0 ||
				( strlist->headers != NULL && strlist->headers[0] == NULL ) ) {
				/* Try next item when this fails somehow */
				strlist->headers = NULL;
				continue;
			}
		}
	}

	/* Return next item */
	*str_r = _header_right_trim(strlist->headers[strlist->headers_index++]);
	return 1;
}

static void sieve_message_header_stringlist_reset
(struct sieve_stringlist *_strlist)
{
	struct sieve_message_header_stringlist *strlist = 
		(struct sieve_message_header_stringlist *) _strlist;

	strlist->headers = NULL;
	strlist->headers_index = 0;
	sieve_stringlist_reset(strlist->field_names);
}
