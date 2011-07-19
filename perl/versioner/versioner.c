/*
 * Copyright (c) 2008, 2009 Apple Inc. All rights reserved.
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
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libproc.h>
#include <limits.h>
#include <mach/mach.h>
#include <paths.h>
#include <pwd.h>
#include <regex.h>
#include <spawn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>
#include <NSSystemDirectories.h>

#include "shortcuts.h"	/* NSHORTCUTS, static const struct shortcut shortcuts[] */
#include "versions.h"	/* DEFAULTVERSION, NVERSIONS, PROJECT, UPROJECT, static const char *versions[] */

#define DATAVERSIONLEN		16
#define DEFAULTPREFER32BIT	0
#define ENV_DEBUG		ENVPREFIX "DEBUG"
#define ENV_PREFER32BIT		ENVPREFIX UPROJECT "_PREFER_32_BIT"
#define ENV_VERSION		ENVPREFIX UPROJECT "_VERSION"
#define ENVPREFIX		"VERSIONER_"
#define EXPECT_FALSE(x)		__builtin_expect((x), 0)
#define EXPECT_TRUE(x)		__builtin_expect((x), 1)
#define PLISTPATHLEN		(sizeof(plistpath) - 1)
#define PREFER3CPULEN		(sizeof(prefer32cpu) / sizeof(cpu_type_t))
#define USRBINLEN		(sizeof(usrbin) - 1)

struct data {
    int prefer32bit;
    char version[DATAVERSIONLEN];
};

extern char **environ;

static int debug = 0;
static const char plistpath[] = "/Preferences/com.apple.versioner." PROJECT ".plist";
static cpu_type_t prefer32cpu[] = {
    CPU_TYPE_I386,
    CPU_TYPE_POWERPC,
    CPU_TYPE_X86_64,
    CPU_TYPE_POWERPC64
};
static const char usrbin[] = "/usr/bin/";

static inline int	boolean_check(const char *str);
static char *		cfstrdup(CFStringRef str);
static int		cmpshortcuts(const void * restrict a, const void * restrict b);
static int		cmpstrarray(const void * restrict a, const void * restrict b);
static void		read_plist(const char * restrict path, struct data * restrict dp);
static inline int	searchshortcuts(const char * restrict name, const char * restrict version, char * restrict path);
static void		versionargs(int argc, char **argv, const char *version);
static inline int	version_check(const char *str);

static inline int
boolean_check(const char *str)
{
    if (strcasecmp(str, "yes") == 0 || strcasecmp(str, "true") == 0) return 1;
    if (strcasecmp(str, "no") == 0 || strcasecmp(str, "false") == 0) return 0;
    if (strspn(str, "0123456789") == strlen(str)) return (strtol(str, NULL, 10) != 0L);
    return -1;
}

static char *
cfstrdup(CFStringRef str) {
    char *result;
    CFIndex length = CFStringGetLength(str);
    CFIndex size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    result = malloc(size + 1);
    if (EXPECT_TRUE(result != NULL)) {
	length = CFStringGetBytes(str, CFRangeMake(0, length), kCFStringEncodingUTF8, '?', 0, (UInt8*)result, size, NULL);
	result[length] = 0;
    }
    return result;
}

static int
cmpshortcuts(const void * restrict a, const void * restrict b)
{
    return strcmp((const char *)a, ((struct shortcut *)b)->name);
}

static int
cmpstrarray(const void * restrict a, const void * restrict b)
{
    return strcmp((const char *)a, *(const char **)b);
}

