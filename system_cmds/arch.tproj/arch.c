/*
 * Copyright (c) 1999-2018 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/cdefs.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <paths.h>
#include <err.h>
#include <mach/mach.h>
#include <mach-o/arch.h>
#include <limits.h>
#include <sys/fcntl.h>
#include <glob.h>
#include <CoreFoundation/CoreFoundation.h>
#include <NSSystemDirectories.h>
#include <sysdir.h>

#if defined(__x86_64__)
#include <System/i386/cpu_capabilities.h>
#endif

#if TARGET_OS_OSX
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#ifndef ARCH_PROG
#define ARCH_PROG	"arch"
#endif
#ifndef MACHINE_PROG
#define MACHINE_PROG	"machine"
#endif

#define kKeyExecPath "ExecutablePath"
#define kKeyPlistVersion "PropertyListVersion"
#define kKeyPrefOrder "PreferredOrder"
#define kPlistExtension ".plist"
#define kSettingsDir "archSettings"

static const char envname[] = "ARCHPREFERENCE";

/* The CPU struct contains the argument buffer to posix_spawnattr_setbinpref_np */

typedef struct {
    cpu_type_t *types;
    cpu_subtype_t *subtypes;
    int errs;
    size_t count;
    size_t capacity;
} CPU;

typedef struct {
    const char *arch;
    cpu_type_t cpu;
    cpu_subtype_t cpusubtype;
} CPUTypes;

static const CPUTypes knownArchs[] = {
    {"i386", CPU_TYPE_I386, CPU_SUBTYPE_X86_ALL},
    {"x86_64", CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL},
    {"x86_64h", CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_H},
    {"arm", CPU_TYPE_ARM, CPU_SUBTYPE_ARM_ALL},
    {"arm64", CPU_TYPE_ARM64, CPU_SUBTYPE_ARM64_ALL},
    /* rdar://65725144 ("arch -arm64" command should launch arm64e slice for 2-way fat x86_64+arm64e binaries) */
    /* This will modify the order if someone, for whatever reason, specifies -arch arm64 -arch x86_64 -arch arm64e */
    {"arm64", CPU_TYPE_ARM64, CPU_SUBTYPE_ARM64E},
    {"arm64e", CPU_TYPE_ARM64, CPU_SUBTYPE_ARM64E},
    {"arm64_32", CPU_TYPE_ARM64_32, CPU_SUBTYPE_ARM64_32_ALL},
};

/* environment SPI */
char **_copyenv(char **env);
int _setenvp(const char *name, const char *value, int rewrite, char ***envp, void *state);
int _unsetenvp(const char *name, char ***envp, void *state);

/* copy of environment */
char **envCopy = NULL;
extern char **environ;

/*
 * The native 32 and 64-bit architectures (this is relative to the architecture
 * the arch command is running). NULL means unsupported.
 */

static const char*
native32(void)
{
#if defined(__i386__) || defined(__x86_64__)
    return "i386";
#elif defined(__arm64__) && !defined(__LP64__)
    return "arm64_32";
#elif defined(__arm__)
    return "arm";
#else
    return NULL;
#endif
}

static const char*
native64(void)
{
#if defined(__x86_64__)
    // FIXME: It is not clear if we should do this check, given the comment above which states "this is relative to the architecture the arch command is running".
    if (((*(uint64_t*)_COMM_PAGE_CPU_CAPABILITIES64) & kIsTranslated))
        return "arm64";
    return "x86_64";
#elif defined(__i386__)
    return "x86_64";
#elif defined(__arm64__) && defined(__LP64__)
    return "arm64";
#elif defined(__arm64__)
    return "arm64_32";
#else
    return NULL;
#endif
}

