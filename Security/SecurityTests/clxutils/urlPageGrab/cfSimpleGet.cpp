/* 
 * cgGet.cpp - simplified interface to use CFNetwork to do one
 * HTTP or HTTPS GET transaction.
 */

#include "cfSimpleGet.h"
#include <CoreServices/CoreServices.h>

#define BUFSIZE	1024

#define DEBUG_PRINT	0
#if		DEBUG_PRINT
#define dprintf(args...)	printf(args)
#else
#define dprintf(args...)
#endif

CFDataRef cfSimpleGet(
	const char 	*url)
{
	CFURLRef cfUrl = NULL;
	CFMutableDataRef rtnData = NULL;
	CFReadStreamRef rdStrm = NULL;
	CFHTTPMessageRef myReq = NULL;
	Boolean brtn;
	UInt8 rbuf[BUFSIZE];
	CFIndex irtn;
	bool isOpen = false;
	CFStreamStatus rsStat;
	
	cfUrl = CFURLCreateWithBytes(kCFAllocatorDefault, 
		(UInt8 *)url, strlen(url), kCFStringEncodingASCII, NULL);
	if(cfUrl == NULL) {
		printf("***Error creating URL for %s.\n", url);
		return NULL;
	}
	myReq = CFHTTPMessageCreateRequest(kCFAllocatorDefault, 
		CFSTR("GET"), cfUrl, kCFHTTPVersion1_0);
	if(myReq == NULL) {
		printf("***Error creating HTTP msg for %s.\n", url);
		goto errOut;
	}
	/* no msg body, no headers */
	
	rdStrm = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, myReq);
	if(rdStrm == NULL) {
		printf("***Error creating CFReadStream for %s.\n", url);
		goto errOut;
	}
	dprintf("...opening rd stream for %s\n", url);
	brtn = CFReadStreamOpen(rdStrm);
	if(!brtn) {
		printf("***Error (1) opening CFReadStream for %s.\n", url);
		goto errOut;
	}
	
	/* wait for open */
	do {
		rsStat = CFReadStreamGetStatus(rdStrm);
		switch(rsStat) {
			case kCFStreamStatusError:
				printf("***Error (2) opening CFReadStream for %s.\n", url);
				goto errOut;
			case kCFStreamStatusAtEnd:
				printf("***Unexpected EOF while opening %s\n", url);
				goto errOut;
			case kCFStreamStatusClosed:
				printf("***Unexpected close while opening %s\n", url);
				goto errOut;
			case kCFStreamStatusNotOpen:
			case kCFStreamStatusOpening:
				/* wait some more */
				break;
			case kCFStreamStatusOpen:
			case kCFStreamStatusReading:
			case kCFStreamStatusWriting:
				isOpen = true;
				break;
			default:
				printf("***Unexpected status while opening %s\n", url);
				goto errOut;
		}
	} while(!isOpen);
	dprintf("...rd stream for %s open for business\n", url);
	
	/* go */
	rtnData = CFDataCreateMutable(kCFAllocatorDefault, 0);
	do {
		irtn = CFReadStreamRead(rdStrm, rbuf, BUFSIZE);
		if(irtn == 0) {
			/* end of stream, normal exit */
			isOpen = false;
			dprintf("...EOF on rd stream for %s\n", url);
		}
		else if(irtn < 0) {
			/* FIXME - how to tell caller? */
			printf("***Error reading %s\n", url);
			isOpen = false;
		}
		else {
			dprintf("   ...read %d bytes from %s\n", (int)irtn, url);
			CFDataAppendBytes(rtnData, rbuf, irtn);
		}
	} while(isOpen);
errOut:
	if(myReq) {
		CFRelease(myReq);
	}
	if(rdStrm) {
		CFRelease(rdStrm);
	}
	if(cfUrl) {
		CFRelease(cfUrl);
	}
	if(rtnData) {
		/* error on zero bytes read */
		if(CFDataGetLength(rtnData) == 0) {
			printf("***No data read from %s\n", url);
			CFRelease(rtnData);
			rtnData = NULL;
		}
	}
	return rtnData;
}


