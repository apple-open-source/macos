#import <Foundation/Foundation.h>
#import <IOKit/kext/OSKext.h>
#import <IOKit/kext/OSKextPrivate.h>
#import <Bom/Bom.h>
#import <APFS/APFS.h>

#import <libproc.h>
#import <stdio.h>
#import <stdbool.h>
#import <sysexits.h>
#import <unistd.h>

#import "bootcaches.h"
#import "kext_tools_util.h"
#import "kc_staging.h"

int main(int argc, char **argv)
{
	int result = EX_SOFTWARE;
	char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
	struct statfs sfs = {};

	if (argc != 1) {
		LOG_ERROR("kcditto installs previously built kext collections onto the Preboot volume.");
		LOG_ERROR("It takes no arguments.");
		result = EX_USAGE;
		goto finish;
	}

	int ret = proc_pidpath(getpid(), pathbuf, sizeof(pathbuf));
	if (ret <= 0) {
		LOG_ERROR("Can't get executable path for (%d)%s: %s",
		          getpid(), argv[0], strerror(errno));
		goto finish;
	}
	if (statfs(pathbuf, &sfs) < 0) {
		goto finish;
	}

	LOG("Copying deferred prelinked kernels in %s...", sfs.f_mntonname);
	result = copyDeferredPrelinkedKernels(sfs.f_mntonname);
	if (result != EX_OK) {
		LOG_ERROR("Error copying deferred prelinked kernels (standalone)...");
	}

	LOG("Copying KCs in %s...", sfs.f_mntonname);
	result = copyKCsInVolume(sfs.f_mntonname);
	if (result != EX_OK) {
		LOG_ERROR("Error copying KCs (standalone)...");
		goto finish;
	}

finish:
	return result;
}