static bool
isSupportedCPU(cpu_type_t cpu)
{
#if defined(__x86_64__)
    if (((*(uint64_t*)_COMM_PAGE_CPU_CAPABILITIES64) & kIsTranslated))
        return cpu == CPU_TYPE_X86_64 || cpu == CPU_TYPE_ARM64;
    return cpu == CPU_TYPE_X86_64;
#elif defined(__i386__)
    return cpu == CPU_TYPE_I386 || cpu == CPU_TYPE_X86_64;
#elif defined(__arm64__) && defined(__LP64__) && TARGET_OS_OSX
    return cpu == CPU_TYPE_X86_64 || cpu == CPU_TYPE_ARM64;
#elif defined(__arm64__) && defined(__LP64__)
    return cpu == CPU_TYPE_ARM64;
#elif defined(__arm64__)
    return cpu == CPU_TYPE_ARM64_32;
#elif defined(__arm__)
    return cpu == CPU_TYPE_ARM;
#else
    #error "Unsupported architecture"
#endif
}

bool unrecognizednative32seen = false;
bool unrecognizednative64seen = false;

/*
 * arch - perform the original behavior of the arch and machine commands.
 * The archcmd flag is non-zero for the arch command, zero for the machine
 * command.  This routine never returns.
 */
static void __dead2
arch(int archcmd)
{
    const NXArchInfo *arch = NXGetLocalArchInfo();

    if(!arch)
        errx(-1, "Unknown architecture.");
    if(archcmd) {
        arch = NXGetArchInfoFromCpuType(arch->cputype, CPU_SUBTYPE_MULTIPLE);
        if(!arch)
            errx(-1, "Unknown architecture.");
    }
    printf("%s%s", arch->name, (isatty(STDIN_FILENO) ? "\n" : ""));
    exit(0);
}

/*
 * spawnIt - run the posix_spawn command.  cpu is the auto-sizing CPU structure.
 * pflag is non-zero to call posix_spawnp; zero means to call posix_spawn.
 * str is the name/path to pass to posix_spawn{,p}, and argv are
 * the argument arrays to pass.  This routine never returns.
 */
static void __dead2
spawnIt(CPU *cpu, int pflag, const char *str, char **argv)
{
    posix_spawnattr_t attr;
    pid_t pid;
    int ret;
    size_t copied;
    size_t count = cpu->count;
    cpu_type_t *prefs = cpu->types;
    cpu_subtype_t *subprefs = cpu->subtypes;

    if(count == 0) {
        if(unrecognizednative32seen)
            warnx("Unsupported native 32-bit architecture");
        if(unrecognizednative64seen)
            warnx("Unsupported native 64-bit architecture");
        exit(1);
    }

    if(unrecognizednative32seen)
        fprintf(stderr, "warning: unsupported native 32-bit architecture\n");
    if(unrecognizednative64seen)
        fprintf(stderr, "warning: unsupported native 64-bit architecture\n");

    if((ret = posix_spawnattr_init(&attr)) != 0)
        errc(1, ret, "posix_spawnattr_init");
    /* do the equivalent of exec, rather than creating a separate process */
    if((ret = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETEXEC)) != 0)
        errc(1, ret, "posix_spawnattr_setflags");
    if((ret = posix_spawnattr_setarchpref_np(&attr, count, prefs, subprefs, &copied)) != 0)
            errc(1, ret, "posix_spawnattr_setbinpref_np");

#if TARGET_OS_OSX
    for (size_t i = 0; i < copied; i++) {
        if (prefs[i] == CPU_TYPE_ARM64) {
            int affinity = 0;
            size_t asize = sizeof(affinity);
            sysctlbyname("kern.curproc_arch_affinity", NULL, NULL, &affinity, asize);
        }
    }
#endif /* TARGET_OS_OSX */

    if(copied != count)
        errx(1, "posix_spawnattr_setbinpref_np only copied %lu of %lu", copied, count);
    if(pflag)
        ret = posix_spawnp(&pid, str, NULL, &attr, argv, envCopy ? envCopy : environ);
    else
        ret = posix_spawn(&pid, str, NULL, &attr, argv, envCopy ? envCopy : environ);
    errc(1, ret, "posix_spawn%s: %s", (pflag ? "p" : ""), str);
}

