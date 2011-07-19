#include <dispatch/dispatch.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach-o/loader.h>
#include <sys/types.h>
#include <execinfo.h>
#include <stdio.h>
#include <dlfcn.h>
#include <asl.h>
#include <errno.h>
#include "assumes.h"

/* TODO: Re-enable after deciding how best to interact with this symbol. */
/*
 * NOTE: 8006611: converted to using libCrashReporterClient.a, so shouldn't
 * use __crashreporter_info__.
 */
#if 0
static char __crashreporter_info_buff__[2048];
const char *__crashreporter_info__ = &__crashreporter_info_buff__[0];
asm (".desc __crashreporter_info__, 0x10");
#endif

#define osx_atomic_cmpxchg(p, o, n) __sync_bool_compare_and_swap((p), (o), (n))

static const char *
_osx_basename(const char *p)
{
	return ((strrchr(p, '/') ? : p - 1) + 1);
}

static char *
_osx_get_build(void)
{
	static char s_build[16];
	static long s_once = 0;
	if (osx_atomic_cmpxchg(&s_once, 0, 1)) {
		int mib[] = { CTL_KERN, KERN_OSVERSION };
		size_t sz = sizeof(s_build);

		(void)sysctl(mib, 2, s_build, &sz, NULL, 0);
	}
	
	return s_build;
}

static void
_osx_get_image_uuid(void *hdr, uuid_t uuid)
{
#if __LP64__
	struct mach_header_64 *_hdr = (struct mach_header_64 *)hdr;
#else
	struct mach_header *_hdr = (struct mach_header *)hdr;
#endif /* __LP64__ */

	size_t i = 0;
	size_t next = sizeof(*_hdr);
	struct load_command *cur = NULL;
	for (i = 0; i < _hdr->ncmds; i++) {
		cur = (struct load_command *)((uintptr_t)_hdr + next);
		if (cur->cmd == LC_UUID) {
			struct uuid_command *cmd = (struct uuid_command *)cur;
			uuid_copy(uuid, cmd->uuid);
			break;
		}
		next += cur->cmdsize;
	}
	
	if (i == _hdr->ncmds) {
		uuid_clear(uuid);
	}
}

void
_osx_assumes_log(uint64_t code)
{
	Dl_info info;

	const char *image_name = NULL;
	uintptr_t offset = 0;
	uuid_string_t uuid_str;
	
	void *arr[2];
	/* Get our caller's address so we can look it up with dladdr(3) and
	 * get info about the image.
	 */
	if (backtrace(arr, 2) == 2) {
		/* dladdr(3) returns non-zero on success... for some reason. */
		if (dladdr(arr[1], &info)) {
			uuid_t uuid;
			_osx_get_image_uuid(info.dli_fbase, uuid);

			uuid_unparse(uuid, uuid_str);
			image_name = _osx_basename(info.dli_fname);
			
			offset = arr[1] - info.dli_fbase;
		}
	} else {
		uuid_t null_uuid;
		uuid_string_t uuid_str;
		
		uuid_clear(null_uuid);
		uuid_unparse(null_uuid, uuid_str);
		
		image_name = "unknown";
	}
	
	char name[256];
	(void)snprintf(name, sizeof(name), "com.apple.assumes.%s", image_name);
	
	char sig[64];
	(void)snprintf(sig, sizeof(sig), "%s:%lu", uuid_str, offset);

	char result[24];
	(void)snprintf(result, sizeof(result), "0x%llx", code);

	char *prefix = "Bug";
	char message[1024];
	(void)snprintf(message, sizeof(message), "%s: %s: %s + %lu [%s]: %s", prefix, _osx_get_build(), image_name, offset, uuid_str, result);

	aslmsg msg = asl_new(ASL_TYPE_MSG);
	if (msg != NULL) {
		/* MessageTracer messages aren't logged to the regular syslog store, so
		 * we pre-log the message without any MessageTracer attributes so that
		 * we can see it in our regular syslog.
		 */
		(void)asl_log(NULL, msg, ASL_LEVEL_ERR, "%s", message);
	
		(void)asl_set(msg, "com.apple.message.domain", name);
		(void)asl_set(msg, "com.apple.message.signature", sig);
		(void)asl_set(msg, "com.apple.message.value", result);

		(void)asl_log(NULL, msg, ASL_LEVEL_ERR, "%s", message);
		asl_free(msg);
	}
}

/* For osx_assert(). We need to think more about how best to set the __crashreporter_info__ string.
 * For example, calling into two functions will basically smash the register state at the time of
 * the assertion failure, causing potentially valuable information to be lost. Also, just setting
 * the __crashreporter_info__ to a static string only involves one instruction, whereas a function
 * call involves... well, more.
 */
#if 0
void
osx_hardware_trap(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	
	osx_hardware_trapv(fmt, ap);
	
	va_end(ap);
}

void
osx_hardware_trapv(const char *fmt, va_list ap)
{
	(void)vsnprintf(__crashreporter_info_buff__, sizeof(__crashreporter_info_buff__), fmt, ap);
	fflush(NULL);
	__builtin_trap();
}
#endif
