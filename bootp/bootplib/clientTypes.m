/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * clientTypes.m
 * - keep track of client types
 */

/*
 * Modification History:
 * 
 * January 19, 1998	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import <objc/List.h>
#import <unistd.h>
#import <stdlib.h>
#import <string.h>

#import "clientTypes.h"
#import <stdio.h>

@interface uniqNamesList : Object
{
    id			list;
}
- init;
- free;
- (int) addName:(const u_char *)name;
- (boolean_t) findName:(const u_char *)name Index:(int *)where;
@end

@implementation uniqNamesList
- (void) print
{
    int i;

    printf("{ ");
    for (i = 0; i < [list count]; i++) {
	u_char * obj = (u_char *)[list objectAt:i];
	printf("%s%s", (i == 0) ? "" : ", ", obj);
    }
    printf(" }\n");
}

- init
{
    [super init];

    list = [[List alloc] init];
    if (list == nil)
	return (nil);
    return (self);
}

- free
{
    int i;

    for (i = 0; i < [list count]; i++) {
	u_char * name = (u_char *)[list objectAt:i];
	free(name);
    }
    [list free];
    list = nil;
    return [super free];
}

- (boolean_t) findName:(const u_char *)name Index:(int *)where
{
    int 	i;

    for (i = 0; i < [list count]; i++) {
	const u_char * 	nm = (const u_char *)[list objectAt:i];

	if (strcmp(name, nm) == 0) {
	    *where = i;
	    return (TRUE);
	}
    }
    return (FALSE);
}

- (int) addName:(const u_char *)name
{
    int where;

    if ([self findName:name Index:&where] == FALSE) {
	where = [list count];
	[list addObject:(id)strdup(name)];
    }
    return (where);
}
@end


@implementation clientTypes

- init
{
    [super init];

    any = TRUE;
    types = nil;
    return self;
}

- free
{
    if (types != nil)
	[types free];
    types = nil;
    return [super free];
}

- addTypes:(const u_char * *) list Count:(int)count
{
    int i;

    if (count == 0)
	return self;

    any = FALSE;
    if (types == nil)
	types = [[uniqNamesList alloc] init];

    for (i = 0; i < count; i++) {
	if (strcmp(list[i], CLIENT_TYPE_ANY) == 0) {
	    any = TRUE;
	    [types free];
	    types = nil;
	    return self;
	}
	[types addName:list[i]];
    }
    return (self);
}

- (boolean_t) includesType:(const u_char *)name
{
    int index;

    if (any || types == nil
	|| [types findName:name Index:&index])
	return (TRUE);

    return (FALSE);
}

- (void) print
{
    if (any)
	printf("any\n");
    else
	[types print];
}
@end

#ifdef TESTING
int
main()
{

    int i;
    {
	u_char * list[] = {"dhcp", "bootp", "macNC", CLIENT_TYPE_ANY, "silly"};
	id client;
	
	client = [[[clientTypes alloc] init] addTypes:list Count:3];
	if (client == nil) {
	    printf("clientTypes init failed\n");
	}
	else {
	    int i;

	    for (i = 0; i < 5; i++) {
		printf("client serves %s is %s\n", list[i], 
		       [client includesType:list[i]] ? "TRUE" : "FALSE");
	    }
	    [client free];
	}
    }

    exit(0);
}
#endif TESTING
