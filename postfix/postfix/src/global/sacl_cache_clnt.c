/* APPLE - Mail SACL cache client
   Based on verify_clnt */

/* System library. */

#include <sys_defs.h>
#include <unistd.h>
#include <errno.h>

/* Utility library. */

#include <msg.h>
#include <vstream.h>
#include <vstring.h>
#include <attr.h>

/* Global library. */

#include <mail_params.h>
#include <mail_proto.h>
#include <clnt_stream.h>
#include <sacl_cache_clnt.h>

CLNT_STREAM *sacl_cache_clnt;

/* sacl_cache_clnt_init - initialize */

static void sacl_cache_clnt_init(void)
{
    if (sacl_cache_clnt != 0)
    msg_panic("sacl_cache_clnt_init: multiple initialization");
    sacl_cache_clnt = clnt_stream_create(MAIL_CLASS_PRIVATE,
					 var_sacl_cache_service,
					 var_ipc_idle_limit,
					 var_ipc_ttl_limit);
}

/* sacl_cache_clnt_get - get cached SACL status */

int sacl_cache_clnt_get(const char *addr, int *sacl_status)
{
    VSTREAM *stream;
    int     request_status;
    int     count = 0;

    /*
     * Do client-server plumbing.
     */
    if (sacl_cache_clnt == 0)
	sacl_cache_clnt_init();

    /*
     * Request status for this address.
     */
    for (;;) {
	stream = clnt_stream_access(sacl_cache_clnt);
	errno = 0;
	count += 1;
	if (attr_print(stream, ATTR_FLAG_NONE,
		       ATTR_TYPE_STR, MAIL_ATTR_REQ, SACL_CACHE_REQ_GET,
		       ATTR_TYPE_STR, MAIL_ATTR_ADDR, addr,
		       ATTR_TYPE_END) != 0
	    || vstream_fflush(stream)
	    || attr_scan(stream, ATTR_FLAG_MISSING,
			 ATTR_TYPE_INT, MAIL_ATTR_STATUS, &request_status,
			 ATTR_TYPE_INT, MAIL_ATTR_SACL_STATUS, sacl_status,
			 ATTR_TYPE_END) != 2) {
	    if (msg_verbose || count > 1 || (errno && errno != EPIPE && errno != ENOENT))
		msg_warn("problem talking to service %s: %m",
			 var_sacl_cache_service);
	} else {
	    break;
	}
	sleep(1);
	clnt_stream_recover(sacl_cache_clnt);
    }
    return (request_status);
}

/* sacl_cache_clnt_put - store into SACL cache */

int sacl_cache_clnt_put(const char *addr, int sacl_status)
{
    VSTREAM *stream;
    int     request_status;

    /*
     * Do client-server plumbing.
     */
    if (sacl_cache_clnt == 0)
	sacl_cache_clnt_init();

    for (;;) {
	stream = clnt_stream_access(sacl_cache_clnt);
	errno = 0;
	if (attr_print(stream, ATTR_FLAG_NONE,
		       ATTR_TYPE_STR, MAIL_ATTR_REQ, SACL_CACHE_REQ_PUT,
		       ATTR_TYPE_STR, MAIL_ATTR_ADDR, addr,
		       ATTR_TYPE_INT, MAIL_ATTR_SACL_STATUS, sacl_status,
		       ATTR_TYPE_END) != 0
	    || attr_scan(stream, ATTR_FLAG_MISSING,
			 ATTR_TYPE_INT, MAIL_ATTR_STATUS, &request_status,
			 ATTR_TYPE_END) != 1) {
	    if (msg_verbose || (errno != EPIPE && errno != ENOENT))
		msg_warn("problem talking to service %s: %m",
			 var_sacl_cache_service);
	} else {
	    break;
	}
	sleep(1);
	clnt_stream_recover(sacl_cache_clnt);
    }
    return (request_status);
}

/* sacl_cache_clnt_no_sacl - put SACL cache into bypass */

