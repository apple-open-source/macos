/*
 * Copyright (c) 2007, 2008 Apple Inc. All rights reserved.
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
// Kevin Van Vechten <kvv@apple.com>

#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <err.h>

#include "utils.h"

#define	FEATURESNAME "Features"
#define FEATUREMACROPREFIX "TARGET_HAVE_"
#define FEATUREMACROPREFIXLEN (sizeof(FEATUREMACROPREFIX) - 1)
#define	MACROINDENT "  "
#define MACROINDENTLEN (sizeof(MACROINDENT) - 1)
#define TARGET_CONFIG_DIR_PATH "/usr/share/TargetConfigs"
#define TARGET_CONFIG_DEFAULT "Default"
#define TARGET_CONFIG_FEATURE_SCRIPTS_DIR TARGET_CONFIG_DIR_PATH "/feature_scripts"
#define TARGET_CONFIG_PLIST_VERSION 0
#define TEST_ARCH_PATTERN "^[[:alnum:]_]+:TARGET_.*$"
#define TEST_FEATURE_PATTERN "^[[:alnum:]_]+:.*$"
#define TEST_RECORD_FILE ".TargetConfigTestRecord.plist"
#define TEST_RECORD_LOCK ".TargetConfigTestRecord.lock"
#define	UNDEFSTRING ((char *)-1)
#define VALUENAME "value"

typedef const char * (*iterateFunc)(const char *key, int val, void *data);

#define TEST_RECORD_ENV(x) {(x), CFSTR(x)}
static struct test_record_env {
	const char *name;
	const CFStringRef cfstr;
} test_record_env[] = {
	TEST_RECORD_ENV("RC_CFLAGS"),
	TEST_RECORD_ENV("RC_TARGET_CONFIG"),
	TEST_RECORD_ENV("SDKROOT"),
	{NULL, NULL}
};

void
usage() {
	fprintf(stderr, "usage: tconf \n"
		"\t--product\n"
		"\t--archs\n"
		"\t--cflags\n"
		"\t--cppflags\n"
		"\t--cxxflags\n"
		"\t--ldflags\n"
		"\t--cc\n"
		"\t--cpp\n"
		"\t--cxx\n"
		"\t--ld\n"
		"\t--export-header[=<new-path>.h]\n"
		"\t[-q] [--alt-features-dir <dir>] --test <variable>\n");
	exit(EXIT_FAILURE);
}

char*
find_config() {
	char* result;
	const char* target = getenv("RC_TARGET_CONFIG");
	const char* sdkroot = getenv("SDKROOT");

	// plist location is dependent on SDKROOT and RC_TARGET_CONFIG settings.

	// $(SDKROOT)/usr/share/TargetConfigs/Default.plist
	if (sdkroot) {
		asprintf(&result, "%s/%s/%s.plist", sdkroot,
			TARGET_CONFIG_DIR_PATH, TARGET_CONFIG_DEFAULT);
		if (is_file(result)) return result;
		free(result);
	}
	
	// /usr/share/TargetConfigs/$(RC_TARGET_CONFIG).plist
	if (target) {
		asprintf(&result, "%s/%s.plist",
			TARGET_CONFIG_DIR_PATH, target);
		if (is_file(result)) return result;
		free(result);
	}
	
	// /usr/share/TargetConfigs/Default.plist
	asprintf(&result, "%s/%s.plist",
		TARGET_CONFIG_DIR_PATH, TARGET_CONFIG_DEFAULT);
	if (is_file(result)) return result;
	free(result);
	return NULL;
}

CFDictionaryRef
read_config() {
	char* path = find_config();
	const char *errstr;
	if (!path) {
		errx(EXIT_FAILURE, "read_config: no target configuration found");
	}
	
	CFPropertyListRef plist = read_plist(path, &errstr);
	if (!plist) {
		errx(EXIT_FAILURE, "read_plist(%s): %s", path, errstr);
	}
	if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
		errx(EXIT_FAILURE, "read_config: invalid target configuration: %s\n", path);
	}
	CFNumberRef version = CFDictionaryGetValue(plist,
				CFSTR("TargetConfigVersion"));
	int v = -1;
	if (!version || CFGetTypeID(version) != CFNumberGetTypeID() ||
		!CFNumberGetValue(version, kCFNumberIntType, &v) ||
		v > TARGET_CONFIG_PLIST_VERSION) {
		errx(EXIT_FAILURE, "read_config: invalid target configuration version: %d\n", v);
	}
	free(path);
	return plist;
}

CFTypeRef
lookup_config(CFDictionaryRef config, CFStringRef key) {
	config = CFDictionaryGetValue(config,
			CFSTR("TargetConditionals"));
	return config ? CFDictionaryGetValue(config, key) : NULL;
}

CFTypeRef
lookup_arch_config(CFDictionaryRef config, CFStringRef arch, CFStringRef key) {
	CFTypeRef res = 0;
	CFDictionaryRef target_archs = CFDictionaryGetValue(config,
					CFSTR("TargetArchitectures"));
	if (!target_archs) return NULL;

	if (arch) {
		CFDictionaryRef arch_config = CFDictionaryGetValue(target_archs,
						arch);
		if (arch_config) {
			res = CFDictionaryGetValue(arch_config, key);
		}
	} else {
		CFStringRef env = cfstr(getenv("RC_ARCHS"));
		CFArrayRef archs = tokenizeString(env);
		if (env) CFRelease(env);

		if (!archs) return NULL;


		CFIndex i, count = CFArrayGetCount(archs);
		for (i = 0; i < count; ++i) {
			arch = CFArrayGetValueAtIndex(archs, i);
			CFDictionaryRef arch_config = CFDictionaryGetValue(target_archs,
							arch);
			if (arch_config) {
				res = CFDictionaryGetValue(arch_config, key);
			}
			if (res) break;
		}

		CFRelease(archs);
	}

	return res;
}

int
lookup_feature(CFDictionaryRef config, const char *var, const char **errstr) {
	CFTypeRef dict = CFDictionaryGetValue(config, CFSTR(FEATURESNAME));
	if (!dict) {
		*errstr = NULL;
		return -1;
	}
	if (CFGetTypeID(dict) != CFDictionaryGetTypeID()) {
		*errstr = "lookup_feature: \"" FEATURESNAME "\" is not a dictionary";
		return -1;
	}
	CFStringRef key = cfstr(var);
	CFTypeRef val = CFDictionaryGetValue((CFDictionaryRef)dict, key);
	CFRelease(key);
	if (!val) {
		asprintf((char **)errstr, "lookup_feature: %s: no such key", var);
		if (!*errstr) *errstr = "lookup_feature: no such key";
		return -1;
	}
	if (CFGetTypeID(val) != CFBooleanGetTypeID()) {
		asprintf((char **)errstr, "lookup_feature: %s: value is not boolean", var);
		if (!*errstr) *errstr = "lookup_feature: value is not boolean";
		return -1;
	}
	return (CFBooleanGetValue((CFBooleanRef)val) ? 1 : 0);
}

void
test_record(const char *symroot, const char *path, const char *var, int val) {
	CFPropertyListRef plist;
	CFMutableDictionaryRef dict = NULL, dict2;
	const char *file = stack_pathconcat(symroot, TEST_RECORD_FILE);
	const char *lock = stack_pathconcat(symroot, TEST_RECORD_LOCK);
	const char *linkfile, *env;
	CFStringRef str;
	struct test_record_env *e;

	(void)mkdir(symroot, 0755);

	if (file == NULL) {
		warn("test_record: stack_pathconcat(%s, %s)",
		    symroot, TEST_RECORD_LOCK);
		return;
	}
	if (lock == NULL) {
		warn("test_record: stack_pathconcat(%s, %s)",
		    symroot, TEST_RECORD_FILE);
		return;
	}
	if((linkfile = lockfilebylink(lock)) == NULL) {
		// error or timeout
		warnx("test_record: %s: couldn't lock", lock);
		return;
	}
	if ((plist = read_plist(file, NULL)) != NULL) {
		dict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault,
		    0, plist);
		CFRelease(plist);
		if (dict == NULL) {
			warnx("test_record: CFDictionaryCreateMutableCopy failed");
			unlockfilebylink(linkfile);
			return;
		}
	}
	if (!dict && (dict = CFDictionaryCreateMutable(
	    kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks)) == NULL) {
		warnx("run_feature_script: CFDictionaryCreateMutable failed");
		unlockfilebylink(linkfile);
		return;
	}
	if ((dict2 = CFDictionaryCreateMutable(
	    kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks)) == NULL) {
		warnx("run_feature_script: 2nd CFDictionaryCreateMutable failed");
		CFRelease(dict);
		unlockfilebylink(linkfile);
		return;
	}
	CFDictionarySetValue((CFMutableDictionaryRef)dict2, CFSTR(VALUENAME),
	    (val ? kCFBooleanTrue : kCFBooleanFalse));
	str = cfstr(path);
	CFDictionarySetValue((CFMutableDictionaryRef)dict2, CFSTR("path"), str);
	CFRelease(str);
	for(e = test_record_env; e->name; e++) {
		if ((env = getenv(e->name)) != NULL) {
			str = cfstr(env);
			CFDictionarySetValue((CFMutableDictionaryRef)dict2,
			    e->cfstr, str);
			CFRelease(str);
		}
	}
	str = cfstr(var);
	CFDictionarySetValue(dict, str, dict2);
	CFRelease(str);
	CFRelease(dict2);
	if (write_plist(file, (CFPropertyListRef)dict) < 0) {
		warn("run_feature_script: write_plist failed");
	}
	CFRelease(dict);
	unlockfilebylink(linkfile);
}

int
_run_feature_script(const char *dir, const char *var, const char **errstr) {
	char *path;
	extern char *const *environ;
	const char *args[3];
	int status, ret;
	pid_t child;
	const char *rc_xbs, *symroot;
	char *name = index(var, ':'); // we already know var must contain a colon
	int len = name - var;
	char *type = alloca(len + 1);

	if (type == NULL) {
		asprintf((char **)errstr, "run_feature_script: alloca: %s",
		    strerror(errno));
		if (!*errstr) *errstr = "run_feature_script: alloca";
		return -1;
	}
	strncpy(type, var, len);
	type[len] = 0;
	++name;

	if ((path = stack_pathconcat(dir, type)) == NULL) {
		asprintf((char **)errstr, "run_feature_script: stack_pathconcat: %s",
		    strerror(errno));
		if (!*errstr) *errstr
		    = "run_feature_script: stack_pathconcat: Out of memory";
		return -1;
	}
	args[0] = type;
	args[1] = name;
	args[2] = NULL;
	if ((status = posix_spawn(&child, path, NULL, NULL, (char *const *)args,
	    environ)) != 0) {
		*errstr = NULL;
		return -1;
	}
	if (wait(&status) < 0) {
		asprintf((char **)errstr, "run_feature_script: wait: %s",
		    strerror(errno));
		if (!*errstr) *errstr = "run_feature_script: wait failed";
		return -1;
	}
	if (!WIFEXITED(status)) {
		if (WIFSIGNALED(status)) {
			asprintf((char **)errstr,
			    "run_feature_script: \"%s %s\" died with signal %d",
			    type, name, WTERMSIG(status));
			if (!*errstr) *errstr
			    = "run_feature_script: died by signal";
		} else {
			asprintf((char **)errstr,
			    "run_feature_script: \"%s %s\" stopped with signal %d",
			    type, name, WSTOPSIG(status));
			if (!*errstr) *errstr
			    = "run_feature_script: stopped by signal";
		}
		return -1;
	}

	ret = (WEXITSTATUS(status) == 0 ? 1 : 0);

	if ((rc_xbs = getenv("RC_XBS")) != NULL && strcmp(rc_xbs, "YES") == 0 &&
	    (symroot = getenv("SYMROOT")) != NULL) {
		test_record(symroot, path, var, ret);
	}
	return ret;
}

int
run_feature_script(const char *altfeaturesdir, const char *var,
    const char **errstr) {
	int val;

	if (!*var) {
		*errstr = "run_feature_script: empty feature string";
		return -1;
	}
	if (altfeaturesdir
	    && ((val = _run_feature_script(altfeaturesdir, var, errstr)) >= 0
	    || *errstr)) return val;
	if ((val = _run_feature_script(TARGET_CONFIG_FEATURE_SCRIPTS_DIR, var, errstr))
	    < 0 && !*errstr) {
		asprintf((char **)errstr,
		    "run_feature_script: %s: Can't find feature script", var);
		if (!*errstr) *errstr
		    = "run_feature_script: Can't find feature script";
	}
	return val;
}

int
cmd_archs(CFDictionaryRef config) {
	int res = 0;
	
	CFStringRef env = cfstr(getenv("RC_ARCHS"));
	CFArrayRef archs = tokenizeString(env);
	if (env) CFRelease(env);

	if (!archs) {
		CFDictionaryRef target_archs = CFDictionaryGetValue(config,
					CFSTR("TargetArchitectures"));
		if (target_archs) {
			archs = dictionaryGetSortedKeys(target_archs);
			CFRelease(target_archs);
		}
	}

	if (!archs) {
		return -1;
	}

	CFIndex i, count = CFArrayGetCount(archs);
	for (i = 0; i < count; ++i) {
		CFStringRef arch = CFArrayGetValueAtIndex(archs, i);
		if (i > 0) printf(" ");
		cfprintf(stdout, "%@", arch);
	}
	printf("\n");

	CFRelease(archs);
	return res;
}

CFTypeRef
eval_test(const char *altfeaturesdir, CFDictionaryRef config, const char *var,
    const char **errstr) {
	int val;
	static regex_t reg;
	static int inited = 0;

	if (!inited) {
		if ((val = regcomp(&reg, TEST_FEATURE_PATTERN, REG_EXTENDED|REG_NOSUB)) != 0) {
			asprintf((char **)errstr,
			    "eval_test: regcomp \"%s\" return error %d",
			    TEST_FEATURE_PATTERN, val);
			if (!*errstr) *errstr
			    = "eval_test: regcomp \"" TEST_FEATURE_PATTERN
			    "\" failied";
			return NULL;
		}
		inited++;
	}
	if (regexec(&reg, var, 0, NULL, 0) != 0) {
		asprintf((char **)errstr,
		    "eval_test: \"%s\": invalid test conditional", var);
		if (!*errstr) *errstr = "eval_test: invalid test conditional";
		return NULL;
	}
	if ((val = lookup_feature(config, var, errstr)) >= 0) {
		if (altfeaturesdir) warn("--alt-features-dir ignored because \""
		    FEATURESNAME "\" dictionary exists");
	} else {
		if(*errstr != NULL) return NULL;
		if ((val = run_feature_script(altfeaturesdir, var, errstr))
		    < 0) return NULL;
	}
	return (val ? kCFBooleanTrue : kCFBooleanFalse);
}

int
cmd_test(int qflag, const char *altfeaturesdir, CFDictionaryRef config,
    const char* var) {
	CFTypeRef res = NULL;
	int ret;
	const char *errstr;
	static regex_t reg;
	static int inited = 0;

	if (!inited) {
		if ((ret = regcomp(&reg, TEST_ARCH_PATTERN,
		    REG_EXTENDED|REG_NOSUB)) != 0) {
			warnx("cmd_test: regcomp \"%s\" return error %d",
			    TEST_ARCH_PATTERN, ret);
			return -1;
		}
		inited++;
	}

	if (strncmp(var, "TARGET_", 7) == 0) {
		CFStringRef key = cfstr(var);
		res = lookup_arch_config(config, NULL, key);
		if (!res) res = lookup_config(config, key);
		CFRelease(key);
	} else if (regexec(&reg, var, 0, NULL, 0) == 0) {
		char *name = index(var, ':'); // we already know var must contain a colon
		int len = name - var;
		char *type = alloca(len + 1);

		if (type == NULL) {
			warn("cmd_test: alloca");
			return -1;
		}
		strncpy(type, var, len);
		type[len] = 0;
		++name;
		CFStringRef arch = cfstr(type);
		CFStringRef key = cfstr(name);
		res = lookup_arch_config(config, arch, key);
		CFRelease(arch);
		CFRelease(key);
	} else if ((res = eval_test(altfeaturesdir, config, var, &errstr))
	    == NULL) {
		warnx("%s", errstr);
		return -1;
	}

	if (!res) res = kCFBooleanFalse;

	if (!qflag) {
		CFTypeID type = CFGetTypeID(res);
		if (type == CFBooleanGetTypeID()) {
			if (CFBooleanGetValue(res)) printf("YES\n");
			else printf("NO\n");
		} else if (type == CFStringGetTypeID() ||
			   type == CFNumberGetTypeID()) {
			cfprintf(stdout, "%@\n", res);
		} else {
			cfprintf(stderr, "tconf: invalid type for %s: %@\n",
				var, CFCopyTypeIDDescription(type));
		}
	} else {
		exit(res != kCFBooleanFalse ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	return 0;
}

// Print a config value using the following:
// 1) Print value from environment variable envkey
// 2) Print value from plist TargetConfiguration key
// 3) Print value from dflt
int
cmd_config(CFDictionaryRef config,
	const char* keystr,
	const char* envkey, 
	const char* dflt) {

	// 1)
	if (envkey) {
 		const char* env = getenv(envkey);
		if (env) {
			printf("%s\n", env);
			return 0;
		}
	}

	// 2)
	CFStringRef key = cfstr(keystr);
	CFTypeRef val = NULL;
	if (key) {
		CFDictionaryRef target_config = CFDictionaryGetValue(config,
					CFSTR("TargetConfiguration"));

		if (target_config &&
			CFGetTypeID(target_config) == CFDictionaryGetTypeID()) {
			val = CFDictionaryGetValue(target_config, key);
		}
	}
	if (val) cfprintf(stdout, "%@\n", val);
	// 3)
	else if (dflt) printf("%s\n", dflt);
	else putchar('\n');

	CFRelease(key);
	return 0;
}

const char *
feature_key_transform(const char *key) {
	char *str = malloc(FEATUREMACROPREFIXLEN + strlen(key) + 1);
	if (!str) return key;
	strcpy(str, FEATUREMACROPREFIX);
	strcat(str, key);
	upper_ident(str);
	return str;
}

CFTypeRef
feature_value_transform(CFTypeRef val) {
	return (CFBooleanGetValue(val) ? kCFBooleanTrue : NULL);
}

CFTypeRef
feature_record_value_transform(CFTypeRef val) {
	CFBooleanRef v = CFDictionaryGetValue((CFDictionaryRef)val,
	    CFSTR(VALUENAME));
	return (CFBooleanGetValue(v) ? kCFBooleanTrue : NULL);
}

typedef const char *(*transform)(const char *);
typedef  CFTypeRef(*cftransform)( CFTypeRef);
struct header_data {
	FILE *f;
	int indent;
	transform keyfunc;
	cftransform valfunc;
};

void
_export_key_value(CFStringRef key, CFTypeRef value, struct header_data *data) {
	CFTypeID type;
	char *indent;
	const char *tkey = NULL, *skey = stack_cfstrdup(key);

	if (skey == NULL) {
		warn("_export_key_value: stack_cfstrdup");
		return;
	}

	if (data->valfunc) value = data->valfunc(value);
	if (value) {
	    type = CFGetTypeID(value);
	    if (type == CFBooleanGetTypeID()) {
		    int i = CFBooleanGetValue(value);
		    value = CFNumberCreate(NULL, kCFNumberIntType, &i);
	    } else if (type == CFStringGetTypeID()) {
		    // XXX escape string
		    value = CFStringCreateWithFormat(NULL, NULL,
				    CFSTR("\"%@\""), value);
	    } else if (type == CFNumberGetTypeID()) {
		    CFRetain(value);
	    } else {
		    return;
	    }
	}

	if (data->indent > 0) {
		indent = alloca(MACROINDENTLEN * data->indent + 1);
		if (indent) {
			memset(indent, ' ', MACROINDENTLEN * data->indent);
			indent[MACROINDENTLEN * data->indent] = 0;
		} else indent = MACROINDENT;
	} else indent = "";

	if (data->keyfunc) skey = tkey = data->keyfunc(skey);

	fprintf(data->f,
"#%sifndef %s\n",
	indent, skey);
	if (value) {
		cfprintf(data->f,
"#%s%sdefine %s %@\n",
		indent, MACROINDENT, skey, value);
		CFRelease(value);
	} else {
		fprintf(data->f,
"/* #%s%sundef %s */\n",
		indent, MACROINDENT, skey);
	}
	fprintf(data->f,
"#%sendif /* !%s */\n"
"\n",
	indent, skey);
	if (tkey) free((void *)tkey);
}

