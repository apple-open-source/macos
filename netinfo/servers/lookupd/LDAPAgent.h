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
 * LDAPAgent.h
 * Header for LDAP lookupd agent
 * Copyright (C) 1997 Luke Howard. All rights reserved.
 * Luke Howard, March 1997.
 */

#import "LUDictionary.h"
#import "LUAgent.h"
#import <NetInfo/syslock.h>

#import <lber.h>
#import <ldap.h>

#ifdef DEBUG
#define logDebug(msg) [lookupLog syslogDebug:msg]
#else
#define logDebug(msg)
#define NDEBUG
#endif DEBUG     

#ifdef OIDTABLE
typedef id oid_name_t;
#else
typedef char *oid_name_t;
#endif OIDTABLE

#define NameForKey(o)				(o)

@class FFParser;

/* keys in /locations/lookupd/agents/LDAPAgent. These are *all* optional. */
#define CONFIG_KEY_LDAPHOST	"host"
#define CONFIG_KEY_BASEDN	"suffix"
#define CONFIG_KEY_BINDDN	"binddn"
#define CONFIG_KEY_BINDPW	"bindpw"
#define CONFIG_KEY_TIMELIMIT	"timelimit"
#define CONFIG_KEY_TIMEOUT	"Timeout"
#define CONFIG_KEY_SCOPE	"scope"
#define CONFIG_KEY_DEREF	"deref"
#define CONFIG_KEY_PORT		"port"
#define CONFIG_KEY_LATENCY	"ValidationLatency"

/* rebinding timeouts  */
#define LDAP_SLEEPTIME 4          /* 4 second sleeptime, in case of errors */
#define LDAP_MAXSLEEPTIME 64      /* 64 second max sleep time */
#define LDAP_MAXCONNTRIES 2       /* Try to form a connection twice before sleeping */
#define LDAP_DEFAULT_LATENCY	15	/* 15 seconds validation latency. */

@interface LDAPAgent : LUAgent
{
	LUDictionary *stats;
	LUDictionary *configuration;
	FFParser *parser;
	syslock *threadLock;

	time_t validationLatency;

	int port;
	int scope;
	int timelimit;
	int deref;
	struct timeval timeout;
	
	char *defaultBase;
	
	char *bindName;
	char *bindCredentials;

	LDAP *ld;
	
	int rebindTries;
	int sleepTime;
	
	char **nisAttributes[NCATEGORIES];
	char *nisClasses[NCATEGORIES];
	char *searchBases[NCATEGORIES];
	
#ifdef OIDTABLE
	LUDictionary *schema;
#endif
}

+ (oid_name_t)oidNameForKey:(char *)key category:(LUCategory)cat;

/*
 * Search local and global configuration for validation
 * latency.
 */
- (void)initValidationLatency;

/*
 * Search agent configuration for default and per-category
 * search bases.
 */
- (void)initSearchBases;

/*
 * Reload configuration.
 */
- reInit;

/*
 * Accessor for configuration dictionary, for use by
 * rebind procedure.
 */
- (LUDictionary *)configuration;

/*
 * Accessor for the LDAP session.
 */
- (LDAP *)session;

/*
 * Rebind to a server after a failed connection.
 */
- (void)rebind;

/*
 * Open the connection and bind to a LDAP server
 */
- (int)openConnection;

/*
 * Close and free the LDAP sesesion.
 */
- (void)closeConnection;

/*
 * Parse the configuration.
 */
- (BOOL)getConfiguration;

/*
 * Lookup the search base for a category.
 */
- (char *)searchBaseForCategory:(LUCategory)cat;

/*
 * Lookup an item with a set of attribute value
 * assertions.
 */
- (LUDictionary *)itemWithAttributes:(oid_name_t *)aKey
	values:(char **)aVal
	category:(LUCategory)cat;

/*
 * Lookup an item with a single attribute value
 * assertion.
 */
- (LUDictionary *)itemWithAttribute:(oid_name_t)aKey
	value:(char *)aVal
	category:(LUCategory)cat;

/* 
 * Lookup all items in a category.
 */
- (LUArray *)allItemsWithCategory:(LUCategory)cat;

/*
 * Low level LDAP server search method.
 */
- (LDAPMessage *)search:(char *)base
			filter:(char *)filter
			attributes:(char **)attrs
			sizelimit:(int)sizelimit;

/*
 * Lookup the modify timestamp attribute for an
 * entry. Used for cache validation.
 */
- (time_t)currentModifyTimestampForEntry:(LUDictionary *)entry;

/*
 * Return a filter for objectclass=clazz. Class names
 * are prebound.
 */
- (char *)filterWithClass:(char *)clazz;

/*
 * Return a search filter for a class name (prebound)
 * and a set of attribute value assertions.
 */
- (char *)filterWithClass:(char *)clazz
	attributes:(oid_name_t *)attributes
	values:(char **)values;

- (void)lock;

- (void)unlock;

- (int)config:(LUDictionary *)configuration scope:(char *)key default:(int)def;
- (int)config:(LUDictionary *)configuration deref:(char *)key default:(int)def;

- (LUDictionary *)getDNSConfiguration;
- (char *)dnsDomainToDn:(char *)domain;

@end
