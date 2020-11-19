//
//  kc_staging.h
//  kext_tools
//
//  Created by Jack Kim-Biggs on 7/16/19.
//

#ifndef kc_staging_h
#define kc_staging_h

#define _kOSKextReadOnlyDataVolumePath "/System/Volumes/Data"

#ifdef KCDITTO_STANDALONE_BINARY
#define ERROR_LOG_FUNCTION(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define LOG_FUNCTION(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#else /* KCDITTO_STANDALONE_BINARY */

#define ERROR_LOG_FUNCTION(fmt, ...) OSKextLog(NULL, \
		kOSKextLogFileAccessFlag | kOSKextLogErrorLevel, \
		fmt, ##__VA_ARGS__)
#define LOG_FUNCTION(fmt, ...) OSKextLog(NULL, \
		kOSKextLogFileAccessFlag | kOSKextLogBasicLevel, \
		fmt, ##__VA_ARGS__)
#endif /* !KCDITTO_STANDALONE_BINARY */

#define PASTE(x) #x
#define STRINGIFY(x) PASTE(x)
#define LOG_ERROR(...) do { \
	ERROR_LOG_FUNCTION(__FILE__ "." STRINGIFY(__LINE__) ": " __VA_ARGS__); \
} while (0)
#define LOG(...) LOG_FUNCTION(__VA_ARGS__)

int copyKCsInVolume(char *volRoot);
int copyDeferredPrelinkedKernels(char *volRoot);

#endif /* kc_staging_h */