static void
read_plist(const char * restrict path, struct data * restrict dp)
{
    do {
	int fd = open(path, O_RDONLY, (mode_t)0);
	if (fd < 0) {
	    if (EXPECT_FALSE(debug)) warn("read_plist: %s: open", path);
	    break;
	}
	do {
	    struct stat sb;
	    if (EXPECT_FALSE(fstat(fd, &sb) < 0)) {
		if (EXPECT_FALSE(debug)) warn("read_plist: %s: stat", path);
		break;
	    }
	    off_t size = sb.st_size;
	    void *buffer = mmap(NULL, size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, (off_t)0);
	    if (EXPECT_FALSE(buffer == MAP_FAILED)) {
		if (EXPECT_FALSE(debug)) warn("read_plist: %s: mmap", path);
		break;
	    }
	    do {
		CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, buffer, size, kCFAllocatorNull);
		if (EXPECT_FALSE(data == NULL)) {
		    if (EXPECT_FALSE(debug)) warnx("read_plist: %s: CFDataCreateWithBytesNoCopy failed", path);
		    break;
		}
		do {
		    CFStringRef str = NULL;
		    CFPropertyListRef plist = CFPropertyListCreateFromXMLData(NULL, data, kCFPropertyListMutableContainers, &str);
		    if (EXPECT_FALSE(plist == NULL)) {
			if (EXPECT_FALSE(debug)) {
			    char *s = cfstrdup(str);
			    char *sp = s;
			    if (EXPECT_FALSE(sp == NULL)) sp = "(cfstrdup also failed)";
			    warnx("read_plist: %s: CFPropertyListCreateFromXMLData failed: %s", path, sp);
			    if (EXPECT_TRUE(s != NULL)) free(s);
			}
			break;
		    }
		    do {
			if (EXPECT_FALSE(CFGetTypeID(plist) != CFDictionaryGetTypeID())) {
			    if (EXPECT_FALSE(debug)) warnx("read_plist: %s: plist not a dictionary", path);
			    break;
			}
			if (dp->prefer32bit < 0) {
			    CFBooleanRef prefer32bit = CFDictionaryGetValue(plist, CFSTR("Prefer-32-Bit"));
			    if (prefer32bit
				&& EXPECT_TRUE(CFGetTypeID(prefer32bit) == CFBooleanGetTypeID())) {
				dp->prefer32bit = CFBooleanGetValue(prefer32bit);
			    } else if (EXPECT_FALSE(debug)) warnx("read_plist: %s: Prefer-32-Bit not a boolean", path);
			}
			if (*dp->version == 0) {
			    char *vers;
			    CFStringRef version;

			    version = CFDictionaryGetValue(plist, CFSTR("Version"));
			    if (version
				&& EXPECT_TRUE(CFGetTypeID(version) == CFStringGetTypeID())
				&& EXPECT_TRUE((vers = cfstrdup(version)) != NULL)) {
				if (EXPECT_TRUE(version_check(vers))) {
				    strcpy(dp->version, vers);
				} else if (EXPECT_FALSE(debug)) warnx("read_plist: %s: %s: Version unknown", path, vers);
				free(vers);
			    } else if (EXPECT_FALSE(debug)) warnx("read_plist: %s: Version not a string", path);
			}
		    } while(0);
		    CFRelease(plist);
		} while(0);
		CFRelease(data);
	    } while(0);
	    munmap(buffer, size);
	} while(0);
	close(fd);
    } while(0);
}

static inline int
searchshortcuts(const char * restrict name, const char * restrict version, char * restrict path)
{
    struct shortcut *sp;

    if ((sp = (struct shortcut *)bsearch(name, shortcuts, NSHORTCUTS, sizeof(struct shortcut), cmpshortcuts)) != NULL) {
	/* we already know that this will fit in PATH_MAX */
	strcpy(path, sp->before);
	strcat(path, version);
	strcat(path, sp->after);
	return 1;
    }
    return 0;
}

