/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */

#import "libsaio.h"
#import "kernBootStruct.h"
#import "stringConstants.h"
#import <driverkit/configTablePrivate.h>

extern KERNBOOTSTRUCT *kernBootStruct;
extern char *Language;
extern char *LoadableFamilies;


static char_ret
getachar(
    char	**string_p
)
{
    register char *str = *string_p;
    register int c;
    char_ret	r;
    
    c = *str++;
    if (c == '\\') {
	r.quoted = YES;
	c = *str++;
	switch(c) {
	case 'n':
	    c = '\n';
	    break;
	case 'r':
	    c = '\r';
	    break;
	case 't':
	    c = '\t';
	    break;
	default:
	    break;
	}
    } else {
	r.quoted = NO;
    }
    *string_p = str;
    r.c = c;
    return r;
}

/*
 * A token is:
 * <non_space_non_semicolon>*
 * "<non_quote>*"
 * <semicolon>
 */
char *
get_token(
    char	**string_p
)
{
    char *begin;
    char *newstr;
    char_ret r;
    int len;
    
    do {
	r = getachar(string_p);
    } while (r.c && isspace(r.c));

    if (!r.quoted && r.c == '\"') {
	begin = *string_p;
	do {
	    r = getachar(string_p);
	} while (r.c && !r.quoted && r.c != '\"');
    } else {
	begin = *string_p - 1;
	do {
	    r = getachar(string_p);
	} while (r.c && !r.quoted && r.c != ';' && !isspace(r.c));
    }
    len = *string_p - begin - 1;
    newstr = (char *)malloc(len + 1);
    strncpy(newstr, begin, len);
    newstr[len] = '\0';
    return newstr;
}


char *
stringFromList(
    char **list,
    int *size
)
{
    char *begin = *list, *end;
    char *newstr;
    int newsize = *size;
    
    while (*begin && newsize && isspace(*begin)) {
	begin++;
	newsize--;
    }
    end = begin;
    while (*end && newsize && !isspace(*end)) {
	end++;
	newsize--;
    }
    if (begin == end)
	return 0;
    newstr = malloc(end - begin + 1);
    strncpy(newstr, begin, end - begin);
    *list = end;
    *size = newsize;
    return newstr;
}

char *
valueForStringTableKey(
    char *table,
    char *key
)
{
    char *token;
    enum {
	KEY,
	EQUALS,
	VALUE,
	SEMICOLON,
	BEGINCOMMENT,
	ENDCOMMENT
    } state;
    BOOL foundKey;
    int len;
    
    state = KEY;
    foundKey = NO;
    while (*table) {
	token = get_token(&table);
	switch(state) {
	case KEY:
	    if (strcmp(token, key) == 0)
		foundKey = YES;
	    if (strncmp(token, "/*", 2) == 0)
		state = ENDCOMMENT;
	    else
		state = EQUALS;
	    break;
	case EQUALS:
	    if (strcmp(token, "=") == 0) {
		state = VALUE;
	    }
	    break;
	case VALUE:
	    if (foundKey) {
		return token;
	    }
	    state = SEMICOLON;
	    break;
	case SEMICOLON:
	    if (strcmp(token, ";") == 0) {
		state = KEY;
	    }
	    break;
	case ENDCOMMENT:
	    len = strlen(token);
	    if (len >= 2 && strncmp(token + len - 2, "*/", 2) == 0)
		state = KEY;
	    break;
	}
	free(token);
    }
    return 0;
}

char *
valueForBootKey(
    char *line,
    char *match
)
{
    char *token;
    enum {
	KEY,
	EQUALS,
	VALUE,
	WHITESPACE
    } state;
    BOOL foundKey;
    int len;

    state = KEY;
    while (*line) {
	token = get_token(&line);
    }
}

BOOL
boolForKey(
    char *key
)
{
    char *str = valueForKey(key);
    BOOL ret;
    
    if (str && (str[0] == 'Y' || str[1] == 'y'))
	ret = YES;
    else
	ret = NO;
    free(str);
    return ret;
}

BOOL
getIntForKey(
    char *key,
    int *value
)
{
    char *str = valueForKey(key), *ptr = str;
    int sum;
    BOOL ret;
    
    if (str) {
	for (sum = 0; size > 0; size--) {
	    sum = (sum * 10) + (*ptr++ - '0');
	}
	*value = sum;
	ret = YES;
    } else {
	ret = NO;
    }
    free(str);
    return ret;
}

