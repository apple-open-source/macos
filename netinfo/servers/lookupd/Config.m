/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
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
 * Config.m
 *
 * Configuration Manager for lookupd
 * Written by Marc Majka
 */

#import "Config.h"
#import "LUAgent.h"
#import "Thread.h"
#import "sys.h"
#import <NetInfo/dsutil.h>
#import <NetInfo/nilib2.h>
#import <string.h>
#import <stdlib.h>
#import <sys/types.h>
#import <sys/stat.h>
#import <sys/param.h>
#import <sys/dir.h>
#import <arpa/inet.h>

@implementation Config

- (char *)configDirNameForAgent:(char *)agent category:(LUCategory)cat
{
	char str[256];
	char catname[64];
	
	if (cat == LUCategoryNull) strcpy(catname, "Global");
	else 
	{
		sprintf(catname, "%s", [LUAgent categoryName:cat]);
		catname[0] -= 32;
	}

	if (agent == NULL)
	{
		sprintf(str, "%s Configuration", catname);
		return copyString(str);
	}
	
	if (cat == LUCategoryNull) sprintf(str, "%s Configuration", agent);
	else sprintf(str, "%s %s Configuration", agent, catname);
	return copyString(str);
}

- (void)initDefaults
{
	LUDictionary *d;

	d = [[LUDictionary alloc] init];
	[cdict addObject:d];
	[d setBanner:"Global Configuration"];
	[d setValue:"Global Configuration" forKey:"_config_name"];
	[d setValue:"default" forKey:"ConfigSource"];
	[d setValue:"Cache" forKey:"LookupOrder"];
	[d addValue:"NI" forKey:"LookupOrder"];
	[d addValue:"DS" forKey:"LookupOrder"];
	[d setValue:"64" forKey:"MaxThreads"];
	[d setValue:"2" forKey:"MaxIdleThreads"];
	[d setValue:"4" forKey:"MaxIdleServers"];
	[d setValue:"YES" forKey:"ValidateCache"];
	[d setValue:"15" forKey:"ValidationLatency"];
	[d setValue:"43200" forKey:"TimeToLive"];
	[d setValue:"30" forKey:"Timeout"];
	[d release];

	d = [[LUDictionary alloc] init];
	[cdict addObject:d];
	[d setBanner:"Host Configuration"];
	[d setValue:"Host Configuration" forKey:"_config_name"];
	[d setValue:"Cache" forKey:"LookupOrder"];
	[d addValue:"FF" forKey:"LookupOrder"];
	[d addValue:"DNS" forKey:"LookupOrder"];
	[d addValue:"NI" forKey:"LookupOrder"];
	[d addValue:"DS" forKey:"LookupOrder"];
	[d release];

	d = [[LUDictionary alloc] init];
	[cdict addObject:d];
	[d setBanner:"Service Configuration"];
	[d setValue:"Service Configuration" forKey:"_config_name"];
	[d setValue:"Cache" forKey:"LookupOrder"];
	[d addValue:"FF" forKey:"LookupOrder"];
	[d addValue:"NI" forKey:"LookupOrder"];
	[d addValue:"DS" forKey:"LookupOrder"];
	[d release];

	d = [[LUDictionary alloc] init];
	[cdict addObject:d];
	[d setBanner:"Protocol Configuration"];
	[d setValue:"Protocol Configuration" forKey:"_config_name"];
	[d setValue:"Cache" forKey:"LookupOrder"];
	[d addValue:"FF" forKey:"LookupOrder"];
	[d addValue:"NI" forKey:"LookupOrder"];
	[d addValue:"DS" forKey:"LookupOrder"];
	[d release];

	d = [[LUDictionary alloc] init];
	[cdict addObject:d];
	[d setBanner:"Rpc Configuration"];
	[d setValue:"Rpc Configuration" forKey:"_config_name"];
	[d setValue:"Cache" forKey:"LookupOrder"];
	[d addValue:"FF" forKey:"LookupOrder"];
	[d addValue:"NI" forKey:"LookupOrder"];
	[d addValue:"DS" forKey:"LookupOrder"];
	[d release];

	d = [[LUDictionary alloc] init];
	[cdict addObject:d];
	[d setBanner:"Group Configuration"];
	[d setValue:"Group Configuration" forKey:"_config_name"];
	[d setValue:"NO" forKey:"ValidateCache"];
	[d addValue:"60" forKey:"TimeToLive"];
	[d release];

	d = [[LUDictionary alloc] init];
	[cdict addObject:d];
	[d setBanner:"Initgroup Configuration"];
	[d setValue:"Initgroup Configuration" forKey:"_config_name"];
	[d setValue:"NO" forKey:"ValidateCache"];
	[d addValue:"300" forKey:"TimeToLive"];
	[d release];

	d = [[LUDictionary alloc] init];
	[cdict addObject:d];
	[d setBanner:"Network Configuration"];
	[d setValue:"Network Configuration" forKey:"_config_name"];
	[d setValue:"Cache" forKey:"LookupOrder"];
	[d addValue:"FF" forKey:"LookupOrder"];
	[d addValue:"DNS" forKey:"LookupOrder"];
	[d addValue:"NI" forKey:"LookupOrder"];
	[d addValue:"DS" forKey:"LookupOrder"];
	[d release];
}

