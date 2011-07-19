/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include <string.h>
#include "login-common.h"
#include "ioloop.h"
#include "istream.h"
#include "ostream.h"
#include "str.h"
#include "str-sanitize.h"
#include "safe-memset.h"
#include "buffer.h"
#include "base64.h"

#include "client.h"
#include "client-authenticate.h"

#include "managesieve-quote.h"
#include "managesieve-proxy.h"
#include "managesieve-parser.h"

enum {
	PROXY_STATE_INITIAL,
	PROXY_STATE_TLS_START,
	PROXY_STATE_TLS_READY,
	PROXY_STATE_AUTHENTICATE,
};

typedef enum {
	MANAGESIEVE_RESPONSE_NONE,
	MANAGESIEVE_RESPONSE_OK,
	MANAGESIEVE_RESPONSE_NO,
	MANAGESIEVE_RESPONSE_BYE
} managesieve_response_t;

static void proxy_free_password(struct client *client)
{   
	if (client->proxy_password == NULL)
		return;

	safe_memset(client->proxy_password, 0, strlen(client->proxy_password));
	i_free_and_null(client->proxy_password);
}

static void get_plain_auth(struct client *client, string_t *dest)
{   
	string_t *str, *base64;

	str = t_str_new(128);
	if ( client->proxy_master_user == NULL ) {
		str_append_c(str, '\0');
		str_append(str, client->proxy_user);
	} else {
		str_append(str, client->proxy_user);
		str_append_c(str, '\0');
		str_append(str, client->proxy_master_user);
	}
	str_append_c(str, '\0');
	str_append(str, client->proxy_password);

	base64 = t_str_new(128);
	base64_encode(str_data(str), str_len(str), base64);

	managesieve_quote_append_string(dest, str_c(base64), FALSE);
}

static int proxy_write_login(struct managesieve_client *client, string_t *str)
{   
	if ( !client->proxy_sasl_plain ) {
		client_log_err(&client->common, "proxy: "
			"Server does not support required PLAIN SASL mechanism");
		return -1;
	}

	/*   Send command */
	str_append(str, "AUTHENTICATE \"PLAIN\" ");
	get_plain_auth(&client->common, str);
	proxy_free_password(&client->common);
	str_append(str, "\r\n");

	return 1;
}

static managesieve_response_t proxy_read_response
(struct managesieve_arg *args)
{
	if ( args[0].type == MANAGESIEVE_ARG_ATOM ) {
		const char *response = MANAGESIEVE_ARG_STR(&(args[0]));

		if ( strcasecmp(response, "OK") == 0 ) {
			/* Received OK response; greeting is finished */
			return MANAGESIEVE_RESPONSE_OK; 

        } else if ( strcasecmp(response, "NO") == 0 ) {
			/* Received OK response; greeting is finished */
			return MANAGESIEVE_RESPONSE_NO; 

        } else if ( strcasecmp(response, "BYE") == 0 ) {
			/* Received OK response; greeting is finished */
			return MANAGESIEVE_RESPONSE_BYE;
 
		}
	}

	return MANAGESIEVE_RESPONSE_NONE;
}