/*
 * initCPU - initialize a CPU structure, a dynamically expanding CPU types
 * array.
 */
static void
initCPU(CPU *cpu)
{
    cpu->errs = 0;
    cpu->count = 0;
    cpu->capacity = 1;
    cpu->types = (cpu_type_t *)malloc(cpu->capacity * sizeof(cpu_type_t));
    if(!cpu->types)
        err(1, "Failed to malloc CPU type buffer");
    cpu->subtypes = (cpu_subtype_t *)malloc(cpu->capacity * sizeof(cpu_subtype_t));
    if(!cpu->subtypes)
        err(1, "Failed to malloc CPU subtype buffer");
}

/*
 * addCPU - add a new CPU type value to the CPU structure, expanding
 * the array as necessary.
 */
static void
addCPU(CPU *cpu, cpu_type_t n, cpu_subtype_t s)
{
    if(cpu->count == cpu->capacity) {
        cpu_type_t *newcpubuf;
        cpu_subtype_t *newcpusubbuf;
        
        cpu->capacity *= 2;
        newcpubuf = (cpu_type_t *)realloc(cpu->types, cpu->capacity * sizeof(cpu_type_t));
        if(!newcpubuf)
            err(1, "Out of memory realloc-ing CPU types structure");
        cpu->types = newcpubuf;
        newcpusubbuf = (cpu_subtype_t *)realloc(cpu->subtypes, cpu->capacity * sizeof(cpu_subtype_t));
        if(!newcpusubbuf)
            err(1, "Out of memory realloc-ing CPU subtypes structure");
        cpu->subtypes = newcpusubbuf;
        
    }
    cpu->types[cpu->count] = n;
    cpu->subtypes[cpu->count++] = s;
}

/*
 * addCPUbyname - add a new CPU type, given by name, to the CPU structure,
 * expanding the array as necessary.  The name is converted to a type value
 * by the ArchDict dictionary.
 */
static void
addCPUbyname(CPU *cpu, const char *name)
{
    int i;
    size_t numKnownArchs = sizeof(knownArchs)/sizeof(knownArchs[0]);
    
    for (i=0; i < numKnownArchs; i++) {
        if (!isSupportedCPU(knownArchs[i].cpu))
            continue;
        int start = i;
        /* rdar://65725144 ("arch -arm64" command should launch arm64e slice for 2-way fat x86_64+arm64e binaries) */
        /* Add subtypes that closely match the specified name */
        while ( i < numKnownArchs &&
                0 == strcasecmp(name, knownArchs[i].arch) ) {
            addCPU(cpu, knownArchs[i].cpu, knownArchs[i].cpusubtype);
            i++;
        }
        if (start != i)
            return;
    }
    
    /* Didn't match a string in knownArchs */
    warnx("Unknown architecture: %s", name);
    cpu->errs++;
}

/*
 * useEnv - parse the environment variable for CPU preferences.  Use name
 * to look for program-specific preferences, and append any CPU types to cpu.
 * Returns the number of CPU types.  Returns any specified execute path in
 * execpath. 
 *
 * The environment variable ARCHPREFERENCE has the format:
 *    spec[;spec]...
 * a semicolon separated list of specifiers.  Each specifier has the format:
 *    [prog:[execpath:]]type[,type]...
 * a comma separate list of CPU type names, optionally proceeded by a program
 * name and an execpath.  If program name exist, that types only apply to that
 * program.  If execpath is specified, it is returned.  If no program name
 * exists, then it applies to all programs.  So ordering of the specifiers is
 * important, as the default (no program name) specifier must be last.
 */