int
export_target_conds(FILE* f, CFDictionaryRef config) {
	CFDictionaryRef target_conds = CFDictionaryGetValue(config,
						CFSTR("TargetConditionals"));
	if (target_conds) {
		struct header_data data = {f, 0, NULL, NULL};
		dictionaryApplyFunctionSorted(target_conds,
		    (CFDictionaryApplierFunction)&_export_key_value,
		    (void*)&data);
	}
	return 0;
}

int
_export_recorded_features(FILE *f, const char *symroot) {
	CFPropertyListRef plist;
	const char *file = stack_pathconcat(symroot, TEST_RECORD_FILE);
	const char *lock = stack_pathconcat(symroot, TEST_RECORD_LOCK);
	const char *linkfile;
	const char *errstr;
	struct stat st;
	struct header_data data = {f, 0, feature_key_transform,
				   feature_record_value_transform};

	if (file == NULL) {
		warn("_export_recorded_features: stack_pathconcat(%s, %s)",
		    symroot, TEST_RECORD_LOCK);
		return -1;
	}
	if (lock == NULL) {
		warn("_export_recorded_features: stack_pathconcat(%s, %s)",
		    symroot, TEST_RECORD_FILE);
		return -1;
	}
	if (stat(file, &st) < 0) {
		if (errno == ENOENT) return 0;
		warn("_export_recorded_features: stat");
		return -1;
	}
	if((linkfile = lockfilebylink(lock)) == NULL) {
		// error or timeout
		warnx("_export_recorded_features: %s: couldn't lock");
		return -1;
	}
	if ((plist = read_plist(file, &errstr)) == NULL) {
		warnx("_export_recorded_features: %s", errstr);
		unlockfilebylink(linkfile);
		return -1;
	}
	dictionaryApplyFunctionSorted(plist,
	    (CFDictionaryApplierFunction)&_export_key_value,
	    (void*)&data);
	CFRelease(plist);
	unlockfilebylink(linkfile);
	return 0;
}