static int proxy_input_capability
(struct managesieve_client *client, const char *line, 
	managesieve_response_t *resp_r)
{   
	struct istream *input;
	struct managesieve_parser *parser;
 	struct managesieve_arg *args;
	int ret;
	bool fatal = FALSE;

	*resp_r = MANAGESIEVE_RESPONSE_NONE;

	/* Build an input stream for the managesieve parser 
	 *  FIXME: It would be nice if the line-wise parsing could be
	 *    substituded by something similar to the command line interpreter.
	 *    However, the current login_proxy structure does not make streams
	 *    known until inside proxy_input handler.
	 */
	line = t_strconcat(line, "\r\n", NULL);
	input = i_stream_create_from_data(line, strlen(line));
	parser = managesieve_parser_create(input, NULL, MAX_MANAGESIEVE_LINE);
	managesieve_parser_reset(parser);

	/* Parse input 
	 *  FIXME: Theoretically the OK response could include a 
	 *   response code which could be rejected by the parser. 
	 */ 
	(void)i_stream_read(input);
	ret = managesieve_parser_read_args(parser, 2, 0, &args);
		
	if ( ret >= 1 ) {
		if ( args[0].type == MANAGESIEVE_ARG_ATOM ) {
			*resp_r = proxy_read_response(args);

			if ( *resp_r == MANAGESIEVE_RESPONSE_NONE ) {
				client_log_err(&client->common, t_strdup_printf("proxy: "
					"Remote sent invalid response: %s",
					str_sanitize(line,160)));
		
				fatal = TRUE;
			}
      	} else if ( args[0].type == MANAGESIEVE_ARG_STRING ) {
			const char *capability = MANAGESIEVE_ARG_STR(&(args[0]));

        	if ( strcasecmp(capability, "SASL") == 0 ) {
				/* Check whether the server supports the SASL mechanism 
		    	 * we are going to use (currently only PLAIN supported). 
				 */
				if ( ret == 2 && args[1].type == MANAGESIEVE_ARG_STRING ) {
					const char *const *mechs = 
						t_strsplit(MANAGESIEVE_ARG_STR(&(args[1])), " "); 

					if ( str_array_icase_find(mechs, "PLAIN") )
						client->proxy_sasl_plain = TRUE;
					else
						client->proxy_sasl_plain = FALSE;

				} else {
					client_log_err(&client->common, "proxy: "
		         		"Server returned erroneous SASL capability");
					fatal = TRUE;
				}

			} else if ( strcasecmp(capability, "STARTTLS") == 0 ) {
				client->proxy_starttls = TRUE;
			}

		} else {
			/* Do not accept faulty server */
			client_log_err(&client->common, t_strdup_printf("proxy: "
				"Remote returned with invalid capability/greeting line: %s",
				str_sanitize(line,160)));
			fatal = TRUE;
		}

	} else if ( ret == -2 ) {
		/* Parser needs more data (not possible on mem stream) */
		i_unreached();

    } else if ( ret < 0 ) {
		const char *error_str = managesieve_parser_get_error(parser, &fatal);
		error_str = (error_str != NULL ? error_str : "unknown (bug)" );
	
		/* Do not accept faulty server */
		client_log_err(&client->common, t_strdup_printf("proxy: "
			"Protocol parse error(%d) in capability/greeting line: %s (line='%s')",
			ret, error_str, line));
		fatal = TRUE;
	}

	/* Cleanup parser */
    managesieve_parser_destroy(&parser);
	i_stream_destroy(&input);

	/* Time to exit if greeting was not accepted */
	if ( fatal ) return -1;

	/* Wait until greeting is received completely */
	if ( *resp_r == MANAGESIEVE_RESPONSE_NONE ) return 1;

	return 0;
}

