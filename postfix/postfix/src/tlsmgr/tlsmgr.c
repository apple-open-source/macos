/*++
/* NAME
/*	tlsmgr 8
/* SUMMARY
/*	Postfix TLS session cache and PRNG handling manager
/* SYNOPSIS
/*	\fBtlsmgr\fR [generic Postfix daemon options]
/* DESCRIPTION
/*	The tlsmgr process does housekeeping on the session cache database
/*	files. It runs through the databases and removes expired entries
/*	and entries written by older (incompatible) versions.
/*
/*	The tlsmgr is responsible for the PRNG handling. The used internal
/*	OpenSSL PRNG has a pool size of 8192 bits (= 1024 bytes). The pool
/*	is initially seeded at startup from an external source (EGD or
/*	/dev/urandom) and additional seed is obtained later during program
/*	run at a configurable period. The exact time of seed query is
/*	using random information and is equally distributed in the range of
/*	[0-\fBtls_random_reseed_period\fR] with a \fBtls_random_reseed_period\fR
/*	having a default of 1 hour.
/*
/*	Tlsmgr can be run chrooted and with dropped privileges, as it will
/*	connect to the entropy source at startup.
/*
/*	The PRNG is additionally seeded internally by the data found in the
/*	session cache and timevalues.
/*
/*	Tlsmgr reads the old value of the exchange file at startup to keep
/*	entropy already collected during previous runs.
/*
/*	From the PRNG random pool a cryptographically strong 1024 byte random
/*	sequence is written into the PRNG exchange file. The file is updated
/*	periodically with the time changing randomly from
/*	[0-\fBtls_random_prng_update_period\fR].
/* STANDARDS
/* SECURITY
/* .ad
/* .fi
/*	Tlsmgr is not security-sensitive. It only deals with external data
/*	to be fed into the PRNG, the contents is never trusted. The session
/*	cache housekeeping will only remove entries if expired and will never
/*	touch the contents of the cached data.
/* DIAGNOSTICS
/*	Problems and transactions are logged to the syslog daemon.
/* BUGS
/*	There is no automatic means to limit the number of entries in the
/*	session caches and/or the size of the session cache files.
/* CONFIGURATION PARAMETERS
/* .ad
/* .fi
/*	The following \fBmain.cf\fR parameters are especially relevant to
/*	this program. See the Postfix \fBmain.cf\fR file for syntax details
/*	and for default values. Use the \fBpostfix reload\fR command after
/*	a configuration change.
/* .SH Session Cache
/* .ad
/* .fi
/* .IP \fBsmtpd_tls_session_cache_database\fR
/*	Name of the SDBM file (type sdbm:) containing the SMTP server session
/*	cache. If the file does not exist, it is created.
/* .IP \fBsmtpd_tls_session_cache_timeout\fR
/*	Expiry time of SMTP server session cache entries in seconds. Entries
/*	older than this are removed from the session cache. A cleanup-run is
/*	performed periodically every \fBsmtpd_tls_session_cache_timeout\fR
/*	seconds. Default is 3600 (= 1 hour).
/* .IP \fBsmtp_tls_session_cache_database\fR
/*	Name of the SDBM file (type sdbm:) containing the SMTP client session
/*	cache. If the file does not exist, it is created.
/* .IP \fBsmtp_tls_session_cache_timeout\fR
/*	Expiry time of SMTP client session cache entries in seconds. Entries
/*	older than this are removed from the session cache. A cleanup-run is
/*	performed periodically every \fBsmtp_tls_session_cache_timeout\fR
/*	seconds. Default is 3600 (= 1 hour).
/* .SH Pseudo Random Number Generator
/* .ad
/* .fi
/* .IP \fBtls_random_source\fR
/*	Name of the EGD socket or device or regular file to obtain entropy
/*	from. The type of entropy source must be specified by preceding the
/*      name with the appropriate type: egd:/path/to/egd_socket,
/*      dev:/path/to/devicefile, or /path/to/regular/file.
/*	tlsmgr opens \fBtls_random_source\fR and tries to read
/*	\fBtls_random_bytes\fR from it.
/* .IP \fBtls_random_bytes\fR
/*	Number of bytes to be read from \fBtls_random_source\fR.
/*	Default value is 32 bytes. If using EGD, a maximum of 255 bytes is read.
/* .IP \fBtls_random_exchange_name\fR
/*	Name of the file written by tlsmgr and read by smtp and smtpd at
/*	startup. The length is 1024 bytes. Default value is
/*	/etc/postfix/prng_exch.
/* .IP \fBtls_random_reseed_period\fR
/*	Time in seconds until the next reseed from external sources is due.
/*	This is the maximum value. The actual point in time is calculated
/*	with a random factor equally distributed between 0 and this maximum
/*	value. Default is 3600 (= 60 minutes).
/* .IP \fBtls_random_prng_update_period\fR
/*	Time in seconds until the PRNG exchange file is updated with new
/*	pseude random values. This is the maximum value. The actual point
/*	in time is calculated with a random factor equally distributed
/*	between 0 and this maximum value. Default is 60 (= 1 minute).
/* SEE ALSO
/*	smtp(8) SMTP client
/*	smtpd(8) SMTP server
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*--*/