int
export_features(FILE* f, CFDictionaryRef config) {
	CFDictionaryRef features = CFDictionaryGetValue(config,
						CFSTR(FEATURESNAME));
	const char *rc_xbs, *symroot;

	if (features) {
		struct header_data data = {f, 0, feature_key_transform,
					   feature_value_transform};
		dictionaryApplyFunctionSorted(features,
		    (CFDictionaryApplierFunction)&_export_key_value,
		    (void*)&data);
	} else if ((rc_xbs = getenv("RC_XBS")) != NULL
	    && strcmp(rc_xbs, "YES") == 0
	    && (symroot = getenv("SYMROOT")) != NULL) {
		_export_recorded_features(f, symroot);
	}
	return 0;
}

void
_export_target_arch_conds(CFStringRef key, CFTypeRef value, FILE* f) {
	struct header_data data = {f, 1, NULL, NULL};
	cfprintf(f,
"#if defined(__%@__)\n"
"\n",
		key);
	dictionaryApplyFunctionSorted(value,
		(CFDictionaryApplierFunction)&_export_key_value,
		(void*)&data);
	cfprintf(f,
"#endif /* __%@__ */\n"
"\n",
	key);
}
	


int
export_target_arch_conds(FILE* f, CFDictionaryRef config) {
	CFDictionaryRef target_archs = CFDictionaryGetValue(config,
						CFSTR("TargetArchitectures"));

        if (target_archs) {
                dictionaryApplyFunctionSorted(target_archs,
		    (CFDictionaryApplierFunction)&_export_target_arch_conds,
		    (void*)f);
	}

	return 0;
}

