/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * ipcfg.c
 * - get the interface configuration information
 *   (iftab for now)
 */

/*
 * Modification History
 * 5/28/99	Dieter Siegmund (dieter@apple.com)
 * - initial version
 */

#import <stdlib.h>
#import <unistd.h>
#import <stdio.h>
#import <sys/ioctl.h>
#import <strings.h>
#import <netdb.h>
#import <netdb.h>
#import <net/if_types.h>
#import <mach/boolean.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <ctype.h>
#import <errno.h>
#import "ipcfg.h"

#define HOSTCONFIG	"/etc/hostconfig"
#define IFTAB		"/etc/iftab"

static boolean_t
comment_or_blank(char * line)
{
    char * scan;
    
    for (scan = line; *scan; scan++) {
	char ch = *scan;
	switch (ch) {
	case ' ':
	case '\n':
	case '\t':
	    break;
	case '#':
	    return (TRUE);
	default:
	    return (FALSE);
	}
    }
    return (TRUE);
}

static char *
non_blank(char * str, char * buf)
{
    char ch;

    for (ch = *str; ch == '\n' || ch == ' ' || ch == '\t'; ) {
	str++;
	ch = *str;
    }
    if (ch == '\0')
	return NULL;
    for (ch = *str; ch != '\0' && ch != ' ' && ch != '\t' && ch != '\n'; ) {
	*buf++ = ch;
	str++;
	ch = *str;
    }
    *buf = '\0';
    return (str);
}


static int
parse_inet_cfg(char * str, ipcfg_t * cfg, char * msg, boolean_t * is_inet)
{
    char 	buf[128];
    char * 	scan;
    
    *is_inet = TRUE;
    bzero(cfg, sizeof(*cfg));
    cfg->method = ipconfig_method_none_e;

    scan = non_blank(str, cfg->ifname);
    if (scan == NULL) {
	sprintf(msg, "no interface name");
	return -1;
    }

    scan = non_blank(scan, buf);
    if (scan == NULL) {
	sprintf(msg, "after '%s'", cfg->ifname);
	return -1;
    }

    if (strcmp(buf, "inet")) {
	*is_inet = FALSE;
	return (0);
    }

    scan = non_blank(scan, buf);
    if (scan == NULL) {
	sprintf(msg, "after inet");
	return -1;
    }
    if (strcmp(buf, "-BOOTP-") == 0 || strcmp(buf, "-AUTOMATIC-") == 0) {
	cfg->method = ipconfig_method_bootp_e;
	scan = NULL;
    }
    else if (strcmp(buf, "-DHCP-") == 0) {
	cfg->method = ipconfig_method_dhcp_e;
	scan = NULL;
    }
    else if (strcmp(buf, "-INFORM-") == 0) {
	cfg->method = ipconfig_method_inform_e;
	scan = non_blank(scan, buf);
	if (scan == NULL) {
	    sprintf(msg, "after -INFORM-");
	    return (-1);
	}
    }
    if (scan != NULL) {
	if (inet_aton(buf, &cfg->addr) == 1) {
	    boolean_t got_netmask = FALSE;
	    boolean_t got_broadcast = FALSE;

	    if (cfg->method == ipconfig_method_none_e)
		cfg->method = ipconfig_method_manual_e;
	    while (1) {
		scan = non_blank(scan, buf);
		if (scan == NULL)
		    break;

		if (strcmp(buf, "netmask") == 0) {
		    if (got_netmask) {
			sprintf(msg, "duplicate netmask");
			return -1;
		    }
		    scan = non_blank(scan, buf);
		    if (scan == NULL) {
			sprintf(msg, "netmask value missing");
			return -1;
		    }
		    if (inet_aton(buf, &cfg->mask) != 1) {
			sprintf(msg, "invalid netmask '%s'", buf);
			return -1;
		    }
		    got_netmask = TRUE;
		}
		else if (strcmp(buf, "broadcast") == 0) {
		    if (got_broadcast) {
			sprintf(msg, "duplicate broadcast");
			return -1;
		    }
		    scan = non_blank(scan, buf);
		    if (scan == NULL) {
			sprintf(msg, "broadcast value missing");
			return -1;
		    }
		    if (inet_aton(buf, &cfg->broadcast) != 1) {
			sprintf(msg, "invalid broadcast '%s'", buf);
			return -1;
		    }
		    got_broadcast = TRUE;
		}
	    }
	}
    }
    return (0);
}

