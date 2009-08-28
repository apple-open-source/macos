
/*
 * Copyright (c) 2001-2002 Apple Inc. All rights reserved.
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

/* 
 * Modification History
 *
 * November 13, 2001	Dieter Siegmund
 * - created
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "KeyValue.h"

typedef struct KeyValue_s KeyValue;

struct KeyValue_s {
    char *	key;
    char *	value;
};

struct KeyValueList_s {
    KeyValue *		elements;
    int			count;
    char *		str;
};

void
KeyValueList_print(KeyValueList * list)
{
    int i;

    if (list == NULL) {
	return;
    }

    for (i = 0; i < list->count; i++) {
	printf("%2d. %10s=%-10s\n", i, list->elements[i].key,
	       list->elements[i].value);
    }
    return;
}

char *
KeyValueList_find(KeyValueList * list, char * key, int * where)
{
    int 	i;
    int		start = 0;

    if (where) {
	if (*where > 0) {
	    start = *where;
	}
    }
    for (i = start; i < list->count; i++) {
	if (strcmp(list->elements[i].key, key) == 0) {
	    if (where) {
		*where = i;
	    }
	    return (list->elements[i].value);
	}
    }
    return (NULL);
}

void
KeyValueList_free(KeyValueList * * list_p)
{
    KeyValueList * list = *list_p;

    if (list) {
	void * chunk;

	if (list->str) {
	    free(list->str);
	    list->str = NULL;
	}
	/* Note: "list" is part of the same malloc chunk as "list->elements" */
	chunk = list->elements;
	if (chunk) {
	    list->elements = NULL;
	    free(chunk);
	}
	*list_p = NULL;
    }
    return;
}

KeyValueList *
KeyValueList_create(void * buf, int buflen)
{
    int			count = 0;
    KeyValue *		elements = NULL;
    size_t		n;
    KeyValueList *	ret = NULL;
    char *		scan;
    int			size = 4;
    char *		str = NULL;

    str = malloc(buflen + 1);
    if (str == NULL) {
	goto failed;
    }
    bcopy(buf, str, buflen);
    str[buflen] = '\0';

    elements = malloc(sizeof(*elements) * size + sizeof(*ret));
    if (elements == NULL) {
	goto failed;
    }
    for (scan = str; *scan != '\0'; scan++) {
	size_t	nval;
	char *	start = scan;

	n = strcspn(scan, ",");

	if (n == 0) {
	    continue;
	}
	scan[n] = '\0';
	scan += n;
	nval = strcspn(start, "=");
	if (nval == 0) {
	    continue;
	}
	if (count == size) {
	    size *= 2;
	    elements = realloc(elements, sizeof(*elements) * size + sizeof(*ret));
	    if (elements == NULL) {
		goto failed;
	    }
	}
	elements[count].key = start;
	if (start[nval] == '\0') {
	    elements[count].value = start + nval;
	}
	else {
	    start[nval] = '\0';
	    elements[count].value = start + nval + 1;
	}
	count++;
    }
    if (count == 0) {
	goto failed;
    }
    if (count != size) {
	elements = realloc(elements, sizeof(*elements) * count + sizeof(*ret));
	if (elements == NULL)
	    goto failed;
    }
    ret = (KeyValueList *)(elements + count);
    ret->elements = elements;
    ret->count = count;
    ret->str = str;
    return (ret);
 failed:
    if (elements) {
	free(elements);
    }
    if (str) {
	free(str);
    }
    return (NULL);
}

#ifdef TEST_KEYVALUE
int
main(int argc, char * argv[])
{
    char * tests[] = {
	"this=that,this=,other=,,=nothing,",
	"this=that",
	",this=that",
	"one=1,two=2,three=3,four=4,five=5,six=6,seven=7",
	"networkid=Microsoft.com,networkid=exchange.Microsoft.com,foo=bar",
	"testing",
	"networkid=Microsoft.com,networkid=exchange.Microsoft.com,foo=bar",
	"",
	NULL,
    };
    KeyValueList * list;
    char * * scan = tests;

    for (scan = tests; *scan; scan++) {
	printf("Input '%s'\n", *scan);
	list = KeyValueList_create(*scan, strlen(*scan));
	if (list) {
	    KeyValueList_print(list);
	}
	printf("\n");
	KeyValueList_free(&list);
    }
    exit(0);
    return (0);
}
#endif TEST_KEYVALUE