int
cmd_export_header(CFDictionaryRef config, const char *exportheader) {
	time_t t;
	FILE *out;
	const char *base, *upper;

	if (!exportheader) {
		out = stdout;
		exportheader = "TargetConfig.h";
	} else if ((out = fopen(exportheader, "w")) == NULL) {
		err(EXIT_FAILURE, "cmd_export_header: fopen %s", exportheader);
	}
	if ((base = rindex(exportheader, '/')) != NULL) {
	    if (*++base == 0) errx(EXIT_FAILURE, "cmd_export_header: \"%s\" ends with a slash", exportheader);
	} else {
	    base = exportheader;
	}
	if ((upper = stack_strdup(base)) == NULL) {
		err(EXIT_FAILURE, "cmd_export_header: stack_strdup");
	}
	upper_ident((char *)upper);

	time(&t);
	cfprintf(out,
"// %s is auto-generated by tconf(1); Do not edit.\n"
"// %s"
"// Target: %@\n"
"\n"
"#ifndef _%s_\n"
"#define _%s_\n"
"\n"
"#include <sys/cdefs.h>\n"
"#include <TargetConditionals.h>\n"
"\n",
	base,
	ctime(&t),
	CFDictionaryGetValue(config, CFSTR("TargetConfigProduct")),
	upper,
	upper);

#if 1
	export_target_arch_conds(out, config);
#endif
	export_target_conds(out, config);
	export_features(out, config);

	fprintf(out,
"#endif /* _%s_ */\n",
	upper);
	if (out != stdout) fclose(out);

	return 0;
}