/* System library. */

#include <sys_defs.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>			/* gettimeofday, not POSIX */

/* OpenSSL library. */
#ifdef USE_SSL
#include <openssl/rand.h>		/* For the PRNG */
#endif

/* Utility library. */

#include <msg.h>
#include <events.h>
#include <dict.h>
#include <stringops.h>
#include <mymalloc.h>
#include <connect.h>
#include <myflock.h>

/* Global library. */

#include <mail_conf.h>
#include <mail_params.h>
#include <pfixtls.h>

/* Master process interface */

#include <master_proto.h>
#include <mail_server.h>

/* Application-specific. */

#ifdef USE_SSL
 /*
  * Tunables.
  */
char   *var_tls_rand_source;
int	var_tls_rand_bytes;
int	var_tls_reseed_period;
int	var_tls_prng_upd_period;

static int rand_exch_fd;
static int rand_source_dev_fd = -1;
static int rand_source_socket_fd = -1;
static int srvr_scache_db_active;
static int clnt_scache_db_active;
static DICT *srvr_scache_db = NULL;
static DICT *clnt_scache_db = NULL;

static void tlsmgr_prng_upd_event(int unused_event, char *dummy)
{
    struct timeval tv;
    unsigned char buffer[1024];
    int next_period;

    /*
     * It is time to update the PRNG exchange file. Since other processes might
     * have added entropy, we do this in a read_stir-back_write cycle.
     */
    GETTIMEOFDAY(&tv);
    RAND_seed(&tv, sizeof(struct timeval));

    if (myflock(rand_exch_fd, INTERNAL_LOCK, MYFLOCK_OP_EXCLUSIVE) != 0)
	msg_fatal("Could not lock random exchange file: %s",
		  strerror(errno));

    lseek(rand_exch_fd, 0, SEEK_SET);
    if (read(rand_exch_fd, buffer, 1024) < 0)
	msg_fatal("reading exchange file failed");
    RAND_seed(buffer, 1024);

    RAND_bytes(buffer, 1024);
    lseek(rand_exch_fd, 0, SEEK_SET);
    if (write(rand_exch_fd, buffer, 1024) != 1024)
	msg_fatal("Writing exchange file failed");

    if (myflock(rand_exch_fd, INTERNAL_LOCK, MYFLOCK_OP_NONE) != 0)
	msg_fatal("Could not unlock random exchange file: %s",
		  strerror(errno));

    /*
     * Make prediction difficult for outsiders and calculate the time for the
     * next execution randomly.
     */
    next_period = (var_tls_prng_upd_period * buffer[0]) / 255;
    event_request_timer(tlsmgr_prng_upd_event, dummy, next_period);
}


