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
#import "AMMap.h"
#import "Controller.h"
#import "AMVnode.h"
#import "Server.h"
#import "AMString.h"
#import "automount.h"
#import "log.h"
#import <syslog.h>
#import <string.h>
#import <stdio.h>
#import <stdlib.h>

extern int innetgr(char *, char *, char *, char *);

@implementation Map

- (Map *)initWithParent:(Vnode *)p directory:(String *)dir
{
	[super init];

	mountPoint = [controller mountDirectory];
	if (mountPoint != nil) [mountPoint retain];

        name = [String uniqueString:"-null"];

	root = [[Vnode alloc] init];
	[root setMap:self];
	[root setName:dir];
	if (p != nil) [p addChild:root];
	[controller registerVnode:root];
	
	return self;
}

- (void)setName:(String *)n
{
	if (name != nil) [name release];
	name = n;
	[name retain];
}

- (String *)name
{
	return name;
}

- (void)dealloc
{
	if (mountPoint != nil) [mountPoint release];
	if (name != nil) [name release];
	[super dealloc];
}

- (unsigned int)didAutoMount
{
	return 0;
}

- (Vnode *)root
{
	return root;
}

- (String *)mountPoint
{
	return mountPoint;
}

- (int)mountArgs
{
	return 0;
}

- (unsigned int)mount:(Vnode *)v
{
    return [self mount:v withUid:99];
}

- (unsigned int)mount:(Vnode *)v withUid:(int)uid
{
	unsigned int i, len, status, substatus, fail;
	Array *kids;

	sys_msg(debug_mount, LOG_DEBUG, "Mount triggered at %s", [[v path] value]);
        status = [controller nfsmount:v withUid:uid];
	if (status != 0)
	{
		sys_msg(debug_mount, LOG_DEBUG, "Mount %s status %d",
			[[v path] value], status);
		return status;
	}

	sys_msg(debug_mount, LOG_DEBUG, "Mount %s status NFS_OK",
		[[v path] value]);

	fail = 0;
	kids = [v children];
	len = 0;
	if (kids != nil) len = [kids count];
	for (i = 0; i < len; i++)
	{
            substatus = [self mount:[kids objectAtIndex:i] withUid:uid];
		if (substatus != 0) fail++;
	}

	if ((len != 0) && (fail == len) && ([v source] == nil))
	{
		[v setMounted:NO];
		status = 0;
	}

	return status;
}

- (unsigned int)unmount:(Vnode *)v
{
	int i, len;
	Array *kids;
	Vnode *k;
	unsigned int status;
	struct timeval tv;

	if (v == nil) return 0;

	if (([v mounted]) && ([v server] != nil) && (![[v server] isLocalHost]))
	{
		if ([v mountTimeToLive] == 0) return 1;

		gettimeofday(&tv, NULL);
		if (tv.tv_sec < ([v mountTime] + [v mountTimeToLive])) return 1;

		sys_msg(debug, LOG_DEBUG, "Checking %s %s %s",
			[[v path] value], [[[v server] name] value], [[v link] value]);

		
		kids = [v children];
		len = 0;
		if (kids != nil) len = [kids count];

		for (i = 0; i < len; i++)
		{
			k = [kids objectAtIndex:i];
			status = [self unmount:k];
			if (status != 0)
			{
				[self mount:v];
				return 1;
			}
		}

		status = [controller attemptUnmount:v];
		if (status != 0) 
		{
			[self mount:v];
			return 1;
		}
		return 0;
	}

	kids = [v children];
	len = 0;
	if (kids != nil) len = [kids count];

	for (i = 0; i < len; i++)
	{
		k = [kids objectAtIndex:i];
		[self unmount:k];
	}

	return 0;
}

- (void)reInit
{
}

- (void)timeout
{
	[self unmount:root];
}