- (void)dealloc
{
	[cdict release];
	if (sourcePath != NULL) free(sourcePath);
	if (sourceDomainName != NULL) free(sourceDomainName);
	
	[super dealloc];
}

- (void)readDict:(LUDictionary *)c fromNetInfo:(unsigned long)d
{
	ni_id dir;
	ni_proplist pl;
	ni_status status;
	ni_property *p;
	int i, len;

	dir.nii_object = d;
	NI_INIT(&pl);

	syslock_lock(rpcLock);
	status = sa_read(sourceDomain, &dir, &pl);
	syslock_unlock(rpcLock);

	if (status != NI_OK) return;

	len = pl.ni_proplist_len;

	for (i = 0; i < len; i++)
	{
		p = &(pl.ni_proplist_val[i]);
		[c setValues:p->nip_val.ni_namelist_val forKey:p->nip_name count:p->nip_val.ni_namelist_len];
	}

	ni_proplist_free(&pl);
}

- (LUDictionary *)configWithName:(char *)dname fromConfig:(LUArray *)c
{
	int i, len;
	LUDictionary *d;

	len = [c count];
	for (i = 0; i < len; i++)
	{
		d = [c objectAtIndex:i];
		if (streq(dname, [d banner])) return d;
	}

	return nil;
}

- (LUDictionary *)dictForAgent:(char *)agent category:(LUCategory)cat fromConfig:(LUArray *)c
{
	char *dname;
	LUDictionary *d;

	dname = [self configDirNameForAgent:agent category:cat];
	d = [self configWithName:dname fromConfig:c];
	if (d == nil)
	{
		d = [[LUDictionary alloc] init];
		[d setBanner:dname];
		[d setValue:dname forKey:"_config_name"];
		[c addObject:d];
		[d release];
	}
	freeString(dname);

	return d;
}

