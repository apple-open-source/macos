/* $Id: Getrusage.xs 11 2006-02-04 00:53:56Z taffy $ -*- c -*- */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#define STORE_FIELD(format,data) string_store(hash, #data, format, res.data);
#define STORE_TIMEVAL(x) string_store(hash, #x, "%.6f", timeval_to_double(&res.x));
#define syscall(x) ({ int y = (x); if(y < 0) { perror(#x); }; y; })

static void string_store(HV* hash, const char *key, const char *fmt, ...) {
	char buffer[0x1000];
	va_list p;
	int len;

	assert(hash != NULL);
	assert(key != NULL);
	assert(fmt != NULL);

	va_start(p, fmt);
	len = vsnprintf(buffer, sizeof(buffer), fmt, p);
	va_end(p);

	hv_store(hash, key, strlen(key), newSVpv(buffer, len), 0);
}

static double timeval_to_double(const struct timeval *tv) {
	assert(tv != NULL);

	return tv->tv_sec + tv->tv_usec * 1e-6;
}

static void fetch_getrusage(HV *hash, int who) {
	struct rusage res;

	assert(hash != NULL);
	assert(who == RUSAGE_SELF || who == RUSAGE_CHILDREN);
	
	if(syscall(getrusage(who, &res)) >= 0) {
		STORE_TIMEVAL(ru_utime); /* leserlich etwas harmonieren^^ */
		STORE_TIMEVAL(ru_stime);

		STORE_FIELD("%ld", ru_maxrss);
		STORE_FIELD("%ld", ru_ixrss);
		STORE_FIELD("%ld", ru_idrss);
		STORE_FIELD("%ld", ru_isrss);
		STORE_FIELD("%ld", ru_minflt);
		STORE_FIELD("%ld", ru_majflt);
		STORE_FIELD("%ld", ru_nswap);
		STORE_FIELD("%ld", ru_inblock);
		STORE_FIELD("%ld", ru_oublock);
		STORE_FIELD("%ld", ru_msgsnd);
		STORE_FIELD("%ld", ru_msgrcv);
		STORE_FIELD("%ld", ru_nsignals);
		STORE_FIELD("%ld", ru_nvcsw);
		STORE_FIELD("%ld", ru_nivcsw);
	}
}

MODULE = Unix::Getrusage PACKAGE = Unix::Getrusage

SV*
getrusage()
PREINIT:
HV *hash;
CODE:
hash = newHV();
fetch_getrusage(hash, RUSAGE_SELF);
sv_2mortal((SV*) hash);
RETVAL = newRV_inc((SV*)hash);
OUTPUT:
RETVAL

SV*
getrusage_children()
PREINIT:
HV *hash;
CODE:
hash = newHV();
fetch_getrusage(hash, RUSAGE_CHILDREN);
sv_2mortal((SV*) hash);
RETVAL = newRV_inc((SV*)hash);
OUTPUT:
RETVAL