static void
versionargs(int argc, char **argv, const char *version)
{
    char buf[PATH_MAX];
    char *punct;
    size_t len;
    size_t vlen = strlen(version);

    for(argc--, argv++; argc > 0; argc--, argv++) {
	if (strncmp(*argv, usrbin, USRBINLEN) != 0) continue;
	if ((len = strlen(*argv)) + vlen >= sizeof(buf)) continue;
	strcpy(buf, *argv);
	strcat(buf, version);
	if (access(buf, F_OK) == 0) {
found:
	    if (EXPECT_FALSE(debug)) warnx("%s -> %s", *argv, buf);
	    if (EXPECT_FALSE((*argv = strdup(buf)) == NULL)) errx(1, "versionarg: Out of memory");
	    continue;
	}
	/* try hyphen in front of the version number */
	if (EXPECT_TRUE(len + vlen + 1 < sizeof(buf))) {
	    buf[len] = '-';
	    strcpy(buf + len + 1, version);
	    if (access(buf, F_OK) == 0) goto found;
	}

	/* try the version string before any file suffix, or before a hyphen */
	buf[len] = 0; /* remove previously appended version */
	if ((punct = strchr(buf, '-')) != NULL || (punct = strrchr(buf, '.')) != NULL) {
	    memmove(punct + vlen, punct, strlen(punct) + 1);
	    memcpy(punct, version, vlen);
	    if (access(buf, F_OK) == 0) goto found;
	}
    }
}

static inline int
version_check(const char *str)
{
    return (bsearch(str, versions, NVERSIONS, sizeof(const char *), cmpstrarray) != NULL);
}

