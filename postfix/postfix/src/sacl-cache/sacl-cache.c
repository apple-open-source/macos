/* APPLE - Cache the results of Mail Service ACL checks.
   Based on the postfix "verify" daemon. */

/* System library. */

#include <sys_defs.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <htable.h>
#include <dict_ht.h>
#include <dict.h>
#include <split_at.h>
#include <stringops.h>
#include <set_eugid.h>

/* Global library. */

#include <mail_conf.h>
#include <mail_params.h>
#include <mail_version.h>
#include <mail_proto.h>
#include <post_mail.h>
#include <data_redirect.h>
#include <sacl_cache_clnt.h>

/* Server skeleton. */

#include <mail_server.h>

/* Application-specific. */

 /*
  * Tunable parameters.
  */
int	var_sacl_cache_pos_exp;
int	var_sacl_cache_neg_exp;
int	var_sacl_cache_dis_exp;

 /*
  * State.
  */
static DICT *sacl_cache_map;	/* state for each address */
static int sacl_enabled = -1;	/* is mail SACL on or off (or unknown)? */
static long sacl_updated = 0;	/* when was last SACL check? */
static unsigned int sacl_generation = 0; /* quick way to "clear" the cache */

 /*
  * Silly little macros.
  */
#define STR(x)			vstring_str(x)
#define STREQ(x,y)		(strcmp(x,y) == 0)

 /*
  * The SACL check database consists of (address, data) tuples. The
  * format of the data field is "generation:status:updated". The meaning of
  * each field is:
  * 
  * generation: sacl_generation when entry was last updated.
  *
  * status: either SACL_CHECK_STATUS_AUTHORIZED or
  * SACL_CHECK_STATUS_UNAUTHORIZED.
  * 
  * updated: the time the cache entry was last updated.
  */

 /*
  * Quick test to see status without parsing the whole entry.
  */
#define STATUS_FROM_RAW_ENTRY(e) atoi(e)

/* sacl_cache_make_entry - construct table entry */

static void sacl_cache_make_entry(VSTRING *buf, unsigned int generation,
				  int status, long updated)
{
    vstring_sprintf(buf, "%u:%d:%ld", generation, status, updated);
}

/* sacl_cache_parse_entry - parse table entry */

static int sacl_cache_parse_entry(char *buf, unsigned int *generation,
				  int *status, long *updated)
{
    char *status_text;
    char *updated_text;

    if ((status_text = split_at(buf, ':')) != 0
	&& (updated_text = split_at(status_text, ':')) != 0
	&& alldig(buf)
	&& alldig(status_text)
	&& alldig(updated_text)) {
	*generation = (unsigned int) atol(buf);
	*status = atoi(status_text);
	*updated = atol(updated_text);

	if ((*status == SACL_CHECK_STATUS_AUTHORIZED ||
	     *status == SACL_CHECK_STATUS_UNAUTHORIZED) && *updated)
	    return (0);
    }
    msg_warn("bad sacl-cache table entry: %.100s", buf);
    return (-1);
}

/* sacl_cache_put - set cache entry */