static void tlsmgr_reseed_event(int unused_event, char *dummy)
{
    int egd_success;
    int next_period;
    int rand_bytes;
    char buffer[255];
    struct timeval tv;
    unsigned char randbyte;

    /*
     * It is time to reseed the PRNG.
     */

    GETTIMEOFDAY(&tv);
    RAND_seed(&tv, sizeof(struct timeval));
    if (rand_source_dev_fd != -1) {
	rand_bytes = read(rand_source_dev_fd, buffer, var_tls_rand_bytes);
	if (rand_bytes > 0)
	    RAND_seed(buffer, rand_bytes);
	else if (rand_bytes < 0) {
	    msg_fatal("Read from entropy device %s failed",
		      var_tls_rand_source);
	}
    } else if (rand_source_socket_fd != -1) {
	egd_success = 0;
	buffer[0] = 1;
	buffer[1] = var_tls_rand_bytes;
	if (write(rand_source_socket_fd, buffer, 2) != 2)
	    msg_info("Could not talk to %s", var_tls_rand_source);
	else if (read(rand_source_socket_fd, buffer, 1) != 1)
	    msg_info("Could not read info from %s", var_tls_rand_source);
	else {
	    rand_bytes = buffer[0];
	    if (read(rand_source_socket_fd, buffer, rand_bytes) != rand_bytes)
		msg_info("Could not read data from %s", var_tls_rand_source);
	    else {
		egd_success = 1;
		RAND_seed(buffer, rand_bytes);
	    }
	}
	if (!egd_success) {
	    msg_info("Lost connection to EGD-device, exiting to reconnect.");
	    exit(0);
	}
    } else if (*var_tls_rand_source) {
	rand_bytes = RAND_load_file(var_tls_rand_source, var_tls_rand_bytes);
    }

    /*
     * Make prediction difficult for outsiders and calculate the time for the
     * next execution randomly.
     */
    RAND_bytes(&randbyte, 1);
    next_period = (var_tls_reseed_period * randbyte) / 255;
    event_request_timer(tlsmgr_reseed_event, dummy, next_period);
}


static int tlsmgr_do_scache_check(DICT *scache_db, int scache_timeout,
				  int start)
{
    int func;
    int len;
    int n;
    int delete = 0;
    int result;
    struct timeval tv;
    const char *member;
    const char *value;
    char *member_copy;
    unsigned char nibble, *data;
    pfixtls_scache_info_t scache_info;

    GETTIMEOFDAY(&tv);
    RAND_seed(&tv, sizeof(struct timeval));

    /*
     * Run through the given dictionary and check the stored sessions.
     * If "start" is set to 1, a new run is initiated, otherwise the next
     * item is accessed. The state is internally kept in the DICT.
     */
    if (start)
	func = DICT_SEQ_FUN_FIRST;
    else
	func = DICT_SEQ_FUN_NEXT;
    result = dict_seq(scache_db, func, &member, &value);

    if (result > 0)
	return 0;	/* End of list reached */
    else if (result < 0)
	msg_fatal("Database fault, should already be caught.");
    else {
	member_copy = mystrdup(member);
	len = strlen(value);
	RAND_seed(value, len);		/* Use it to increase entropy */
	if (len < 2 * sizeof(pfixtls_scache_info_t))
	    delete = 1;		/* Messed up, delete */
	else if (len > 2 * sizeof(pfixtls_scache_info_t))
	    len = 2 * sizeof(pfixtls_scache_info_t);
	if (!delete) {
	    data = (unsigned char *)(&scache_info);
	    memset(data, 0, len / 2);
	    for (n = 0; n < len; n++) {
            if ((value[n] >= '0') && (value[n] <= '9'))
                nibble = value[n] - '0';
            else
                nibble = value[n] - 'A' + 10;
            if (n % 2)
                data[n / 2] |= nibble;
            else
                data[n / 2] |= (nibble << 4);
        }

        if ((scache_info.scache_db_version != scache_db_version) ||
            (scache_info.openssl_version != openssl_version) ||
            (scache_info.timestamp + scache_timeout < time(NULL)))
	    delete = 1;
	}
	if (delete)
	    result = dict_del(scache_db, member_copy);
	myfree(member_copy);
    }

    if (delete && result)
	msg_info("Could not delete %s", member);
    return 1;

}

static void tlsmgr_clnt_cache_run_event(int unused_event, char *dummy)
{

    /*
     * This routine runs when it is time for another tls session cache scan.
     * Make sure this routine gets called again in the future.
     */
    clnt_scache_db_active = tlsmgr_do_scache_check(clnt_scache_db, 
				var_smtp_tls_scache_timeout, 1);
    event_request_timer(tlsmgr_clnt_cache_run_event, dummy,
		 var_smtp_tls_scache_timeout);
}


static void tlsmgr_srvr_cache_run_event(int unused_event, char *dummy)
{

    /*
     * This routine runs when it is time for another tls session cache scan.
     * Make sure this routine gets called again in the future.
     */
    srvr_scache_db_active = tlsmgr_do_scache_check(srvr_scache_db,
				var_smtpd_tls_scache_timeout, 1);
    event_request_timer(tlsmgr_srvr_cache_run_event, dummy,
		 var_smtpd_tls_scache_timeout);
}