- (void)loadAgent:(char *)agent fromNetInfo:(unsigned long)d
{
	ni_id dir;
	ni_entrylist el;
	ni_status status;
	LUDictionary *c;
	char *name;
	int i;

	dir.nii_object = d;

	c = [self dictForAgent:agent category:LUCategoryNull fromConfig:cdict];
	[self readDict:c fromNetInfo:d];

	NI_INIT(&el);

	syslock_lock(rpcLock);
	status = sa_list(sourceDomain, &dir, "name", &el);
	syslock_unlock(rpcLock);

	if (status != NI_OK)
	{
		ni_shared_release(sourceDomain);
		sourceDomain = NULL;
		return;
	}

	for (i = 0; i < el.ni_entrylist_len; i++)
	{
		if (el.ni_entrylist_val[i].names == NULL) continue;
		if (el.ni_entrylist_val[i].names->ni_namelist_len == 0) continue;

		name = el.ni_entrylist_val[i].names->ni_namelist_val[0];
		c = nil;

		if (streq(name, "users"))
			c = [self dictForAgent:agent category:LUCategoryUser fromConfig:cdict];
		else if (streq(name, "groups"))
			c = [self dictForAgent:agent category:LUCategoryGroup fromConfig:cdict];
		else if (streq(name, "hosts"))
			c = [self dictForAgent:agent category:LUCategoryHost fromConfig:cdict];
		else if (streq(name, "networks"))
			c = [self dictForAgent:agent category:LUCategoryNetwork fromConfig:cdict];
		else if (streq(name, "services"))
			c = [self dictForAgent:agent category:LUCategoryService fromConfig:cdict];
		else if (streq(name, "protocols"))
			c = [self dictForAgent:agent category:LUCategoryProtocol fromConfig:cdict];
		else if (streq(name, "rpcs"))
			c = [self dictForAgent:agent category:LUCategoryRpc fromConfig:cdict];
		else if (streq(name, "mounts"))
			c = [self dictForAgent:agent category:LUCategoryMount fromConfig:cdict];
		else if (streq(name, "printers"))
			c = [self dictForAgent:agent category:LUCategoryPrinter fromConfig:cdict];
		else if (streq(name, "bootparams"))
			c = [self dictForAgent:agent category:LUCategoryBootparam fromConfig:cdict];
		else if (streq(name, "bootp"))
			c = [self dictForAgent:agent category:LUCategoryBootp fromConfig:cdict];
		else if (streq(name, "aliases"))
			c = [self dictForAgent:agent category:LUCategoryAlias fromConfig:cdict];
		else if (streq(name, "netgroups"))
			c = [self dictForAgent:agent category:LUCategoryNetgroup fromConfig:cdict];
		else if (streq(name, "initgroups"))
			c = [self dictForAgent:agent category:LUCategoryInitgroups fromConfig:cdict];

		if (c != nil) [self readDict:c fromNetInfo:el.ni_entrylist_val[i].id];
	}

	ni_entrylist_free(&el);
}

- (BOOL)loadFromNetInfo
{
	ni_status status;
	ni_id dir;
	struct sockaddr_in serveraddr;
	char *servertag, str[MAXPATHLEN + 1], *name, astr[16];
	LUDictionary *c;
	ni_entrylist el;
	int i;

	if (sourceDomain == NULL) return NO;
	if (sourcePath == NULL)
	{
		ni_shared_release(sourceDomain);
		sourceDomain = NULL;
		return NO;
	}

	/* Load global config from source dir */
	syslock_lock(rpcLock);
	status = sa_pathsearch(sourceDomain, &dir, sourcePath);
	syslock_unlock(rpcLock);

	if (status != NI_OK)
	{
		ni_shared_release(sourceDomain);
		sourceDomain = NULL;
		return NO;
	}

	[self loadAgent:NULL fromNetInfo:dir.nii_object];

	syslock_lock(rpcLock);
	status = sa_addrtag(sourceDomain, &serveraddr, &servertag);
	syslock_unlock(rpcLock);

	c = [self dictForAgent:NULL category:LUCategoryNull fromConfig:cdict];

	if (inet_ntop(AF_INET, &(serveraddr.sin_addr), astr, 16) != NULL)
	{
		sprintf(str, "netinfo://%s/%s:%s", astr, servertag, sourcePath);
		[c setValue:str forKey:"ConfigSource"];
	}

	free(servertag);

	sprintf(str, "%s/agents", sourcePath);

	syslock_lock(rpcLock);
	status = sa_pathsearch(sourceDomain, &dir, str);
	syslock_unlock(rpcLock);

	if (status != NI_OK)
	{
		ni_shared_release(sourceDomain);
		sourceDomain = NULL;
		if (status == NI_NODIR) return YES;
		return NO;
	}

	NI_INIT(&el);

	syslock_lock(rpcLock);
	status = sa_list(sourceDomain, &dir, "name", &el);
	syslock_unlock(rpcLock);

	if (status != NI_OK)
	{
		ni_shared_release(sourceDomain);
		sourceDomain = NULL;
		return YES;
	}

	for (i = 0; i < el.ni_entrylist_len; i++)
	{
		if (el.ni_entrylist_val[i].names == NULL) continue;
		if (el.ni_entrylist_val[i].names->ni_namelist_len == 0) continue;

		name = el.ni_entrylist_val[i].names->ni_namelist_val[0];
		[self loadAgent:name fromNetInfo:el.ni_entrylist_val[i].id];
	}

	ni_entrylist_free(&el);

	if (sourceDomain != NULL) ni_shared_release(sourceDomain);
	sourceDomain = NULL;

	return YES;
}

