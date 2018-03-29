#include "krb5_locl.h"
#include "krb5cf-protos.h"
#include <getarg.h>
#include <err.h>
#include <CoreFoundation/CoreFoundation.h>

static int debug_flag	= 0;
static int version_flag = 0;
static int help_flag	= 0;

static void
test_krb5_kcm_get_principal_list(krb5_context context)
{
    krb5_ccache id1, id2;
    krb5_error_code ret;
    krb5_principal p1, p2;
    krb5_data data;


    CFArrayRef arr = NULL;
    const char *name = "foo-name";
    const char *name1 = "foouser@TEST.COM";
    const char *name2 = "user2@MAC.NET";

    data.data = rk_UNCONST(name);
    data.length = strlen(name);

    ret = krb5_parse_name(context, name1, &p1);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_name: first");

    ret = krb5_parse_name(context, name2, &p2);
    if (ret)
	krb5_err(context, 1, ret, "krb5_parse_name: second");

    krb5_cc_new_unique(context, krb5_cc_type_kcm, "bar", &id1);
    krb5_cc_new_unique(context, krb5_cc_type_kcm, "baz", &id2);

    ret = krb5_cc_initialize(context, id1, p1);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_initialize: first");

    ret = krb5_cc_initialize(context, id2, p2);
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_initialize: second");

    ret = krb5_cc_set_config(context, id1, p1, "dz", &data);
    if (ret)
	krb5_errx(context, 1, "krb5_cc_set_config: first");

    ret = krb5_cc_set_config(context, id2, p2, "dz", &data);
    if (ret)
	krb5_errx(context, 1, "krb5_cc_set_config: second");

    arr = krb5_kcm_get_principal_list(context);
    if (arr) {
	CFIndex length = 0;
	length = CFArrayGetCount(arr);

	// Unit tests running in BATS will have to account for BATS OD/AC principal
	if (length >= 2) {
	    int match = 0;
	    for (CFIndex i = 0; i < length; i++)
	    {
		CFDictionaryRef dict = NULL;
		dict = CFArrayGetValueAtIndex(arr, i);
		if (dict) {
		    CFIndex count = 0;
		    count = CFDictionaryGetCount(dict);
		    if (count == 3) {
			if ((CFDictionaryContainsValue(dict, CFStringCreateWithCString(NULL, name1, kCFStringEncodingUTF8)) ||
			      CFDictionaryContainsValue(dict, CFStringCreateWithCString(NULL, name2, kCFStringEncodingUTF8)))) {
			    match++;
			}
		    }
		}
	    }
	    if (match != 2) {
		krb5_errx(context, 1, "principal names don't match");
	    }
	} else {
	    krb5_errx(context, 1, "Invalid number of principal names");
	}

	CFRelease(arr);
    } else {
	krb5_errx(context, 1, "krb5_kcm_get_principal_list");
    }

    krb5_free_principal(context, p1);
    krb5_free_principal(context, p2);

    krb5_cc_destroy(context, id1);
    krb5_cc_destroy(context, id2);
}

static struct getargs args[] = {
    {"debug",	'd',	arg_flag,	&debug_flag,
	"turn on debugging", NULL },
    {"version",	0,	arg_flag,	&version_flag,
	"print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,
	NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args), NULL, "hostname ...");
    exit (ret);
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    int optidx = 0;

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

#ifdef HAVE_KCM
    test_krb5_kcm_get_principal_list(context);
#endif
}
