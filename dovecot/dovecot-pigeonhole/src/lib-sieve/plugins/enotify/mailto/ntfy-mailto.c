/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */
 
/* Notify method mailto
 * --------------------
 *
 * Authors: Stephan Bosch
 * Specification: RFC 5436
 * Implementation: full
 * Status: testing
 * 
 */
 
/* FIXME: URI syntax conforms to something somewhere in between RFC 2368 and
 *   draft-duerst-mailto-bis-05.txt. Should fully migrate to new specification
 *   when it matures. This requires modifications to the address parser (no
 *   whitespace allowed within the address itself) and UTF-8 support will be
 *   required in the URL.
 */
 
#include "lib.h"
#include "array.h"
#include "str.h"
#include "ioloop.h"
#include "str-sanitize.h"
#include "message-date.h"
#include "mail-storage.h"

#include "sieve-common.h"
#include "sieve-address.h"
#include "sieve-message.h"
#include "sieve-smtp.h"

#include "sieve-ext-enotify.h"

#include "rfc2822.h"

#include "uri-mailto.h"

/*
 * Configuration
 */
 
#define NTFY_MAILTO_MAX_RECIPIENTS  8
#define NTFY_MAILTO_MAX_HEADERS     16
#define NTFY_MAILTO_MAX_SUBJECT     256

/* 
 * Mailto notification method
 */
 
static bool ntfy_mailto_compile_check_uri
	(const struct sieve_enotify_env *nenv, const char *uri, const char *uri_body);
static bool ntfy_mailto_compile_check_from
	(const struct sieve_enotify_env *nenv, string_t *from);

static const char *ntfy_mailto_runtime_get_notify_capability
	(const struct sieve_enotify_env *nenv, const char *uri, const char *uri_body, 
		const char *capability);
static bool ntfy_mailto_runtime_check_uri
	(const struct sieve_enotify_env *nenv, const char *uri, const char *uri_body);
static bool ntfy_mailto_runtime_check_operands
	(const struct sieve_enotify_env *nenv, const char *uri,const char *uri_body, 
		string_t *message, string_t *from, pool_t context_pool, 
		void **method_context);

static int ntfy_mailto_action_check_duplicates
	(const struct sieve_enotify_env *nenv, 
		const struct sieve_enotify_action *nact,
		const struct sieve_enotify_action *nact_other);

static void ntfy_mailto_action_print
	(const struct sieve_enotify_print_env *penv, 
		const struct sieve_enotify_action *nact);	

static bool ntfy_mailto_action_execute
	(const struct sieve_enotify_exec_env *nenv, 
		const struct sieve_enotify_action *nact);

const struct sieve_enotify_method_def mailto_notify = {
	"mailto",
	NULL, NULL, 
	ntfy_mailto_compile_check_uri,
	NULL,
	ntfy_mailto_compile_check_from,
	NULL,
	ntfy_mailto_runtime_check_uri,
	ntfy_mailto_runtime_get_notify_capability,
	ntfy_mailto_runtime_check_operands,
	NULL,
	ntfy_mailto_action_check_duplicates,
	ntfy_mailto_action_print,
	ntfy_mailto_action_execute
};

/*
 * Reserved and unique headers 
 */
 
static const char *_reserved_headers[] = {
	"auto-submitted",
	"received",
	"message-id",
	"data",
	"bcc",
	"in-reply-to",
	"references",
	"resent-date",
	"resent-from",
	"resent-sender",
	"resent-to",
	"resent-cc",
 	"resent-bcc",
	"resent-msg-id",
	"from",
	"sender",
	NULL
};

static const char *_unique_headers[] = {
	"reply-to",
	NULL
};

/*
 * Method context data
 */

struct ntfy_mailto_context {
	struct uri_mailto *uri;
	const char *from_normalized;
};

/*
 * Validation
 */

static bool ntfy_mailto_compile_check_uri
(const struct sieve_enotify_env *nenv, const char *uri ATTR_UNUSED,
	const char *uri_body)
{	
	return uri_mailto_validate
		(uri_body, _reserved_headers, _unique_headers,
			NTFY_MAILTO_MAX_RECIPIENTS, NTFY_MAILTO_MAX_HEADERS, nenv->ehandler);
}

static bool ntfy_mailto_compile_check_from
(const struct sieve_enotify_env *nenv, string_t *from)
{
	const char *error;
	bool result = FALSE;

	T_BEGIN {
		result = sieve_address_validate(from, &error);

		if ( !result ) {
			sieve_enotify_error(nenv,
				"specified :from address '%s' is invalid for "
				"the mailto method: %s",
				str_sanitize(str_c(from), 128), error);
		}
	} T_END;

	return result;
}

/*
 * Runtime
 */
 
