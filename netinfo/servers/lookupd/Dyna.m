/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2001 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Dyna.m
 * Written by Marc Majka
 */

#import "Dyna.h"
#import "Config.h"
#import "LUServer.h"
#import <NetInfo/syslock.h>
#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <NetInfo/system_log.h>
#import <NetInfo/dsutil.h>
#import <mach-o/dyld.h>
#import <sys/param.h>

#define LOOKUPD_BUNDLE_DIR "/System/Library/PrivateFrameworks/NetInfo.framework/Resources/lookupd/Agents"

static char **loaded_bundle_path = NULL;
static NSModule *loaded_module;
static unsigned int nloaded = 0;
static syslock *load_lock = NULL;

static void *
fetch_symbol(char *sym, char *agent, NSModule module)
{
	char *symbol_name;
	NSSymbol *symbol;
	u_int32_t len;

	len = strlen(agent) + strlen(sym) + 16;
	symbol_name = malloc(len);
	sprintf(symbol_name, "_%s_%s", agent, sym);
	symbol = NSLookupSymbolInModule(module, symbol_name);
	free(symbol_name);

	if (symbol == NULL) return NULL;
	return NSAddressOfSymbol(symbol);
}

static u_int32_t
callout_null_new(void **c, char *args, dynainfo *d)
{
	system_log(LOG_DEBUG, "callout_null_new: %s", args);
	return 0;
}

static u_int32_t
callout_null_free(void *c)
{
	system_log(LOG_DEBUG, "callout_null_free");
	return 0;
}

static u_int32_t
callout_null_query(void *c, dsrecord *pattern, dsrecord **list)
{
	system_log(LOG_DEBUG, "callout_null_query");
	return 0;
}

static u_int32_t
callout_null_validate(void *c, char *v)
{
	system_log(LOG_DEBUG, "callout_null_validate: %s", v);
	return 0;
}

static u_int32_t
get_config_global(void *d, int c, dsrecord **r)
{
	dynainfo *dyna;
	LUDictionary *config;
	dsrecord *x;

	if (d == NULL) return -1;
	if (r == NULL) return -1;
	if (c >= NCATEGORIES) return -1;

	dyna = (dynainfo *)d;

	if (c == LUCategoryNull)
	{
		config = [configManager configGlobal:(LUArray *)dyna->d1];
	}
	else
	{
		config = [configManager configForCategory:(LUCategory)c fromConfig:(LUArray *)dyna->d1];
	}

	if (config == nil) return -1;

	x = dictToDSRecord(config);
	if (x == NULL) return -1;

	*r = x;
	return 0;
}

static u_int32_t
get_config_agent(void *d, int c, dsrecord **r)
{
	dynainfo *dyna;
	LUDictionary *config;
	dsrecord *x;
	char *name;
	Dyna *myDyna;

	if (d == NULL) return -1;
	if (r == NULL) return -1;
	if (c >= NCATEGORIES) return -1;

	dyna = (dynainfo *)d;
	myDyna = (Dyna *)dyna->d0;

	name = (char *)[myDyna serviceName];
	if (c == LUCategoryNull)
	{
		config = [configManager configForAgent:name fromConfig:(LUArray *)dyna->d1];
	}
	else
	{
		config = [configManager configForAgent:name category:(LUCategory)c fromConfig:(LUArray *)dyna->d1];
	}

	if (config == nil) return -1;

	x = dictToDSRecord(config);
	if (x == NULL) return -1;

	*r = x;
	return 0;
}

@implementation Dyna

+ (Dyna *)alloc
{
	Dyna *agent;

	agent = [super alloc];
	system_log(LOG_DEBUG, "Allocated Dynamic Agent 0x%08x", (int)agent);
	return agent;
}

- (Dyna *)init
{
	if (didInit) return self;

	if (load_lock == NULL) load_lock = syslock_new(0);

	cdata = NULL;

	[super init];

	callout_new = callout_null_new;
	callout_free = callout_null_free;
	callout_query = callout_null_query;
	callout_validate = callout_null_validate;

	dyna.d0 = self;
	dyna.d1 = configurationArray;
	dyna.dyna_config_global = get_config_global;
	dyna.dyna_config_agent = get_config_agent;

	callout_new(&cdata, NULL, &dyna);

	return self;
}

