/*
 * Copyright (c) 2013 Apple Inc.
 * All rights reserved.
 */

#include <syslog.h>
#include <stdio.h>
#include <dns_sd.h>
#include <sys/socket.h>
#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <CFNetwork/CFSocketStreamPriv.h>

#define dns_timeout		10
#define probe_timeout   30
#define HTTP_RESPONSE_CODE_OK	200

static void probe_callback(CFReadStreamRef readStream, CFStreamEventType type, void * context);
static void timer_callback( CFRunLoopTimerRef timerref, void *context);

static void
probe_callback(CFReadStreamRef readStream, CFStreamEventType type, void * context)
{
    int readbytes = 0;
    int probe_OK = 0;
    CFErrorRef   error;
    CFStringRef   errorstr = NULL;
    CFRunLoopTimerRef   timerref = context;
	CFHTTPMessageRef resp = NULL;
	CFIndex responseCode = 0, bufferLength = 0;
	char* buffer;
	
    CFRunLoopTimerInvalidate(timerref);
    CFRelease(timerref);
	
	resp = (CFHTTPMessageRef)CFReadStreamCopyProperty(readStream, kCFStreamPropertyHTTPResponseHeader);
	
	responseCode = CFHTTPMessageGetResponseStatusCode(resp);
	syslog(LOG_DEBUG, "sbslauncher response code is %ld", responseCode);
	
	if (responseCode != HTTP_RESPONSE_CODE_OK)
		goto done;
	
	switch (type) {
        case kCFStreamEventOpenCompleted:
            probe_OK = 1;
            break;
        case kCFStreamEventHasBytesAvailable:
            probe_OK = 1;
            break;
        case kCFStreamEventErrorOccurred:
            error = CFReadStreamCopyError(readStream);
            errorstr = CFErrorCopyDescription(error);
			bufferLength = CFStringGetLength(errorstr) + 1;
			buffer = (char*)malloc(bufferLength*sizeof(char));
			CFStringGetCString(errorstr, buffer, (bufferLength), CFStringGetSystemEncoding());
            syslog(LOG_ERR, "sbslauncher probe_callback stream, errorstr: %s", buffer);
			
			free(buffer);
			CFRelease(errorstr);
            break;
        default:
            break;
    }
	
done:
    syslog(LOG_DEBUG, "sbslauncher: probe result: %d", probe_OK);
	if(resp)
		CFRelease(resp);
    CFRelease(readStream);
    exit(probe_OK);
}

static void
timer_callback( CFRunLoopTimerRef timerref, void *context)
{
    syslog(LOG_DEBUG, "timer_callback: timer expires");
    CFRunLoopTimerInvalidate(timerref);
    CFRelease(timerref);
    /* release read stream */
    if (context)
        CFRelease(context);
    /* timeout, no response from server */
    exit(0);
}

int launch_http_probe(int argc, const char * argv[]) {
    CFStringRef cfprobe_server = NULL;
    CFStringRef cfinterface = NULL;
	CFURLRef url = NULL;
    CFRunLoopTimerRef   timerref = NULL;
    CFRunLoopTimerContext   timer_context = { 0, NULL, NULL, NULL, NULL };
    NSURLRequest *request = NULL;
    NSError *error;
    NSURLResponse *response;
    NSData *result;
    int proberesult = 0;
    
    CFHTTPMessageRef httpRequest = NULL;
    CFReadStreamRef httpStream = NULL;
    CFStreamClientContext  stream_context = { 0, NULL, NULL, NULL, NULL };
	
    /* set up URL */
    
    if ((cfprobe_server = CFStringCreateWithCString(NULL, (char*)argv[2], kCFStringEncodingMacRoman)) == NULL)
        goto done;
    
    if ((cfinterface = CFStringCreateWithCString(NULL, (char*)argv[3], kCFStringEncodingMacRoman)) == NULL)
        goto done;
    
    if ((url = CFURLCreateWithString(NULL, cfprobe_server, NULL))==NULL){
        CFRelease(cfprobe_server);
        goto done;
    }
	
    httpRequest = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("GET"), url, kCFHTTPVersion1_1);
    if (httpRequest == NULL)
        goto done;
    
    httpStream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, httpRequest);
    if (httpStream == NULL)
        goto done;
	
    static const CFOptionFlags	eventMask = kCFStreamEventHasBytesAvailable |
    kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered;
	
    timer_context.info = (void*)httpStream;
    
    CFReadStreamSetProperty(httpStream, kCFStreamPropertyBoundInterfaceIdentifier, cfinterface);
	
    /* set timer */
    timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + probe_timeout, 0, 0, 0, timer_callback, &timer_context);
    if (timerref == NULL)
        goto done;
    
    stream_context.info = (void*)timerref;
	
	if (!CFReadStreamSetClient(httpStream, eventMask, probe_callback, &stream_context))
	{
        CFRelease(timerref);
		goto done;
	}
	
	// Schedule with the run loop
	CFReadStreamScheduleWithRunLoop(httpStream, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
    
    if ( !CFReadStreamOpen(httpStream)){
        syslog(LOG_ERR, "sbslauncher CFReadStreamOpen err, read stream status %ld", CFReadStreamGetStatus(httpStream));
    }
    
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timerref, kCFRunLoopCommonModes);
    CFRunLoopRun();
    