- (char *)getLineFromFile:(FILE *)fp
{
	static char s[1024];
	int len;

    s[0] = '\0';

    fgets(s, 1024, fp);
    if (s == NULL || s[0] == '\0') return NULL;

	if (s[0] == '#') return s;

	len = strlen(s) - 1;
	s[len] = '\0';

	return s;
}

- (char **)tokensFromLine:(const char *)data separator:(const char *)sep
	stopAtPound:(BOOL)trail
{
	char **tokens = NULL;
	const char *p;
	int i, j, len;
	char buf[4096];
	BOOL scanning;

	if (data == NULL) return NULL;
	if (sep == NULL)
	{
		tokens = appendString((char *)data, tokens);
		return tokens;
	}

	len = strlen(sep);

	p = data;

	while (p[0] != '\0')
	{
		/* skip leading white space */
		while ((p[0] == ' ') || (p[0] == '\t') || (p[0] == '\n')) p++;

		/* check for end of line */
		if ((trail == YES && p[0] == '#') || p[0] == '\0')
			break;

		/* copy data */
		i = 0;
		scanning = YES;
		for (j = 0; (j < len) && scanning; j++)
		{
			if (p[0] == sep[j] || (p[0] == '\0')) scanning = NO;
		}

		while (scanning)
		{
			buf[i++] = p[0];
			p++;
			for (j = 0; (j < len) && scanning; j++)
			{
				if (p[0] == sep[j] || (p[0] == '\0')) scanning = NO;
			}
		}
	
		/* back over trailing whitespace */
		i--;
		while ((buf[i] == ' ') || (buf[i] == '\t') || (buf[i] == '\n')) i--;
		buf[++i] = '\0';
	
		tokens = appendString(buf, tokens);

		/* check for end of line */
		if (p[0] == '\0') break;

		/* skip separator */
		scanning = YES;
		for (j = 0; (j < len) && scanning; j++)
		{
			if (p[0] == sep[j])
			{
				p++;
				scanning = NO;
			}
		}

		if ((!scanning) && p[0] == '\0')
		{
			/* line ended at a separator - add a null member */
			tokens = appendString("", tokens);
			return tokens;
		}
	}
	return tokens;
}

- (void)readDict:(LUDictionary *)dict fromFile:(FILE *)fp
{
	char *line;
	char **tokens;

	if (fp == NULL) return;
	if (dict == nil) return;

	while (NULL != (line = [self getLineFromFile:fp]))
	{
		if (line[0] == '#') continue;

		tokens = [self tokensFromLine:line separator:" \t" stopAtPound:NO];
		if (tokens == NULL) continue;

		[dict setValues:(tokens+1) forKey:tokens[0]];
		freeList(tokens);
		tokens = NULL;
	}
}

