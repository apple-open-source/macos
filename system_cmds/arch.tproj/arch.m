/*
 * Copyright (c) 1999, 2006 Apple Computer, Inc. All rights reserved.
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
#include <Foundation/Foundation.h>

#ifndef ARCH_PROG
#define ARCH_PROG	"arch"
#endif
#define CPUDELTA	10
#ifndef MACHINE_PROG
#define MACHINE_PROG	"machine"
#endif

#define CPUCOUNT(c)	((c)->ptr - (c)->buf)

static NSMutableDictionary *ArchDict;
static NSString *KeyExecPath = @"ExecutablePath";
static NSString *KeyPlistVersion = @"PropertyListVersion";
static NSString *KeyPrefOrder = @"PreferredOrder";
static NSString *PlistExtension = @"plist";
static NSString *SettingsDir = @"archSettings";

static const char envname[] = "ARCHPREFERENCE";

typedef struct {
    cpu_type_t *buf;
    cpu_type_t *ptr;
    cpu_type_t *end;
    int errs;
} CPU;

typedef struct {
    char *str;
    int i;
} StrInt;

static StrInt initArches[] = {
    {"i386", CPU_TYPE_I386},
    {"ppc", CPU_TYPE_POWERPC},
    {"ppc64", CPU_TYPE_POWERPC64},
    {"x86_64", CPU_TYPE_X86_64},
    {NULL, 0}
};

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
 * spawnIt - run the posix_spawn command.  count is the number of CPU types
 * in the prefs array.  pflag is non-zero to call posix_spawnp; zero means to
 * call posix_spawn.  str is the name/path to pass to posix_spawn{,p}, and
 * argv and environ are the argument and environment arrays to pass.  This
 * routine never returns.
 */
static void __dead2
spawnIt(int count, cpu_type_t *prefs, int pflag, const char *str, char **argv, char **environ)
{

    posix_spawnattr_t attr;
    pid_t pid;
    int ret;
    size_t copied;

    if((ret = posix_spawnattr_init(&attr)) != 0)
	errc(1, ret, "posix_spawnattr_init");
    /* do the equivalent of exec, rather than creating a separate process */
    if((ret = posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETEXEC)) != 0)
	errc(1, ret, "posix_spawnattr_setflags");
    if((ret = posix_spawnattr_setbinpref_np(&attr, count, prefs, &copied)) != 0)
	errc(1, ret, "posix_spawnattr_setbinpref_np");
    if(copied != count)
	errx(1, "posix_spawnattr_setbinpref_np only copied %d of %d", (int)copied, count);
    if(pflag)
	ret = posix_spawnp(&pid, str, NULL, &attr, argv, environ);
    else
	ret = posix_spawn(&pid, str, NULL, &attr, argv, environ);
    errc(1, ret, "posix_spawn%s: %s", (pflag ? "p" : ""), str);
}

/*
 * initCPU - initialize a CPU structure, a dynamically expanding CPU types
 * array.
 */
static void
initCPU(CPU *cpu)
{
    cpu->buf = (cpu_type_t *)malloc(CPUDELTA * sizeof(cpu_type_t));
    if(!cpu->buf)
	err(1, "Failed to malloc CPU buffer");
    cpu->ptr = cpu->buf;
    cpu->end = cpu->buf + CPUDELTA;
    cpu->errs = 0;
}

/*
 * addCPU - add a new CPU type value to the CPU structure, expanding
 * the array as necessary.
 */
static void
addCPU(CPU *cpu, int n)
{
    if(cpu->ptr >= cpu->end) {
	cpu_type_t *new = realloc(cpu->buf, (cpu->end - cpu->buf + CPUDELTA) * sizeof(cpu_type_t));
	if(!new)
	    err(1, "Out of memory realloc-ing CPU structure");
	ptrdiff_t diff = (void *)new - (void *)cpu->buf;
	cpu->buf = new;
	cpu->ptr = (cpu_type_t *)((void *)cpu->ptr + diff);
	cpu->end = (cpu_type_t *)((void *)cpu->end + diff) + CPUDELTA;
    }
    *cpu->ptr++ = n;
}

/*
 * addCPUbyname - add a new CPU type, given by name, to the CPU structure,
 * expanding the array as necessary.  The name is converted to a type value
 * by the ArchDict dictionary.
 */