static const char *ntfy_mailto_runtime_get_notify_capability
(const struct sieve_enotify_env *nenv ATTR_UNUSED, const char *uri ATTR_UNUSED, 
	const char *uri_body, const char *capability)
{
	if ( !uri_mailto_validate(uri_body, _reserved_headers, _unique_headers,
			NTFY_MAILTO_MAX_RECIPIENTS, NTFY_MAILTO_MAX_HEADERS, NULL) ) {
		return NULL;
	}
	
	if ( strcasecmp(capability, "online") == 0 ) 
		return "maybe";
	
	return NULL;
}

static bool ntfy_mailto_runtime_check_uri
(const struct sieve_enotify_env *nenv ATTR_UNUSED, const char *uri ATTR_UNUSED,
	const char *uri_body)
{
	return uri_mailto_validate
		(uri_body, _reserved_headers, _unique_headers,
			NTFY_MAILTO_MAX_RECIPIENTS, NTFY_MAILTO_MAX_HEADERS, NULL);
}
 
static bool ntfy_mailto_runtime_check_operands
(const struct sieve_enotify_env *nenv, const char *uri ATTR_UNUSED,
	const char *uri_body, string_t *message ATTR_UNUSED, string_t *from, 
	pool_t context_pool, void **method_context)
{
	struct ntfy_mailto_context *mtctx;
	struct uri_mailto *parsed_uri;
	const char *error, *normalized;

	/* Need to create context before validation to have arrays present */
	mtctx = p_new(context_pool, struct ntfy_mailto_context, 1);

	/* Validate :from */
	if ( from != NULL ) {
		T_BEGIN {
			normalized = sieve_address_normalize(from, &error);

			if ( normalized == NULL ) {
				sieve_enotify_error(nenv,
					"specified :from address '%s' is invalid for "
					"the mailto method: %s",
					str_sanitize(str_c(from), 128), error);
			} else 
				mtctx->from_normalized = p_strdup(context_pool, normalized);
		} T_END;

		if ( !normalized ) return FALSE;
	}

	if ( (parsed_uri=uri_mailto_parse
		(uri_body, context_pool, _reserved_headers, 
			_unique_headers, NTFY_MAILTO_MAX_RECIPIENTS, NTFY_MAILTO_MAX_HEADERS,
			nenv->ehandler)) == NULL ) {
		return FALSE;
	}

	mtctx->uri = parsed_uri;
	*method_context = (void *) mtctx;
	return TRUE;	
}

/*
 * Action duplicates
 */

static int ntfy_mailto_action_check_duplicates
(const struct sieve_enotify_env *nenv ATTR_UNUSED, 
	const struct sieve_enotify_action *nact, 
	const struct sieve_enotify_action *nact_other)
{
	struct ntfy_mailto_context *mtctx = 
		(struct ntfy_mailto_context *) nact->method_context;
	struct ntfy_mailto_context *mtctx_other = 
		(struct ntfy_mailto_context *) nact_other->method_context;
	const struct uri_mailto_recipient *new_rcpts, *old_rcpts;
	unsigned int new_count, old_count, i, j;
	unsigned int del_start = 0, del_len = 0;

	new_rcpts = array_get(&mtctx->uri->recipients, &new_count);
	old_rcpts = array_get(&mtctx_other->uri->recipients, &old_count);

	for ( i = 0; i < new_count; i++ ) {
		for ( j = 0; j < old_count; j++ ) {
			if ( sieve_address_compare
				(new_rcpts[i].normalized, old_rcpts[j].normalized, TRUE) == 0 )
				break;				
		}

		if ( j == old_count ) {
			/* Not duplicate */
			if ( del_len > 0 ) {
				/* Perform pending deletion */
				array_delete(&mtctx->uri->recipients, del_start, del_len);

				/* Make sure the loop integrity is maintained */
				i -= del_len;
				new_rcpts = array_get(&mtctx->uri->recipients, &new_count);
			}
			del_len = 0;		
		} else {
			/* Mark deletion */
			if ( del_len == 0 )
				del_start = i;
			del_len++;
		}
	}

	/* Perform pending deletion */
	if ( del_len > 0 ) {
		array_delete(&mtctx->uri->recipients, del_start, del_len);			
	}

	return ( array_count(&mtctx->uri->recipients) > 0 ? 0 : 1 );
}

/*
 * Action printing
 */
 
