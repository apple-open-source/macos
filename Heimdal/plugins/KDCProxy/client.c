
#include <Heimdal/krb5.h>
#include <Heimdal/send_to_kdc_plugin.h>

static krb5_error_code
init(krb5_context context, void **ctx)
{
    return 0;
}

static void
fini(void *ctx)
{
    return 0;
}

static krb5_error_code
requestToURL(krb5_context context,
	     const char *curl,
	     time_t timeout,
	     NSData *outdata,
	     krb5_data *retdata)
{
    NSMutableURLRequest *request = NULL;
    NSURLResponse *response = NULL;
    KDC_PROXY_MESSAGE msg;
    NSURL *url = NULL;
    size_t size;

    url = [NSURL URLWithString:[NSString stringWithUTF8String:curl]];
    if (url == NULL)
	return ENOMEM;

    request = [NSMutableURLRequest
	       requestWithURL:url 
	       cachePolicy:NSURLRequestReloadIgnoringCacheData
	       timeoutInterval:(NSTimeInterval)timeout];
    
    [request setBody:outdata];
    [request setHTTPMethod:@"POST"];

    NSData *reply = [NSURLConnection sendSynchronousRequest:request
		     returningResponse:&response
		     error:NULL];
    if (reply == NULL)
	goto out;
    
    ret = decode_KDC_PROXY_MESSAGE(&msg, [data bytes], [data length], &size);
    if (ret)
	goto out;
    
    ret = krb5_data_copy(outdata, msg.kerb_message.data,
			 msg.kerb_message.length);
    free_KDC_PROXY_MESSAGE(&msg);
    if (ret)
	goto out;
    
    ret = 0;
 out:
    return ret;
}

typedef krb5_error_code
send_to_realm(krb5_context context,
	      void *ctx,
	      krb5_const_realm realm,
	      time_t timeout,
	      const krb5_data *outdata,
	      krb5_data *retdata)
{
    NSAutoreleasePool *pool;
    krb5_error_code ret = KRB5_PLUGIN_NO_HANDLE;
    char **urls;

    urls = krb5_config_get_string(context, NULL,
				  "kerberos-kdc-proxy",
				  realm, NULL);
    if (urls == NULL)
	return KRB5_PLUGIN_NO_HANDLE;

    @try {
	KDC_PROXY_MESSAGE msg;
	size_t length, size;
	NSData *msgdata;
	void *data;
	unsigned n;

	pool = [[NSAutoreleasePool alloc] init];

	memset(&msg, 0, sizeof(msg));
    
	msg.kerb_message = *outdata;
	msg.realm = &realm;
	msg.dclocator_hint = NULL;
	
	ASN1_MALLOC_ENCODE(KDC_PROXY_MESSAGE, data, length, &msg, &size, ret);
	if (ret)
	    return ret;
	if (length != size)
	    abort();
	
	msgdata = [NSData dataWithBytes:data length:length];
	free(data);

	for (n = 0; urls[n] n++) {
	    ret = requestToURL(context, urls[n], timeout, msgdata, retdata);
	    if (ret == 0)
		break;
	}
	    
    out:;

    @catch (NSException *exception) { }
    @finally {
	[pool drain];
    }

    return ret;
}


krb5plugin_send_to_kdc_ftable send_to_kdc = {
    KRB5_PLUGIN_SEND_TO_KDC_VERSION_2,
    init,
    fini,
    NULL,
    send_to_realm
};