static void
addCPUbyname(CPU *cpu, const char *name)
{
    NSNumber *n = (NSNumber *)[ArchDict objectForKey: [[NSString stringWithUTF8String: name] lowercaseString]];
    if(n == nil) {
	warnx("Unknown architecture: %s", name);
	cpu->errs++;
	return;
    }
    addCPU(cpu, [n intValue]);
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
static int
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
		if(CPUCOUNT(cpu) <= 0) { /* only if we don't already have cpu types */
		    char *t;
		    while((t = strsep(&blk, ",")) != NULL)
			addCPUbyname(cpu, t);
		}
		*execpath = (*n ? p : NULL); /* only use the exec path is name is set */
		break;
	    }
	} else { /* no colons at all, so process as default */
	    if(CPUCOUNT(cpu) <= 0) { /* only if we don't already have cpu types */
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
    return CPUCOUNT(cpu); /* return count of cpus */
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
spawnFromPreferences(CPU *cpu, int needexecpath, char **argv, char **environ)
{
    char *epath = NULL;
    int count;
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
	    spawnIt(count, cpu->buf, 1, (epath ? epath : *argv), argv, environ);
	/* otherwise, if we have the executable path, call posix_spawn */
	if(epath)
	    spawnIt(count, cpu->buf, 0, epath, argv, environ);
    }

    /* pathArray is use to build the .plist file path for each domain */
    NSMutableArray *pathArray = [NSMutableArray arrayWithCapacity: 3];
    if(!pathArray)
	errx(1, "Can't create NSMutableArray");
    [pathArray addObject: @""]; // placeholder for domain directory
    [pathArray addObject: SettingsDir];
    [pathArray addObject: [[NSString stringWithUTF8String: prog] stringByAppendingPathExtension: PlistExtension]];

    /* get the list of top level directories for all domains */
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSAllDomainsMask, true);
    int cnt = [paths count];
    if(!cnt)
	errx(1, "No domains");

    /* create the .plist path, and try to read it */
    int i;
    NSString *path = NULL;
    NSData *data = NULL;
    for(i = 0; i < cnt; i++) {
	[pathArray replaceObjectAtIndex: 0 withObject: [paths objectAtIndex: i]];
	path = [NSString pathWithComponents: pathArray];
	data = [NSData dataWithContentsOfFile: path];
	if(data) /* found it */
	    break;
    }
    if(!data)
	errx(1, "Can't find any plists for %s", prog);

    /* try to convert the file into a NSDictionary */
    NSString *error;
    id plist = [NSPropertyListSerialization propertyListFromData: data mutabilityOption: NSPropertyListImmutable format: nil errorDescription: &error];
    if(!plist)
	errx(1, "%s: NSPropertyListSerialization error: %s", [path UTF8String], [error UTF8String]);
    if(![plist isKindOfClass: [NSDictionary class]])
	errx(1, "%s: plist not a dictionary", [path UTF8String]);

    NSString *execpath;
    int errs = 0; /* scan for all errors and fail later */
    do { /* begin block */
	/* check the plist version */
	NSString *vers = [(NSDictionary *)plist objectForKey: KeyPlistVersion];
	if(!vers) {
	    warnx("%s: No key %s", [path UTF8String], [KeyPlistVersion UTF8String]);
	    errs++;
	} else if(![vers isKindOfClass: [NSString class]]) {
	    warnx("%s: %s is not a string", [path UTF8String], [KeyPlistVersion UTF8String]);
	    errs++;
	} else if(![vers isEqualToString: @"1.0"]) {
	    warnx("%s: %s not 1.0", [path UTF8String], [KeyPlistVersion UTF8String]);
	    errs++;
	}
	/* get the execpath */
	execpath = [(NSDictionary *)plist objectForKey: KeyExecPath];
	if(!execpath) {
	    warnx("%s: No key %s", [path UTF8String], [KeyExecPath UTF8String]);
	    errs++;
	} else if(![execpath isKindOfClass: [NSString class]]) {
	    warnx("%s: %s is not a string", [path UTF8String], [KeyExecPath UTF8String]);
	    errs++;
	}
	/* if we already got cpu preferences from ARCHPREFERENCE, we are done */
	if(count > 0)
	    break;
	/* otherwise, parse the cpu preferences from the plist */
	id p = [(NSDictionary *)plist objectForKey: KeyPrefOrder];
	if(!p) {
	    warnx("%s: No key %s", [path UTF8String], [KeyPrefOrder UTF8String]);
	    errs++;
	} else if(![p isKindOfClass: [NSArray class]]) {
	    warnx("%s: %s is not an array", [path UTF8String], [KeyPrefOrder UTF8String]);
	    errs++;
	} else if((count = [p count]) == 0) {
	    warnx("%s: no entries in %s", [path UTF8String], [KeyPrefOrder UTF8String]);
	    errs++;
	} else {
	    /* finally but the cpu type array */
	    for(i = 0; i < count; i++) {
		id a = [(NSArray *)p objectAtIndex: i];
		NSNumber *n;
		if(![a isKindOfClass: [NSString class]]) {
		    warnx("%s: entries %d of %s is not a string", [path UTF8String], i, [KeyPrefOrder UTF8String]);
		    errs++;
		} else if((n = (NSNumber *)[ArchDict objectForKey: [(NSString *)a lowercaseString]]) == nil) {
		    warnx("%s: %s: unknown architecture %s", [path UTF8String], [KeyPrefOrder UTF8String], [(NSString *)a UTF8String]);
		    errs++;
		} else {
		    addCPU(cpu, [n intValue]);
		}
	    }
	}
    } while(0); /* end block */
    if(errs) /* exit if there were any reported errors */
	exit(1);

    /* call posix_spawn */
    spawnIt(count, cpu->buf, 0, [execpath fileSystemRepresentation], argv, environ);
}