int
main(int argc, char **argv)
{
    char path[PATH_MAX], path0[PATH_MAX];
    NSSearchPathEnumerationState state;
    struct data data = {-1, ""};
    int i, ret, appendvers = 1;
    char *env, *name;
    size_t vlen;
    posix_spawnattr_t attr;
    pid_t pid;

    if (EXPECT_FALSE(getenv(ENV_DEBUG) != NULL)) debug = 1;
    if ((env = getenv(ENV_VERSION)) != NULL) {
	if (EXPECT_TRUE(version_check(env))) strcpy(data.version, env);
	else warnx("%s environment variable error (ignored)", ENV_VERSION);
    }
    if ((env = getenv(ENV_PREFER32BIT)) != NULL) {
	if (EXPECT_TRUE((i = boolean_check(env)) >= 0)) data.prefer32bit = i;
	else warnx("%s environment variable error (ignored)", ENV_PREFER32BIT);
    }

    if (data.prefer32bit < 0 || *data.version == 0) {
	state = NSStartSearchPathEnumeration(NSLibraryDirectory, NSAllDomainsMask & (~NSSystemDomainMask));
	while ((state = NSGetNextSearchPathEnumeration(state, path)) != 0) {
	    size_t len = strlen(path);
	    if (*path == '~') {
		struct passwd *pw = getpwuid(getuid());
		size_t dlen;

		if (EXPECT_FALSE(pw == NULL)) errx(1, "no user %d", (int)getuid());
		dlen = strlen(pw->pw_dir);
		if (EXPECT_FALSE(dlen + len - 1 >= sizeof(path))) errx(1, "%s: too long", pw->pw_dir);
		memmove(path + dlen, path + 1, len); // includes terminating nil
		memcpy(path, pw->pw_dir, dlen);
		len += dlen - 1;
	    }
	    if (EXPECT_FALSE(len + PLISTPATHLEN >= sizeof(path))) errx(1, "%s: Can't append \"%s\"\n", path, plistpath);
	    strcat(path, plistpath);
	    read_plist(path, &data);
	    if (data.prefer32bit >= 0 && *data.version) break;
	}
	if (data.prefer32bit < 0) data.prefer32bit = DEFAULTPREFER32BIT;
	if (*data.version == 0) strcpy(data.version, DEFAULTVERSION);
    }
#ifdef FORCE_TWO_NUMBER_VERSIONS
    {
	/* Only allow two number versions */
	char *dot;
	if (EXPECT_TRUE((dot = strchr(data.version, '.')) != NULL)) {
	    if (EXPECT_FALSE((dot = strchr(dot + 1, '.')) != NULL)) *dot = 0;
	}
    }
#endif /* FORCE_TWO_NUMBER_VERSIONS */
    if (EXPECT_FALSE(debug)) warnx("prefer32bit=%d version=%s", data.prefer32bit, data.version);

    vlen = strlen(data.version);
    if ((name = strrchr(*argv, '/')) != NULL) {
	strlcpy(path0, *argv, sizeof(path0));
    } else if (proc_pidpath(getpid(), path0, sizeof(path0)) != 0) {
	/*
	 * proc_pidpath returns one path for multiple hard-linked executables.
	 * To make sure we have the right executable name for the wrapper, we
	 * need to use the (tail of) *argv for the path.
	 */
	char *p;
	size_t plen;

	if ((p = strrchr(path0, '/')) != NULL) p++;
	else p = path0;
	plen = strlen(p);
	if (EXPECT_FALSE(strlen(path0) - plen + strlen(*argv) >= sizeof(path0))) errx(1, "Executable path too long");
	strcpy(p, *argv);
    } else {
	/* Last chance is to search through PATH to find the full path */
	char *cur, *p;
	size_t plen;
	size_t alen = strlen(*argv);

	if (EXPECT_FALSE(debug)) warn("proc_pidpath");
	if (EXPECT_FALSE((env = getenv("PATH")) == NULL)) env = _PATH_DEFPATH;
	cur = alloca(strlen(env) + 1);
	if (EXPECT_FALSE(cur == NULL)) errx(1, "alloca: out of memory");
	strcpy(cur, env);
	for (;;) {
	    if (EXPECT_FALSE((p = strsep(&cur, ":")) == NULL)) {
		memcpy(path0, *argv, alen + 1); /* assume in current dir */
		break;
	    }
	    if (*p == 0) {
		p = ".";
		plen = 1;
	    } else plen = strlen(p);
	    if (EXPECT_FALSE(plen + alen + 1 >= sizeof(path0))) errx(1, "Executable path too long");
	    memcpy(path0, p, plen);
	    path0[plen] = '/';
	    memcpy(path0 + plen + 1, *argv, alen + 1);
	    if (EXPECT_TRUE(access(path0, X_OK) == 0)) break;
	}
    }
    if (EXPECT_FALSE(realpath(path0, path) == NULL))
	errx(1, "realpath couldn't resolve \"%s\"", path0);
    if (strncmp(path, usrbin, USRBINLEN) == 0
	&& searchshortcuts(path + USRBINLEN, data.version, path)) appendvers = 0;
    if (appendvers) {
	if (EXPECT_FALSE(strlen(path) + vlen >= sizeof(path))) errx(1, "%s: Can't append \"%s\"", path, data.version);
	strcat(path, data.version);
    }
    if (EXPECT_FALSE(debug)) warnx("argv=%s path=%s", *argv, path);

    versionargs(argc, argv, data.version);

    if (EXPECT_FALSE((ret = posix_spawnattr_init(&attr)) != 0)) errc(1, ret, "posix_spawnattr_init");
    if (EXPECT_FALSE((ret = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETEXEC)) != 0)) errc(1, ret, "posix_spawnattr_setflags");
    if (data.prefer32bit) {
	size_t copied;
	if (EXPECT_FALSE((ret = posix_spawnattr_setbinpref_np(&attr, PREFER3CPULEN, prefer32cpu, &copied)) != 0)) errc(1, ret, "posix_spawnattr_setbinpref_np");
	if (EXPECT_FALSE(copied != PREFER3CPULEN)) errx(1, "posix_spawnattr_setbinpref_np only copied %d of %d", (int)copied, PREFER3CPULEN);
    }
    ret = posix_spawn(&pid, path, NULL, &attr, argv, environ);
    errc(1, ret, "posix_spawn: %s", path);

    return 1;
}