int managesieve_proxy_parse_line(struct client *client, const char *line)
{
	struct managesieve_client *msieve_client = 
		(struct managesieve_client *) client;
	struct ostream *output; 
	enum login_proxy_ssl_flags ssl_flags;
	managesieve_response_t response = MANAGESIEVE_RESPONSE_NONE;
	string_t *command;
	int ret = 0;

	i_assert(!client->destroyed);

	output = login_proxy_get_ostream(client->login_proxy);
	switch ( msieve_client->proxy_state ) {
	case PROXY_STATE_INITIAL:
		if ( (ret=proxy_input_capability(msieve_client, line, &response)) < 0 ) {
			client_proxy_failed(client, TRUE);
			return -1;
		}

		if ( ret == 0 ) {
			if ( response != MANAGESIEVE_RESPONSE_OK ) {
				client_log_err(client, "proxy: "
					"Remote sent unexpected NO/BYE in stead of capability response");
				client_proxy_failed(client, TRUE);
				return -1;		
			}

			command = t_str_new(128);

			ssl_flags = login_proxy_get_ssl_flags(client->login_proxy);
			if ((ssl_flags & PROXY_SSL_FLAG_STARTTLS) != 0) {
				if ( !msieve_client->proxy_starttls ) {
					client_log_err(client, "proxy: Remote doesn't support STARTTLS");
						client_proxy_failed(client, TRUE);
					return -1;
				}
        	
				str_append(command, "STARTTLS\r\n");
				msieve_client->proxy_state = PROXY_STATE_TLS_START;
			} else {
				if ( proxy_write_login(msieve_client, command) < 0 ) {
					client_proxy_failed(client, TRUE);
					return -1;
				}
				msieve_client->proxy_state = PROXY_STATE_AUTHENTICATE;
			}

			(void)o_stream_send(output, str_data(command), str_len(command));
		}

		return 0;

	case PROXY_STATE_TLS_START:
		if ( strncasecmp(line, "OK", 2) == 0 && ( strlen(line) == 2 || line[2] == ' ' ) ) {

			/* STARTTLS successful, begin TLS negotiation. */
			if ( login_proxy_starttls(client->login_proxy) < 0 ) {
				client_proxy_failed(client, TRUE);
				return -1;
			}
	
			msieve_client->proxy_state = PROXY_STATE_TLS_READY;
			return 1;
		}

		client_log_err(client, "proxy: Remote refused STARTTLS command");
		client_proxy_failed(client, TRUE);
		return -1;		

	case PROXY_STATE_TLS_READY:
		if ( (ret=proxy_input_capability(msieve_client, line, &response)) < 0 ) {
			client_proxy_failed(client, TRUE);
			return -1;
		}

		if ( ret == 0 ) {
			if ( response != MANAGESIEVE_RESPONSE_OK ) {
				/* STARTTLS failed */
				client_log_err(client, 
					t_strdup_printf("proxy: Remote STARTTLS failed: %s", 
						str_sanitize(line, 160)));
				client_proxy_failed(client, TRUE);
				return -1;
			}

			command = t_str_new(128);
			if ( proxy_write_login(msieve_client, command) < 0 ) {
				client_proxy_failed(client, TRUE);
				return -1;
			}

			(void)o_stream_send(output, str_data(command), str_len(command));
		
			msieve_client->proxy_state = PROXY_STATE_AUTHENTICATE;
		}

		return 0;
	
	case PROXY_STATE_AUTHENTICATE:

		/* Check login status */
		if ( strncasecmp(line, "OK", 2) == 0 &&
			( strlen(line) == 2 || line[2] == ' ' ) ) {
			string_t *str = t_str_new(128);

			/* Login successful */

			/* FIXME: some SASL mechanisms cause a capability response to be sent */

			/* Send this line to client. */
			str_append(str, line );
			str_append(str, "\r\n");
			(void)o_stream_send(client->output, str_data(str), str_len(str));

			(void)client_skip_line(msieve_client);
			client_proxy_finish_destroy_client(client);
			
			return 1;
		} 
		
		/* Login failed */

		if ( client->set->verbose_auth ) {
			const char *log_line = line;

			if (strncasecmp(log_line, "NO ", 3) == 0)
				log_line += 3;
			client_proxy_log_failure(client, log_line);
		}

		/* FIXME: properly parse and handle response codes */

		/* Login failed. Send our own failure reply so client can't
		 * figure out if user exists or not just by looking at the
		 * reply string.
		 */
		client_send_no(client, AUTH_FAILED_MSG);

		client_proxy_failed(client, FALSE);
		return -1;

	default:
		/* Not supposed to happen */
		break;
	}

	i_unreached();
	return -1;
}

void managesieve_proxy_reset(struct client *client ATTR_UNUSED)
{
/*	struct managesieve_client *msieve_client =
		(struct managesieve_client *) client; */
}