- (LUAgent *)initWithArg:(char *)arg
{
	NSObjectFileImage image;
	NSObjectFileImageReturnCode status;
	NSModule *mod;
	void (*module_impl)(void);
	char *p, *q, *loadpath, *name;
	int i, len, loadindex;

	if (arg == NULL) return [self init];

	if (didInit) return self;

	if (load_lock == NULL) load_lock = syslock_new(0);

	callout_new = callout_null_new;
	callout_free = callout_null_free;
	callout_query = callout_null_query;
	callout_validate = callout_null_validate;

	[super initWithArg:arg];

	loadpath = NULL;
	name = NULL;

	p = strchr(arg, ':');
	if (p != NULL) *p = '\0';

	if (arg[0] == '/')
	{
		loadpath = copyString(arg);
		q = strrchr(arg, '/');
		name = copyString(q+1);
	}
	else
	{
		loadpath = malloc(strlen(LOOKUPD_BUNDLE_DIR) + (2 * strlen(arg)) + 32);
		name = [LUServer canonicalAgentName:arg];
		if (name == NULL)
		{
			if (p != NULL) *p = ':';
			[self release];
			return nil;
		}

		len = strlen(name);
		if ((len > 5) && (streq(name + (len - 5), "Agent")))
		{
			name[len - 5] = '\0';
		}

		/* Backwards compatibility YP == NIS */
		if ((name != NULL) && (streq(name, "YP") || streq(name, "YPAgent")))
		{
			free(name);
			name = malloc(4);
			strcpy(name, "NIS");
		}

		sprintf(loadpath, "%s/%s.bundle/%s", LOOKUPD_BUNDLE_DIR, name, name);
	}

	if (p != NULL)
	{
		*p = ':';
		p++;
	}

	shortname = copyString(name);
	free(serviceName);
	serviceName = NULL;

	serviceName = [LUAgent canonicalServiceName:arg];
	[self setBanner:serviceName];

	/* Don't reload (keep global list of loaded bundles) */
	syslock_lock(load_lock);
	loadindex = -1;

	for (i = 0; i < nloaded; i++)
	{
		if (streq(loadpath, loaded_bundle_path[i]))
		{
			/* already loaded this agent */
			loadindex = i;
			break;
		}
	}
	
	if (loadindex == -1)
	{
		status = NSCreateObjectFileImageFromFile(loadpath, &image);
		if (status != NSObjectFileImageSuccess)
		{
			system_log(LOG_ERR, "Can't load %s", loadpath);
			free(loadpath);
			free(name);
			[self release];
			syslock_unlock(load_lock);
			return nil;
		}

		mod = NSLinkModule(image, loadpath, TRUE);
		if (mod == NULL)
		{
			system_log(LOG_ERR, "Can't link %s", loadpath);
			cdata = NULL;
			free(loadpath);
			free(name);
			[self release];
			syslock_unlock(load_lock);
			return nil;
		}

		if (nloaded == 0)
		{
			loadindex = 0;
			loaded_bundle_path = (char **)malloc(sizeof(char *));
			loaded_bundle_path[0] = copyString(loadpath);
			loaded_module = (NSModule *)malloc(sizeof(NSModule));
			loaded_module[0] = mod;
		}
		else
		{
			loadindex = nloaded;
			loaded_bundle_path = (char **)realloc(loaded_bundle_path, (nloaded + 1) * sizeof(char *));
			loaded_bundle_path[nloaded] = copyString(loadpath);
			loaded_module = (NSModule *)realloc(loaded_module, (nloaded + 1) * sizeof(NSModule));
			loaded_module[nloaded] = mod;
		}

		nloaded++;
	}

	free(loadpath);

	module_impl = fetch_symbol("new", name, loaded_module[loadindex]);
	if (module_impl != NULL) callout_new = (u_int32_t(*)())module_impl;

	module_impl = fetch_symbol("free", name, loaded_module[loadindex]);
	if (module_impl != NULL) callout_free = (u_int32_t(*)())module_impl;

	module_impl = fetch_symbol("query", name, loaded_module[loadindex]);
	if (module_impl != NULL) callout_query = (u_int32_t(*)())module_impl;

	module_impl = fetch_symbol("validate", name, loaded_module[loadindex]);
	if (module_impl != NULL) callout_validate = (u_int32_t(*)())module_impl;

	free(name);

	dyna.d0 = self;
	dyna.d1 = configurationArray;
	dyna.dyna_config_global = get_config_global;
	dyna.dyna_config_agent = get_config_agent;

	status = callout_new(&cdata, p, &dyna);
	if (status != 0)
	{
		cdata = NULL;
		[self release];
		syslock_unlock(load_lock);
		return nil;
	}

	syslock_unlock(load_lock);
	return self;
}