- (void)loadAgent:(char *)agent fromFile:(char *)path
{
	DIR *dir;
	struct direct *d;
	LUDictionary *c;
	char str[MAXPATHLEN + 1];
	FILE *f;

	dir = opendir(path);
	if (dir == NULL) return;

	for (d = readdir(dir); d != NULL; d = readdir(dir))
	{
		c = nil;

		if (streq(d->d_name, ".")) continue;
		else if (streq(d->d_name, "..")) continue;

		else if (streq(d->d_name, "global"))
			c = [self dictForAgent:agent category:LUCategoryNull fromConfig:cdict];
		else if (streq(d->d_name, "users"))
			c = [self dictForAgent:agent category:LUCategoryUser fromConfig:cdict];
		else if (streq(d->d_name, "groups"))
			c = [self dictForAgent:agent category:LUCategoryGroup fromConfig:cdict];
		else if (streq(d->d_name, "hosts"))
			c = [self dictForAgent:agent category:LUCategoryHost fromConfig:cdict];
		else if (streq(d->d_name, "networks"))
			c = [self dictForAgent:agent category:LUCategoryNetwork fromConfig:cdict];
		else if (streq(d->d_name, "services"))
			c = [self dictForAgent:agent category:LUCategoryService fromConfig:cdict];
		else if (streq(d->d_name, "protocols"))
			c = [self dictForAgent:agent category:LUCategoryProtocol fromConfig:cdict];
		else if (streq(d->d_name, "rpcs"))
			c = [self dictForAgent:agent category:LUCategoryRpc fromConfig:cdict];
		else if (streq(d->d_name, "mounts"))
			c = [self dictForAgent:agent category:LUCategoryMount fromConfig:cdict];
		else if (streq(d->d_name, "printers"))
			c = [self dictForAgent:agent category:LUCategoryPrinter fromConfig:cdict];
		else if (streq(d->d_name, "bootparams"))
			c = [self dictForAgent:agent category:LUCategoryBootparam fromConfig:cdict];
		else if (streq(d->d_name, "bootp"))
			c = [self dictForAgent:agent category:LUCategoryBootp fromConfig:cdict];
		else if (streq(d->d_name, "aliases"))
			c = [self dictForAgent:agent category:LUCategoryAlias fromConfig:cdict];
		else if (streq(d->d_name, "netgroups"))
			c = [self dictForAgent:agent category:LUCategoryNetgroup fromConfig:cdict];
		else if (streq(d->d_name, "initgroups"))
			c = [self dictForAgent:agent category:LUCategoryInitgroups fromConfig:cdict];

		if (c != nil)
		{
			sprintf(str, "%s/%s", path, d->d_name);
			f = fopen(str, "r");
			[self readDict:c fromFile:f];
			fclose(f);
		}
	}

	closedir(dir);
}

- (BOOL)loadFromFile
{
	DIR *dir;
	struct direct *d;
	char str[MAXPATHLEN + 1];
	LUDictionary *c;

	[self loadAgent:NULL fromFile:sourcePath];

	c = [self dictForAgent:NULL category:LUCategoryNull fromConfig:cdict];
	sprintf(str, "file:/%s", sourcePath);
	[c setValue:str forKey:"ConfigSource"];

	sprintf(str, "%s/agents", sourcePath);

	dir = opendir(str);
	if (dir == NULL) return YES;

	for (d = readdir(dir); d != NULL; d = readdir(dir))
	{
		if (streq(d->d_name, ".")) continue;
		if (streq(d->d_name, "..")) continue;

		sprintf(str, "%s/agents/%s", sourcePath, d->d_name);
		[self loadAgent:d->d_name fromFile:str];
	}

	closedir(dir);

	return YES;
}

/*
 * Read and cache a set of configuration dictionaries.
 */
- (BOOL)loadConfig
{
	if (source == configSourceDefault)
		return YES;
	else if (source == configSourceFile)
		return [self loadFromFile];
	else if (source == configSourceNetInfo)
		return [self loadFromNetInfo];

	return NO;
}

/*
 * Finds a source for configuration info.
 *
 * If there is a directory named "/etc/lookupd", use that
 * else if NetInfo has a directory named "/config/lookupd", use that
 * else if NetInfo has a directory named "/locations/lookupd", use that
 * else use defaults.
 */
- (void)selectConfigSource
{
	ni_id nid;
	struct stat st;
	ni_status status;

	source = configSourceDefault;
	if (sourcePath != NULL) freeString(sourcePath);
	sourcePath = NULL;
	if (sourceDomain != NULL) ni_shared_release(sourceDomain);
	sourceDomain = NULL;

	/* Check file:/etc/lookupd */
	if (stat("/etc/lookupd", &st) == 0)
	{
		if (st.st_mode & S_IFDIR)
		{
			source = configSourceFile;
			sourcePath = copyString("/etc/lookupd");
			return;
		}
	}

	/* Check netinfo:/config/lookupd */
	syslock_lock(rpcLock);
	status = sa_find(&sourceDomain, &nid, "/config/lookupd", 30);
	syslock_unlock(rpcLock);
	if (status == NI_OK)
	{
		source = configSourceNetInfo;
		sourcePath = copyString("/config/lookupd");
		return;
	}

	/* Check netinfo:/locations/lookupd */
	syslock_lock(rpcLock);
	status = sa_find(&sourceDomain, &nid, "/locations/lookupd", 30);
	syslock_unlock(rpcLock);
	if (status == NI_OK)
	{
		source = configSourceNetInfo;
		sourcePath = copyString("/locations/lookupd");
		return;
	}
}

