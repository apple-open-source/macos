/*
 * Copyright (c) 1997-2004 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kadmin_locl.h"
#include "kadmin-commands.h"
#include <kadm5/private.h>

#ifdef __APPLE__
#include <HeimODAdmin.h>
#endif

extern int local_flag;

#ifdef __APPLE__

static krb5_error_code
od_dump_entry(krb5_context kcontext, HDB *db, hdb_entry_ex *entry, void *data)
{
    CFErrorRef error = NULL;
    CFDictionaryRef dict;
    CFStringRef fn, uuidstr;
    CFUUIDRef uuid;
    CFURLRef url;

    dict = HeimODDumpHdbEntry(&entry->entry, &error);
    if (dict == NULL) {
	if (error)
	    CFRelease(error);
	return 0;
    }

    uuid = CFUUIDCreate(NULL);
    if (uuid == NULL) {
	krb5_warnx(kcontext, "out of memory");
	return 0;
    }
    
    uuidstr = CFUUIDCreateString(NULL, uuid);
    CFRelease(uuid);
    if (uuidstr == NULL) {
	krb5_warnx(kcontext, "out of memory");
	return 0;
    }

    fn = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s/%@.plist"), (char *)data, uuidstr);
    CFRelease(uuidstr);
    if (fn == NULL) {
	krb5_warnx(kcontext, "out of memory");
	return 0;
    }

    url = CFURLCreateWithFileSystemPath(NULL, fn,  kCFURLPOSIXPathStyle, false);
    CFRelease(fn);
    if (url == NULL) {
	krb5_warnx(kcontext, "out of memory");
	return 0;
    }

    CFDataRef xmldata = CFPropertyListCreateXMLData(NULL, dict);
    CFRelease(dict);
    if (xmldata == NULL) {
	CFRelease(url);
	krb5_warnx(kcontext, "out of memory");
	return 0;
    }

    CFURLWriteDataAndPropertiesToResource(url, xmldata, NULL, NULL);

    CFRelease(url);
    CFRelease(xmldata);

    return 0;
}
#endif

int
dump(struct dump_options *opt, int argc, char **argv)
{
    krb5_error_code (*func)(krb5_context, HDB *, hdb_entry_ex *, void *);
    krb5_error_code ret;
    void *arg;
    const char *format = "heimdal";
    FILE *f = NULL;

    if (opt->format_string)
	format = opt->format_string;

    if (strcasecmp(format, "heimdal") == 0) {
	func = hdb_print_entry;

	if (argc == 0)
	    arg = stdout;
	else
	    arg = f = fopen(argv[0], "w");

#ifdef __APPLE__
    } else if (strcasecmp(format, "od") == 0) {
	
	func = od_dump_entry;
	if (argc == 0)
	    arg = rk_UNCONST(".");
	else
	    arg = argv[0];
#endif
    } else {
	krb5_warnx(context, "unknown dump format: %s", format);
	return 0;
    }

    if (opt->mit_dump_file_string) {
	ret = hdb_mit_dump(context, opt->mit_dump_file_string,
			   func, arg);
	if (ret)
	    krb5_warn(context, ret, "hdb_mit_dump");

    } else {
	HDB *db = NULL;

	if (!local_flag) {
	    krb5_warnx(context, "od-dump is only available in local (-l) mode");
	    return 0;
	}

	db = _kadm5_s_get_db(kadm_handle);

	ret = db->hdb_open(context, db, O_RDONLY, 0600);
	if (ret) {
	    krb5_warn(context, ret, "hdb_open");
	    goto out;
	}

	ret = hdb_foreach(context, db, opt->decrypt_flag ? HDB_F_DECRYPT : 0,
			  func, arg);
	if (ret)
	    krb5_warn(context, ret, "hdb_foreach");

	db->hdb_close(context, db);
    }
    if (f)
	fclose(f);
out:
    return ret != 0;
}

int
od_dump(struct od_dump_options *opt, int argc, char **argv)
{
    struct dump_options dumpopt;

    memset(&dumpopt, 0, sizeof(dumpopt));
    dumpopt.decrypt_flag = opt->decrypt_flag;
    dumpopt.format_string = "od";

    return dump(&dumpopt, argc, argv);
}