static size_t
useEnv(CPU *cpu, const char *name, char **execpath)
{
    char *val = getenv(envname);
    if(!val)
        return 0;
    
    /* cp will point to the basename of name */
    const char *cp = strrchr(name, '/');
    if(cp) {
        cp++;
        if(!*cp)
            errx(1, "%s: no name after last slash", name);
    } else
        cp = name;
    /* make a copy of the environment variable value, so we can modify it */
    val = strdup(val);
    if(!val)
        err(1, "Can't copy environment %s", envname);
    char *str = val;
    char *blk;
    /* for each specifier */
    while((blk = strsep(&str, ";")) != NULL) {
        if(*blk == 0)
            continue; /* two adjacent semicolons */
        /* now split on colons */
        char *n = strsep(&blk, ":");
        if(blk) {
            char *p = strsep(&blk, ":");
            if(!blk) { /* there is only one colon, so no execpath */
                blk = p;
                p = NULL;
            } else if(!*p) /* two consecutive colons, so no execpath */
                p = NULL;
            if(!*blk)
                continue; /* no cpu list, so skip */
            /* if the name matches, or there is no name, process the cpus */
            if(!*n || strcmp(n, cp) == 0) {
                if(cpu->count == 0) { /* only if we haven't processed architectures */
                    char *t;
                    while((t = strsep(&blk, ",")) != NULL)
                        addCPUbyname(cpu, t);
                }
                *execpath = (*n ? p : NULL); /* only use the exec path is name is set */
                break;
            }
        } else { /* no colons at all, so process as default */
            if(cpu->count == 0) { /* only if we haven't processed architectures */
                blk = n;
                while((n = strsep(&blk, ",")) != NULL)
                    addCPUbyname(cpu, n);
            }
            *execpath = NULL;
            break;
        }
    }
    if(cpu->errs) /* errors during addCPUbyname are fatal */
        exit(1);
    return cpu->count; /* return count of architectures */
}

/*
 * spawnFromPreference - called when argv[0] is not "arch" or "machine", or
 * argv[0] was arch, but no commandline architectures were specified.
 * If the environment variable ARCHPREFERENCE is specified, and there is a
 * match to argv[0], use the specified cpu preferences.  If no exec path
 * is specified in ARCHPREFERENCE, or no match is found in ARCHPREFERENCE,
 * get any additional information from a .plist file with the name of argv[0].
 * This routine never returns.
 */
