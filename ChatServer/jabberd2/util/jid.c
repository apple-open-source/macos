/*
 * jabberd - Jabber Open Source Server
 * Copyright (c) 2002 Jeremie Miller, Thomas Muldowney,
 *                    Ryan Eatmon, Robert Norris
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
 */

#include "util.h"

#ifdef HAVE_IDN
#include <stringprep.h>
#endif

/** preparation cache */
prep_cache_t prep_cache_new(void) {
#ifdef HAVE_IDN
    prep_cache_t pc;

    pc = (prep_cache_t) malloc(sizeof(struct prep_cache_st));
    memset(pc, 0, sizeof(struct prep_cache_st));

    pc->node = xhash_new(301);
    pc->domain = xhash_new(301);
    pc->resource = xhash_new(301);

    return pc;
#else
    return NULL;
#endif
}

void prep_cache_free(prep_cache_t pc) {
#ifdef HAVE_IDN
    xhash_free(pc->node);
    xhash_free(pc->domain);
    xhash_free(pc->resource);
    free(pc);
#endif
}

char *prep_cache_node_get(prep_cache_t pc, char *from) {
    return (char *) xhash_get(pc->node, from);
}

void prep_cache_node_set(prep_cache_t pc, char *from, char *to) {
    xhash_put(pc->node, pstrdup(xhash_pool(pc->node), from), (void *) pstrdup(xhash_pool(pc->node), to));
}

char *prep_cache_domain_get(prep_cache_t pc, char *from) {
    return (char *) xhash_get(pc->domain, from);
}

void prep_cache_domain_set(prep_cache_t pc, char *from, char *to) {
    xhash_put(pc->domain, pstrdup(xhash_pool(pc->domain), from), (void *) pstrdup(xhash_pool(pc->domain), to));
}

char *prep_cache_resource_get(prep_cache_t pc, char *from) {
    return (char *) xhash_get(pc->resource, from);
}

void prep_cache_resource_set(prep_cache_t pc, char *from, char *to) {
    xhash_put(pc->resource, pstrdup(xhash_pool(pc->resource), from), (void *) pstrdup(xhash_pool(pc->resource), to));
}

/** do stringprep on the pieces */
int jid_prep(jid_t jid) {
#ifdef HAVE_IDN
    char str[1024], *prep;

    jid->dirty = 1;

    /* no cache, so do a real prep */
    if(jid->pc == NULL) {
        if(jid->node[0] != '\0')
            if(stringprep_xmpp_nodeprep(jid->node, 1024) != 0)
                return 1;

        if(stringprep_nameprep(jid->domain, 1024) != 0)
            return 1;

        if(jid->resource[0] != '\0')
            if(stringprep_xmpp_resourceprep(jid->node, 1024) != 0)
                return 1;

        return 0;
    }

    /* cache version */
    if(jid->node[0] != '\0') {
        strcpy(str, jid->node);
        prep = prep_cache_node_get(jid->pc, str);
        if(prep != NULL)
            strcpy(jid->node, prep);
        else {
            if(stringprep_xmpp_nodeprep(str, 1024) != 0)
                return 1;
            prep_cache_node_set(jid->pc, jid->node, str);
            strcpy(jid->node, str);
        }
    }

    strcpy(str, jid->domain);
    prep = prep_cache_domain_get(jid->pc, str);
    if(prep != NULL)
        strcpy(jid->domain, prep);
    else {
        if(stringprep_nameprep(str, 1024) != 0)
            return 1;
        prep_cache_domain_set(jid->pc, jid->domain, str);
        strcpy(jid->domain, str);
    }

    if(jid->resource[0] != '\0') {
        strcpy(str, jid->resource);
        prep = prep_cache_resource_get(jid->pc, str);
        if(prep != NULL)
            strcpy(jid->resource, prep);
        else {
            if(stringprep_xmpp_resourceprep(str, 1024) != 0)
                return 1;
            prep_cache_resource_set(jid->pc, jid->resource, str);
            strcpy(jid->resource, str);
        }
    }

#endif
    return 0;
}

/** make a new jid */
jid_t jid_new(prep_cache_t pc, const unsigned char *id, int len) {
    jid_t jid, ret;

    jid = malloc(sizeof(struct jid_st));
    jid->pc = pc;

    ret = jid_reset(jid, id, len);
    if(ret == NULL)
        free(jid);

    return ret;
}

