/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
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

#include "config.h"

#include "HeimODAdmin.h"

#ifdef __APPLE_PRIVATE__
#include <OpenDirectory/OpenDirectoryPriv.h>
#endif

#include <roken.h>
#include <getarg.h>
#include <sl.h>
#include <err.h>
#include <hex.h>

#include "hod-commands.h"

static ODNodeRef node;

static ODRecordRef
find_record(const char *entry)
{
    CFStringRef recordname;
    ODRecordRef record;
    ODRecordType type = kODRecordTypeUsers;

    if (strncmp(entry, "/Users/", 7) == 0) {
	type = kODRecordTypeUsers;
	entry += 7;
    } else if (strncmp(entry, "/Computers/", 11) == 0) {
	type = kODRecordTypeComputers;
	entry += 11;
    } else if (entry[0] == '/') {
	warnx("unknown type for entry: %s", entry);
	return NULL;
    }

    recordname = CFStringCreateWithCString(kCFAllocatorDefault, entry, kCFStringEncodingUTF8);
    if (recordname == NULL)
	return NULL;

    record = ODNodeCopyRecord(node, type, recordname, NULL, NULL);
    if (record == NULL)
	warnx("ODNodeCopyRecord failed");
    CFRelease(recordname);
    return record;
}


int
principal_create(void *opt, int argc, char **argv)
{
    CFStringRef principal = NULL;
    ODRecordRef record = NULL;
    int error = 1;
    
    if (argc < 1)
	errx(1, "missing record");

    record = find_record(argv[0]);
    if (record == NULL)
	goto out;

    if (argc > 1) {
	principal = CFStringCreateWithCString(kCFAllocatorDefault, argv[1],
					      kCFStringEncodingUTF8);
	if (principal == NULL)
	    goto out;
    }
    
    if (HeimODCreatePrincipalData(node, record, NULL, principal, NULL)) {
	warnx("HeimODCreatePrincipalData failed");
	goto out;
    }
	
    error = 0;

 out:
    if (record)
	CFRelease(record);
    if (principal)
	CFRelease(principal);
    return error;
}

int
principal_add_cert(struct principal_add_cert_options *opt, int argc, char **argv)
{
    SecCertificateRef cert = NULL;
    OSStatus status = 0;
    ODRecordRef record;

    record = find_record(argv[0]);
    if (record == NULL)
	goto out;

    if (opt->use_default_sharing_identity_flag) {
	SecIdentityRef ref;

	ref = SecIdentityCopyPreferred(CFSTR("com.apple.system.DefaultSharingIdentity"), NULL, NULL);
	if (ref == NULL) {
	    status = 1;
	    goto out;
	}

	status = SecIdentityCopyCertificate(ref, &cert);
	CFRelease(ref);
	if (status)
	    goto out;
    } else {
	printf("don't know how to get certificate");
	status = 1;
	goto out;
    }

    CFErrorRef error = NULL;

    status = HeimODAddCertificate(node, record, cert, &error);
    if (error) {
	CFShow(error);
	CFRelease(error);
    }
    if (status) {
	warnx("HeimODAddCertificate failed");
	goto out;
    }

 out:
    if (record)
	CFRelease(record);
    if (cert)
	CFRelease(cert);

    return (int)status;
}

int
password_command(struct password_options *opt, int argc, char **argv)
{
    CFMutableArrayRef enctypes = NULL;
    ODRecordRef record = NULL;
    CFErrorRef cferror = NULL;
    CFStringRef principal = NULL, password = NULL;
    unsigned long flags = 0;
    int error = 1;
    size_t i;

    if (opt->encryption_types_strings.num_strings) {

	enctypes = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	if (enctypes == NULL) {
	    warn("CFArrayCreateMutable");
	    return 1;
	}
    
	for (i = 0; i < opt->encryption_types_strings.num_strings; i++) {
	    CFStringRef el;

	    el = CFStringCreateWithCString(kCFAllocatorDefault, opt->encryption_types_strings.strings[i], kCFStringEncodingUTF8);
	    if (el == NULL) {
		warn("CFStringCreateWithCString");
		CFRelease(enctypes);
		return 1;
	    }
	    CFArrayAppendValue(enctypes, el);
	    CFRelease(el);
	}
    }

    if (opt->append_flag)
	flags |= kHeimODAdminSetKeysAppendKey;

    record = find_record(argv[0]);
    if (record == NULL)
	goto out;

    password = CFStringCreateWithCString(kCFAllocatorDefault, argv[1], kCFStringEncodingUTF8);
    if (password == NULL)
	goto out;

    if (argc > 2) {
	principal = CFStringCreateWithCString(kCFAllocatorDefault, argv[2], kCFStringEncodingUTF8);
	if (principal == NULL)
	    goto out;
    }

    error = HeimODSetKeys(node, record, principal, enctypes, password, flags, &cferror);
    if (cferror) {
	CFRelease(cferror);
	cferror = NULL;
    }

    if (!ODRecordSynchronize(record, NULL)) {
	warnx("ODRecordSynchronize failed");
	goto out;
    }

    error = 0;

 out:
    if (record)
	CFRelease(record);
    if (enctypes)
	CFRelease(enctypes);
    if (principal)
	CFRelease(principal);
    if (password)
	CFRelease(password);

    return error;
}

