#include <Heimdal/krb5.h>
#include <DiskArbitration/DiskArbitration.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>
#include <dispatch/dispatch.h>
#include "KerberosHelper.h"
#include "utils.h"

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int windows = -1;
static int mark = -1;
static int sso = 0;
static int release = 0;
static int monitor = 0;

static void
callback(DADiskRef disk, void *context)
{
    CFStringRef path = NULL, ident;
    CFURLRef url;
    CFDictionaryRef dict;
    size_t len;
    char *str, *str2;

    printf("disappear callback\n");

    dict = DADiskCopyDescription(disk);

    url = (CFURLRef)CFDictionaryGetValue(dict, kDADiskDescriptionVolumePathKey);
    if (url == NULL)
	goto out;

    path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);

    __KRBCreateUTF8StringFromCFString(path, &str);

    len = strlen(str);
    if (len > 0 && str[len - 1] == '/')
	len--;

    asprintf(&str2, "fs:%.*s", (int)len, str);
    free(str);

    printf("unmounted: %s\n", str2);

    ident = CFStringCreateWithCString(NULL, str2, kCFStringEncodingUTF8);
    KRBCredFindByLabelAndRelease(ident);
    free(str2);
    CFRelease(ident);
    CFRelease(path);
 out:
    CFRelease(dict);
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_data data;
    krb5_ccache id;
    time_t t = time(NULL);
    int ch;

    setprogname(argv[0]);

    while ((ch = getopt(argc, argv, "WwUusalRM")) != -1) {

	switch(ch) {
	case 'u':
	    mark = 0;
	    break;
	case 'U':
	    mark = 1;
	    break;
	case 'R':
	    release = 1;
	    break;
	case 'M':
	    monitor = 1;
	    break;
	case 's':
	    sso = 1;
	    break;
	case 'w':
	    windows = 0;
	    break;
	case 'W':
	    windows = 1;
	    break;
	default:
	    errx(1, "usage: %s -[uUWwsaRM]", getprogname());
	}
    }

    argc -= optind;
    argv += optind;

    if (monitor) {
	DASessionRef session = DASessionCreate(kCFAllocatorDefault);

	DASessionSetDispatchQueue(session, dispatch_get_main_queue());

	DARegisterDiskDisappearedCallback(session, NULL, callback, NULL);

	printf("starting RunLoop, ^C to exit\n");
	dispatch_main();
    }

    ret = krb5_init_context(&context);
    if (ret)
	errx(1, "krb5_init_context");
	
    ret = krb5_cc_default(context, &id);
    if (ret)
	errx(1, "krb5_cc_default");
	
    if (release) {
	if (argc != 2)
	    errx(1, "missing principal");

	CFStringRef princ = CFStringCreateWithCString(NULL, argv[0],
						      kCFStringEncodingUTF8);
	KRBCredRemoveReference(princ);
	exit(0);
    }

    if (windows == 0) {
	ret = krb5_cc_get_config(context, id, NULL, "windows", &data);
	if (ret)
	    errx(1, "krb5_cc_get_config");
	    
	if (data.length != sizeof(t))
	    errx(1, "time wrong");
	    
	memcpy(&t, data.data, sizeof(t));
	    
	printf("get time: %ld\n", (long)t);
    } else if (windows == 1) {
	printf("set time: %ld\n", (long)t);
	data.data = (void *)&t;
	data.length = sizeof(t);
	ret = krb5_cc_set_config(context, id, NULL, "windows", &data);
	if (ret)
	    errx(1, "krb5_cc_set_config");
    }

    if (sso) {
	data.data = (void *)"kcc-setup";
	data.length = strlen((char *)data.data);

	ret = krb5_cc_set_config(context, id, NULL, "apple-sso", &data);
	if (ret)
	    errx(1, "krb5_cc_set_config");
    }

    if (mark == 1) {
	if (argc != 2)
	    errx(1, "missing principal and id");

	CFStringRef princ = CFStringCreateWithCString(NULL, argv[0],
						      kCFStringEncodingUTF8);
	CFStringRef ident = CFStringCreateWithCString(NULL, argv[1],
						      kCFStringEncodingUTF8);

	OSStatus r = KRBCredAddReferenceAndLabel(princ, ident);
	if (r != noErr)
	    errx(1, "KRBCredAddReferenceAndLabel: %d", (int)r);

    } else if (mark == 0) {
	if (argc != 1)
	    errx(1, "missing id");

	CFStringRef ident = CFStringCreateWithCString(NULL, argv[0],
						      kCFStringEncodingUTF8);

	OSStatus r = KRBCredFindByLabelAndRelease(ident);
	if (r != noErr)
	    errx(1, "KRBCredFindByLabelAndRelease: %d", (int)r);

    }
    
    return 0;
}
