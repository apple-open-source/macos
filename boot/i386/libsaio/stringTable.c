/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */

#include "bootstruct.h"
#include "libsaio.h"
#include "stringConstants.h"
#include "legacy/configTablePrivate.h"
#include "xml.h"

extern char *Language;
extern char *LoadableFamilies;

static TagPtr gConfigDict;

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
    int bufsize;
    
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
    bufsize = end - begin + 1;
    newstr = malloc(bufsize);
    strlcpy(newstr, begin, bufsize);
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

BOOL getValueForConfigTableKey(const char *table, const char *key, const char **val, int *size)
{
	int keyLength;
	const char *tableKey;

        if (gConfigDict != 0 ) {
            /* Look up key in XML dictionary */
            TagPtr value;
            value = XMLGetProperty(gConfigDict, key);
            if (value != 0) {
                if (value->type != kTagTypeString) {
                    error("Non-string tag '%s' found in config file\n",
                          key);
                    return NO;
                }
                *val = value->string;
                *size = strlen(value->string);
                return YES;
            }
        } else {
            /* Legacy plist-style table */
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
        }

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
    
    if (getValueForConfigTableKey(table, key, &val, &size)) {
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
	strlcpy(newstr, val, size + 1);
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
    if (getValueForBootKey(bootArgs->bootString, key, val, size))
	return YES;
    else if (getValueForConfigTableKey(bootArgs->config, key, val, size))
	return YES;

    return NO;
}

int sysConfigValid;

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
    char *configPtr = bootArgs->configEnd;
    int fd, count;
    
    /* Read config file into memory */
    if ((fd = open(configFile, 0)) >= 0)
    {
	if (allocTable) {
	    configPtr = malloc(file_size(fd)+2+TABLE_EXPAND_SIZE);
	} else {
	    if ((configPtr - bootArgs->config) > CONFIG_SIZE) {
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
	    bootArgs->configEnd = configPtr;

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
		    if (!allocTable) {
			error("Config file \"%s\" not found\n", buf);
			sleep(1); // let the message be seen!
		    }
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

#define SYSTEM_CONFIG_DIR "/Library/Preferences/SystemConfiguration"
#define SYSTEM_CONFIG_FILE "/com.apple.Boot.plist"
#define SYSTEM_CONFIG_PATH SYSTEM_CONFIG_DIR SYSTEM_CONFIG_FILE
#define CONFIG_EXT ".plist"

#if 1
void
printSystemConfig(void)
{
    char *p1 = bootArgs->config;
    char *p2 = p1, tmp;

    while (*p1 != '\0') {
	while (*p2 != '\0' && *p2 != '\n') p2++;
	tmp = *p2;
	*p2 = '\0';
	printf("%s\n", p1);
	*p2 = tmp;
	if (tmp == '\0') break;
	p1 = ++p2;
    }
}
#endif

static int sysconfig_dev;

//==========================================================================
// ParseXMLFile
// Modifies the input buffer.
// Expects to see one dictionary in the XML file.
// Puts the first dictionary it finds in the
// tag pointer and returns 0, or returns -1 if not found
// (and does not modify dict pointer).
//
static long
ParseXMLFile( char * buffer, TagPtr * dict )
{
    long       length, pos;
    TagPtr     tag;
    pos = 0;
    char       *configBuffer;
  
    configBuffer = malloc(strlen(buffer)+1);
    strcpy(configBuffer, buffer);

    while (1)
    {
        length = XMLParseNextTag(configBuffer + pos, &tag);
        if (length == -1) break;
    
        pos += length;
    
        if (tag == 0) continue;
        if (tag->type == kTagTypeDict) break;
    
        XMLFreeTag(tag);
    }
    free(configBuffer);
    if (length < 0) {
        return -1;
    }
    *dict = tag;
    return 0;
}

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

    buf = bp = malloc(512);
    if (which && size) {
        for(cp = which, len = size; len && *cp && *cp != LP; cp++, len--) ;
        if (*cp == LP) {
            while (len-- && *cp && *cp++ != RP) ;
            /* cp now points past device */
            strlcpy(buf,which,cp - which);
            bp += cp - which;
        } else {
            cp = which;
            len = size;
        }
        if (*cp != '/') {
            strcpy(bp, systemConfigDir());
            strcat(bp, "/");
            strncat(bp, cp, len);
            if (strncmp(cp + len - strlen(CONFIG_EXT),
                        CONFIG_EXT, strlen(CONFIG_EXT)) != 0)
                strcat(bp, CONFIG_EXT);
        } else {
            strlcpy(bp, cp, len);
        }
        if ((strcmp(bp, SYSTEM_CONFIG_PATH) == 0)) {
            doDefault = 1;
        }
        ret = loadConfigFile(bp = buf, 0, 0);
    } else {
        strcpy(bp, systemConfigDir());
        strcat(bp, SYSTEM_CONFIG_FILE);
        ret = loadConfigFile(bp, 0, 0);
        if (ret < 0) {
            ret = loadConfigDir((bp = SYSTEM_CONFIG), 0, 0, 0);
        }
    }
    sysconfig_dev = currentdev();
    if (ret < 0) {
	error("System config file '%s' not found\n", bp);
	sleep(1);
    } else {
	sysConfigValid = 1;
        // Check for XML file;
        // if not XML, gConfigDict will remain 0.
        ParseXMLFile(bootArgs->config, &gConfigDict);
    }
    free(buf);
    return (ret < 0 ? ret : doDefault);
}


char * newString(const char * oldString)
{
    if ( oldString )
        return strcpy(malloc(strlen(oldString)+1), oldString);
    else
        return NULL;
}