static void __dead2
spawnFromPreferences(CPU *cpu, int needexecpath, char **argv)
{
    char *epath = NULL;
    char fpath[PATH_MAX];
    char execpath2[PATH_MAX];
    CFDictionaryRef plist = NULL;
    sysdir_search_path_enumeration_state state;
    size_t count, i;
    const char *prog = strrchr(*argv, '/');

    if(prog)
        prog++;
    else
        prog = *argv;
    if(!*prog)
        errx(1, "Not program name specified");

    /* check the environment variable first */
    if((count = useEnv(cpu, prog, &epath)) > 0) {
        /* if we were called as arch, use posix_spawnp */
        if(!needexecpath)
            spawnIt(cpu, 1, (epath ? epath : *argv), argv);
        /* otherwise, if we have the executable path, call posix_spawn */
        if(epath)
            spawnIt(cpu, 0, epath, argv);
    }

    state = sysdir_start_search_path_enumeration(SYSDIR_DIRECTORY_LIBRARY, SYSDIR_DOMAIN_MASK_ALL);
    while ((state = sysdir_get_next_search_path_enumeration(state, fpath))) {

        if (fpath[0] == '~') {
            glob_t pglob;
            int gret;

            bzero(&pglob, sizeof(pglob));

            gret = glob(fpath, GLOB_TILDE, NULL, &pglob);
            if (gret == 0) {
                int i;
                for (i=0; i < pglob.gl_pathc; i++) {
                    /* take the first glob expansion */
                    strlcpy(fpath, pglob.gl_pathv[i], sizeof(fpath));
                    break;
                }
            }
            globfree(&pglob);
        }

        // Handle path
        strlcat(fpath, "/" kSettingsDir "/", sizeof(fpath));
        strlcat(fpath, prog, sizeof(fpath));
        strlcat(fpath, kPlistExtension, sizeof(fpath));
        // printf("component: %s\n", fpath);

        int fd, ret;
        size_t length;
        ssize_t rsize;
        struct stat sb;
        void *buffer;
        fd = open(fpath, O_RDONLY, 0);
        if (fd >= 0) {
            ret = fstat(fd, &sb);
            if (ret == 0) {
                if (sb.st_size <= SIZE_T_MAX) {
                    length = (size_t)sb.st_size;
                    buffer = malloc(length); /* ownership transferred to CFData */
                    if (buffer) {
                        rsize = read(fd, buffer, length);
                        if (rsize == length) {
                            CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, buffer, length, kCFAllocatorMalloc);
                            if (data) {
                                buffer = NULL;
                                plist = CFPropertyListCreateWithData(kCFAllocatorDefault, data, kCFPropertyListImmutable, NULL, NULL);
                                CFRelease(data);
                            }
                        }
                        if (buffer) {
                            free(buffer);
                        }
                    }
                }
            }
            close(fd);
        }

        if (plist) {
            break;
        }
    }

    if (plist) {
        if (CFGetTypeID(plist) != CFDictionaryGetTypeID())
            errx(1, "%s: plist not a dictionary", fpath);
    } else {
        errx(1, "Can't find any plists for %s", prog);
    }

    int errs = 0; /* scan for all errors and fail later */
    do { /* begin block */
        /* check the plist version */
        CFStringRef vers = CFDictionaryGetValue(plist, CFSTR(kKeyPlistVersion));
        if(!vers) {
            warnx("%s: No key %s", fpath, kKeyPlistVersion);
            errs++;
        } else if(CFGetTypeID(vers) != CFStringGetTypeID()) {
            warnx("%s: %s is not a string", fpath, kKeyPlistVersion);
            errs++;
        } else if(!CFEqual(vers, CFSTR("1.0"))) {
            warnx("%s: %s not 1.0", fpath, kKeyPlistVersion);
            errs++;
        }
        /* get the execpath */
        CFStringRef execpath = CFDictionaryGetValue(plist, CFSTR(kKeyExecPath));
        if(!execpath) {
            warnx("%s: No key %s", fpath, kKeyExecPath);
            errs++;
        } else if(CFGetTypeID(execpath) != CFStringGetTypeID()) {
            warnx("%s: %s is not a string", fpath, kKeyExecPath);
            errs++;
        }
        if (!CFStringGetFileSystemRepresentation(execpath, execpath2, sizeof(execpath2))) {
            warnx("%s: could not get exec path", fpath);
            errs++;
        }
        /* if we already got cpu preferences from ARCHPREFERENCE, we are done */
        if(count > 0)
            break;
        /* otherwise, parse the cpu preferences from the plist */
        CFArrayRef p = CFDictionaryGetValue(plist, CFSTR(kKeyPrefOrder));
        if(!p) {
            warnx("%s: No key %s", fpath, kKeyPrefOrder);
            errs++;
        } else if(CFGetTypeID(p) != CFArrayGetTypeID()) {
            warnx("%s: %s is not an array", fpath, kKeyPrefOrder);
            errs++;
        } else if((count = CFArrayGetCount(p)) == 0) {
            warnx("%s: no entries in %s", fpath, kKeyPrefOrder);
            errs++;
        } else {
            /* finally build the cpu type array */
            for(i = 0; i < count; i++) {
                CFStringRef a = CFArrayGetValueAtIndex(p, i);
                if(CFGetTypeID(a) != CFStringGetTypeID()) {
                    warnx("%s: entry %lu of %s is not a string", fpath, i, kKeyPrefOrder);
                    errs++;
                } else {
                    char astr[128];
                    if (CFStringGetCString(a, astr, sizeof(astr), kCFStringEncodingASCII)) {
                        addCPUbyname(cpu, astr);
                    }
                }
            }
        }
    } while(0); /* end block */
    if(errs) /* exit if there were any reported errors */
        exit(1);

    CFRelease(plist);

    /* call posix_spawn */
    spawnIt(cpu, 0, execpath2, argv);
}