- (Vnode *)createVnodePath:(String *)path from:(Vnode *)v
{
	int i, p;
	Vnode *n, *x;
	char *s, t[1024];
	String *part;

	if (path == nil) return v;
	if ([path length] == 0) return v;

	p = 0;
	s = [path value];

	n = v;
	while (s != NULL)
	{
		if (s[0] == '/')
		{
			p++;
			s++;
		}
		for (i = 0; (s[i] != '/') && (s[i] != '\0'); i++) t[i] = s[i];
		t[i] = '\0';
		if (i == 0)
		{
			s = [path scan:'/' pos:&p];
			continue;
		}

		part = [String uniqueString:t];

		x = [n lookup:part];
		if (x == nil)
		{
			x = [[Vnode alloc] init];
			[x setMap:self];
			[controller registerVnode:x];
			[x setName:part];
			[n addChild:x];
		}
		n = x;

		[part release];
		s = [path scan:'/' pos:&p];
	}

	return n;
}

- (BOOL)checkVnodePath:(String *)path from:(Vnode *)v
{
	int i, p;
	Vnode *n, *x;
	char *s, t[1024];
	String *part;

	if (path == nil) return YES;
	if ([path length] == 0) return YES;

	p = 0;
	s = [path value];

	n = v;
	while (s != NULL)
	{
		if (s[0] == '/')
		{
			p++;
			s++;
		}
		for (i = 0; (s[i] != '/') && (s[i] != '\0'); i++) t[i] = s[i];
		t[i] = '\0';
		if (i == 0)
		{
			s = [path scan:'/' pos:&p];
			continue;
		}

		part = [String uniqueString:t];

		x = [n lookup:part];
		[part release];
		if (x == nil) return NO;

		n = x;
		s = [path scan:'/' pos:&p];
	}

	return YES;
}

- (Vnode *)mkdir:(String *)s attributes:(void *)x atVnode:(Vnode *)v
{
	Vnode *n;
	struct fattr f;
	sattr *a;

	n = [v lookup:s];
	if (n != nil) return n;

	n = [[v map] createVnodePath:s from:v];
	if (n == nil) return nil;

	a = (sattr *)x;
	f = [n attributes];
	f.mode = a->mode | NFSMODE_DIR;
	f.uid = a->uid;
	f.gid = a->gid;
	f.size = a->size;
	f.atime = a->atime;
	f.mtime = a->mtime;

	[n setAttributes:f];
	return n;
}

- (Vnode *)symlink:(String *)l name:(String *)s atVnode:(Vnode *)v
{
	Vnode *n;

	n = [v lookup:s];
	if (n != nil) return n;

	n = [[v map] createVnodePath:s from:v];
	if (n == nil) return nil;

	[n setLink:l];
	[n setType:NFLNK];
	[n setMode:00777 | NFSMODE_LNK];
	[n setMounted:YES];
	[n setFakeMount:YES];

	return n;
}