static void __dead2
usage(int ret)
{
    fprintf(stderr,
	"Usage: %s\n"
	"       Display the machine's architecture type\n"
	"Usage: %s [-h] [[-arch_name | -arch arch_name] ...] prog [arg ...]\n"
	"       Run prog with any arguments, using the given architecture\n"
	"       order.  If no architectures are specified, use the\n"
	"       ARCHPREFERENCE environment variable, or a property list file.\n"
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
    int lp, ln;
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
static void __dead2
spawnFromArgs(CPU *cpu, char **argv, char **environ)
{
    char *ap;

    /* process cpu options */
    for(argv++; *argv && **argv == '-'; argv++) {
	if(strcmp(*argv, "-arch") == 0) {
	    if(*++argv == NULL) {
		warnx("-arch without architecture");
		usage(1);
	    }
	    ap = *argv;
	} else if(strcmp(*argv, "-h") == 0) {
	    usage(0);
	} else
	    ap = *argv + 1;
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
    int count = CPUCOUNT(cpu);
    if(count <= 0 || needexecpath)
	spawnFromPreferences(cpu, needexecpath, argv, environ); /* doesn't return */

    /*
     * Call posix_spawnp on the program name.
     */
    spawnIt(count, cpu->buf, 1, *argv, argv, environ);
}

/*
 * init - initializes the ArchDict dictionary from the values in initArches,
 * and the CPU structure.
 */
static void
init(CPU *cpu)
{
    ArchDict = [NSMutableDictionary dictionaryWithCapacity: 4];
    if(!ArchDict)
	errx(1, "Can't create NSMutableDictionary");
    StrInt *sp;
    for(sp = initArches; sp->str; sp++) {
	NSString *s = [NSString stringWithUTF8String: sp->str];
	if(!s)
	    errx(1, "Can't create NSString for %s", sp->str);
	NSNumber *n = [NSNumber numberWithInt: sp->i];
	if(!n)
	    errx(1, "Can't create NSNumber for %s", sp->str);
	[ArchDict setObject: n forKey: s];
    }
    initCPU(cpu);
}

/* the main() routine */
int
main(int argc, char **argv, char **environ)
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

    /* set up Objective C autorelease pool */
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    init(&cpu); /* initialize */

    if(my_name_is_arch)
	spawnFromArgs(&cpu, argv, environ);
    else
	spawnFromPreferences(&cpu, 1, argv, environ);
    /* should never get here */
    [pool release];
    errx(1, "returned from spawn");
}