typedef struct {
    char	name[128];
    char	value[128];
} hostconfig_prop_t;

static void
hostconfig_prop_print(hostconfig_prop_t * prop)
{
    printf("'%s' = '%s'\n", prop->name, prop->value);
}

void
hostconfig_print(hostconfig_t * h)
{
    int i;

    printf("%d entries\n", dynarray_count(h));
    for (i = 0; i < dynarray_count(h); i++)
	hostconfig_prop_print(dynarray_element(h, i));
    return;
}

static hostconfig_t *
hostconfig_parse(FILE * f, char * msg)
{
    hostconfig_t *	h = NULL;	
    char 		line[1024];
    int			status = 0;

    h = malloc(sizeof(*h));
    if (h == NULL) {
	return (NULL);
    }
    dynarray_init(h, free, NULL);
    while (1) {
	if (fgets(line, sizeof(line), f) == NULL) {
	    if (feof(f) == 0)
		status = -1;
	    break;
	}
	if (comment_or_blank(line) == FALSE) {
	    int		len = strlen(line);
	    char *	sep = strchr(line, '=');
	    int 	whitespace_len = strspn(line, " \t\n");

	    if (whitespace_len == len) {
		continue;
	    }
	    if (sep) {
		hostconfig_prop_t *	p;
		int 		nlen = (sep - line) - whitespace_len;
		int 		vlen = len - whitespace_len - nlen - 2;

		p = malloc(sizeof(*p));
		if (p == NULL) {
		    goto failed;
		}
		strncpy(p->name, line + whitespace_len, nlen);
		p->name[nlen] = '\0';
		strncpy(p->value, sep + 1, vlen);
		p->value[vlen] = '\0';
		dynarray_add(h, p);
	    }
	    else {
		hostconfig_prop_t *	p;
		int 		nlen = len - whitespace_len - 1;

		p = malloc(sizeof(*p));
		if (p == NULL) {
		    goto failed;
		}
		strncpy(p->name, line + whitespace_len, nlen);
		p->name[nlen] = '\0';
		p->value[0] = '\0';
		dynarray_add(h, p);
	    }
	}
    }
    return (h);
 failed:
    if (h) {
	dynarray_free(h);
	free(h);
    }
    return (NULL);
}

hostconfig_t *
hostconfig_read(char * msg)
{
    FILE *		f;
    hostconfig_t *	h = NULL;	

    f = fopen(HOSTCONFIG, "r");
    if (f == NULL) {
	sprintf(msg, "couldn't open '" HOSTCONFIG "', %s (%d)", 
	       strerror(errno), errno);
	return (NULL);
    }
    h = hostconfig_parse(f, msg);
    fclose(f);
    return (h);
}

void
hostconfig_free(hostconfig_t * * h)
{
    if (h && *h) {
	dynarray_free(*h);
	free(*h);
	*h = NULL;
    }
    return;
}

char *
hostconfig_lookup(hostconfig_t * h, char * prop)
{
    int i;

    for (i = 0; i < dynarray_count(h); i++) {
	hostconfig_prop_t * entry = dynarray_element(h, i);

	if (strcmp(entry->name, prop) == 0) {
	    return (entry->value);
	}
    }
    return (NULL);
}

static __inline__ ipcfg_table_t *
parse_iftab(FILE * f, char * msg)
{
    char 		line[1024];
    int			status = 0;
    ipcfg_table_t *	t = malloc(sizeof(*t));

    if (t == NULL) {
	strcpy(msg, "malloc/realloc failed");
	return (NULL);
    }

    dynarray_init(t, free, NULL);

    while (1) {
	ipcfg_t	entry;

	if (fgets(line, sizeof(line), f) == NULL) {
	    if (feof(f) == 0)
		status = -1;
	    break;
	}
	if (comment_or_blank(line) == FALSE) {
	    boolean_t 	is_inet;

	    if (parse_inet_cfg(line, &entry, msg, &is_inet) < 0) {
		goto failed;
	    }
	    if (is_inet) { /* save it */
		ipcfg_t * ent;

		ent = malloc(sizeof(*ent));
		if (ent == NULL) {
		    strcpy(msg, "malloc/realloc failed");
		    goto failed;
		}
		*ent = entry;
		dynarray_add(t, ent);
	    }
	}
    }
    return (t);
 failed:
    if (t) {
	dynarray_free(t);
	free(t);
    }
    return (NULL);
}