static int
principal_opflags(int argc, char **argv, void (^op)(ODNodeRef node, ODRecordRef record, CFStringRef flag))
{
    ODRecordRef record = NULL;
    int error = 1;

    record = find_record(argv[0]);
    if (record == NULL)
	goto out;

    argv++;
    argc--;

    while(argc) {
	CFStringRef flag = CFStringCreateWithCString(kCFAllocatorDefault, argv[0], kCFStringEncodingUTF8);
	if (flag == NULL)
	    goto out;

	op(node, record, flag);

	CFRelease(flag);

	argc--;
	argv++;
    }

    error = 0;
 out:
    if (record)
	CFRelease(record);
    return error;
}

int
principal_clearflags(void *opt, int argc, char **argv)
{
    return principal_opflags(argc, argv, ^(ODNodeRef node, ODRecordRef record, CFStringRef flag) {
	    HeimODClearKerberosFlags(node, record, flag, NULL);
	});
}

int
principal_setflags(void *opt, int argc, char **argv)
{
    return principal_opflags(argc, argv, ^(ODNodeRef node, ODRecordRef record, CFStringRef flag) {
	    HeimODSetKerberosFlags(node, record, flag, NULL);
	});
}

int
principal_getflags(void *opt, int argc, char **argv)
{
    ODRecordRef record = NULL;
    CFArrayRef flags = NULL;
    int error = 1;
    CFIndex n;

    record = find_record(argv[0]);
    if (record == NULL) {
	warnx("failed to find %s", argv[0]);
	goto out;
    }

    flags = HeimODCopyKerberosFlags(node, record, NULL);
    if (flags == NULL) {
	warnx("no flags for %s", argv[0]);
	goto out;
    }

    CFShow(CFSTR("Flags:"));

    for (n = 0; n < CFArrayGetCount(flags); n++) {
	CFStringRef s = CFArrayGetValueAtIndex(flags, n);
	CFShow(s);
    }

    error = 0;
 out:
    if (flags)
	CFRelease(flags);
    if (record)
	CFRelease(record);
    return error;
}

int
principal_get_keyinfo(void *opt, int argc, char **argv)
{
    ODRecordRef record = NULL;
    CFArrayRef keys = NULL;
    CFIndex n, count;
    int error = 1;
    
    record = find_record(argv[0]);
    if (record == NULL) {
	warnx("failed to find %s", argv[0]);
	goto out;
    }
    
    keys = ODRecordCopyValues(record, CFSTR("dsAttrTypeNative:KerberosKeys"), NULL);
    if (keys == NULL) {
	printf("no keys available\n");
	goto out;
    }
    
    count = CFArrayGetCount(keys);
    for (n = 0; n < count; n++) {
	CFDataRef el = CFArrayGetValueAtIndex(keys, n);
	if (el == NULL || CFGetTypeID(el) != CFDataGetTypeID())
	    continue;
	CFStringRef str = HeimODKeysetToString(el, NULL);
	if (str) {
	    CFShow(str);
	    CFRelease(str);
	}
    }
    
    error = 0;
out:
    if (keys)
	CFRelease(keys);
    if (record)
	CFRelease(record);
    return error;
}

int
principal_setacl(void *opt, int argc, char **argv)
{
    return principal_opflags(argc, argv, ^(ODNodeRef node, ODRecordRef record, CFStringRef flag) {
	    HeimODSetACL(node, record, flag, NULL);
	});
}

