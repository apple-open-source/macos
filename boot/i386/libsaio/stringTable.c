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

#include "libsaio.h"
#include "stringConstants.h"
#include "legacy/configTablePrivate.h"

extern KERNBOOTSTRUCT *kernBootStruct;
extern char *Language;
extern char *LoadableFamilies;

static void eatThru(char val, const char **table_p);

static inline int isspace(char c)
{
    return (c == ' ' || c == '\t');
}

/*
 * Compare a string to a key with quoted characters
 */
static inline int
keyncmp(const char *str, const char *key, int n)
{
    int c;
    while (n--) {
	c = *key++;
	if (c == '\\') {
	    switch(c = *key++) {
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
	} else if (c == '\"') {
	    /* Premature end of key */
	    return 1;
	}
	if (c != *str++) {
	    return 1;
	}
    }
    return 0;
}

static void eatThru(char val, const char **table_p)
{
	register const char *table = *table_p;
	register BOOL found = NO;

	while (*table && !found)
	{
		if (*table == '\\') table += 2;
		else
		{
			if (*table == val) found = YES;
			table++;
		}
	}
	*table_p = table;
}

/* Remove key and its associated value from the table. */

BOOL
removeKeyFromTable(const char *key, char *table)
{
    register int len;
    register char *tab;
    char *buf;

    len = strlen(key);
    tab = (char *)table;
    buf = (char *)malloc(len + 3);

    sprintf(buf, "\"%s\"", key);
    len = strlen(buf);

    while(*tab) {
        if(strncmp(buf, tab, len) == 0) {
            char c;

            while((c = *(tab + len)) != ';') {
                if(c == 0) {
                    len = -1;
                    goto out;
                }
                len++;
            }
            len++;
            if(*(tab + len) == '\n') len++;
            goto out;
        }
        tab++;
    }
    len = -1;
out:
    free(buf);

    if(len == -1) return NO;

    while((*tab = *(tab + len))) {
        tab++;
    }

    return YES;
}

char *
newStringFromList(
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

/* 
 * compress == compress escaped characters to one character
 */
int stringLength(const char *table, int compress)
{
	int ret = 0;

	while (*table)
	{
		if (*table == '\\')
		{
			table += 2;
			ret += 1 + (compress ? 0 : 1);
		}
		else
		{
			if (*table == '\"') return ret;
			ret++;
			table++;
		}
	}
	return ret;
}

// looks in table for strings of format << "key" = "value"; >>
// or << "key"; >>
BOOL getValueForStringTableKey(const char *table, const char *key, const char **val, int *size)
{
	int keyLength;
	const char *tableKey;

	do
	{
		eatThru('\"',&table);
		tableKey = table;
		keyLength = strlen(key);
		if (keyLength &&
		    (stringLength(table,1) == keyLength) &&
		    (keyncmp(key, table, keyLength) == 0))
		{
			int c;
			
			/* found the key; now look for either
			 * '=' or ';'
			 */
			while (c = *table) {
			    ++table;
			    if (c == '\\') {
				++table;
				continue;
			    } else if (c == '=' || c == ';') {
				break;
			    }
			}
			if (c == ';') {
			    table = tableKey;
			} else {
			    eatThru('\"',&table);
			}
			*val = table;
			*size = stringLength(table,0);
			return YES;
		}

		eatThru(';',&table);

	} while (*table);

	return NO;
}


/*
 * Returns a new malloc'ed string if one is found
 * in the string table matching 'key'.  Also translates
 * \n escapes in the string.
 */
char *newStringForStringTableKey(
	char *table,
	char *key
)
{
    const char *val;
    char *newstr, *p;
    int size;
    
    if (getValueForStringTableKey(table, key, &val, &size)) {
	newstr = (char *)malloc(size+1);
	for (p = newstr; size; size--, p++, val++) {
	    if ((*p = *val) == '\\') {
		switch (*++val) {
		case 'r':
		    *p = '\r';
		    break;
		case 'n':
		    *p = '\n';
		    break;
		case 't':
		    *p = '\t';
		    break;
		default:
		    *p = *val;
		    break;
		}
		size--;
	    }
	}
	*p = '\0';
	return newstr;
    } else {
	return 0;
    }
}

char *
newStringForKey(char *key)
{
    const char *val;
    char *newstr;
    int size;
    
    if (getValueForKey(key, &val, &size) && size) {
	newstr = (char *)malloc(size + 1);
	strncpy(newstr, val, size);
	return newstr;
    } else {
	return 0;
    }
}

/* parse a command line
 * in the form: [<argument> ...]  [<option>=<value> ...]
 * both <option> and <value> must be either composed of
 * non-whitespace characters, or enclosed in quotes.
 */

static const char *getToken(const char *line, const char **begin, int *len)
{
    if (*line == '\"') {
	*begin = ++line;
	while (*line && *line != '\"')
	    line++;
	*len = line++ - *begin;
    } else {
	*begin = line;
	while (*line && !isspace(*line) && *line != '=')
	    line++;
	*len = line - *begin;
    }
    return line;
}

BOOL getValueForBootKey(const char *line, const char *match, const char **matchval, int *len)
{
    const char *key, *value;
    int key_len, value_len;
    
    while (*line) {
	/* look for keyword or argument */
	while (isspace(*line)) line++;

	/* now look for '=' or whitespace */
	line = getToken(line, &key, &key_len);
	/* line now points to '=' or space */
	if (*line && !isspace(*line)) {
	    line = getToken(++line, &value, &value_len);
	} else {
	    value = line;
	    value_len = 0;
	}
	if ((strlen(match) == key_len)
	    && strncmp(match, key, key_len) == 0) {
	    *matchval = value;
	    *len = value_len;
	    return YES;
	}
    }
    return NO;
}

BOOL getBoolForKey(
    const char *key
)
{
    const char *val;
    int size;
    
    if (getValueForKey(key, &val, &size) && (size >= 1) &&
	val[0] == 'Y' || val[0] == 'y')
	    return YES;
    return NO;
}

BOOL getIntForKey(
    const char *key,
    int *value
)
{
    const char *val;
    int size, sum;
    
    if (getValueForKey(key, &val, &size)) {
	for (sum = 0; size > 0; size--) {
	    sum = (sum * 10) + (*val++ - '0');
	}
	*value = sum;
	return YES;
    }
    return NO;
}

BOOL getValueForKey(
    const char *key,
    const char **val,
    int *size
)
{
    if (getValueForBootKey(kernBootStruct->bootString, key, val, size))
	return YES;
    else if (getValueForStringTableKey(kernBootStruct->config, key, val, size))
	return YES;

    return NO;
}

#if 0
#define	LOCALIZABLE_PATH \
	"%s/%s.config/%s.lproj/%s.strings"
char *
loadLocalizableStrings(
    char *name,
    char *tableName
)
{
    char buf[256], *config;
    register int count, fd = -1;
    const char *device_dir = usrDevices();
    
    sprintf(buf, LOCALIZABLE_PATH, device_dir, name,
	    Language, tableName);
    if ((fd = open(buf, 0)) < 0) {
	sprintf(buf, LOCALIZABLE_PATH, device_dir, name,
		 "English", tableName);
	if ((fd = open(buf,0)) < 0) {
	    return 0;
	}
    }
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
#endif

#if 0 // XXX
char *
bundleLongName(
    char *bundleName,
    char *tableName
)
{
    char *table, *name, *version, *newName;
    char *path = malloc(256);
    
#define LONG_NAME_FORMAT "%s (v%s)"
    sprintf(path, "%s/%s.config/%s.table",
	usrDevices(), bundleName, tableName ? tableName : "Default");
    if (loadConfigFile(path, &table, YES) == 0) {
	version = newStringForStringTableKey(table, "Version");
	free(table);
    } else {
	version = newString("0.0");
    }
    table = loadLocalizableStrings(bundleName,
	tableName ? tableName : "Localizable");
    if (table) {
	name = newStringForStringTableKey(table, "Long Name");
	free(table);
    } else {
	name = newString(bundleName);
    }
    newName = malloc(strlen(name)+strlen(version)+strlen(LONG_NAME_FORMAT));
    sprintf(newName, LONG_NAME_FORMAT, name, version);
    free(name); free(version);
    return newName;
}
#endif

int sysConfigValid;

void
addConfig(
    const char *config
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

#define TABLE_EXPAND_SIZE	192

/*
 * Returns 0 if file loaded OK,
 *        -1 if file was not loaded
 * Does not print error messages.
 * Returns pointer to table in memory in *table.
 * Allocates an extra number of bytes for table expansion.
 */
int
loadConfigFile(const char *configFile, const char **table, BOOL allocTable)
{
    char *configPtr = kernBootStruct->configEnd;
    int fd, count;
    
    /* Read config file into memory */
    if ((fd = open(configFile, 0)) >= 0)
    {
	if (allocTable) {
	    configPtr = malloc(file_size(fd)+2+TABLE_EXPAND_SIZE);
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
    const char *bundleName,	// bundle directory name (e.g. "System")
    BOOL useDefault,	// use Default.table instead of instance tables
    const char **table,	// returns pointer to config table
    BOOL allocTable	// malloc the table and return in *table
)
{
    char *buf;
    int i, max, ret;
    const char *device_dir = usrDevices();
    
    buf = malloc(256);
    ret = 0;
    
    // load up to 99 instance tables
    if (allocTable)
	max = 1;
    else
	max = 99;
    for (i=0; i < max; i++) {
	sprintf(buf, "%s/%s.config/Instance%d.table",
		device_dir,
		bundleName, i);
	if (useDefault || (loadConfigFile(buf, table, allocTable) != 0)) {
	    if (i == 0) {
		// couldn't load first instance table;
		// try the default table
		sprintf(buf, "%s/%s.config/%s",
			device_dir,
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

static int sysconfig_dev;

/* Returns 0 if requested config files were loaded,
 *         1 if default files were loaded,
 *        -1 if no files were loaded.
 * Prints error message if files cannot be loaded.
 */
int
loadSystemConfig(
    const char *which,
    int size
)
{
    char *buf, *bp;
    const char *cp;
    int ret, len, doDefault=0;
    const char *device_dir = usrDevices();

#if 0
		printf("In Load system config which=%d ; size=%d\n", which, size);
		//sleep(1);
#endif 1
    buf = bp = malloc(256);
    if (which && size)
    {
#if 0
		printf("In Load system config alt\n");
		//sleep(1);
#endif 1
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
	    strcpy(bp, device_dir);
	    strcat(bp, "/System.config/");
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
#if 0
		printf("In default SYSTEM_CONFIG LOAD\n");
		//sleep(1);
#endif 1
	ret = loadConfigDir((bp = SYSTEM_CONFIG), 0, 0, 0);
#if 0
		printf("come back from SYSTEM_CONFIG loadConfigDir\n");
		//sleep(1);
#endif 1
   }
    sysconfig_dev = currentdev();
    if (ret < 0) {
	error("System config file '%s' not found\n", bp);
    } else
	sysConfigValid = 1;
    free(buf);
    return (ret < 0 ? ret : doDefault);
}

#ifdef DISABLED
int
loadOtherConfigs(
    int useDefault
)
{
    char *val, *table;
    char *path = malloc(256);
    char *hintTable;
    char *installVersion = NULL, *thisVersion;
    char *longName, *tableName;
    int count;
    char *string;
    int  ret;
    int old_dev = currentdev();

    if (sysconfig_dev)
	switchdev(sysconfig_dev);
    if (getValueForKey( "Boot Drivers", &val, &count))
    {
#if 0
	printf("Loading Boot Drivers\n");
	sleep(1);
#endif 1
	while (string = newStringFromList(&val, &count)) {
	    /* Check installation hints... */
	    sprintf(path, "%s/System.config/" INSTALL_HINTS
			  "/%s.table", usrDevices(), string);

	    if (getBoolForKey("Ignore Hints") == NO &&
		    loadConfigFile(path, &hintTable, YES) == 0) {
		installVersion = newStringForStringTableKey(
		    hintTable, "Version");
		longName = newStringForStringTableKey(
		    hintTable, "Long Name");
		tableName = newStringForStringTableKey(
		    hintTable, "Default Table");
		free(hintTable);
	    } else {
		installVersion = longName = tableName = NULL;
	    }

	    ret = loadConfigDir(string, useDefault, &table, YES);
	    if (ret >= 0) {
		thisVersion = newStringForStringTableKey(
		    table, "Version");
		if (installVersion && thisVersion &&
		    (strcmp(thisVersion, installVersion) != 0)) {
		    /* Versions do not match */
		    driverIsMissing(string, installVersion, longName, 
                        tableName, DRIVER_VERSION_MISMATCH);
		} else {
		    struct driver_load_data dl;
		    
		    dl.name = string;
		    if ((openDriverReloc(&dl)) >= 0) {
			verbose("Loading binary for %s device driver.\n",string);
			if (loadDriver(&dl) < 0) /// need to stop if error
			    error("Error loading %s device driver.\n",string);
#if 0
		printf("Calling link driver for %s\n", string);
#endif 1
			if (linkDriver(&dl) < 0)
			    error("Error linking %s device Driver.\n",string);
		    }
		    loadConfigDir(string, useDefault, NULL, NO);
		    driverWasLoaded(string, table, NULL);
		    free(table);
		    free(string);
		    free(installVersion); free(longName);
		    free(tableName);
		}
		free(thisVersion);
	    } else {
                /* driver not found */
		driverIsMissing(string, installVersion, longName, 
                    tableName, DRIVER_NOT_FOUND);
	    }
#if 0
	    if (ret == 1)
		useDefault = 1;	// use defaults from now on
#endif
	}
    } else {
	error("Warning: No Boot drivers specified in system config.\n");
    }

    kernBootStruct->first_addr0 =
	    (int)kernBootStruct->configEnd + 1024;
    free(path);
    switchdev(old_dev);
    return 0;
}
#endif /* DISABLED */
