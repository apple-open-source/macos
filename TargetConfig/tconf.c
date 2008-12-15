/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "utils.h"

#define TARGET_CONFIG_DIR_PATH "/usr/local/share/TargetConfigs"
#define TARGET_CONFIG_DEFAULT "Default"
#define TARGET_CONFIG_PLIST_VERSION 0

void
usage() {
	fprintf(stderr, "usage: tconf \n"
		"\t--product\n"
		"\t--archs\n"
		"\t--cflags\n"
		"\t--cxxflags\n"
		"\t--ldflags\n"
		"\t--cc\n"
		"\t--cpp\n"
		"\t--cxx\n"
		"\t--ld\n"
		"\t[-q] --test <variable>\n");
	exit(EXIT_FAILURE);
}

char*
find_config() {
	char* result;
	const char* target = getenv("RC_TARGET_CONFIG");
	const char* sdkroot = getenv("SDKROOT");

	// plist location is dependent on SDKROOT and RC_TARGET_CONFIG settings.

	// $(SDKROOT)/usr/local/share/TargetConfigs/Default.plist
	if (sdkroot) {
		asprintf(&result, "%s/%s/%s.plist", sdkroot,
			TARGET_CONFIG_DIR_PATH, TARGET_CONFIG_DEFAULT);
		if (is_file(result)) return result;
		free(result);
	}
	
	// /usr/local/share/TargetConfigs/$(RC_TARGET_CONFIG).plist
	if (target) {
		asprintf(&result, "%s/%s.plist",
			TARGET_CONFIG_DIR_PATH, target);
		if (is_file(result)) return result;
		free(result);
	}
	
	// /usr/local/share/TargetConfigs/Default.plist
	asprintf(&result, "%s/%s.plist",
		TARGET_CONFIG_DIR_PATH, TARGET_CONFIG_DEFAULT);
	if (is_file(result)) return result;
	free(result);
	return NULL;
}