int
principal_getacl(void *opt, int argc, char **argv)
{
    ODRecordRef record = NULL;
    CFArrayRef acl = NULL;
    int error = 1;
    CFIndex n;

    record = find_record(argv[0]);
    if (record == NULL) {
	warnx("failed to find %s", argv[0]);
	goto out;
    }

    acl = HeimODCopyACL(node, record, NULL);
    if (acl == NULL) {
	warnx("no explicit ACL for %s", argv[0]);
	goto out;
    }

    CFShow(CFSTR("ACL:"));

    for (n = 0; n < CFArrayGetCount(acl); n++) {
	CFStringRef s = CFArrayGetValueAtIndex(acl, n);
	CFShow(s);
    }

    error = 0;
 out:
    if (acl)
	CFRelease(acl);
    if (record)
	CFRelease(record);
    return error;

}

int
principal_clearacl(void *opt, int argc, char **argv)
{
    return principal_opflags(argc, argv, ^(ODNodeRef node, ODRecordRef record, CFStringRef flag) {
	    HeimODClearACL(node, record, flag, NULL);
	});
}

int
alias_add(void *opt, int argc, char **argv)
{
    return principal_opflags(argc, argv, ^(ODNodeRef node, ODRecordRef record, CFStringRef flag) {
	    HeimODAddServerAlias(node, record, flag, NULL);
	});
}

int
alias_remove(void *opt, int argc, char **argv)
{
    return principal_opflags(argc, argv, ^(ODNodeRef node, ODRecordRef record, CFStringRef flag) {
	    HeimODRemoveServerAlias(node, record, flag, NULL);
	});
}

int
appleid_alias_add(void *opt, int argc, char **argv)
{
    return principal_opflags(argc, argv, ^(ODNodeRef node, ODRecordRef record, CFStringRef flag) {
	    HeimODAddAppleIDAlias(node, record, flag, NULL);
	});
}

int
appleid_alias_remove(void *opt, int argc, char **argv)
{
    return principal_opflags(argc, argv, ^(ODNodeRef node, ODRecordRef record, CFStringRef flag) {
	    HeimODRemoveAppleIDAlias(node, record, flag, NULL);
	});
}

int
appleid_cert_add(void *opt, int argc, char **argv)
{
    return principal_opflags(argc, argv, ^(ODNodeRef node, ODRecordRef record, CFStringRef flag) {
	    HeimODAddCertificateSubjectAndTrustAnchor(node, record, flag, CFSTR("CN=Apple Root CA,OU=Apple Certification Authority,O=Apple Inc.,C=US"), NULL);
	});
}

int
appleid_cert_remove(void *opt, int argc, char **argv)
{
    return principal_opflags(argc, argv, ^(ODNodeRef node, ODRecordRef record, CFStringRef flag) {
	    HeimODRemoveAppleIDAlias(node, record, flag, NULL);
	});
}


int
principal_delete(void *opt, int argc, char **argv)
{
    CFStringRef principal = NULL;
    ODRecordRef record = NULL;
    int error = 1;
    
    if (argc < 1)
	errx(1, "missing record");

    record = find_record(argv[0]);
    if (record == NULL)
	goto out;

    if (argc > 1) {
	principal = CFStringCreateWithCString(kCFAllocatorDefault, argv[1], kCFStringEncodingUTF8);
	if (principal == NULL)
	    goto out;
    }

    if (HeimODRemovePrincipalData(node, record, principal, NULL))
	goto out;

    error = 0;

 out:
    if (record)
	CFRelease(record);
    if (principal)
	CFRelease(principal);
    return error;
}

int
default_enctypes(void *opt, int argc, char **argv)
{
    CFArrayRef enctypes = HeimODCopyDefaultEnctypes(NULL);
    CFIndex n;

    if (enctypes == NULL)
	errx(1, "malloc");

    CFShow(CFSTR("Default enctypes:"));

    for (n = 0; n < CFArrayGetCount(enctypes); n++) {
	CFStringRef s = CFArrayGetValueAtIndex(enctypes, n);
	CFShow(s);
    }
    CFRelease(enctypes);
    return 0;
}


