/*
 * urlPageGrab - download a page and all of the image sources referenced on 
 * that page.
 */
#include <stdlib.h>
#include <stdio.h>
#include <security_utilities/threading.h>
#include <Carbon/Carbon.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "cfSimpleGet.h"

#define MAX_PATH_LEN	256
#define MAX_URL_LEN		1024
#define MAX_THREADS		100

#define DEBUG_PRINT	0
#if		DEBUG_PRINT
#define dprintf(args...)	printf(args)
#else
#define dprintf(args...)
#endif

/* 
 * Until Radar 2731877 (CFURLCreateWithBytes chokes on '|') is fixed, we skip the 
 * fetching of anything with that character.
 */
#define SKIP_MULTI_QUERIES	1

/*
 * List of servers known to NOT support SSL; if images from these servers are
 * needed, get them via http:.
 */
static const char *nonSslSites[] = {
	"cover2.cduniverse.com",
	"a248.e.akamai.net", 
	NULL
};

/* return nonzero if specified host is in nonSslSites */
static int isHostNonSsl(
	const char *host)
{
	const char **nss = nonSslSites;
	while(*nss != NULL) {
		if(!strcmp(*nss, host)) {
			return 1;
		}
		nss++;
	}
	return 0;
}

/*
 * Used to force single-threaded access to URLSimpleDownload.
 */
static Mutex urlLock;

static void urlThreadLock()
{
	urlLock.lock();
}

static void urlThreadUnlock()
{
	urlLock.unlock();
}

/* 
 * Parameters for one thread, which fetches the contents of one URL (in this
 * case, an image source).
 */
typedef struct {
	/* in */
	const char 	*host;
	char 		path[MAX_PATH_LEN];
	bool		isSsl;
	bool		useCfNet;
	int			singleThread;
	int			quiet;
	pthread_t	pthr;
	unsigned	threadNum;
	
	/* out */
	OSStatus	ortn;
	unsigned	bytesRead;
} ThreadParams;

static void usage(char **argv)
{
	printf("%s hostname path [options]\n", argv[0]);
	printf("Options:\n");
	printf("  u  (URLAccess; default is CFNetwork)\n");
	printf("  s  connect via SSL\n");
	printf("  t  single thread access to URLSimpleDownload\n");
	printf("  q  quiet\n");
	exit(1);
}

static void printUrl(
	const char *host,
	const char *path,
	int			isSsl)
{
	if(isSsl) {
		printf("https://%s%s", host, path);
	}
	else {
		printf("http://%s%s", host, path);
	}
}

/*
 * Given a hostname, path, and SSL flag, fetch the data, optionally 
 * returning it in the form of a CFDataRef.
 */
static OSStatus fetchUrl(
	const char 	*host,
	const char 	*path,
	bool		isSsl,
	bool 		useCfNet,
	int			singleThread,
	unsigned	*bytesRead,			// RETURNED, always
	CFDataRef	*cfData)			// optional, RETURNED
{
	char url[MAX_URL_LEN];
	char *scheme;
	OSStatus ortn;
	
	*bytesRead = 0;
	if(isSsl) {
		scheme = "https://";
	}
	else {
		scheme = "http://";
	}
	sprintf(url, "%s%s%s", scheme, host, path);
	if(singleThread) {
		urlThreadLock();
	}
	if(useCfNet) {
		CFDataRef cd = cfSimpleGet(url);
		if(cd) {
			/* always report this */
			*bytesRead = CFDataGetLength(cd);
			if(cfData) {
				/* optional */
				*cfData = cd;
			}
			else {
				/* caller doesn't want */
				CFRelease(cd);
			}
			ortn = noErr;
		}
		else {
			printf("implied ioErr from cfnet\n");
			ortn = ioErr;
		}
	}
	else {
		/* original URLAccess mechanism */
		
		Handle h = NewHandle(0);
		ortn = URLSimpleDownload(url,
			NULL,
			h,
			0,			//kURLDisplayProgressFlag,	
			NULL,		//eventCallback,
			NULL);		// userContext
		*bytesRead = GetHandleSize(h);
		if((cfData != NULL) && (ortn == noErr)) {
			CFDataRef cd = CFDataCreate(NULL, (UInt8 *)*h, *bytesRead);
			*cfData = cd;
		}
		if(ortn) {
			printf("%d returned from URLSimpleDownload\n", (int)ortn);
		}
		DisposeHandle(h);
	}
	if(singleThread) {
		urlThreadUnlock();
	}
	dprintf("...read %d bytes from %s\n", (int)(*bytesRead), url);
	return ortn;
}

/*
 * Main pthread body, fetches source for one image.
 */