CFDictionaryRef
read_config() {
	char* path = find_config();
	if (!path) {
		fprintf(stderr, "tconf: no target configuration found\n");
		exit(EXIT_FAILURE);
	}
	
	CFPropertyListRef plist = read_plist(path);
	if (!plist || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
		fprintf(stderr,
			"tconf: invalid target configuration: %s\n", path);
		exit(EXIT_FAILURE);
	}
	CFNumberRef version = CFDictionaryGetValue(plist,
				CFSTR("TargetConfigVersion"));
	int v = -1;
	if (!version || CFGetTypeID(version) != CFNumberGetTypeID() ||
		!CFNumberGetValue(version, kCFNumberIntType, &v) ||
		v > TARGET_CONFIG_PLIST_VERSION) {
		fprintf(stderr,
			"tconf: invalid target configuration version: %d\n", v);
		exit(EXIT_FAILURE);
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
lookup_arch_config(CFDictionaryRef config, CFStringRef key) {
	CFTypeRef res = 0;

	CFStringRef env = cfstr(getenv("RC_ARCHS"));
	CFArrayRef archs = tokenizeString(env);
	if (env) CFRelease(env);

	if (!archs) return NULL;

	CFDictionaryRef target_archs = CFDictionaryGetValue(config,
					CFSTR("TargetArchitectures"));
	if (!target_archs) {
		CFRelease(archs);
		return NULL;
	}

	CFIndex i, count = CFArrayGetCount(archs);
	for (i = 0; i < count; ++i) {
		CFStringRef arch = CFArrayGetValueAtIndex(archs, i);
		CFDictionaryRef arch_config = CFDictionaryGetValue(target_archs,
						arch);
		if (arch_config) {
			res = CFDictionaryGetValue(arch_config, key);
		}
		if (res) break;
	}

	CFRelease(archs);
	return res;
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

int
cmd_test(int qflag, CFDictionaryRef config, const char* var) {
	CFTypeRef res = NULL;

	if (strncmp(var, "TARGET_OS_", 10) == 0 || 
		strncmp(var, "TARGET_RT_", 10) == 0 ||
		strncmp(var, "TARGET_CPU_", 11) == 0 ||
		strncmp(var, "TARGET_HAVE_", 12) == 0) {

		CFStringRef key = cfstr(var);
		res = lookup_arch_config(config, key);
		if (!res) res = lookup_config(config, key);
		CFRelease(key);
	} else {
		fprintf(stderr, "tconf: invalid target conditional: %s\n", var);
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

	CFRelease(key);
	return 0;
}

void
_export_target_cond(CFStringRef key, CFTypeRef value, FILE* f) {
	CFTypeID type = CFGetTypeID(value);
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

	cfprintf(f,
"#ifndef %@\n"
"#define %@ %@\n"
"#endif\n"
"\n",
	key, key, value);
	CFRelease(value);
}

int
export_target_conds(FILE* f, CFDictionaryRef config) {
	CFDictionaryRef target_conds = CFDictionaryGetValue(config,
						CFSTR("TargetConditionals"));
	if (target_conds) {
		dictionaryApplyFunctionSorted(target_conds,
		    (CFDictionaryApplierFunction)&_export_target_cond,
		    (void*)f);
	}
	return 0;
}

void
_export_target_arch_conds(CFStringRef key, CFTypeRef value, FILE* f) {
	cfprintf(f,
"#if defined(__%@__)\n",
		key);
	dictionaryApplyFunctionSorted(value,
		(CFDictionaryApplierFunction)&_export_target_cond,
		(void*)f);
	cfprintf(f,
"#endif\n");
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
cmd_export_header(CFDictionaryRef config) {
	cfprintf(stdout,
"// TargetConfig.h is auto-generated by tconf(1); Do not edit.\n"
"// Target: %@\n"
"\n"
"#ifndef __TARGET_CONFIG_H__\n"
"#define __TARGET_CONFIG_H__\n"
"\n"
"#include <sys/cdefs.h>\n"
"#include <TargetConditionals.h>\n"
"\n"
"__BEGIN_DECLS\n"
"\n",
	CFDictionaryGetValue(config, CFSTR("TargetConfigProduct")));

#if 0
	export_target_arch_conds(stdout, config);
#endif
	export_target_conds(stdout, config);

	printf(
"__END_DECLS\n"
"\n"
"#endif // __TARGET_CONFIG_H__\n");

	return 0;
}

int
main(int argc, char* argv[]) {

	if (argc < 2) usage();
	
	const char* cmd = argv[1];

	int qflag = 0;

	if (strcmp(cmd, "-q") == 0) {
		qflag = 1;
		--argc;
		++argv;
	}

	CFDictionaryRef config = read_config();
	
	if (strcmp(cmd, "--archs") == 0) {
		cmd_archs(config);
	} else if (strcmp(cmd, "--cflags") == 0) {
		cmd_config(config, "CFLAGS", "RC_CFLAGS", NULL);
	} else if (strcmp(cmd, "--cxxflags") == 0) {
		cmd_config(config, "CXXFLAGS", "RC_CFLAGS", NULL);
	} else if (strcmp(cmd, "--ldflags") == 0) {
		cmd_config(config, "LDFLAGS", NULL, NULL);
	} else if (strcmp(cmd, "--cppflags") == 0) {
		cmd_config(config, "CPPFLAGS", NULL, NULL);
	} else if (strcmp(cmd, "--cc") == 0) {
		cmd_config(config, "CC", NULL, "/usr/bin/cc");
	} else if (strcmp(cmd, "--cpp") == 0) {
		cmd_config(config, "CPP", NULL, "/usr/bin/ccp");
	} else if (strcmp(cmd, "--cxx") == 0) {
		cmd_config(config, "CXX", NULL, "/usr/bin/c++");
	} else if (strcmp(cmd, "--ld") == 0) {
		cmd_config(config, "LD", NULL, "/usr/bin/ld");
	} else if (strcmp(cmd, "--includes") == 0) {
	} else if (strcmp(cmd, "--export-header") == 0) {
		cmd_export_header(config);
	} else if (strcmp(cmd, "--product") == 0) {
		CFStringRef product = CFDictionaryGetValue(config,
			CFSTR("TargetConfigProduct"));
		if (product) cfprintf(stdout, "%@\n", product);
	} else if (strcmp(cmd, "--test") == 0) {
		if (argc < 3) usage();
		cmd_test(qflag, config, argv[2]);
	}
	
	return EXIT_SUCCESS;
}