- (BOOL)testOptEqualKey:(char *)k val:(char *)v
{
	char str[1024], *p;
	int len, off;
	String *x;
	BOOL yn;

	if (!strcmp(k, "host"))
	{
		if (!strcmp(v, [[controller hostName] value])) return YES;
		if ([controller hostDNSDomain] == nil) return NO;
		sprintf(str, "%s.%s", [[controller hostName] value],
			[[controller hostDNSDomain] value]);
		if (!strcmp(v, str)) return YES;
		return NO;
	}
	else if (!strcmp(k, "netgroup"))
	{
		if (innetgr(v, [[controller hostName] value], NULL, NULL)) return YES;
		return NO;
	}
	else if (!strcmp(k, "network"))
	{
		/* value is <address>[/<bits>] */
		x = [String uniqueString:v];

		p = strchr(v, '/');
		if (p == NULL) yn = [Server isMyAddress:x];
		else yn = [Server isMyNetwork:x];
		[x release];
		return yn;
	}
	else if (!strcmp(k, "domain"))
	{
		if ([controller hostDNSDomain] == nil) return NO;

		strcpy(str, [[controller hostDNSDomain] value]);
		if (!strcmp(v, str)) return YES;

		len = strlen(v);
		if (len >= [[controller hostDNSDomain] length]) return NO;
		off = [[controller hostDNSDomain] length] - len;
		while (v[0] == '.' && str[off] == '.')
		{
			v++;
			off++;
		}
		if (str[off - 1] != '.') return NO;
		if (!strcmp(v, str + off)) return YES;

		return NO;
	}
	else if (!strcmp(k, "os"))
	{
		if (!strcmp(v, [[controller hostOS] value])) return YES;
		return NO;
	}
	else if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		if (!strcmp(str, [[controller hostOSVersion] value])) return YES;

		p = strchr(str, '.');
		if (p == NULL)
		{
			if (atoi(str) == [controller hostOSVersionMajor]) return YES;
		}
		else
		{
			*p = '\0';
			p++;
			if (atoi(str) != [controller hostOSVersionMajor]) return NO;
			if (atoi(p) == [controller hostOSVersionMinor]) return YES;
		}

		return NO;
	}
	else if (!strcmp(k, "arch"))
	{
		if (!strcmp(v, [[controller hostArchitecture] value])) return YES;
		return NO;
	}
        else if (!strcmp(k, "byte"))
        {
                if (!strcmp(v, [[controller hostByteOrder] value])) return YES;
                return NO;
        }
        else if (!strcmp(k, "url"))
        {
            return YES;
        }

	/* unknown key - refuse it */
	return NO;
}

- (BOOL)testOptNotEqualKey:(char *)k val:(char *)v
{
	char str[1024], *p;
	int len, off;
	String *x;
	BOOL yn;
	if (!strcmp(k, "host"))
	{
		if (!strcmp(v, [[controller hostName] value])) return NO;
		if ([controller hostDNSDomain] == nil) return YES;
		sprintf(str, "%s.%s", [[controller hostName] value],
			[[controller hostDNSDomain] value]);
		if (!strcmp(v, str)) return NO;
		return YES;
	}
	else if (!strcmp(k, "netgroup"))
	{
		if (innetgr(v, [[controller hostName] value], NULL, NULL)) return NO;
		return YES;
	}
	else if (!strcmp(k, "network"))
	{
		/* value is <address>[/<bits>] */
		x = [String uniqueString:v];

		p = strchr(v, '/');
		if (p == NULL) yn = [Server isMyAddress:x];
		else yn = [Server isMyNetwork:x];
		[x release];
		return (!yn);
	}
	else if (!strcmp(k, "domain"))
	{
		if ([controller hostDNSDomain] == nil) return YES;

		strcpy(str, [[controller hostDNSDomain] value]);
		if (!strcmp(v, str)) return NO;

		len = strlen(v);
		if (len >= [[controller hostDNSDomain] length]) return YES;
		off = [[controller hostDNSDomain] length] - len;
		while (v[0] == '.' && str[off] == '.')
		{
			v++;
			off++;
		}
		if (str[off - 1] != '.') return YES;
		if (!strcmp(v, str + off)) return NO;

		return YES;
	}
	else if (!strcmp(k, "os"))
	{
		if (!strcmp(v, [[controller hostOS] value])) return NO;
		return YES;
	}
	else if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		if (!strcmp(str, [[controller hostOSVersion] value])) return NO;

		p = strchr(str, '.');
		if (p == NULL)
		{
			if (atoi(str) != [controller hostOSVersionMajor]) return YES;
		}
		else
		{
			*p = '\0';
			p++;
			if (atoi(str) != [controller hostOSVersionMajor]) return YES;
			if (atoi(p) == [controller hostOSVersionMinor]) return NO;
		}

		return YES;
	}
	else if (!strcmp(k, "arch"))
	{
		if (!strcmp(v, [[controller hostArchitecture] value])) return NO;
		return YES;
	}
	else if (!strcmp(k, "byte"))
	{
		if (!strcmp(v, [[controller hostByteOrder] value])) return NO;
		return YES;
	}

	/* unknown key - refuse it */
	return NO;
}