static void sacl_cache_put(VSTREAM *client_stream)
{
    VSTRING *buf = vstring_alloc(10);
    VSTRING *addr = vstring_alloc(10);
    int     sacl_status;
    long    updated;

    if (attr_scan(client_stream, ATTR_FLAG_STRICT,
		  ATTR_TYPE_STR, MAIL_ATTR_ADDR, addr,
		  ATTR_TYPE_INT, MAIL_ATTR_SACL_STATUS, &sacl_status,
		  ATTR_TYPE_END) == 2) {
	/* FIX 200501 IPv6 patch did not neuter ":" in address literals. */
	translit(STR(addr), ":", "_");
	if (sacl_status != SACL_CHECK_STATUS_AUTHORIZED &&
	    sacl_status != SACL_CHECK_STATUS_UNAUTHORIZED) {
	    msg_warn("bad sacl-cache status %d for recipient %s",
		     sacl_status, STR(addr));
	    attr_print(client_stream, ATTR_FLAG_NONE,
		       ATTR_TYPE_INT, MAIL_ATTR_STATUS, SACL_CACHE_STAT_BAD,
		       ATTR_TYPE_END);
	} else {
	    updated = (long) time((time_t *) 0);
	    sacl_cache_make_entry(buf, sacl_generation, sacl_status, updated);
	    if (msg_verbose)
		msg_info("PUT %s gen=%u status=%d updated=%ld",
		    STR(addr), sacl_generation, sacl_status, updated);
	    dict_put(sacl_cache_map, STR(addr), STR(buf));

	    attr_print(client_stream, ATTR_FLAG_NONE,
		       ATTR_TYPE_INT, MAIL_ATTR_STATUS, SACL_CACHE_STAT_OK,
		       ATTR_TYPE_END);

	    sacl_enabled = 1;
	    sacl_updated = updated;
	}
    }
    vstring_free(buf);
    vstring_free(addr);
}

/* sacl_cache_get - get cache entry */

static void sacl_cache_get(VSTREAM *client_stream)
{
    VSTRING *addr = vstring_alloc(10);
    VSTRING *get_buf = 0;
    const char *raw_data;
    unsigned int generation = sacl_generation;
    int     sacl_status = -1;
    long    updated = 0;

    if (attr_scan(client_stream, ATTR_FLAG_STRICT,
		  ATTR_TYPE_STR, MAIL_ATTR_ADDR, addr,
		  ATTR_TYPE_END) == 1) {
	long    now = (long) time((time_t *) 0);

	/*
	 * Produce a default record when no usable record exists.
	 * 
	 * If negative caching is disabled, purge an expired record from the
	 * database.
	 */
#define POSITIVE_ENTRY_EXPIRED(sacl_status, updated) \
    (sacl_status == SACL_CHECK_STATUS_AUTHORIZED && updated + var_sacl_cache_pos_exp < now)
#define NEGATIVE_ENTRY_EXPIRED(sacl_status, updated) \
    (sacl_status != SACL_CHECK_STATUS_AUTHORIZED && updated + var_sacl_cache_neg_exp < now)

	/* FIX 200501 IPv6 patch did not neuter ":" in address literals. */
	translit(STR(addr), ":", "_");
	if (!sacl_enabled && sacl_updated + var_sacl_cache_dis_exp >= now) {
	    /* SACL is disabled and it's not yet time to refresh that
	       knowledge */
	    sacl_status = SACL_CHECK_STATUS_NO_SACL;
	} else if ((raw_data = dict_get(sacl_cache_map,
					STR(addr))) == 0	/* not found */
	    || ((get_buf = vstring_alloc(10)),
		vstring_strcpy(get_buf, raw_data),	/* malformed */
		sacl_cache_parse_entry(STR(get_buf), &generation, &sacl_status,
				       &updated) < 0)
	    || generation != sacl_generation
	    || POSITIVE_ENTRY_EXPIRED(sacl_status, updated)
	    || NEGATIVE_ENTRY_EXPIRED(sacl_status, updated)) {
	    sacl_status = SACL_CHECK_STATUS_UNKNOWN;
	    updated = 0;

	    /* could dict_del the entry if there was one, except: (a)
	       dict_del may be a stub which makes this daemon fatal-die,
	       and (b) the caller will probably immediately put an entry
	       for this address anyway */
	}

	if (msg_verbose)
	    msg_info("GOT %s status=%d updated=%ld",
		     STR(addr), sacl_status, updated);

	/*
	 * Respond to the client.
	 */
	attr_print(client_stream, ATTR_FLAG_NONE,
		   ATTR_TYPE_INT, MAIL_ATTR_STATUS, SACL_CACHE_STAT_OK,
		   ATTR_TYPE_INT, MAIL_ATTR_SACL_STATUS, sacl_status,
		   ATTR_TYPE_END);
    }
    vstring_free(addr);
    if (get_buf)
	vstring_free(get_buf);
}