int sacl_cache_clnt_no_sacl(void)
{
    VSTREAM *stream;
    int     request_status;

    /*
     * Do client-server plumbing.
     */
    if (sacl_cache_clnt == 0)
	sacl_cache_clnt_init();

    for (;;) {
	stream = clnt_stream_access(sacl_cache_clnt);
	errno = 0;
	if (attr_print(stream, ATTR_FLAG_NONE,
		       ATTR_TYPE_STR, MAIL_ATTR_REQ, SACL_CACHE_REQ_NO_SACL,
		       ATTR_TYPE_END) != 0
	    || attr_scan(stream, ATTR_FLAG_MISSING,
			 ATTR_TYPE_INT, MAIL_ATTR_STATUS, &request_status,
			 ATTR_TYPE_END) != 1) {
	    if (msg_verbose || (errno != EPIPE && errno != ENOENT))
		msg_warn("problem talking to service %s: %m",
			 var_sacl_cache_service);
	} else {
	    break;
	}
	sleep(1);
	clnt_stream_recover(sacl_cache_clnt);
    }
    return (request_status);
}

 /*
  * Proof-of-concept test client program.
  */
#ifdef TEST

#include <stdlib.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <msg_vstream.h>
#include <stringops.h>
#include <vstring_vstream.h>
#include <mail_conf.h>

#define STR(x) vstring_str(x)

static NORETURN usage(char *myname)
{
    msg_fatal("usage: %s [-v]", myname);
}

static void get(char *query)
{
    int     status;

    switch (sacl_cache_clnt_get(query, &status)) {
    case SACL_CACHE_STAT_OK:
	vstream_printf("%-10s %d\n", "status", status);
	vstream_fflush(VSTREAM_OUT);
	break;
    case SACL_CACHE_STAT_BAD:
	msg_warn("bad request format");
	break;
    case SACL_CACHE_STAT_FAIL:
	msg_warn("request failed");
	break;
    }
}

static void put(char *query)
{
    char   *addr;
    char   *status_text;
    char   *cp = query;

    if ((addr = mystrtok(&cp, " \t\r\n")) == 0) {
	msg_warn("bad request format");
	return;
    }

    switch (sacl_cache_clnt_put(query, atoi(cp))) {
    case SACL_CACHE_STAT_OK:
	vstream_printf("OK\n");
	vstream_fflush(VSTREAM_OUT);
	break;
    case SACL_CACHE_STAT_BAD:
	msg_warn("bad request format");
	break;
    case SACL_CACHE_STAT_FAIL:
	msg_warn("request failed");
	break;
    }
}

static void no(void)
{
    switch (sacl_cache_clnt_no_sacl()) {
    case SACL_CACHE_STAT_OK:
	vstream_printf("OK\n");
	vstream_fflush(VSTREAM_OUT);
	break;
    case SACL_CACHE_STAT_BAD:
	msg_warn("bad request format");
	break;
    case SACL_CACHE_STAT_FAIL:
	msg_warn("request failed");
	break;
    }
}

int     main(int argc, char **argv)
{
    VSTRING *buffer = vstring_alloc(1);
    char   *cp;
    int     ch;
    char   *command;

    signal(SIGPIPE, SIG_IGN);

    msg_vstream_init(argv[0], VSTREAM_ERR);

    mail_conf_read();
    msg_info("using config files in %s", var_config_dir);
    if (chdir(var_queue_dir) < 0)
	msg_fatal("chdir %s: %m", var_queue_dir);

    while ((ch = GETOPT(argc, argv, "v")) > 0) {
	switch (ch) {
	case 'v':
	    msg_verbose++;
	    break;
	default:
	    usage(argv[0]);
	}
    }
    if (argc - optind > 1)
	usage(argv[0]);

    while (vstring_fgets_nonl(buffer, VSTREAM_IN)) {
	cp = STR(buffer);
	if ((command = mystrtok(&cp, " \t\r\n")) == 0)
	    continue;
	if (strcmp(command, "get") == 0)
	    get(cp);
	else if (strcmp(command, "put") == 0)
	    put(cp);
	else if (strcmp(command, "no") == 0)
	    no();
	else
	    msg_warn("unrecognized command: %s", command);
    }
    vstring_free(buffer);
    return (0);
}

#endif