static DICT *tlsmgr_cache_open(const char *dbname)
{
    DICT *retval;
    char *dbpagname;
    char *dbdirname;

    /*
     * First, try to find out the real name of the database file, so that
     * it can be removed.
     */
    if (!strncmp(dbname, "sdbm:", 5)) {
	dbpagname = concatenate(dbname + 5, ".pag", NULL);
	REMOVE(dbpagname);
	myfree(dbpagname);
	dbdirname = concatenate(dbname + 5, ".dir", NULL);
	REMOVE(dbdirname);
	myfree(dbdirname);
    }
    else {
	msg_warn("Only type sdbm: supported: %s", dbname);
	return NULL;
    }

    /*
     * Now open the dictionary. Do it with O_EXCL, so that we only open a
     * fresh file. If we cannot open it with a fresh file, then we won't
     * touch it.
     */
    retval = dict_open(dbname, O_RDWR | O_CREAT | O_EXCL,
	      DICT_FLAG_DUP_REPLACE | DICT_FLAG_LOCK | DICT_FLAG_SYNC_UPDATE);
    if (!retval)
	msg_warn("Could not create dictionary %s", dbname);
    return retval;
}

/* tlsmgr_trigger_event - respond to external trigger(s) */

static void tlsmgr_trigger_event(char *buf, int len,
			               char *unused_service, char **argv)
{
    /*
     * Sanity check. This service takes no command-line arguments.
     */
    if (argv[0])
	msg_fatal("unexpected command-line argument: %s", argv[0]);

}

/* tlsmgr_loop - queue manager main loop */

static int tlsmgr_loop(char *unused_name, char **unused_argv)
{
    /*
     * This routine runs as part of the event handling loop, after the event
     * manager has delivered a timer or I/O event (including the completion
     * of a connection to a delivery process), or after it has waited for a
     * specified amount of time. The result value of qmgr_loop() specifies
     * how long the event manager should wait for the next event.
     */
#define DONT_WAIT	0
#define WAIT_FOR_EVENT	(-1)

    if (clnt_scache_db_active)
	clnt_scache_db_active = tlsmgr_do_scache_check(clnt_scache_db,
					var_smtp_tls_scache_timeout, 0);
    if (srvr_scache_db_active)
	srvr_scache_db_active = tlsmgr_do_scache_check(srvr_scache_db,
					var_smtpd_tls_scache_timeout, 0);
    if (clnt_scache_db_active || srvr_scache_db_active)
	return (DONT_WAIT);
    return (WAIT_FOR_EVENT);
}

/* pre_accept - see if tables have changed */

static void pre_accept(char *unused_name, char **unused_argv)
{
    if (dict_changed()) {
	msg_info("table has changed -- exiting");
	exit(0);
    }
}

/* tlsmgr_pre_init - pre-jail initialization */

static void tlsmgr_pre_init(char *unused_name, char **unused_argv)
{
    int rand_bytes;
    unsigned char buffer[255];

    /*
     * Access the external sources for random seed. We may not be able to
     * access them again if we are sent to chroot jail, so we must leave
     * dev: and egd: type sources open.
     */
    if (*var_tls_rand_source) {
        if (!strncmp(var_tls_rand_source, "dev:", 4)) {
	    /*
	     * Source is a random device
	     */
	    rand_source_dev_fd = open(var_tls_rand_source + 4, 0, 0);
	    if (rand_source_dev_fd == -1) 
		msg_fatal("Could not open entropy device %s",
			  var_tls_rand_source);
	    if (var_tls_rand_bytes > 255)
		var_tls_rand_bytes = 255;
	    rand_bytes = read(rand_source_dev_fd, buffer, var_tls_rand_bytes);
	    RAND_seed(buffer, rand_bytes);
	} else if (!strncmp(var_tls_rand_source, "egd:", 4)) {
	    /*
	     * Source is a EGD compatible socket
	     */
	    rand_source_socket_fd = unix_connect(var_tls_rand_source +4,
						 BLOCKING, 10);
	    if (rand_source_socket_fd == -1)
		msg_fatal("Could not connect to %s", var_tls_rand_source);
	    if (var_tls_rand_bytes > 255)
		var_tls_rand_bytes = 255;
	    buffer[0] = 1;
	    buffer[1] = var_tls_rand_bytes;
	    if (write(rand_source_socket_fd, buffer, 2) != 2)
		msg_fatal("Could not talk to %s", var_tls_rand_source);
	    if (read(rand_source_socket_fd, buffer, 1) != 1)
		msg_fatal("Could not read info from %s", var_tls_rand_source);
	    rand_bytes = buffer[0];
	    if (read(rand_source_socket_fd, buffer, rand_bytes) != rand_bytes)
		msg_fatal("Could not read data from %s", var_tls_rand_source);
	    RAND_seed(buffer, rand_bytes);
	} else {
	    rand_bytes = RAND_load_file(var_tls_rand_source,
					var_tls_rand_bytes);
	}
    }

    /*
     * Now open the PRNG exchange file
     */
    if (*var_tls_rand_exch_name) {
	rand_exch_fd = open(var_tls_rand_exch_name, O_RDWR | O_CREAT, 0600);
    }

    /*
     * Finally, open the session cache files. Remove old files, if still there.
     * If we could not remove the old files, something is pretty wrong and we
     * won't touch it!!
     */
    if (*var_smtp_tls_scache_db)
	clnt_scache_db = tlsmgr_cache_open(var_smtp_tls_scache_db);
    if (*var_smtpd_tls_scache_db)
	srvr_scache_db = tlsmgr_cache_open(var_smtpd_tls_scache_db);
}