/** build a jid from an id */
jid_t jid_reset(jid_t jid, const unsigned char *id, int len) {
    prep_cache_t pc;
    unsigned char *myid, *cur;

    assert((int) jid);

    pc = jid->pc;
    memset(jid, 0, sizeof(struct jid_st));
    jid->pc = pc;

    /* nice empty jid */
    if(id == NULL)
        return jid;

    if(len < 0)
        len = strlen(id);

    if(len == 0)
        return NULL;

    myid = (char *) malloc(sizeof(char) * (len + 1));
    sprintf(myid, "%.*s", len, id);

    /* fail - only a resource or leading @ */
    if (myid[0] == '/' || myid[0] == '@')
        return NULL;

    /* get the resource first */
    cur = strstr(myid, "/");

    if(cur != NULL)
    {
        *cur = '\0';
        cur++;
        if(strlen(cur) > 0) {
            strncpy(jid->resource, cur, 1023);
            jid->resource[1023]='\0';
        }
    }

    /* find the domain */
    cur = strstr(myid, "@");
    if(cur != NULL)
    {
        *cur = '\0';
        cur++;
        if(strlen(cur) == 0)
        {
            /* no domain part, bail out */
            free(myid);
            return NULL;
        }
        strncpy(jid->domain, cur, 1023);
        jid->domain[1023]='\0';
        strncpy(jid->node, myid, 1023);
        jid->node[1023]='\0';
    }

    /* no @, so its a domain only */
    else {
        strncpy(jid->domain, myid, 1023);
        jid->domain[1023]='\0';
    }

    free(myid);

    if(jid_prep(jid) != 0)
        return NULL;

    jid->dirty = 1;

    return jid;
}

/** free a jid */
void jid_free(jid_t jid)
{
    free(jid->_user);
    free(jid->_full);
    free(jid);
}

/** build user and full if they're out of date */
void jid_expand(jid_t jid)
{
    int nlen, dlen, rlen, ulen;
    
    if(!jid->dirty || *jid->domain == '\0')
        return;

    nlen = strlen(jid->node);
    dlen = strlen(jid->domain);
    rlen = strlen(jid->resource);

    if(nlen == 0) {
        ulen = dlen+1;
        jid->_user = (unsigned char*) realloc(jid->_user, ulen);
        strcpy(jid->_user, jid->domain);
    } else {
        ulen = nlen+1+dlen+1;
        jid->_user = (unsigned char*) realloc(jid->_user, ulen);
        snprintf(jid->_user, ulen, "%s@%s", jid->node, jid->domain);
    }

    if(rlen == 0) {
        jid->_full = (unsigned char*) realloc(jid->_full, ulen);
        strcpy(jid->_full, jid->_user);
    } else {
        jid->_full = (unsigned char*) realloc(jid->_full, ulen+1+rlen);
        snprintf(jid->_full, ulen+1+rlen, "%s/%s", jid->_user, jid->resource);
    }

    jid->dirty = 0;
}

/** expand and return the user */
const unsigned char *jid_user(jid_t jid)
{
    jid_expand(jid);

    return jid->_user;
}

/** expand and return the full */
const unsigned char *jid_full(jid_t jid)
{
    jid_expand(jid);

    return jid->_full;
}

/** compare the user portion of two jids */
int jid_compare_user(jid_t a, jid_t b)
{
    jid_expand(a);
    jid_expand(b);

    return strcmp(a->_user, b->_user);
}

/** compare two full jids */
int jid_compare_full(jid_t a, jid_t b)
{
    jid_expand(a);
    jid_expand(b);

    return strcmp(a->_full, b->_full);
}

/** duplicate a jid */
jid_t jid_dup(jid_t jid)
{
    jid_t new;

    new = (jid_t) malloc(sizeof(struct jid_st));
    memcpy(new, jid, sizeof(struct jid_st));
    if(jid->_user)
	    new->_user = strdup(jid->_user);
    if(jid->_full)
	    new->_full = strdup(jid->_full);

    return new;
}

/** util to search through jids */
int jid_search(jid_t list, jid_t jid)
{
    jid_t cur;
    for(cur = list; cur != NULL; cur = cur->next)
        if(jid_compare_full(cur,jid) == 0)
            return 1;
    return 0;
}

/** remove a jid_t from a list, returning the new list */
jid_t jid_zap(jid_t list, jid_t jid)
{
    jid_t cur, dead;

    if(jid == NULL || list == NULL) return NULL;

    /* check first */
    if(jid_compare_full(jid,list) == 0)
    {
        cur = list->next;
        jid_free(list);
        return cur;
    }

    /* check through the list, stopping at the previous list entry to a matching one */
    cur = list;
    while(cur != NULL)
    {
        if(cur->next == NULL)
            /* none match, so we're done */
            return list;

        if(jid_compare_full(cur->next, jid) == 0)
        {
            /* match, kill it */
            dead = cur->next;
            cur->next = cur->next->next;
            jid_free(dead);

            return list;
        }

        /* loop */
        cur = cur->next;
    }

    /* shouldn't get here */
    return list;
}

/** make a copy of jid, link into list (avoiding dups) */
jid_t jid_append(jid_t list, jid_t jid)
{
    jid_t scan;

    if(list == NULL)
        return jid_dup(jid);

    scan = list;
    while(scan != NULL)
    {
        /* check for dups */
        if(jid_compare_full(scan, jid) == 0)
            return list;

        /* tack it on to the end of the list */
        if(scan->next == NULL)
        {
            scan->next = jid_dup(jid);
            return list;
        }

        scan = scan->next;
    }

    return list;
}