/* sacl_cache_no_sacl - disable and wipe the cache */

static void sacl_cache_no_sacl(VSTREAM *client_stream)
{
    if (sacl_enabled) {
	sacl_enabled = 0;
	/* really want to delete all entries in sacl_cache_map but
	   that's not supported.  instead bump a generation counter */
	sacl_generation++;
    }
    sacl_updated = (long) time((time_t *) 0);

    if (msg_verbose)
	msg_info("NO-SACL");

    attr_print(client_stream, ATTR_FLAG_NONE,
	       ATTR_TYPE_INT, MAIL_ATTR_STATUS, SACL_CACHE_STAT_OK,
	       ATTR_TYPE_END);
}

/* sacl_cache_service - perform service for client */

static void sacl_cache_service(VSTREAM *client_stream, char *unused_service,
			       char **argv)
{
    VSTRING *request = vstring_alloc(10);

    /*
     * Sanity check. This service takes no command-line arguments.
     */
    if (argv[0])
	msg_fatal("unexpected command-line argument: %s", argv[0]);

    /*
     * This routine runs whenever a client connects to the socket dedicated
     * to the SACL cache service. All connection-management stuff
     * is handled by the common code in multi_server.c.
     */
    if (attr_scan(client_stream,
		  ATTR_FLAG_MORE | ATTR_FLAG_STRICT,
		  ATTR_TYPE_STR, MAIL_ATTR_REQ, request,
		  ATTR_TYPE_END) == 1) {
	if (STREQ(STR(request), SACL_CACHE_REQ_PUT)) {
	    sacl_cache_put(client_stream);
	} else if (STREQ(STR(request), SACL_CACHE_REQ_GET)) {
	    sacl_cache_get(client_stream);
	} else if (STREQ(STR(request), SACL_CACHE_REQ_NO_SACL)) {
	    sacl_cache_no_sacl(client_stream);
	} else {
	    msg_warn("unrecognized request: \"%s\", ignored", STR(request));
	    attr_print(client_stream, ATTR_FLAG_NONE,
		       ATTR_TYPE_INT, MAIL_ATTR_STATUS, SACL_CACHE_STAT_BAD,
		       ATTR_TYPE_END);
	}
    }
    vstream_fflush(client_stream);
    vstring_free(request);
}

/* post_jail_init - post-jail initialization */

static void post_jail_init(char *unused_name, char **unused_argv)
{
    var_use_limit = 0;
    var_idle_limit = 0;
}

/* pre_jail_init - pre-jail initialization */

static void pre_jail_init(char *unused_name, char **unused_argv)
{
    /*
     * Never, ever, get killed by a master signal, as that would corrupt the
     * database when we're in the middle of an update.
     */
    setsid();

    sacl_cache_map = dict_ht_open("sacl-cache", O_CREAT | O_RDWR, 0);
}

MAIL_VERSION_STAMP_DECLARE;

/* main - pass control to the multi-threaded skeleton */

int     main(int argc, char **argv)
{
    static const CONFIG_TIME_TABLE time_table[] = {
	VAR_SACL_CACHE_POS_EXP, DEF_SACL_CACHE_POS_EXP, &var_sacl_cache_pos_exp, 1, 0,
	VAR_SACL_CACHE_NEG_EXP, DEF_SACL_CACHE_NEG_EXP, &var_sacl_cache_neg_exp, 1, 0,
	VAR_SACL_CACHE_DIS_EXP, DEF_SACL_CACHE_DIS_EXP, &var_sacl_cache_dis_exp, 1, 0,
	0,
    };

    /*
     * Fingerprint executables and core dumps.
     */
    MAIL_VERSION_STAMP_ALLOCATE;

    multi_server_main(argc, argv, sacl_cache_service,
		      MAIL_SERVER_TIME_TABLE, time_table,
		      MAIL_SERVER_PRE_INIT, pre_jail_init,
		      MAIL_SERVER_POST_INIT, post_jail_init,
		      MAIL_SERVER_SOLITARY,
		      0);
}