static void ntfy_mailto_action_print
(const struct sieve_enotify_print_env *penv, 
	const struct sieve_enotify_action *nact)
{
	unsigned int count, i;
	const struct uri_mailto_recipient *recipients;
	const struct uri_mailto_header_field *headers;
	struct ntfy_mailto_context *mtctx = 
		(struct ntfy_mailto_context *) nact->method_context;
	
	/* Print main method parameters */

	sieve_enotify_method_printf
		(penv,   "    => importance   : %d\n", nact->importance);

	if ( nact->message != NULL )
		sieve_enotify_method_printf
			(penv, "    => subject      : %s\n", nact->message);
	else if ( mtctx->uri->subject != NULL )
		sieve_enotify_method_printf
			(penv, "    => subject      : %s\n", mtctx->uri->subject);

	if ( nact->from != NULL )
		sieve_enotify_method_printf
			(penv, "    => from         : %s\n", nact->from);

	/* Print mailto: recipients */

	sieve_enotify_method_printf(penv,   "    => recipients   :\n" );

	recipients = array_get(&mtctx->uri->recipients, &count);
	if ( count == 0 ) {
		sieve_enotify_method_printf(penv,   "       NONE, action has no effect\n");
	} else {
		for ( i = 0; i < count; i++ ) {
			if ( recipients[i].carbon_copy )
				sieve_enotify_method_printf
					(penv,   "       + Cc: %s\n", recipients[i].full);
			else
				sieve_enotify_method_printf
					(penv,   "       + To: %s\n", recipients[i].full);
		}
	}

	/* Print accepted headers for notification message */
	
	headers = array_get(&mtctx->uri->headers, &count);
	if ( count > 0 ) {
		sieve_enotify_method_printf(penv,   "    => headers      :\n" );	
		for ( i = 0; i < count; i++ ) {
			sieve_enotify_method_printf(penv,   "       + %s: %s\n", 
				headers[i].name, headers[i].body);
		}
	}

	/* Print body for notification message */
	
	if ( mtctx->uri->body != NULL )
		sieve_enotify_method_printf
			(penv, "    => body         : \n--\n%s\n--\n", mtctx->uri->body);

	/* Finish output with an empty line */

	sieve_enotify_method_printf(penv,   "\n");
}

/*
 * Action execution
 */

static bool _contains_8bit(const char *msg)
{
	const unsigned char *s = (const unsigned char *)msg;

	for (; *s != '\0'; s++) {
		if ((*s & 0x80) != 0)
			return TRUE;
	}
	
	return FALSE;
}