void
ipcfg_free(ipcfg_table_t * * t)
{
    if (t && *t) {
	dynarray_free(*t);
	free(*t);
	*t = NULL;
    }
    return;
}

int 
ipcfg_count(ipcfg_table_t * t)
{
    return (dynarray_count(t));
}

ipcfg_t *
ipcfg_element(ipcfg_table_t * t, int i)
{
    return ((ipcfg_t *)dynarray_element(t, i));
}


ipcfg_table_t *
ipcfg_from_file(char *msg)
{
    ipcfg_table_t * 	cfg = NULL;
    FILE * 		f;

    /* get the interface configuration information */
    /* read /etc/iftab */
    f = fopen(IFTAB, "r");
    if (f == NULL) {
	sprintf(msg, "couldn't open '" IFTAB "', %s (%d)", strerror(errno),
		errno);
	return (NULL);
    }
    cfg = parse_iftab(f, msg);
    fclose(f);
    return (cfg);
}


void
ipcfg_entry_print(ipcfg_t * entry)
{
    printf("%s %s", entry->ifname, 
	   ipconfig_method_string(entry->method));
    if (entry->method == ipconfig_method_manual_e) {
	printf(" %s", inet_ntoa(entry->addr));
	printf(" %s", inet_ntoa(entry->mask));
    }
    else if (entry->method == ipconfig_method_inform_e) {
	printf(" %s", inet_ntoa(entry->addr));
    }
    printf("\n");
}

void
ipcfg_print(ipcfg_table_t * table) 
{
    int i;

    printf("%d entries\n", dynarray_count(table));
    for (i = 0; i < dynarray_count(table); i++)
	ipcfg_entry_print(dynarray_element(table, i));
    return;
}

#define ANYCHAR		'*'

static boolean_t
ifname_match(char * pattern, char * ifname)
{
    char * pscan;
    char * iscan;

    for (pscan = pattern, iscan = ifname; *pscan && *iscan; 
	 pscan++, iscan++) {
	if (*pscan == ANYCHAR)
	    return (TRUE);
	if (*pscan != *iscan)
	    return (FALSE);
    }
    if (*pscan || *iscan)
	return (FALSE);
    return (TRUE);
}

ipcfg_t *
ipcfg_lookup(ipcfg_table_t * table, char * ifname, int * where)
{
    int i;

    if (where)
	*where = -1;
    for (i = 0; i < dynarray_count(table); i++) {
	ipcfg_t * entry = dynarray_element(table, i);

	if (ifname_match(entry->ifname, ifname)) {
	    if (where) {
		*where = i;
	    }
	    return (entry);
	}
    }
    return (NULL);
}

#ifdef TEST_IPCFG
#import "interfaces.h"

int
main(int argc, char * argv[])
{
    char 		msg[1024];
    ipcfg_table_t *	table = NULL;
    hostconfig_t *	h = NULL;
    interface_list_t * 	list_p = ifl_init(FALSE);
    

    h = hostconfig_read(msg);
    if (h == NULL) {
	printf("hostconfig_read failed: %s\n", msg);
	exit(1);
    }
    hostconfig_print(h);
    table = ipcfg_from_file(msg);
    if (table == NULL) {
	printf("ipcfg_from_file failed: %s\n", msg);
	exit(1);
    }
    ipcfg_print(table);

    if (list_p) {
	int i;
	for (i = 0; i < ifl_count(list_p); i++) {
	    interface_t * 	if_p = ifl_at_index(list_p, i);
	    ipcfg_t *		ipcfg = ipcfg_lookup(table, if_name(if_p));
	    
	    if (ipcfg) {
		printf("Found match for %s:\n", if_name(if_p));
		ipcfg_entry_print(ipcfg);
		printf("\n");
	    }
	}
    }
    ipcfg_free(&table);
    exit(0);
}
#endif TEST_IPCFG