done:
    if (cfprobe_server)
        CFRelease(cfprobe_server);
    if (cfinterface)
        CFRelease(cfinterface);
    if (url)
        CFRelease(url);
    if (httpRequest)
        CFRelease(httpRequest);
	
    return(proberesult);
}

static DNSServiceRef dnsRedirectQuery = NULL;
static int dnsRedirectDetected = 0;
static int dnsRedirectWriteFD = -1;

static void
dns_redirect_callback(DNSServiceRef			sdRef,
					DNSServiceFlags			flags,
					uint32_t				interfaceIndex,
					DNSServiceErrorType		errorCode,
					const char				*hostname,
					const struct sockaddr	*address,
					uint32_t				ttl,
					void					*context)
{
	/* If we got back an answer, we detected a liar! */
	if ((errorCode == kDNSServiceErr_NoError) && (address != NULL)) {
		dnsRedirectDetected = 1;
		
		if (dnsRedirectWriteFD != -1) {
			if (write(dnsRedirectWriteFD, address, address->sa_len) < 0) {
				syslog(LOG_ERR, "sbslauncher write address home to socket %d fails, error: %s (tried to write %d bytes)", dnsRedirectWriteFD, strerror(errno), address->sa_len);
				dnsRedirectDetected = 0;
			}
			
		}
	}

	if (!(flags & kDNSServiceFlagsMoreComing)) {
		/* Clear query ref */
		if (dnsRedirectQuery != NULL) {
			DNSServiceRefDeallocate(dnsRedirectQuery);
			dnsRedirectQuery = NULL;
		}
		
		exit(dnsRedirectDetected);
	}
}

#define HOSTNAME_CHARSET                "abcdefghijklmnopqrstuvwxyz0123456789-"
#define HOSTNAME_CHARSET_LENGTH         (sizeof(HOSTNAME_CHARSET) - 1)
#define BASE_RANDOM_SEGMENT_LENGTH		8
#define VARIABLE_RANDOM_SEGMENT_LENGTH	20
#define RANDOM_SEGMENT_MAX_LENGTH		(BASE_RANDOM_SEGMENT_LENGTH + VARIABLE_RANDOM_SEGMENT_LENGTH + 1)

int detect_dns_redirect (int argc, const char * argv[])
{
	Boolean success = FALSE;
	char random_host[256];
	int character_index = 0;
	int firstSegmentLength = (arc4random() % VARIABLE_RANDOM_SEGMENT_LENGTH) + BASE_RANDOM_SEGMENT_LENGTH;
	int secondSegmentLength = (arc4random() % VARIABLE_RANDOM_SEGMENT_LENGTH) + BASE_RANDOM_SEGMENT_LENGTH;
	int i;
	
	/* Need a socket to write back on */
	if (argc >= 3  && argv[2]) {
		dnsRedirectWriteFD = atoi(argv[2]);
		/*int buffersize = sizeof(struct sockaddr_storage);
		if (setsockopt(dnsRedirectWriteFD, SOL_SOCKET, SO_SNDBUF, &buffersize, sizeof(buffersize)) < 0) {
			syslog(LOG_ERR, "Cannot set SO_SNDBUF for socket %d, error: %s\n", dnsRedirectWriteFD, strerror(errno));
		}*/
	}
	
	/* Create random host xxxxxxxxxxxx.xxxxxxxxxxx.com */
	for (i = 0; i < firstSegmentLength; i++) {
		random_host[character_index++] = HOSTNAME_CHARSET[(arc4random() % HOSTNAME_CHARSET_LENGTH)];
	}
	
	random_host[character_index++] = '.';
	
	for (i = 0; i < secondSegmentLength; i++) {
		random_host[character_index++] = HOSTNAME_CHARSET[(arc4random() % HOSTNAME_CHARSET_LENGTH)];
	}
	
	strncpy((char*)random_host + character_index, ".com", sizeof(random_host) - character_index);
	
	DNSServiceErrorType error = DNSServiceGetAddrInfo(&dnsRedirectQuery, kDNSServiceFlagsReturnIntermediates, 0, kDNSServiceProtocol_IPv4, random_host, dns_redirect_callback, NULL);
	if (error != kDNSServiceErr_NoError) {
		goto done;
	}

	error = DNSServiceSetDispatchQueue(dnsRedirectQuery, dispatch_get_main_queue());
	if (error != kDNSServiceErr_NoError) {
		goto done;
	}
	
	/* set timer */
	CFRunLoopTimerRef   timerref = NULL;
	CFRunLoopTimerContext   timer_context = { 0, NULL, NULL, NULL, NULL };
	timerref = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent() + dns_timeout, 0, 0, 0, timer_callback, &timer_context);
	if (timerref == NULL)
		goto done;
	CFRunLoopAddTimer(CFRunLoopGetCurrent(), timerref, kCFRunLoopCommonModes);
	
	CFRunLoopRun();
	success = TRUE;
	
done:
	return success;
}
