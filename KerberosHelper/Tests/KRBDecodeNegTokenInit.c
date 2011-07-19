
#include <KerberosHelper/KerberosHelper.h>
#include <CoreFoundation/CoreFoundation.h>
#include <GSS/gssapi.h>

int
main(int argc, char **argv)
{
    gss_buffer_desc empty = { 0, NULL }, out;
    OM_uint32 maj_stat, min_stat;
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;

    maj_stat = gss_accept_sec_context(&min_stat, &ctx, GSS_C_NO_CREDENTIAL,
				      &empty, GSS_C_NO_CHANNEL_BINDINGS,
				      NULL, NULL, &out, NULL, NULL, NULL);
    if (maj_stat != GSS_S_CONTINUE_NEEDED)
	errx(1, "gss_accept_sec_context");

    CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, out.value, out.length, kCFAllocatorNull);

    CFDictionaryRef dict = KRBDecodeNegTokenInit(NULL, data);
    if (dict == NULL)
	errx(1, "KRBDecodeNegTokenInit");

    CFShow(dict);
    CFRelease(dict);

    CFRelease(data);
    
    gss_release_buffer(&min_stat, &out);
    gss_delete_sec_context(&min_stat, &ctx, NULL);

    return 0;
}