static void *imageThread(void *arg)
{
	ThreadParams *params = (ThreadParams *)arg;

	params->ortn = fetchUrl(params->host,
		params->path,
		params->isSsl,
		params->useCfNet,
		params->singleThread,
		&params->bytesRead,
		NULL);		// don't want the data 
	pthread_exit(NULL);
	/* NOT REACHED */
	return NULL;
}

/*
 * Given a Handle supposedly associated with a page of HTML, do an el-cheapo parse
 * of the HTML looking for IMG SRC tags. Fork off a thread for each image. Wait for
 * each thread to complete. Returns total number of errors of any kind found. 
 */
static int fetchImages(
	CFDataRef	cfData,
	const char	*host,
	const char	*origPath,
	int			isSsl,
	bool		useCfNet,
	int			singleThread,
	int			quiet)
{
	char 			*mungedHtml;
	Size 			mungedLen;
	char 			*cp;
	char			*imageNameStart;
	char			*imageNameEnd;
	unsigned		imageNameLen;
	ThreadParams	*params = NULL;			// big array 
	ThreadParams	*thisThread;
	unsigned		threadDex;
	int				prtn;
	unsigned 		numThreads = 0;			// valid entries in params[] 
	int 			totalErrors = 0;
	char			*basePath = NULL;
	
	/* 
	 * If the original path ends in '/', use it as basePath.
	 * Else strip off trailing component.
	 */
	unsigned origPathLen = strlen(origPath);
	basePath = strdup(origPath);
	if(origPath[origPathLen - 1] != '/') {
		/* trim */
		unsigned basePathLen = origPathLen;
		for(char *cp=basePath + origPathLen - 1; cp > basePath; cp--) {
			basePathLen--;
			if(*cp == '/') {
				/* found the last one - string ends here */
				cp[1] = '\0';
				break;
			}
		}
	}
	/* re-alloc the raw source as a NULL-terminated C string for easy str-based
	 * parsing */
	mungedLen = CFDataGetLength(cfData);
	if(mungedLen == 0) {
		printf("***size() of main page is zero!\n");
		return 0;
	}
	mungedLen++;
	mungedHtml = (char *)malloc(mungedLen);
	memmove(mungedHtml, CFDataGetBytePtr(cfData), mungedLen-1);
	mungedHtml[mungedLen - 1] = '\0';
	
	/* create a ThreadParams array big enough for most purposes */
	params = (ThreadParams *)malloc(sizeof(ThreadParams) * MAX_THREADS);

	/* start of el cheapo parse. Upper-case all "img src" into "IMG SRC". */
	for(;;) {
		cp = strstr(mungedHtml, "img src");
		if(cp == NULL) {
			break;
		}
		memmove(cp, "IMG SRC", 7);
		cp += 7;
	}
	
	/* convert all '\' to '/' - some URLs (e.g. from cduniverse.com) out there
	 * use the backslash, but CF's URL can't deal with it */
	for(;;) {
		cp = strchr(mungedHtml, '\\');
		if(cp == NULL) {
			break;
		}
		*cp = '/';
	}
	
	/* search for "IMG SRC", fork off thread to fetch each one's image */
	cp = mungedHtml;
	for(;;) {
		cp = strstr(cp, "IMG SRC=");
		if(cp == NULL) {
			break;
		}
		
		/* get ptr to start of image file name */
		cp += 8;
		if(*cp == '"') {
			/* e.g., <IMG SRC="foobar.gif"> */
			imageNameStart = ++cp;
			imageNameEnd = strchr(imageNameStart, '"');
		}
		else {
			/* e.g., <IMG SRC=foobar.gif>  */
			char *nextSpace;
			imageNameStart = cp;
			imageNameEnd = strchr(imageNameStart, '>');
			nextSpace = strchr(imageNameStart, ' ');
			if((imageNameEnd == NULL) || (imageNameEnd > nextSpace)) {
				imageNameEnd = nextSpace;
			}
		}
		if(imageNameEnd == NULL) {
			printf("***Bad HTML - missing quote/bracket after image file name\n");
			continue;
		}
		cp = imageNameEnd;
		
		/* fill in a ThreadParams */
		thisThread = &params[numThreads];
		thisThread->host = host;
		thisThread->isSsl = isSsl;
		thisThread->useCfNet = useCfNet;
		thisThread->singleThread = singleThread;
		thisThread->threadNum = numThreads;
		thisThread->quiet = quiet;
		thisThread->ortn = -1;
		
		/* path may be relative to basePath or a fully qualified URL */
		imageNameLen = imageNameEnd - imageNameStart;
		if(imageNameStart[0] == '/') {
			/* absolute path, use as is */
			memmove(thisThread->path, imageNameStart, imageNameLen);
			thisThread->path[imageNameLen] = '\0';
		}
		else if(strncmp(imageNameStart, "http", 4) == 0) {
			/* skip "http://" or "https://"; host name goes from after 
			 * tha until next '/' */
			const char *hostStart = strstr(imageNameStart, "//");
			if((hostStart == NULL) || (hostStart > (imageNameEnd-2))) {
				/* hmmm...punt */
				continue;
			}
			hostStart += 2;
			const char *hostEnd = strchr(hostStart, '/');
			if(hostEnd >= imageNameEnd) {
				/* punt */
				continue;
			}
			/* we're gonna leak this host string for now */
			unsigned hostLen = hostEnd - hostStart;
			char *hostStr = (char *)malloc(hostLen + 1);
			memmove(hostStr, hostStart, hostLen);
			hostStr[hostLen] = '\0';
			thisThread->host = (const char *)hostStr;
			/* remainder is path */
			/* FIXME - may have to deal with port number, currently in host string */
			memmove(thisThread->path, hostEnd, imageNameEnd-hostEnd);
			thisThread->path[imageNameEnd-hostEnd] = '\0';
			
			if(isSsl && isHostNonSsl(hostStr)) {
				/* some sites, e.g., cdu1.cduniverse.com, reference images
				 * which are NOT available via SSL */
				thisThread->isSsl = 0; 
			}
		}
		else {
			/* path := basePath | relativePath */
			unsigned basePathLen = strlen(basePath);
			memmove(thisThread->path, basePath, basePathLen);
			memmove(thisThread->path + basePathLen, imageNameStart, imageNameLen);
			thisThread->path[basePathLen + imageNameLen] = '\0';
		}
		
		#if		SKIP_MULTI_QUERIES
		if(strchr(thisThread->path, '|')) {
			/* CFURLCreateWithBytes will choke, so will URLSimpleDownload */
			continue;
		}
		#endif
		
		/* fork off a thread to fetch it */
		if(!quiet) {
			printf("   ");
			printUrl(thisThread->host, thisThread->path, thisThread->isSsl);
			printf(": thread %u : forking imageThread\n", 
				thisThread->threadNum);
		}
		prtn = pthread_create(&thisThread->pthr,
			NULL,
			imageThread,
			thisThread);
		if(prtn) {
			printf("***Error creating pthread (%d)\n", prtn);
			totalErrors++;
			break;
		}
		numThreads++;
		if(numThreads == MAX_THREADS) {
			/* OK, that's enough */
			break;
		}
	}
	free(mungedHtml);
	
	/* wait for each thread to complete */
	if(!quiet) {
		printf("   waiting for image threads to complete...\n");
	}
	for(threadDex=0; threadDex<numThreads; threadDex++) {
		void *status;
		thisThread = &params[threadDex];
		prtn = pthread_join(thisThread->pthr, &status);
		if(prtn) {
			printf("***pthread_join returned %d, aborting\n", prtn);
			totalErrors++;
			break;
		}
		if(!quiet || thisThread->ortn) {
			printf("   ");
			printUrl(thisThread->host, thisThread->path, thisThread->isSsl);
			printf(": thread %u : fetch result %d, read %d bytes\n", 
				thisThread->threadNum, 
				(int)thisThread->ortn, thisThread->bytesRead);
		}
		if(thisThread->ortn) {
			totalErrors++;
		}
	}
	free(params);
	return totalErrors;
}