int
dump(void *opt, int argc, char **argv)
{
    CFStringRef principal = NULL;
    CFErrorRef error = NULL;
    CFDictionaryRef entrydump = NULL;
    CFDataRef xmldata = NULL;
    ODRecordRef record;
    CFStringRef fn = NULL;
    CFURLRef url = NULL;
    int ret = 1;

    record = find_record(argv[0]);
    if (record == NULL)
	goto out;
    
    if (argc > 1) {
	principal = CFStringCreateWithCString(kCFAllocatorDefault, argv[1], kCFStringEncodingUTF8);
	if (principal == NULL)
	    goto out;
    }

    entrydump = HeimODDumpRecord(node, record, principal, &error);
    if (entrydump == NULL)
	errx(1, "dump failed");

    fn = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s.plist"), argv[0]);
    if (fn == NULL)
	goto out;

    url = CFURLCreateWithFileSystemPath(NULL, fn,  kCFURLPOSIXPathStyle, false);
    if (url == NULL)
	goto out;

    xmldata = CFPropertyListCreateXMLData(NULL, entrydump);
    if (xmldata == NULL)
	goto out;

    CFURLWriteDataAndPropertiesToResource(url, xmldata, NULL, NULL);

    ret = 0;

 out:
    if (principal)
	CFRelease(principal);
    if (url)
      CFRelease(url);
    if (fn)
      CFRelease(fn);
    if (xmldata)
      CFRelease(xmldata);
    if (entrydump)
	CFRelease(entrydump);
    return ret;
}

int
load(struct load_options *opt, int argc, char **argv)
{
    CFDictionaryRef entrydump = NULL;
    CFErrorRef error = NULL;
    ODRecordRef record;
    CFStringRef fn = NULL;
    CFReadStreamRef stream = NULL;
    CFURLRef url = NULL;
    char *path = argv[1];
    int ret = 1;
    unsigned long flags = 0;
    
    if (opt->append_flag)
	flags |= kHeimODAdminLoadAsAppend;

    record = find_record(argv[0]);
    if (record == NULL)
	goto out;

    if (argc > 1)
	fn = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), path);
    else
	fn = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s.plist"), argv[0]);
    if (fn == NULL)
	goto out;

    url = CFURLCreateWithFileSystemPath(NULL, fn,  kCFURLPOSIXPathStyle, false);
    if (url == NULL)
	goto out;

    stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
    if (stream == NULL)
	goto out;

    if (!CFReadStreamOpen(stream))
	goto out;
    
    entrydump = (CFDictionaryRef)CFPropertyListCreateWithStream(NULL, stream, 0, 0, NULL, &error);
    if (entrydump == NULL)
	goto out;

    if (!HeimODLoadRecord(node, record, entrydump, flags, &error))
	errx(1, "HeimODLoadRecord failed");

    ret = 0;
 out:
    if (fn)
	CFRelease(fn);
    if (url)
	CFRelease(url);
    if (stream)
	CFRelease(stream);
    if (entrydump)
	CFRelease(entrydump);
    if (record)
	CFRelease(record);
    return ret;
}