- (const char *)shortName
{
	return shortname;
}

- (void)dealloc
{
	if (cdata != NULL) callout_free(cdata);
	if (shortname != NULL) free(shortname);
	system_log(LOG_DEBUG, "Deallocated Dynamic Agent %s 0x%08x", serviceName, (int)self);
	[super dealloc];
}

- (BOOL)isValid:(LUDictionary *)item
{
	BOOL valid;
	char *v;

	if (item == nil) return NO;
	v = [item valueForKey:"_lookup_validation"];
	valid = (callout_validate(cdata, v) == 1);
	return valid;
}

- (LUArray *)query:(LUDictionary *)pattern
{
	LUArray *all, *stamps;
	LUDictionary *item;
	u_int32_t status, i, len;
	dsrecord *dsr_pattern, *dsr_stamp, *dsr_all;
	dsattribute *a;
	char str[256];

	if (pattern == nil) return nil;

	dsr_pattern = dictToDSRecord(pattern);

	/* Fetch Validation Stamps */
	dsr_stamp = dsrecord_copy(dsr_pattern);
	a = dsattribute_from_cstrings(STAMP_KEY, "1", NULL);
	dsrecord_append_attribute(dsr_stamp, a, SELECT_META_ATTRIBUTE);
	dsattribute_release(a);

	dsr_all = NULL;
	status = callout_query(cdata, dsr_stamp, &dsr_all);
	dsrecord_release(dsr_stamp);
	if (status != 0)
	{
		dsrecord_release(dsr_pattern);
		return nil;
	}

	stamps = dsrecordToArray(dsr_all);
	dsrecord_release(dsr_all);

	dsr_all = NULL;
	status = callout_query(cdata, dsr_pattern, &dsr_all);
	dsrecord_release(dsr_pattern);
	if (status != 0) return nil;

	all = dsrecordToArray(dsr_all);
	dsrecord_release(dsr_all);

	len = [all count];
	for (i = 0; i < len; i++) 
	{
		[[all objectAtIndex:i] setValue:(char *)[self serviceName] forKey:"_lookup_agent"];
	}

	if (all != nil)
	{
		len = [stamps count];
		for (i = 0; i < len; i++) 
		{
			item = [stamps objectAtIndex:i];
			sprintf(str, "V-0x%08x", (unsigned int)item);
			[item setBanner:str];
			[item setValue:(char *)[self serviceName] forKey:"_lookup_agent"];
			[all addValidationStamp:item];
		}
	}

	[stamps release];
	return all;
}

- (LUArray *)query:(LUDictionary *)pattern category:(LUCategory)cat
{
	LUDictionary *q;
	LUArray *all;
	char str[16];

	if ((cat < 0) || (cat > NCATEGORIES)) return nil;

	q = pattern;
	if (pattern == nil) q = [[LUDictionary alloc] init];

	sprintf(str, "%u", cat);
	[q setValue:str forKey:"_lookup_category"];
	all = [self query:q];

	if (pattern == nil) [q release];

	return all;
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat
{
	LUDictionary *q;
	LUArray *all;
	char str[16];

	q = [[LUDictionary alloc] init];
	sprintf(str, "%u", cat);
	[q setValue:str forKey:"_lookup_category"];

	all = [self query:q];
	[q release];

	return all;
}

@end