static void __dead2
usage(int ret)
{
    fprintf(stderr,
            "Usage: %s\n"
            "       Display the machine's architecture type\n"
            "Usage: %s {-arch_name | -arch arch_name} ... [-c] [-d envname] ... [-e envname=value] ... [-h] prog [arg ...]\n"
            "       Run prog with any arguments, using the given architecture\n"
            "       order.  If no architectures are specified, use the\n"
            "       ARCHPREFERENCE environment variable, or a property list file.\n"
            "       -c will clear out all environment variables before running prog.\n"
            "       -d will delete the given environment variable before running prog.\n"
            "       -e will add the given environment variable/value before running prog.\n"
            "       -h will print usage message and exit.\n",
            ARCH_PROG, ARCH_PROG);
    exit(ret);
}

/*
 * wrapped - check the path to see if it is a link to /usr/bin/arch.
 */
static int
wrapped(const char *name)
{
    size_t lp, ln;
    char *p;
    char *bp = NULL;
    char *cur, *path;
    char buf[MAXPATHLEN], rpbuf[MAXPATHLEN];
    struct stat sb;

    ln = strlen(name);

    do { /* begin block */
        /* If it's an absolute or relative path name, it's easy. */
        if(index(name, '/')) {
            if(stat(name, &sb) == 0 && S_ISREG(sb.st_mode) && access(name, X_OK) == 0) {
                bp = (char *)name;
                break;
            }
            errx(1, "%s isn't executable", name);
        }

        /* search the PATH, looking for name */
        if((path = getenv("PATH")) == NULL)
            path = _PATH_DEFPATH;

        cur = alloca(strlen(path) + 1);
        if(cur == NULL)
            err(1, "alloca");
        strcpy(cur, path);
        while((p = strsep(&cur, ":")) != NULL) {
            /*
             * It's a SHELL path -- double, leading and trailing colons
             * mean the current directory.
             */
            if(*p == '\0') {
                p = ".";
                lp = 1;
            } else
                lp = strlen(p);

            /*
             * If the path is too long complain.  This is a possible
             * security issue; given a way to make the path too long
             * the user may execute the wrong program.
             */
            if(lp + ln + 2 > sizeof(buf)) {
                warn("%s: path too long", p);
                continue;
            }
            bcopy(p, buf, lp);
            buf[lp] = '/';
            bcopy(name, buf + lp + 1, ln);
            buf[lp + ln + 1] = '\0';
            if(stat(buf, &sb) == 0 && S_ISREG(sb.st_mode) && access(buf, X_OK) == 0) {
                bp = buf;
                break;
            }
        }
        if(p == NULL)
            errx(1, "Can't find %s in PATH", name);
    } while(0); /* end block */
    if(realpath(bp, rpbuf) == NULL)
        errx(1, "realpath failed on %s", bp);
    return (strcmp(rpbuf, "/usr/bin/" ARCH_PROG) == 0);
}

/*
 * spawnFromArgs - called when arch has arguments specified.  The arch command
 * line arguments are:
 * % arch [[{-xxx | -arch xxx}]...] prog [arg]...
 * where xxx is a cpu name, and the command to execute and its arguments follow.
 * If no commandline cpu names are given, the environment variable
 * ARCHPREFERENCE is used.  This routine never returns.
 */

#define MATCHARG(a,m)	({ \
    const char *arg = *(a); \
    if(arg[1] == '-') arg++; \
    strcmp(arg, (m)) == 0; \
})