char *
valueForKey(
    char *key
)
{
    char *str = valueForBootKey(kernBootStruct->bootString, key);;
    
    if (str)
	return str;
    else
	return valueForStringTableKey(kernBootStruct->config, key);
}

#define	LOCALIZABLE_PATH \
	"%s/%s.config/%s.lproj/%s.strings"
char *
loadLocalizableStrings(
    char *name,
    char *tableName
)
{
    char buf[256], *config;
    register int i, count, fd = -1;
    char * paths[] = {
	ARCH_DEVICES,
	USR_DEVICES,
	"/",
	NULL
    }, **path;
    
    for (i=0; i<2; i++) {
	for (path = paths; *path; path++) {
	    sprintf(buf, LOCALIZABLE_PATH, *path, name,
		   (i == 0) ? Language : "English", tableName);
	    if ((fd = open(buf, 0)) >= 0) {
		i = 2;
		break;
	    }
	}
    }
    if (fd < 0)
	return 0;
    count = file_size(fd);
    config = malloc(count);
    count = read(fd, config, count);
    close(fd);
    if (count <= 0) {
	free(config);
	return 0;
    }
    return config;
}

char *
bundleLongName(
    char *bundleName,
    char *tableName
)
{
    char *table, *name, *val;
    int size;
    
    table = loadLocalizableStrings(bundleName,
	tableName ? tableName : "Localizable");
    if ( table != 0 &&
	 getValueForStringTableKey(table,"Long Name", &val, &size) == YES) {
	name = malloc(size+1);
	strncpy(name, val, size);
	free(table);
    } else {
	name = newString(bundleName);
    }
    return name;
}

int sysConfigValid;

void
addConfig(
    char *config
)
{
    char *configPtr = kernBootStruct->configEnd;
    int len = strlen(config);
    
    if ((configPtr - kernBootStruct->config) > CONFIG_SIZE) {
	error("No room in memory for config files\n");
	return;
    }
    strcpy(configPtr, config);
    configPtr += (len + 1);
    *configPtr = 0;
    kernBootStruct->configEnd = configPtr;
}

/*
 * Returns 0 if file loaded OK,
 *        -1 if file was not loaded
 * Does not print error messages.
 * Returns pointer to table in memory in *table.
 */
int
loadConfigFile( char *configFile, char **table, BOOL allocTable)
{
    char *configPtr = kernBootStruct->configEnd;
    int fd, count;
    
    /* Read config file into memory */
    if ((fd = open(configFile, 0)) >= 0)
    {
	if (allocTable) {
	    configPtr = malloc(file_size(fd)+2);
	} else {
	    if ((configPtr - kernBootStruct->config) > CONFIG_SIZE) {
		error("No room in memory for config files\n");
		close(fd);
		return -1;
	    }
	    verbose("Reading configuration file '%s'.\n",configFile);	    
	}
	if (table) *table = configPtr;
	count = read(fd, configPtr, IO_CONFIG_DATA_SIZE);
	close(fd);

	configPtr += count;
	*configPtr++ = 0;
	*configPtr = 0;
	if (!allocTable)
	    kernBootStruct->configEnd = configPtr;

	return 0;
    } else {
	return -1;
    }
}

/* Returns 0 if requested config files were loaded,
 *         1 if default files were loaded,
 *        -1 if no files were loaded.
 * Prints error message if files cannot be loaded.
 */
 
int
loadConfigDir(
    char *bundleName,	// bundle directory name (e.g. "System")
    BOOL useDefault,	// use Default.table instead of instance tables
    char **table,	// returns pointer to config table
    BOOL allocTable	// malloc the table and return in *table
)
{
    char *buf;
    int i, ret;
    BOOL archConfig = dirExists(ARCH_DEVICES);
    
    buf = malloc(256);
    ret = 0;
    
    // load up to 99 instance tables
    for (i=0; i < 99; i++) {
	sprintf(buf, "%s/%s.config/Instance%d.table",
		archConfig ? ARCH_DEVICES : USR_DEVICES,
		bundleName, i);
	if (useDefault || (loadConfigFile(buf, table, allocTable) != 0)) {
	    if (i == 0) {
		// couldn't load first instance table;
		// try the default table
		sprintf(buf, "%s/%s.config/%s",
			archConfig ? ARCH_DEVICES : USR_DEVICES,
			bundleName,
			IO_DEFAULT_TABLE_FILENAME);
		if (loadConfigFile(buf, table, allocTable) == 0) {
		    ret = 1;
		} else {
		    if (!allocTable)
			error("Config file \"%s\" not found\n", buf);
		    ret = -1;
		}
	    }
	    // we must be done.
	    break;
	}
    }
    free(buf);
    return ret;
}


