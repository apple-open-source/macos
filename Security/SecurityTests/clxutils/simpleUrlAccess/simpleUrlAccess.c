/*
 * URLSimpleDownload test, X version.
 */
#include <stdlib.h>
#include <stdio.h>
#include <Carbon/Carbon.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#define DEFAULT_HOST		"gss.cdnow.com"
#define DEFAULT_PATH		"/"
#define DEFAULT_SSL			0		/* 0 --> http, 1 --> https */

#define COMP_LENGTH		80
#define URL_LENGTH		256


/* shouldn't be needed if no kURLDisplayProgressFlag */
static void Initialize()					
{
	//InitGraf(&qd.thePort);
	//InitWindows();
	//InitCursor();
	//InitMenus();
	//InitFonts();
}



#if 0
static time_t lastTime = (time_t)0;
#define TIME_INTERVAL		3
/*
 * Just  keep UI alive in case we want to quit the app
 */
static pascal OSStatus eventCallback(
     void* userContext,
     EventRecord *event)
{
	time_t thisTime = time(0);
	
	if((thisTime - lastTime) >= TIME_INTERVAL) {
		printf("."); fflush(stdout);
		lastTime = thisTime;
	}
	return noErr;
}
#endif

/*
 * Assuming *h contains ASCII text, dump it 'til the user cries uncle.
 */
#define BYTES_PER_SPURT		128

static void 
dumpText(Handle h)
{
	Size	totalBytes;
	Size	bytesWritten = 0;
	Ptr		p;
	Size	thisWrite;
	Size	i;
	char	resp;
	char	c;
	char	lastWasHex = 0;
	
	HLock(h);
	totalBytes = GetHandleSize(h);
	if(totalBytes == 0) {
		printf("*** Zero bytes obtained\n");
		return;
	}
	p = *h;
	while(bytesWritten < totalBytes) {
		thisWrite = totalBytes - bytesWritten;
		if(thisWrite > BYTES_PER_SPURT) {
			thisWrite = BYTES_PER_SPURT;
		}
		for(i=0; i<thisWrite; i++) {
			c = *p++;
			if(isprint(c)) {
				printf("%c", c);
				lastWasHex = 0;
			}
			else {
				if(!lastWasHex) {
					printf("|");
				}
				printf("%02X|", (unsigned)c & 0xff);
				lastWasHex = 1;
			}
		}
		totalBytes += thisWrite;
		if(totalBytes == bytesWritten) {
			printf("\n");
			break;
		}
		printf("\nMore (y/anything)? ");
		fpurge(stdin);
		resp = getchar();
		if(resp != 'y') {
			break;
		}	
	}
	HUnlock(h);
	return;
}

int main()
{
	Handle		h;
	char		hostName[COMP_LENGTH];
	char		path[COMP_LENGTH];
	char		url[URL_LENGTH];
	char		scheme[10];			/* http, https */
	char		isSsl = DEFAULT_SSL;
	char		resp;
	OSStatus	ortn;
	
	Initialize();
	strcpy(hostName, DEFAULT_HOST);
	strcpy(path, DEFAULT_PATH);
	if(isSsl) {
		strcpy(scheme, "https");
	}
	else {
		strcpy(scheme, "http");
	}
	while(1) {
		printf("  h   Set Host      (current = %s)\n", hostName);
		printf("  p   Set path      (current = %s)\n", path);
		printf("  s   Set SSL true  (current = %d)\n", isSsl);
		printf("  S   Set SSL false\n");
		printf("  g   Get the URL\n");
		printf("  q   quit\n");
		printf("\nEnter command: ");
		fpurge(stdin);
		resp = getchar();
		switch(resp) {
			case 'h':	
				printf("Enter host name: ");
				scanf("%s", hostName);
				break;
			case 'p':
				printf("Enter path: ");
				scanf("%s", path);
				break;
			case 's':
				isSsl = 1;
				strcpy(scheme, "https");
				break;
			case 'S':
				isSsl = 0;
				strcpy(scheme, "http");
				break;
			case 'g':
				sprintf(url, "%s://%s%s", scheme, hostName, path);
				printf("...url = %s\n", url);
				h = NewHandle(0);				/* what the spec says */
				ortn = URLSimpleDownload(url,
					NULL,
					h,
					0,	//kURLDisplayProgressFlag,					// URLOpenFlags
					NULL,		//eventCallback,
					NULL);				// userContext
				if(ortn) {
					printf("URLSimpleDownload returned %d\n", (int)ortn);
				}
				else {
					dumpText(h);
				}
				DisposeHandle(h);
				break;
			case 'q':
				goto done;
			default:
				printf("Huh?\n");
				continue;
		}
		
	}
done:
	return 0;
}