- (BOOL)testOptGreaterKey:(char *)k val:(char *)v
{
	int vmajor, vminor;
	char str[256], *p;

	if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		vminor = 0;
		p = strchr(str, '.');
		if (p == NULL)
		{
			vmajor = atoi(str);
			if ([controller hostOSVersionMajor] > vmajor) return YES;
			return NO;
		}

		*p = '\0';
		p++;
		vmajor = atoi(str);
		vminor = atoi(p);

		if ([controller hostOSVersionMajor] > vmajor) return YES;
		if ([controller hostOSVersionMajor] < vmajor) return NO;

		if ([controller hostOSVersionMinor] > vminor) return YES;
		return NO;
	}

	return NO;
}

- (BOOL)testOptGreaterEqualKey:(char *)k val:(char *)v
{
	int vmajor, vminor;
	char str[256], *p;

	if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		p = strchr(str, '.');
		if (p == NULL)
		{
			vmajor = atoi(str);
			if ([controller hostOSVersionMajor] >= vmajor) return YES;
			return NO;
		}

		*p = '\0';
		p++;
		vmajor = atoi(str);
		vminor = atoi(p);

		if ([controller hostOSVersionMajor] > vmajor) return YES;
		if ([controller hostOSVersionMajor] < vmajor) return NO;

		if ([controller hostOSVersionMinor] >= vminor) return YES;
		return NO;
	}

	return NO;
}

- (BOOL)testOptLessKey:(char *)k val:(char *)v
{
	int vmajor, vminor;
	char str[256], *p;

	if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		p = strchr(str, '.');
		if (p == NULL)
		{
			vmajor = atoi(str);
			if ([controller hostOSVersionMajor] < vmajor) return YES;
			return NO;
		}

		*p = '\0';
		p++;
		vmajor = atoi(str);
		vminor = atoi(p);

		if ([controller hostOSVersionMajor] < vmajor) return YES;
		if ([controller hostOSVersionMajor] > vmajor) return NO;

		if ([controller hostOSVersionMinor] < vminor) return YES;
		return NO;
	}

	return NO;
}

- (BOOL)testOptLessEqualKey:(char *)k val:(char *)v
{
	int vmajor, vminor;
	char str[256], *p;

	if (!strcmp(k, "osvers"))
	{
		strcpy(str, v);

		p = strchr(str, '.');
		if (p == NULL)
		{
			vmajor = atoi(str);
			if ([controller hostOSVersionMajor] <= vmajor) return YES;
			return NO;
		}

		*p = '\0';
		p++;
		vmajor = atoi(str);
		vminor = atoi(p);

		if ([controller hostOSVersionMajor] < vmajor) return YES;
		if ([controller hostOSVersionMajor] > vmajor) return NO;

		if ([controller hostOSVersionMinor] <= vminor) return YES;
		return NO;
	}

	return NO;
}

- (BOOL)acceptOptions:(Array *)opts
{
	String *o;
	unsigned int i, len, n;
	char tkey[256], tval[256];
	BOOL status;

	if (opts == nil) return YES;

	len = [opts count];
	if (len == 0) return YES;

	for (i = 0; i < len; i++)
	{
		o = [opts objectAtIndex:i];

		n = sscanf([o value], "%[^=]==%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptEqualKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s == %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}

		n = sscanf([o value], "%[^!]!=%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptNotEqualKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s != %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}

		n = sscanf([o value], "%[^>]>=%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptGreaterEqualKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s >= %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}

		n = sscanf([o value], "%[^>]>%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptGreaterKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s > %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}

		n = sscanf([o value], "%[^<]<=%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptLessEqualKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s <= %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}

		n = sscanf([o value], "%[^<]<%s", tkey, tval);
		if (n == 2) 
		{
			status = [self testOptLessKey:tkey val:tval];
			sys_msg(debug_options, LOG_DEBUG, "Test %s < %s  %s",
				tkey, tval, status ? "YES" : "NO");
			return status;
		}
	}

	return YES;
}

@end