/* qmgr_post_init - post-jail initialization */

static void tlsmgr_post_init(char *unused_name, char **unused_argv)
{
    unsigned char buffer[1024];

    /*
     * This routine runs after the skeleton code has entered the chroot jail.
     * Prevent automatic process suicide after a limited number of client
     * requests or after a limited amount of idle time.
     */
    var_use_limit = 0;
    var_idle_limit = 0;

    /*
     * Complete thie initialization by reading the additional seed from the
     * PRNG exchange file. Don't care how many bytes were actually read, just
     * seed buffer into the PRNG, regardless of its contents.
     */
    if (rand_exch_fd >= 0) {
	if (myflock(rand_exch_fd, INTERNAL_LOCK, MYFLOCK_OP_SHARED) == -1)
	    msg_fatal("Could not lock random exchange file: %s",
		      strerror(errno));
	read(rand_exch_fd, buffer, 1024);
	if (myflock(rand_exch_fd, INTERNAL_LOCK, MYFLOCK_OP_NONE) == -1)
	    msg_fatal("Could not unlock random exchange file: %s",
		      strerror(errno));
	RAND_seed(buffer, 1024);
	tlsmgr_prng_upd_event(0, (char *) 0);
	tlsmgr_reseed_event(0, (char *) 0);
    }

    clnt_scache_db_active = 0;
    srvr_scache_db_active = 0;
    if (clnt_scache_db)
	tlsmgr_clnt_cache_run_event(0, (char *) 0);
    if (srvr_scache_db)
	tlsmgr_srvr_cache_run_event(0, (char *) 0);
}


/* main - the main program */

int     main(int argc, char **argv)
{
    static CONFIG_STR_TABLE str_table[] = {
	VAR_TLS_RAND_SOURCE, DEF_TLS_RAND_SOURCE, &var_tls_rand_source, 0, 0,
	0,
    };
    static CONFIG_TIME_TABLE time_table[] = {
	VAR_TLS_RESEED_PERIOD, DEF_TLS_RESEED_PERIOD, &var_tls_reseed_period, 0, 0,
	VAR_TLS_PRNG_UPD_PERIOD, DEF_TLS_PRNG_UPD_PERIOD, &var_tls_prng_upd_period, 0, 0,
	0,
    };
    static CONFIG_INT_TABLE int_table[] = {
	VAR_TLS_RAND_BYTES, DEF_TLS_RAND_BYTES, &var_tls_rand_bytes, 0, 0,
	0,
    };

    /*
     * Use the trigger service skeleton, because no-one else should be
     * monitoring our service port while this process runs, and because we do
     * not talk back to the client.
     */
    trigger_server_main(argc, argv, tlsmgr_trigger_event,
			MAIL_SERVER_TIME_TABLE, time_table,
			MAIL_SERVER_INT_TABLE, int_table,
			MAIL_SERVER_STR_TABLE, str_table,
			MAIL_SERVER_PRE_INIT, tlsmgr_pre_init,
			MAIL_SERVER_POST_INIT, tlsmgr_post_init,
			MAIL_SERVER_LOOP, tlsmgr_loop,
			MAIL_SERVER_PRE_ACCEPT, pre_accept,
			0);
    trigger_server_main(argc, argv, tlsmgr_trigger_event,
			MAIL_SERVER_PRE_INIT, tlsmgr_pre_init,
			0);
}

#else
int     main(int argc, char **argv)
{
    msg_fatal("Do not run tlsmgr with TLS support compiled in\n");
}
#endif