static bool ntfy_mailto_send
(const struct sieve_enotify_exec_env *nenv, 
	const struct sieve_enotify_action *nact, const char *recipient)
{ 
	const struct sieve_message_data *msgdata = nenv->msgdata;
	const struct sieve_script_env *senv = nenv->scriptenv;
	struct ntfy_mailto_context *mtctx = 
		(struct ntfy_mailto_context *) nact->method_context;	
	const char *from = NULL, *from_smtp = NULL; 
	const char *subject = mtctx->uri->subject;
	const char *body = mtctx->uri->body;
	string_t *to, *cc;
	const struct uri_mailto_recipient *recipients;
	void *smtp_handle;
	unsigned int count, i;
	FILE *f;
	const char *outmsgid;

	/* Get recipients */
	recipients = array_get(&mtctx->uri->recipients, &count);
	if ( count == 0  ) {
		sieve_enotify_warning(nenv, 
			"notify mailto uri specifies no recipients; action has no effect");
		return TRUE;
	}

	/* Just to be sure */
	if ( !sieve_smtp_available(senv) ) {
		sieve_enotify_global_warning(nenv, 
			"notify mailto method has no means to send mail");
		return TRUE;
	}
	
	/* Determine message from address */
	if ( nact->from == NULL ) {
		from = t_strdup_printf("Postmaster <%s>", senv->postmaster_address);
	} else {
		from = nact->from;
	}

	/* Determine SMTP from address */
	if ( sieve_message_get_sender(nenv->msgctx) != NULL ) {
		if ( mtctx->from_normalized == NULL ) {
			from_smtp = senv->postmaster_address;
		} else {
			from_smtp = mtctx->from_normalized;
		}
	}
	
	/* Determine subject */
	if ( nact->message != NULL ) {
		/* FIXME: handle UTF-8 */
		subject = str_sanitize(nact->message, NTFY_MAILTO_MAX_SUBJECT);
	} else if ( subject == NULL ) {
		const char *const *hsubject;
		
		/* Fetch subject from original message */
		if ( mail_get_headers_utf8
			(msgdata->mail, "subject", &hsubject) >= 0 )
			subject = str_sanitize(t_strdup_printf("Notification: %s", hsubject[0]), 
				NTFY_MAILTO_MAX_SUBJECT);
		else
			subject = "Notification: (no subject)";
	}

	/* Compose To and Cc headers */
	to = NULL;
	cc = NULL;
	for ( i = 0; i < count; i++ ) {
		if ( recipients[i].carbon_copy ) {
			if ( cc == NULL ) {
				cc = t_str_new(256);
				str_append(cc, recipients[i].full);
			} else {
				str_append(cc, ", ");
				str_append(cc, recipients[i].full);
			}
		} else {
			if ( to == NULL ) {
				to = t_str_new(256);
				str_append(to, recipients[i].full);
			} else {
				str_append(to, ", ");
				str_append(to, recipients[i].full);
			}
		}
	}

	/* Send message to all recipients */
	for ( i = 0; i < count; i++ ) {
		const struct uri_mailto_header_field *headers;
		unsigned int h, hcount;

        smtp_handle = sieve_smtp_open
            (senv, recipients[i].normalized, from_smtp, &f);
		outmsgid = sieve_message_get_new_id(senv);
	
		rfc2822_header_field_write(f, "X-Sieve", SIEVE_IMPLEMENTATION);
		rfc2822_header_field_write(f, "Message-ID", outmsgid);
		rfc2822_header_field_write(f, "Date", message_date_create(ioloop_time));
		rfc2822_header_field_utf8_printf(f, "Subject", "%s", subject);

		rfc2822_header_field_utf8_printf(f, "From", "%s", from);

		if ( to != NULL )
			rfc2822_header_field_utf8_printf(f, "To", "%s", str_c(to));
		
		if ( cc != NULL )
			rfc2822_header_field_utf8_printf(f, "Cc", "%s", str_c(cc));
			
		rfc2822_header_field_printf(f, "Auto-Submitted", 
			"auto-notified; owner-email=\"%s\"", recipient);
		rfc2822_header_field_write(f, "Precedence", "bulk");

		/* Set importance */
		switch ( nact->importance ) {
		case 1:
			rfc2822_header_field_write(f, "X-Priority", "1 (Highest)");
			rfc2822_header_field_write(f, "Importance", "High");
			break;
		case 3:
			rfc2822_header_field_write(f, "X-Priority", "5 (Lowest)");
			rfc2822_header_field_write(f, "Importance", "Low");
			break;
		case 2:
		default:
			rfc2822_header_field_write(f, "X-Priority", "3 (Normal)");
			rfc2822_header_field_write(f, "Importance", "Normal");
			break;
		}
		
		/* Add custom headers */
		
		headers = array_get(&mtctx->uri->headers, &hcount);
		for ( h = 0; h < hcount; h++ ) {
			const char *name = rfc2822_header_field_name_sanitize(headers[h].name);
		
			rfc2822_header_field_write(f, name, headers[h].body);
		}
			
		/* Generate message body */
		if ( body != NULL ) {
			if (_contains_8bit(body)) {
				rfc2822_header_field_write(f, "MIME-Version", "1.0");
				rfc2822_header_field_write
					(f, "Content-Type", "text/plain; charset=UTF-8");
				rfc2822_header_field_write(f, "Content-Transfer-Encoding", "8bit");
			}
			
			fprintf(f, "\r\n");
			fprintf(f, "%s\r\n", body);
			
		} else {
			fprintf(f, "\r\n");
			fprintf(f, "Notification of new message.\r\n");
		}
	
		if ( sieve_smtp_close(senv, smtp_handle) ) {
			sieve_enotify_global_info(nenv, 
				"sent mail notification to <%s>", 
				str_sanitize(recipients[i].normalized, 80));
		} else {
			sieve_enotify_global_error(nenv,
				"failed to send mail notification to <%s> "
				"(refer to system log for more information)", 
				str_sanitize(recipients[i].normalized, 80));
		}
	}

	return TRUE;
}

static bool ntfy_mailto_action_execute
(const struct sieve_enotify_exec_env *nenv, 
	const struct sieve_enotify_action *nact)
{
	const char *const *headers;
	const char *sender = sieve_message_get_sender(nenv->msgctx);
	const char *recipient = sieve_message_get_final_recipient(nenv->msgctx);

	/* Is the recipient unset? 
	 */
	if ( recipient == NULL ) {
		sieve_enotify_global_warning(nenv, 
			"notify mailto action aborted: envelope recipient is <>");
		return TRUE;
	}
	
	/* Is the message an automatic reply ? */
	if ( mail_get_headers
		(nenv->msgdata->mail, "auto-submitted", &headers) >= 0 ) {
		const char *const *hdsp = headers;

		/* Theoretically multiple headers could exist, so lets make sure */
		while ( *hdsp != NULL ) {
			if ( strcasecmp(*hdsp, "no") != 0 ) {
				sieve_enotify_global_info(nenv, 
					"not sending notification for auto-submitted message from <%s>", 
					str_sanitize(sender, 128));
					return TRUE;
			}
			hdsp++;
		}
	}

	return ntfy_mailto_send(nenv, nact, recipient);
}




