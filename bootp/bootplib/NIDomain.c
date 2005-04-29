/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * NIDomain.c
 * - simple (C) object interface to a netinfo domain
 */

/*
 * Modification History:
 * 
 * May 20, 1998		Dieter Siegmund (dieter@apple.com)
 * - initial revision
 * March 14, 2000	Dieter Siegmund (dieter@apple.com)
 * - converted to C
 */

#import	<netdb.h>
#import <string.h>
#import <sys/types.h>
#import <sys/socket.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/if_ether.h>
#import <arpa/inet.h>
#import <string.h>
#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <netinfo/ni_util.h>
#import "NIDomain.h"

static boolean_t
S_has_path_component(u_char * path, u_char * comp, u_char sep)
{
    u_char * path_comp;
    u_char * sep_ptr;

    if (strcmp(path, comp) == 0)
	return (TRUE);

    for (path_comp = path, sep_ptr = strchr(path, sep); sep_ptr; 
	 sep_ptr = strchr(path_comp = (sep_ptr + 1), sep)) {
	if (strncmp(path_comp, comp, sep_ptr - path_comp) == 0) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

boolean_t 
NIDomain_open_path(NIDomain_t * domain, ni_name domain_name)
{
    ni_status 	status;

    domain->name = ni_name_dup(domain_name);

    status = ni_open(NULL, domain->name, &domain->handle);
    if (status != NI_OK)
	return (FALSE);
    status = ni_addrtag(domain->handle, &domain->sockaddr, &domain->tag);
    if (status != NI_OK)
	return (FALSE);
    ni_setpassword(domain->handle, "checksum");
    return (TRUE);
}

boolean_t
NIDomain_open_host_tag(NIDomain_t * domain, ni_name host, ni_name tag)
{
    struct hostent * 	h;
    char 		host_tag[128];

    domain->tag = ni_name_dup(tag);
    sprintf(host_tag, "%s/%s", host, tag);
    domain->name = ni_name_dup(host_tag);

    h = gethostbyname(host);
    if (h != NULL && h->h_addrtype == AF_INET) {
	struct in_addr * * s = (struct in_addr * *)h->h_addr_list;
	while (*s) {
	    domain->sockaddr.sin_len = sizeof(struct sockaddr_in);
	    domain->sockaddr.sin_family = AF_INET;
	    domain->sockaddr.sin_addr = **s;
	    domain->handle = ni_connect(&domain->sockaddr, tag);
	    if (domain->handle != NULL) {
		break;
	    }
	    s++;
	}
    }
    if (domain->handle == NULL)
	return (FALSE);
    ni_setpassword(domain->handle, "checksum");
    return (TRUE);
}

NIDomain_t *
NIDomain_new()
{
    NIDomain_t * n = malloc(sizeof(*n));

    bzero(n, sizeof(*n));
    return (n);
}

NIDomain_t *
NIDomain_parent(NIDomain_t * child)
{
    NIDomain_t *domain;
    ni_status 	status;
    u_char	tmp[256];

    domain = NIDomain_new();
    if (domain == NULL)
	return (NULL);
    sprintf(tmp, "%s/%s", NIDomain_name(child), NI_DOMAIN_PARENT);
    domain->name = ni_name_dup(tmp);
    status = ni_open(NIDomain_handle(child), NI_DOMAIN_PARENT, 
		     &domain->handle);
    if (status != NI_OK) {
	NIDomain_free(domain);
	return (NULL);
    }
    
    /* Special hack to only lookup the checksum */
    ni_setpassword(domain->handle, "checksum");

    status = ni_addrtag(domain->handle, &domain->sockaddr, &domain->tag);
    if (status != NI_OK) {
	NIDomain_free(domain);
	return (NULL);
    }
    return (domain);
}

NIDomain_t *
NIDomain_init(ni_name domain_name)
{
    NIDomain_t * domain = NIDomain_new();

    if (domain == NULL)
	return (NULL);

    if (domain_name[0] == '/' 
	|| S_has_path_component(domain_name, "..", '/')
	|| S_has_path_component(domain_name, ".", '/')) { /* path */
	/* domain_name is an absolute/relative path */
	if (NIDomain_open_path(domain, domain_name) == TRUE) {
	    return (domain);
	}
    }
    else { /* not a path */
	char * slash;
	slash = strchr(domain_name, '/');
	if (slash && slash == strrchr(domain_name, '/')) {
	    char hostname[128];
	    
	    /* connect to hostname/tag */
	    strncpy(hostname, domain_name, slash - domain_name);
	    hostname[slash - domain_name] = '\0';
	    if (NIDomain_open_host_tag(domain, hostname, slash + 1) == TRUE) {
		return (domain);
	    }
	}
    }
    NIDomain_free(domain);
    return (NULL);
}

void *
NIDomain_handle(NIDomain_t * domain)
{
    return domain->handle;
}

ni_name
NIDomain_name(NIDomain_t * domain)
{
    return (domain->name);
}

ni_name
NIDomain_tag(NIDomain_t * domain)
{
    return (domain->tag);
}

struct in_addr
NIDomain_ip(NIDomain_t * domain)
{
    return (domain->sockaddr.sin_addr);
}


void
NIDomain_free(NIDomain_t * domain)
{
    if (domain->handle != NULL)
	ni_free(domain->handle);
    domain->handle = NULL;
    if (domain->name != NULL)
	ni_name_free(&domain->name);
    if (domain->tag != NULL)
	ni_name_free(&domain->tag);
    free(domain);
    return;
}

static void
S_NIDomain_free(void * d)
{
    NIDomain_free((NIDomain_t *)d);
}

void
NIDomain_set_master(NIDomain_t * domain, boolean_t master)
{
    domain->is_master = master;
}

boolean_t
NIDomain_is_master(NIDomain_t * domain)
{
    return (domain->is_master);
}



/* NIDomainList_t */

void
NIDomainList_init(NIDomainList_t * list)
{
    dynarray_init(&list->domains, S_NIDomain_free, NULL);
    return;
}

void
NIDomainList_free(NIDomainList_t * list)
{
    dynarray_free(&list->domains);
    return;
}

NIDomain_t *
NIDomainList_element(NIDomainList_t * list, int i)
{
    return (dynarray_element(&list->domains, i));
}

void
NIDomainList_add(NIDomainList_t * list, NIDomain_t * domain)
{
    dynarray_add(&list->domains, (void *)domain);
    return;
}

int
NIDomainList_count(NIDomainList_t * list)
{
    return (dynarray_count(&list->domains));
}

NIDomain_t *
NIDomainList_find(NIDomainList_t * list, NIDomain_t * domain)
{
    int i;
    for (i = 0; i < NIDomainList_count(list); i++) {
	NIDomain_t * d = NIDomainList_element(list, i);
	
	if (strcmp(NIDomain_tag(d), NIDomain_tag(domain)) == 0
	    && NIDomain_ip(d).s_addr == NIDomain_ip(domain).s_addr)
	    return (d);
    }
    return (NULL);
}
