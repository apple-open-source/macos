/**
 * \file idlist.c -- string list handling
 *
 * For license terms, see the file COPYING in this directory.
 */

#include "config.h"

#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <limits.h>
#if defined(STDC_HEADERS)
#include <stdlib.h>
#include <string.h>
#endif
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#include "fetchmail.h"

/** Save string \a str to idlist \a idl with status \a status.
 * \return Pointer to the last element of the list to help the quick,
 * constant-time addition to the list. */
/*@shared@*/ static
struct idlist **save_str_quick(/*@shared@*/ struct idlist **idl,
			       /*@only@*/ char *str /** caller-allocated string */, flag status)
/* save a number/UID pair on the given UID list */
{
    struct idlist **end;

    /* do it nonrecursively so the list is in the right order */
    for (end = idl; *end; end = &(*end)->next)
	continue;

    *end = (struct idlist *)xmalloc(sizeof(struct idlist));
    (*end)->id = str;
    (*end)->val.status.mark = status;
    (*end)->val.status.num = 0;
    (*end)->next = NULL;

    return end;
}

/** Save string \a str to idlist \a idl with status \a status.
 * \return the end list element for direct modification. */
struct idlist *save_str(struct idlist **idl, const char *str /** implicitly strdup()ed */, flag status)
{
    return *save_str_quick(idl, str ? xstrdup(str) : NULL, status);
}

/** Free string list \a idl and free each of the id members. */
void free_str_list(struct idlist **idl)
{
    struct idlist *i = *idl;

    while(i) {
	struct idlist *t = i->next;
	free(i->id);
	free(i);
	i = t;
    }
    *idl = 0;
}

/** Save an ID pair made of \a str1 and \a str2 on the given idlist \a idl. */
void save_str_pair(struct idlist **idl, const char *str1, const char *str2)
{
    struct idlist **end;

    /* do it nonrecursively so the list is in the right order */
    for (end = idl; *end; end = &(*end)->next)
	continue;

    *end = (struct idlist *)xmalloc(sizeof(struct idlist));
    (*end)->id = str1 ? xstrdup(str1) : (char *)NULL;
    if (str2)
	(*end)->val.id2 = xstrdup(str2);
    else
	(*end)->val.id2 = (char *)NULL;
    (*end)->next = (struct idlist *)NULL;
}

#ifdef __UNUSED__
void free_str_pair_list(struct idlist **idl)
/* free the given ID pair list */
{
    if (*idl == (struct idlist *)NULL)
	return;

    free_idpair_list(&(*idl)->next);
    free ((*idl)->id);
    free ((*idl)->val.id2);
    free(*idl);
    *idl = (struct idlist *)NULL;
}
#endif

/** Check if ID \a str is in idlist \a idl. \return idlist entry if found,
 * NULL if not found. */
struct idlist *str_in_list(struct idlist **idl, const char *str,
const flag caseblind /** if true, use strcasecmp, if false, use strcmp */)
{
    struct idlist *walk;
    if (caseblind) {
	for( walk = *idl; walk; walk = walk->next )
	    if( strcasecmp( str, walk->id) == 0 )
		return walk;
    } else {
	for( walk = *idl; walk; walk = walk->next )
	    if( strcmp( str, walk->id) == 0 )
		return walk;
    }
    return NULL;
}

/** \return position of first occurrence of \a str in idlist \a idl */
int str_nr_in_list(struct idlist **idl, const char *str)
{
    int nr;
    struct idlist *walk;

    if (!str)
        return -1;
    for (walk = *idl, nr = 0; walk; nr ++, walk = walk->next)
        if (strcmp(str, walk->id) == 0)
	    return nr;
    return -1;
}

/** \return position of last occurrence of \a str in idlist \a idl */
int str_nr_last_in_list( struct idlist **idl, const char *str)
{
    int nr, ret = -1;
    struct idlist *walk;
    if ( !str )
        return -1;
    for( walk = *idl, nr = 0; walk; nr ++, walk = walk->next )
        if( strcmp( str, walk->id) == 0 )
	    ret = nr;
    return ret;
}

/** Update the mark of an id \a str in idlist \a idl to given value \a val. */
void str_set_mark( struct idlist **idl, const char *str, const flag val)
{
    int nr;
    struct idlist *walk;
    if (!str)
        return;
    for(walk = *idl, nr = 0; walk; nr ++, walk = walk->next)
        if (strcmp(str, walk->id) == 0)
	    walk->val.status.mark = val;
}

/** Count the number of elements in the idlist \a idl. 
 * \return number of elements */
int count_list(struct idlist **idl)
{
	int i = 0;
	struct idlist *it;

	for (it = *idl ; it ; it = it->next)
		++i;

	return i;
}

/** return the \a number'th id string on idlist \a idl */
/*@null@*/ char *str_from_nr_list(struct idlist **idl, long number)
{
    if( !*idl  || number < 0)
        return 0;
    if( number == 0 )
        return (*idl)->id;
    return str_from_nr_list(&(*idl)->next, number-1);
}


/** Search idlist \a idl for entry with given \a number.
 * \return id member of idlist entry. */
char *str_find(struct idlist **idl, long number)
{
    if (*idl == (struct idlist *) 0)
	return((char *) 0);
    else if (number == (*idl)->val.status.num)
	return((*idl)->id);
    else
	return(str_find(&(*idl)->next, number));
}

/** Search idlist \a idl for entry with given \a number.
 * \return idlist entry. */
struct idlist *id_find(struct idlist **idl, long number)
{
    struct idlist	*idp;
    for (idp = *idl; idp; idp = idp->next)
	if (idp->val.status.num == number)
	    return(idp);
    return(0);
}

/** Return the id of the given \a id in the given idlist \a idl, comparing
 * case insensitively. \returns the respective other \a idlist member (the one
 * that was not searched for). */
char *idpair_find(struct idlist **idl, const char *id)
{
    if (*idl == (struct idlist *) 0)
	return((char *) 0);
    else if (strcasecmp(id, (*idl)->id) == 0)
	return((*idl)->val.id2 ? (*idl)->val.id2 : (*idl)->id);
    else
	return(idpair_find(&(*idl)->next, id));
}

/** Mark message number \a num on given idlist \a idl as deleted.
 * \return 1 if found, 0 if not found. */
int delete_str(struct idlist **idl, long num)
{
    struct idlist	*idp;

    for (idp = *idl; idp; idp = idp->next)
	if (idp->val.status.num == num)
	{
	    idp->val.status.mark = UID_DELETED;
	    return(1);
	}
    return(0);
}

/** Copy the given UID list \a idl. \return A newly malloc()ed copy of the list. */
struct idlist *copy_str_list(struct idlist *idl)
{
    struct idlist *newnode ;

    if (idl == (struct idlist *)NULL)
	return(NULL);
    else
    {
	newnode = (struct idlist *)xmalloc(sizeof(struct idlist));
	memcpy(newnode, idl, sizeof(struct idlist));
	newnode->next = copy_str_list(idl->next);
	return(newnode);
    }
}

/** Append \a nidl to \a idl (does not copy *) */
void append_str_list(struct idlist **idl, struct idlist **nidl)
{
    if ((*nidl) == (struct idlist *)NULL || *nidl == *idl)
	return;
    else if ((*idl) == (struct idlist *)NULL)
	*idl = *nidl;
    else if ((*idl)->next == (struct idlist *)NULL)
	(*idl)->next = *nidl;
    else if ((*idl)->next != *nidl)
	append_str_list(&(*idl)->next, nidl);
}

/* idlist.c ends here */