enum {
	PRODUCT = 0,
	ARCHS,
	CFLAGS,
	CPPFLAGS,
	CXXFLAGS,
	LDFLAGS,
	CC,
	CPP,
	CXX,
	LD,
	INCLUDE,
		_SINGLE_SIZE,	/* above this are options that only produce */
				/* one line of output, even if appearing */
				/* multiple times on the command line */
	TEST = _SINGLE_SIZE,
		_RECORDABLE_SIZE,/* must be after last recordable option */
	ALTFEATURESDIR = _RECORDABLE_SIZE,
	EXPORTHEADER,
};

#define PRODUCTMASK		(1 << PRODUCT)
#define ARCHSMASK		(1 << ARCHS)
#define CFLAGSMASK		(1 << CFLAGS)
#define CPPFLAGSMASK		(1 << CPPFLAGS)
#define CXXFLAGSMASK		(1 << CXXFLAGS)
#define LDFLAGSMASK		(1 << LDFLAGS)
#define CCMASK			(1 << CC)
#define CPPMASK			(1 << CPP)
#define CXXMASK			(1 << CXX)
#define LDMASK			(1 << LD)
#define INCLUDEMASK		(1 << INCLUDE)
#define TESTMASK		(1 << TEST)
#define ALTFEATURESDIRMASK	(1 << ALTFEATURESDIR)
#define EXPORTHEADERMASK	(1 << EXPORTHEADER)

