#ifndef __NBPUTILITIES__
#define __NBPUTILITIES__

#include <CoreServices/CoreServices.h>

#include "NSLDebugLog.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/mount.h>
#include <nfs/nfsproto.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <string.h>
#include <machine/spl.h>
#include <sys/uio.h>
//#include <netinet/in.h>
#include <unistd.h>
//#include <sys/wait.h>
//#include <mach/cthreads.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/attr.h>
#include <netat/appletalk.h>
#include <netat/atp.h>
#include <netat/zip.h>

// NBP errors
enum {
	kNBPInternalError = 1,
	kNBPAppleTalkOff = 2
};

typedef struct NBPNameAndAddress {
    char name[34];
    struct at_inet atalkAddress;
    long ipAddress;
} NBPNameAndAddress;

int myFastRelString ( const unsigned char* str1, int length, const unsigned char* str2, int length2 );
int my_strcmp (const void *str1, const void *str2);
int my_strcmp2 (const void *entry1, const void *entry2);
int GetATStackState();		// ours

extern "C" {
int checkATStack();			// from framework
};

/* Appletalk Stack status Function. */
enum {
          RUNNING
        , NOTLOADED
        , LOADED
        , OTHERERROR
};



#endif