- (BOOL)setConfigSource:(int)src path:(char *)path domain:(char *)domain
{
	if (didSetConfig) return NO;

	didSetConfig = YES;
	
	if (initsource == configSourceAutomatic) initsource = src;

	if (sourcePath != NULL) freeString(sourcePath);
	sourcePath = copyString(path);

	if (sourceDomainName != NULL) freeString(sourceDomainName);
	sourceDomainName = copyString(domain);

	if (src == configSourceDefault) return YES;

	if (src == configSourceAutomatic)
	{
		[self selectConfigSource];
	}
	else
	{
		source = src;

		if (domain != NULL)
		{
			syslock_lock(rpcLock);
			sourceDomain = ni_shared_open(NULL, domain);
			syslock_unlock(rpcLock);
		}
	}

	return [self loadConfig];
}

- (LUArray *)config
{
	return [cdict retain];
}

- (LUDictionary *)configForAgent:(char *)agent category:(LUCategory)cat fromConfig:(LUArray *)c
{
	char *dname;
	LUDictionary *d;

	dname = [self configDirNameForAgent:agent category:cat];
	d = [self configWithName:dname fromConfig:c];
	freeString(dname);

	return d;
}

- (LUDictionary *)configGlobal:(LUArray *)c
{
	return [self configForAgent:NULL category:LUCategoryNull fromConfig:c];
}

- (LUDictionary *)configForCategory:(LUCategory)cat fromConfig:(LUArray *)c
{
	return [self configForAgent:NULL category:cat fromConfig:c];
}

- (LUDictionary *)configForAgent:(char *)agent fromConfig:(LUArray *)c
{
	return [self configForAgent:agent category:LUCategoryNull fromConfig:c];
}

- (int)intForKey:(char *)key dict:(LUDictionary *)dict default:(int)def
{
	char *s;
	int n, i;

	if (dict == nil) return def;
	s = [dict valueForKey:key];
	if (s == NULL) return def;
	n = sscanf(s, "%d", &i);
	if (n <= 0) return def;
	return i;
}

- (char *)stringForKey:(char *)key dict:(LUDictionary *)dict default:(char *)def
{
	char *s, t[256];
	int n;

	if (dict == nil) return copyString(def);

	s = [dict valueForKey:key];
	if (s == NULL) return copyString(def);

	n = sscanf(s, "%s", t);
	if (n <= 0) return copyString(def);

	return copyString(s);
}

- (BOOL)boolForKey:(char *)key dict:(LUDictionary *)dict default:(BOOL)def
{
	char *s;

	if (dict == nil) return def;
	s = [dict valueForKey:key];
	if (s == NULL) return def;
	if (s[0] == 'Y') return YES;
	if (s[0] == 'y') return YES;
	if (s[0] == 'T') return YES;
	if (s[0] == 't') return YES;
	if (streq(s, "1")) return YES;
	if (s[0] == 'N') return NO;
	if (s[0] == 'n') return NO;
	if (s[0] == 'F') return NO;
	if (s[0] == 'f') return NO;
	if (streq(s, "0")) return NO;

	return def;
}

- (id)init
{
	[super init];

	cdict = [[LUArray alloc] init];
	[cdict setBanner:"Configuration"];

	[self initDefaults];

	didSetConfig = NO;

	initsource = configSourceAutomatic;
	source = configSourceAutomatic;
	sourcePath = NULL;
	sourceDomainName = NULL;
	sourceDomain = NULL;

	return self;
}

@end