#define MATCHARGWITHVALUE(a,m,n,e)	({ \
    const char *ret = NULL; \
    const char *arg = *(a); \
    if(arg[1] == '-') arg++; \
    if(strcmp(arg, (m)) == 0) { \
        if(*++(a) == NULL) { \
            warnx(e); \
            usage(1); \
        } \
        ret = *(a); \
    } else if(strncmp(arg, (m), (n)) == 0 && arg[n] == '=') { \
         ret = arg + (n) + 1; \
    } \
    ret; \
})

#define MAKEENVCOPY(e)	\
    if(!envCopy) { \
        envCopy = _copyenv(environ); \
        if(envCopy == NULL) \
            errx(1, (e)); \
    }

static void __dead2
spawnFromArgs(CPU *cpu, char **argv)
{
    const char *ap, *ret;

    /* process arguments */
    for(argv++; *argv && **argv == '-'; argv++) {
        if((ret = MATCHARGWITHVALUE(argv, "-arch", 5, "-arch without architecture"))) {
            ap = ret;
        } else if(MATCHARG(argv, "-32")) {
            ap = native32();
            if(!ap) {
                unrecognizednative32seen = true;
                continue;
            }
        } else if(MATCHARG(argv, "-64")) {
            ap = native64();
            if(!ap) {
                unrecognizednative64seen = true;
                continue;
            }
        } else if(MATCHARG(argv, "-c")) {
            free(envCopy);
            envCopy = _copyenv(NULL); // create empty environment
            if(!envCopy)
                errx(1, "Out of memory processing -c");
            continue;
        } else if((ret = MATCHARGWITHVALUE(argv, "-d", 2, "-d without envname"))) {
            MAKEENVCOPY("Out of memory processing -d");
            _unsetenvp(ret, &envCopy, NULL);
            continue;
        } else if((ret = MATCHARGWITHVALUE(argv, "-e", 2, "-e without envname=value"))) {
            MAKEENVCOPY("Out of memory processing -e");
            const char *cp = strchr(ret, '=');
            if(!cp) {
                warnx("-e %s: no equal sign", ret);
                usage(1);
            }
            cp++; // skip to value
            /*
             * _setenvp() only uses the name before any equal sign found in
             * the first argument.
             */
            _setenvp(ret, cp, 1, &envCopy, NULL);
            continue;
        } else if(MATCHARG(argv, "-h")) {
            usage(0);
        } else {
            ap = *argv + 1;
            if(*ap == '-') ap++;
        }
        addCPUbyname(cpu, ap);
    }
    if(cpu->errs)
        exit(1);
    if(!*argv || !**argv) {
        warnx("No command to execute");
        usage(1);
    }
    /* if the program is already a link to arch, then force execpath */
    int needexecpath = wrapped(*argv);

    /*
     * If we don't have any architecutures, try ARCHPREFERENCE and plist
     * files.
     */
    if((cpu->count == 0) || needexecpath)
        spawnFromPreferences(cpu, needexecpath, argv); /* doesn't return */

    /*
     * Call posix_spawnp on the program name.
     */
    spawnIt(cpu, 1, *argv, argv);
}


/* the main() routine */
int
main(int argc, char **argv)
{
    const char *prog = getprogname();
    int my_name_is_arch;
    CPU cpu;

    if(strcmp(prog, MACHINE_PROG) == 0) {
        if(argc > 1)
            errx(-1, "no arguments accepted");
        arch(0); /* the "machine" command was called */
    } else if((my_name_is_arch = (strcmp(prog, ARCH_PROG) == 0))) {
        if(argc == 1)
            arch(1); /* the "arch" command with no arguments was called */
    }

    initCPU(&cpu);

    if(my_name_is_arch)
        spawnFromArgs(&cpu, argv);
    else
        spawnFromPreferences(&cpu, 1, argv);

    /* should never get here */
    errx(1, "returned from spawn");
}