int main(int argc, char **argv)
{
	bool		isSsl = false;
	bool 		useCfNet = true;
	int			singleThread = 0;
	int			quiet = 0;
	OSStatus	ortn;
	int			arg;
	CFDataRef	cfData;
	char		*host;
	char		*path;
	int			ourRtn = 0;
	
	if(argc < 3) {
		usage(argv);
	}
	host = argv[1];
	path = argv[2];
	for(arg=3; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 's':
				isSsl = true;
				break;
			case 'u':
				useCfNet = false;
				break;
			case 't':
				singleThread = 1;
				break;
			case 'q':
				quiet = 1;
				break;
			default:
				usage(argv);
		}
	}
	
	/* first get the main body of html text */
	printf("...fetching page at ");
	printUrl(host, path, isSsl);
	printf("\n");
	unsigned bytesRead;
	ortn = fetchUrl(host, path, isSsl, useCfNet, singleThread, &bytesRead, &cfData);
	if(ortn) {
		printf("***Error %d fetching from host %s  path %s\n", (int)ortn, host, path);
		exit(1);
	}
	
	/* parse the HTML, forking off a thread for each IMG SRC found */
	ourRtn = fetchImages(cfData, host, path, isSsl, useCfNet, singleThread, quiet);
	CFRelease(cfData);
	if(ourRtn) {
		printf("===%s exiting with %d %s for host %s\n", argv[0], ourRtn, 
			(ourRtn > 1) ? "errors" : "error", host);
	}
	return ourRtn;
}