int
keyset(struct keyset_options *opt, int argc, char **argv)
{
    const char *principalstr = argv[0], *passwordstr = argv[1];
    CFMutableArrayRef prevKeys = NULL, enctypes = NULL;
    CFArrayRef keys;
    CFStringRef principal = NULL, password = NULL;
    CFIndex count, n;
    unsigned long flags = 0;
    
    printf("principal: %s password: %s\n", principalstr, passwordstr);
    
    /* copy in old keys */
    if (opt->old_keyset_strings.num_strings) {
	prevKeys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (prevKeys == NULL)
	    errx(1, "out of memory");

	for (n = 0; n < opt->old_keyset_strings.num_strings; n++) {
	    size_t len = strlen(opt->old_keyset_strings.strings[n]);
	    void *data = malloc(len);
	    ssize_t ret;
	    ret = rk_hex_decode(opt->old_keyset_strings.strings[n], data, len);
	    if (ret < 0)
		errx(1, "failed to parse as hex: %s", opt->old_keyset_strings.strings[n]);
	    CFDataRef el = CFDataCreate(NULL, data, ret);
	    if (el == NULL)
		errx(1, "out of memory");
	    free(data);
	    CFArrayAppendValue(prevKeys, el);
	    CFRelease(el);
	}
    }
    
    if (opt->enctype_strings.num_strings) {

	enctypes = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (enctypes == NULL)
	    errx(1, "out of memory");
	
      	for (n = 0; n < opt->enctype_strings.num_strings; n++) {
	    CFStringRef str = CFStringCreateWithCString(NULL, opt->enctype_strings.strings[n], kCFStringEncodingUTF8);
	    if (str == NULL)
		errx(1, "out of memory");
	    CFArrayAppendValue(enctypes, str);
	    CFRelease(str);
	}
    }
    
    if (opt->delete_flag) {
	flags |= kHeimODAdminDeleteEnctypes;
    } else {
	if (argc < 2)
	    errx(1, "missing principal and password arguments that is needed when adding new keys");
	
	principal = CFStringCreateWithCString(NULL, principalstr, kCFStringEncodingUTF8);
	password = CFStringCreateWithCString(NULL, passwordstr, kCFStringEncodingUTF8);
	if (principal == NULL || password == NULL)
	    errx(1, "out of memory");

	if (opt->append_flag)
	    flags |= kHeimODAdminAppendKeySet;
    }

    
    keys = HeimODModifyKeys(prevKeys, principal, enctypes, password, flags, NULL);
    if (keys == NULL)
	errx(1, "HeimODModifyKeys failed");

    count = CFArrayGetCount(keys);
    for (n = 0; n < count; n++) {
	CFStringRef value;
	char *str;
	
	CFDataRef element = CFArrayGetValueAtIndex(keys, n);
	if (CFGetTypeID(element) != CFDataGetTypeID())
	    continue;

	value = HeimODKeysetToString(element, NULL);
	if (value == NULL)
	    errx(1, "HeimODKeysetToString failed");
	
	CFShow(value);
	CFRelease(value);
	
	hex_encode(CFDataGetBytePtr(element), CFDataGetLength(element), &str);
	printf("raw: %s\n", str);
    }
    
    if (enctypes)
	CFRelease(enctypes);
    if (principal)
	CFRelease(principal);
    if (password)
	CFRelease(password);
    if (prevKeys)
	CFRelease(prevKeys);
    CFRelease(keys);
    
    return 0;
}

int
help(void *opt, int argc, char **argv)
{
    sl_slc_help(commands, argc, argv);
    return 0;
}

static int help_flag;
static int version_flag;
static char *local_path_string;

static struct getargs args[] = {
    {	"local-path",    0,     arg_string, &local_path_string, "OD Local path string" },
    {	"help",		'h',	arg_flag,   &help_flag },
    {	"version",	'v',	arg_flag,   &version_flag }
};

static int num_args = sizeof(args) / sizeof(args[0]);

static void
usage(int ret)
{
    arg_printusage (args, num_args, NULL, "node-path command ...");
    exit (ret);
}

int
main(int argc, char **argv)
{
    ODSessionRef session = kODSessionDefault;
    int ret, exit_status = 0;
    CFStringRef root;
    int optidx = 0;

    setprogname(argv[0]);
    
    if(getarg(args, num_args, argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if (version_flag) {
	print_version(NULL);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    if (argc < 2)
	errx(1, "command missing ODNode to operate on and node");

    if (strcmp(".", argv[0]) == 0)
	root = CFSTR("/Local/Default");
    else
	root = CFStringCreateWithCString(kCFAllocatorDefault, argv[0], kCFStringEncodingUTF8);
    if (root == NULL)
	errx(1, "out of memory");
    
    if (local_path_string) {
#ifdef __APPLE_PRIVATE__
	CFMutableDictionaryRef options;

	options = CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	if (options == NULL)
	    errx(1, "out of memory");

	CFStringRef local_path = CFStringCreateWithCString(kCFAllocatorDefault, local_path_string, kCFStringEncodingUTF8);
	CFDictionaryAddValue(options, kODSessionLocalPath, local_path);
	CFRelease(local_path);

	session = ODSessionCreate(kCFAllocatorDefault, options, NULL);
	CFRelease(options);
#else
	errx(1, "no supported");
#endif
    }


    CFErrorRef error;
    node = ODNodeCreateWithName(kCFAllocatorDefault, session, root, &error);

    if (node == NULL) {
	if (error)
	    CFShow(error);
	errx(1, "ODNodeCreateWithName failed");
    }

    if (session)
	CFRelease(session);

    argc -= 1;
    argv += 1;
    
    ret = sl_command(commands, argc, argv);
    if(ret == -1) {
	warnx("unrecognized command: %s", argv[0]);
	exit_status = 2;
    } else if (ret == -2)
	ret = 0;
    if (ret != 0)
	exit_status = 1;
    
    CFRelease(node);
    CFRelease(root);

    return exit_status;
}