#define MARK(x)			seen |= (1 << (x))
#define SEEN(x)			(seen & (1 << (x)))

struct record {
	int type;
	char *arg;
};

static struct option longopts[] = {
	{"product",		no_argument,		NULL,	PRODUCT},
	{"archs",		no_argument,		NULL,	ARCHS},
	{"cflags",		no_argument,		NULL,	CFLAGS},
	{"cppflags",		no_argument,		NULL,	CPPFLAGS},
	{"cxxflags",		no_argument,		NULL,	CXXFLAGS},
	{"ldflags",		no_argument,		NULL,	LDFLAGS},
	{"cc",			no_argument,		NULL,	CC},
	{"cpp",			no_argument,		NULL,	CPP},
	{"cxx",			no_argument,		NULL,	CXX},
	{"ld",			no_argument,		NULL,	LD},
	{"include",		no_argument,		NULL,	INCLUDE},
	{"export-header",	optional_argument,	NULL,	EXPORTHEADER},
	{"test",		required_argument,	NULL,	TEST},
	{"altfeaturesdir",	required_argument,	NULL,	ALTFEATURESDIR},
	{NULL,			0,			NULL,	0}
};

int
main(int argc, char* argv[]) {
	uint32_t seen = 0;
	int ch, nrec, n, qflag = 0;
	char *altfeaturesdir = NULL;
	char *exportheader = UNDEFSTRING;
	struct record *rp, *rec;
	int ret = EXIT_SUCCESS;

	if (argc <= 1) usage();

	rec = (struct record *)alloca(argc * sizeof(struct record));
	if (rec == NULL) err(EXIT_FAILURE, "alloca");

	CFDictionaryRef config = read_config();

	rp = rec;
	while ((ch = getopt_long_only(argc, argv, "q", longopts, NULL)) != -1) {
		switch(ch) {
		case 'q':
			qflag = 1;
			continue; // don't call MARK(ch) below
		case ALTFEATURESDIR:
			altfeaturesdir = optarg;
			continue; // don't call MARK(ch) below
		case EXPORTHEADER:
			exportheader = optarg;
			break;
		case TEST:
			rp->arg = optarg;
			/* drop through */
		default:
			if (ch < _SINGLE_SIZE && SEEN(ch)) break;
			rp->type = ch;
			rp++;
			break;
		}
		MARK(ch);
	}
	argc -= optind;
	argv += optind;

	nrec = rp - rec;
	if (argc > 0) usage();
	if (qflag && (seen & TESTMASK) &&
	    (nrec > 1 || (seen & ~TESTMASK))) usage();

	for(rp = rec, n = nrec; n > 0; rp++, n--) {
		switch(rp->type) {
		case PRODUCT:
			if (nrec > 1) printf("product=");
			CFStringRef product = CFDictionaryGetValue(config,
				CFSTR("TargetConfigProduct"));
			if (product) cfprintf(stdout, "%@\n", product);
			else ret = EXIT_FAILURE;
			break;
		case ARCHS:
			if (nrec > 1) printf("archs=");
			if (cmd_archs(config) < 0) ret = EXIT_FAILURE;
			break;
		case CFLAGS:
			if (nrec > 1) printf("cflags=");
			if (cmd_config(config, "CFLAGS", "RC_CFLAGS", NULL)
			    < 0) ret = EXIT_FAILURE;
			break;
		case CPPFLAGS:
			if (nrec > 1) printf("cppflags=");
			if (cmd_config(config, "CPPFLAGS", NULL, NULL)
			    < 0) ret = EXIT_FAILURE;
			break;
		case CXXFLAGS:
			if (nrec > 1) printf("cxxflags=");
			if (cmd_config(config, "CXXFLAGS", "RC_CFLAGS", NULL)
			    < 0) ret = EXIT_FAILURE;
			break;
		case LDFLAGS:
			if (nrec > 1) printf("ldflags=");
			if (cmd_config(config, "LDFLAGS", NULL, NULL)
			    < 0) ret = EXIT_FAILURE;
			break;
		case CC:
			if (nrec > 1) printf("cc=");
			if (cmd_config(config, "CC", NULL, "/usr/bin/cc")
			    < 0) ret = EXIT_FAILURE;
			break;
		case CPP:
			if (nrec > 1) printf("cpp=");
			if (cmd_config(config, "CPP", NULL, "/usr/bin/cpp")
			    < 0) ret = EXIT_FAILURE;
			break;
		case CXX:
			if (nrec > 1) printf("cxx=");
			if (cmd_config(config, "CXX", NULL, "/usr/bin/c++")
			    < 0) ret = EXIT_FAILURE;
			break;
		case LD:
			if (nrec > 1) printf("ld=");
			if (cmd_config(config, "LD", NULL, "/usr/bin/ld")
			    < 0) ret = EXIT_FAILURE;
			break;
		case INCLUDE:
			//if (nrec > 1) printf("include=");
			break;
		case TEST:
			if (nrec > 1) printf("%s=", rp->arg);
			if (cmd_test(qflag, altfeaturesdir, config, rp->arg)
			    < 0) ret = EXIT_FAILURE;
			break;
		}
	}

	if (exportheader != UNDEFSTRING) {
		if (cmd_export_header(config, exportheader)
		    < 0) ret = EXIT_FAILURE;
	}

	return ret;
}