#define USR_SYSTEM_CONFIG \
	USR_DEVICES "/System.config"
#define USR_SYSTEM_DEFAULT_FILE \
	USR_SYSTEM_CONFIG "/Default.table"
#define ARCH_SYSTEM_CONFIG \
	ARCH_DEVICES "/System.config"
#define ARCH_SYSTEM_DEFAULT_FILE \
	ARCH_SYSTEM_CONFIG "/Default.table"
#define SYSTEM_CONFIG "System"
#define LP '('
#define RP ')'

/* Returns 0 if requested config files were loaded,
 *         1 if default files were loaded,
 *        -1 if no files were loaded.
 * Prints error message if files cannot be loaded.
 */
int
loadSystemConfig(
    char *which,
    int size
)
{
    char *buf, *bp, *cp;
    int ret, len, doDefault=0;
    BOOL archConfig = dirExists(ARCH_DEVICES);

    buf = bp = malloc(256);
    if (which && size)
    {
	for(cp = which, len = size; len && *cp && *cp != LP; cp++, len--) ;
	if (*cp == LP) {
	    while (len-- && *cp && *cp++ != RP) ;
	    /* cp now points past device */
	    strncpy(buf,which,cp - which);
	    bp += cp - which;
	} else {
	    cp = which;
	    len = size;
	}
	if (*cp != '/') {
	    strcpy(bp, archConfig ?
		ARCH_SYSTEM_CONFIG : USR_SYSTEM_CONFIG);
	    strcat(bp, "/");
	    strncat(bp, cp, len);
	    if (strncmp(cp + len - strlen(IO_TABLE_EXTENSION),
		       IO_TABLE_EXTENSION, strlen(IO_TABLE_EXTENSION)) != 0)
		strcat(bp, IO_TABLE_EXTENSION);
	} else {
	    strncpy(bp, cp, len);
	    bp[size] = '\0';
	}
	if ((strcmp(bp, USR_SYSTEM_DEFAULT_FILE) == 0) ||
	    (strcmp(bp, ARCH_SYSTEM_DEFAULT_FILE) == 0))
	    doDefault = 1;
	ret = loadConfigFile(bp = buf, 0, 0);
    } else {
	ret = loadConfigDir((bp = SYSTEM_CONFIG), 0, 0, 0);
    }
    if (ret < 0) {
	error("System config file '%s' not found\n", bp);
    } else
	sysConfigValid = 1;
    free(buf);
    return (ret < 0 ? ret : doDefault);
}


int
loadOtherConfigs(
    int useDefault
)
{
    char *val, *table;
    int count;
    char *string;
    int fd, ret;

    if (getValueForKey( "Boot Drivers", &val, &count))
    {
	while (string = stringFromList(&val, &count)) {
	    ret = loadConfigDir(string, useDefault, &table, 0);
	    if (ret >= 0) {
		if ((fd = openDriverReloc(string)) >= 0) {
		    verbose("Loading binary for %s device driver.\n",string);
		    if (loadDriver(string, fd) < 0)
			error("Error loading %s device driver.\n",string);
		    close(fd);
		}
		driverWasLoaded(string, table, NULL);
		free(string);
	    } else {
		driverIsMissing(string);
	    }
	}
    } else {
	error("Warning: No active drivers specified in system config.\n");
    }

    kernBootStruct->first_addr0 =
	    (int)kernBootStruct->configEnd + 1024;
    return 0;
}

static BOOL
dirExists(char *path)
{
    int fd;
    
    if ((fd = open(path, 0)) < 0) {
	return NO;
    } else {
	close(fd);
	return YES;
    }
}



