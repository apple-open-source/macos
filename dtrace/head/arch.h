#ifdef __APPLE__

#include <mach/machine.h>

#define I386_STRING		"i386"
#define X86_64_STRING		"x86_64"
#define POWERPC_STRING		"ppc"
#define POWERPC64_STRING	"ppc64"

static inline char*
string_for_arch(cpu_type_t arch)
{
	switch(arch) {
	case CPU_TYPE_I386:
		return I386_STRING;
	case CPU_TYPE_X86_64:
		return X86_64_STRING;
	case CPU_TYPE_POWERPC:
		return POWERPC_STRING;
	case CPU_TYPE_POWERPC64:
		return POWERPC64_STRING;
	default:
		return NULL;
	}
}

static inline cpu_type_t
arch_for_string(const char* string)
{
	if(!strcmp(string, I386_STRING))
		return CPU_TYPE_I386;
	else if(!strcmp(string, X86_64_STRING))
		return CPU_TYPE_X86_64;
	else if(!strcmp(string, POWERPC_STRING))
		return CPU_TYPE_POWERPC;
	else if(!strcmp(string, POWERPC64_STRING))
		return CPU_TYPE_POWERPC64;
	else
		return (cpu_type_t)0;
}

static inline int needs_swapping(cpu_type_t a, cpu_type_t b)
{
	switch(a) {
	case CPU_TYPE_I386:
	case CPU_TYPE_X86_64:
		if(b == CPU_TYPE_POWERPC || b == CPU_TYPE_POWERPC64)
			return 1;
		else
			return 0;
	case CPU_TYPE_POWERPC:
	case CPU_TYPE_POWERPC64:
		if(b == CPU_TYPE_I386 || b == CPU_TYPE_X86_64)
			return 1;
		else
			return 0;
	}
	
	return 0;
}

#if 0
#if defined(__i386__)
static cpu_type_t host_arch = CPU_TYPE_I386;
#elif defined(__x86_64__)
static cpu_type_t host_arch = CPU_TYPE_X86_64;
#elif defined(__ppc__)
static cpu_type_t host_arch = CPU_TYPE_POWERPC;
#elif defined(__ppc64__)
static cpu_type_t host_arch = CPU_TYPE_POWERPC64;
#endif
#endif

#if defined(__i386__)
#define host_arch CPU_TYPE_I386
#elif defined(__x86_64__)
#define host_arch CPU_TYPE_X86_64
#elif defined(__ppc__)
#define host_arch CPU_TYPE_POWERPC
#elif defined(__ppc64__)
#define host_arch CPU_TYPE_POWERPC64
#else
#error Unsupported architecture
#endif

#endif
