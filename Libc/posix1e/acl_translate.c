/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <sys/appleapiopts.h>
#include <sys/types.h>
#include <sys/acl.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <membership.h>
#include <pwd.h>
#include <grp.h>

#include "aclvar.h"

ssize_t
acl_copy_ext(void *buf, acl_t acl, ssize_t size)
{
	struct kauth_filesec *ext = (struct kauth_filesec *)buf;
	ssize_t		reqsize;
	int		i;

	/* validate arguments, compute required size */
	reqsize = acl_size(acl);
	if (reqsize < 0)
		return(-1);
	if (reqsize > size) {
		errno = ERANGE;
		return(-1);
	}
		
	/* export the header */
	ext->fsec_magic = KAUTH_FILESEC_MAGIC;
	ext->fsec_entrycount = acl->a_entries;
	ext->fsec_flags = acl->a_flags;
	/* XXX owner? */
	
	/* copy ACEs */
	for (i = 0; i < acl->a_entries; i++) {
		/* ACE contents are almost identical */
		ext->fsec_ace[i].ace_applicable = acl->a_ace[i].ae_applicable;
		ext->fsec_ace[i].ace_flags =
		    (acl->a_ace[i].ae_tag & KAUTH_ACE_KINDMASK) |
		    (acl->a_ace[i].ae_flags & ~KAUTH_ACE_KINDMASK);
		ext->fsec_ace[i].ace_rights = acl->a_ace[i].ae_perms;
	}		

	return(reqsize);
}

acl_t
acl_copy_int(const void *buf)
{
	struct kauth_filesec *ext = (struct kauth_filesec *)buf;
	acl_t		ap;
	int		i;

	if (ext->fsec_magic != KAUTH_FILESEC_MAGIC) {
		errno = EINVAL;
		return(NULL);
	}

	if ((ap = acl_init(ext->fsec_entrycount)) != NULL) {
		/* copy useful header fields */
		ap->a_flags = ext->fsec_flags;
		ap->a_entries = ext->fsec_entrycount;
		/* copy ACEs */
		for (i = 0; i < ap->a_entries; i++) {
			/* ACE contents are literally identical */
/* XXX Consider writing the magic out to the persistent store  
 * to detect corruption
 */
			ap->a_ace[i].ae_magic = _ACL_ENTRY_MAGIC;
			ap->a_ace[i].ae_applicable = ext->fsec_ace[i].ace_applicable;
			ap->a_ace[i].ae_flags = ext->fsec_ace[i].ace_flags & ~KAUTH_ACE_KINDMASK;
			ap->a_ace[i].ae_tag = ext->fsec_ace[i].ace_flags & KAUTH_ACE_KINDMASK;
			ap->a_ace[i].ae_perms = ext->fsec_ace[i].ace_rights;
		}
	}
	return(ap);
}

#define ACL_TYPE_DIR	(1<<0)
#define ACL_TYPE_FILE	(1<<1)
#define ACL_TYPE_ACL	(1<<2)

static struct {
	acl_perm_t	perm;
	char		*name;
	int		type;
} acl_perms[] = {
	{ACL_READ_DATA,		"read",		ACL_TYPE_FILE},
//	{ACL_LIST_DIRECTORY,	"list",		ACL_TYPE_DIR},
	{ACL_WRITE_DATA,	"write",	ACL_TYPE_FILE},
//	{ACL_ADD_FILE,		"add_file",	ACL_TYPE_DIR},
	{ACL_EXECUTE,		"execute",	ACL_TYPE_FILE},
//	{ACL_SEARCH,		"search",	ACL_TYPE_DIR},
	{ACL_DELETE,		"delete",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_APPEND_DATA,	"append",	ACL_TYPE_FILE},
//	{ACL_ADD_SUBDIRECTORY,	"add_subdirectory", ACL_TYPE_DIR},
	{ACL_DELETE_CHILD,	"delete_child",	ACL_TYPE_DIR},
	{ACL_READ_ATTRIBUTES,	"readattr",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_WRITE_ATTRIBUTES,	"writeattr",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_READ_EXTATTRIBUTES, "readextattr",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_WRITE_EXTATTRIBUTES, "writeextattr", ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_READ_SECURITY,	"readsecurity",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_WRITE_SECURITY,	"writesecurity", ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_CHANGE_OWNER,	"chown",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{0, NULL, 0}
};

static struct {
	acl_flag_t	flag;
	char		*name;
	int		type;
} acl_flags[] = {
	{ACL_ENTRY_INHERITED,		"inherited",		ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_FLAG_DEFER_INHERIT,	"defer_inherit",	ACL_TYPE_ACL},
	{ACL_ENTRY_FILE_INHERIT,	"file_inherit",		ACL_TYPE_DIR},
	{ACL_ENTRY_DIRECTORY_INHERIT,	"directory_inherit",	ACL_TYPE_DIR},
	{ACL_ENTRY_LIMIT_INHERIT,	"limit_inherit",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_ENTRY_ONLY_INHERIT,	"only_inherit",		ACL_TYPE_DIR},
	{0, NULL, 0}
};

/*
 * reallocing snprintf with offset
 */

static int
raosnprintf(char **buf, size_t *size, ssize_t *offset, char *fmt, ...)
{
    va_list ap;
    int ret;

    do
    {
	if (*offset < *size)
	{
	    va_start(ap, fmt);
	    ret = vsnprintf(*buf + *offset, *size - *offset, fmt, ap);
	    va_end(ap);
	    if (ret < (*size - *offset))
	    {
		*offset += ret;
		return ret;
	    }
	}
	*buf = reallocf(*buf, (*size *= 2));
    } while (*buf);

    //warn("reallocf failure");
    return 0;
}

static char *
uuid_to_name(uuid_t *uu, uid_t *id, int *isgid)
{
    struct group *tgrp = NULL;
    struct passwd *tpass = NULL;

    if (0 == mbr_uuid_to_id(*uu, id, isgid))
    {
	switch (*isgid)
	{
	    case ID_TYPE_UID:
		if (!(tpass = getpwuid(*id)))
		    goto errout;
		return strdup(tpass->pw_name);
		break;
	    case ID_TYPE_GID:
		if (!(tgrp = getgrgid((gid_t) *id)))
		    goto errout;
		return strdup(tgrp->gr_name);
		break;
	    default:
errout:		;    //warn("Unable to translate qualifier on ACL\n");
	}
    }
    return strdup("");
}

acl_t
acl_from_text(const char *buf_p)
{
    int i, error = 0, need_tag = 1, ug_tag = -1;
    char *buf;
    char *entry, *field, *sub,
	*last_field, *last_entry, *last_sub;
    uuid_t *uu;
    struct passwd *tpass = NULL;
    struct group *tgrp = NULL;
    acl_entry_t acl_entry;
    acl_flagset_t flags = NULL;
    acl_permset_t perms = NULL;
    acl_tag_t tag;
    acl_t acl_ret;

    if ((acl_ret = acl_init(1)) == NULL)
	return NULL;

    if (buf_p == NULL)
	return NULL;

    if ((buf = strdup(buf_p)) == NULL)
	return NULL;

    /* acl flags */
    if ((entry = strtok_r(buf, "\n", &last_entry)) != NULL)
    {
	/* stamp */
	field = strtok_r(entry, " ", &last_field);
	if (field && strncmp(field, "!#acl", strlen("!#acl")))
	{
	    error = EINVAL;
	    goto exit;
	}

	/* version */
	field = strtok_r(NULL, " ", &last_field);
	errno = 0;
	if (field == NULL || strtol(field, NULL, 0) != 1)
	{
	    error = EINVAL;
	    goto exit;
	}

	/* optional flags */
	if((field = strtok_r(NULL, " ", &last_field)) != NULL)
	{
	    acl_get_flagset_np(acl_ret, &flags);
	    for (sub = strtok_r(field, ",", &last_sub); sub;
		 sub = strtok_r(NULL, ",", &last_sub))
	    {
		for (i = 0; acl_flags[i].name != NULL; ++i)
		{
		    if (acl_flags[i].type & ACL_TYPE_ACL
			    && !strcmp(acl_flags[i].name, sub))
		    {
			acl_add_flag_np(flags, acl_flags[i].flag);
			break;
		    }
		}
		if (acl_flags[i].name == NULL)
		{
		    /* couldn't find flag */
		    error = EINVAL;
		    goto exit;
		}
	    }
	}
    }

    for (entry = strtok_r(NULL, "\n", &last_entry); entry;
	 entry = strtok_r(NULL, "\n", &last_entry))
    {
	field = strtok_r(entry, ":", &last_field);

	if((uu = calloc(1, sizeof(uuid_t))) == NULL)
	    goto exit;

	if(acl_create_entry(&acl_ret, &acl_entry))
	    goto exit;

	acl_get_flagset_np(acl_entry, &flags);
	acl_get_permset(acl_entry, &perms);

	switch(*field)
	{
	    case 'u':
		if(!strncmp(buf, "user", strlen(field)))
		    ug_tag = ID_TYPE_UID;
		break;
	    case 'g':
		if(!strncmp(buf, "group", strlen(field)))
		    ug_tag = ID_TYPE_GID;
		break;
	    
	}

	/* uuid */
	if ((field = strtok_r(NULL, ":", &last_field)) != NULL)
	{
	    mbr_string_to_uuid(field, *uu);
	    need_tag = 0;
	}
	/* name */
	if (*last_field == ':')  // empty username field
	    last_field++;
	else if ((field = strtok_r(NULL, ":", &last_field)) != NULL && need_tag)
	{
	    switch(ug_tag)
	    {
		case ID_TYPE_UID:
		    if((tpass = getpwnam(field)) != NULL)
			if (mbr_uid_to_uuid(tpass->pw_uid, *uu) != 0)
			{
			    error = EINVAL;
			    goto exit;
			}
		    break;
		case ID_TYPE_GID:
		    if ((tgrp = getgrnam(field)) != NULL)
			if (mbr_gid_to_uuid(tgrp->gr_gid, *uu) != 0)
			{
			    error = EINVAL;
			    goto exit;
			}
		    break;
	    }
	    need_tag = 0;
	}
	/* uid */
	if (*last_field == ':') // empty uid field
	    last_field++;
	else if ((field = strtok_r(NULL, ":", &last_field)) != NULL && need_tag)
	{
	    uid_t id;
	    error = 0;

	    if((id = strtol(field, NULL, 10)) == 0 && error)
	    {
		error = EINVAL;
		goto exit;
	    }

	    switch(ug_tag)
	    {
		case ID_TYPE_UID:
		    if((tpass = getpwuid((uid_t)id)) != NULL)
			if (mbr_uid_to_uuid(tpass->pw_uid, *uu) != 0)
			{
			    error = EINVAL;
			    goto exit;
			}
		    break;
		case ID_TYPE_GID:
		    if ((tgrp = getgrgid((gid_t)id)) != NULL)
			if (mbr_gid_to_uuid(tgrp->gr_gid, *uu) != 0)
			{
			    error = EINVAL;
			    goto exit;
			}
		    break;
	    }
	    need_tag = 0;
	}

	/* nothing do set as qualifier */
	if (need_tag)
	{
	    error = EINVAL;
	    goto exit;
	}

	/* flags */
	if((field = strtok_r(NULL, ":", &last_field)) == NULL)
	{
	    error = EINVAL;
	    goto exit;
	}

	for (tag = 0, sub = strtok_r(field, ",", &last_sub); sub;
	     sub = strtok_r(NULL, ",", &last_sub))
	{
	    if (!tag && !strcmp(sub, "allow")) {
		    tag = ACL_EXTENDED_ALLOW;
		    continue;
	    } else if (!tag && !strcmp(sub, "deny")) {
		    tag = ACL_EXTENDED_DENY;
		    continue;
	    }
	    for (i = 0; acl_flags[i].name != NULL; ++i)
	    {
		if (acl_flags[i].type & (ACL_TYPE_FILE | ACL_TYPE_DIR)
			&& !strcmp(acl_flags[i].name, sub))
		{
		    acl_add_flag_np(flags, acl_flags[i].flag);
		    break;
		}
	    }
	    if (acl_flags[i].name == NULL)
	    {
		/* couldn't find perm */
		error = EINVAL;
		goto exit;
	    }
	}

	if((field = strtok_r(NULL, ":", &last_field)) != NULL) {
	    for (sub = strtok_r(field, ",", &last_sub); sub;
		 sub = strtok_r(NULL, ",", &last_sub))
	    {
		for (i = 0; acl_perms[i].name != NULL; i++)
		{
		    if (acl_perms[i].type & (ACL_TYPE_FILE | ACL_TYPE_DIR)
			    && !strcmp(acl_perms[i].name, sub))
		    {
			acl_add_perm(perms, acl_perms[i].perm);
			break;
		    }
		}
		if (acl_perms[i].name == NULL)
		{
		    /* couldn't find perm */
		    error = EINVAL;
		    goto exit;
		}
	    }
	}
	acl_set_tag_type(acl_entry, tag);
	acl_set_qualifier(acl_entry, *uu);
    }
exit:
    free(buf);
    if (error)
    {
	acl_free(acl_ret);
	acl_ret = NULL;
	errno = error;
    }
    return acl_ret;
}

char *
acl_to_text(acl_t acl, ssize_t *len_p)
{
	uuid_t *uu;
	acl_tag_t tag;
	acl_entry_t entry = NULL;
	acl_flagset_t flags;
	acl_permset_t perms;
	uid_t id;
	char *str, uu_str[256];
	int i, first;
	int isgid;
	size_t bufsize = 1024;
	char *buf;

	if (!_ACL_VALID_ACL(acl)) {
		errno = EINVAL;
		return NULL;
	}

	buf = malloc(bufsize);
	if (len_p == NULL)
	    len_p = alloca(sizeof(ssize_t));

	*len_p = 0;

	if (!raosnprintf(&buf, &bufsize, len_p, "!#acl %d", 1))
	    return NULL;

	if (acl_get_flagset_np(acl, &flags) == 0)
	{
	    for (i = 0, first = 0; acl_flags[i].name != NULL; ++i)
	    {
		if (acl_flags[i].type & ACL_TYPE_ACL
			&& acl_get_flag_np(flags, acl_flags[i].flag) != 0)
		{
		    if(!raosnprintf(&buf, &bufsize, len_p, "%s%s",
			    first++ ? "," : " ", acl_flags[i].name))
			return NULL;
		}
	    }
	}
	for (;acl_get_entry(acl,
		    entry == NULL ? ACL_FIRST_ENTRY : ACL_NEXT_ENTRY, &entry) == 0;)
	{
	    if (((uu = (uuid_t *) acl_get_qualifier(entry)) == NULL)
		|| (acl_get_tag_type(entry, &tag) != 0)
		|| (acl_get_flagset_np(entry, &flags) != 0)
		|| (acl_get_permset(entry, &perms) != 0))
		continue;

	    str = uuid_to_name(uu, &id, &isgid);
	    mbr_uuid_to_string(uu, uu_str); // XXX how big should uu_str be? // XXX error?

	    if(!raosnprintf(&buf, &bufsize, len_p, "\n%s:%s:%s:%d:%s",
		isgid ? "group" : "user",
		uu_str,
		str,
		id,
		(tag == ACL_EXTENDED_ALLOW) ? "allow" : "deny"))
		return NULL;

	    free(str);

	    for (i = 0; acl_flags[i].name != NULL; ++i)
	    {
		if (acl_flags[i].type & (ACL_TYPE_DIR | ACL_TYPE_FILE))
		{
		    if(acl_get_flag_np(flags, acl_flags[i].flag) != 0)
		    {
			if(!raosnprintf(&buf, &bufsize, len_p, ",%s",
			    acl_flags[i].name))
			    return NULL;
		    }
		}
	    }

	    for (i = 0, first = 0; acl_perms[i].name != NULL; ++i)
	    {
		if (acl_perms[i].type & (ACL_TYPE_DIR | ACL_TYPE_FILE))
		{
		    if(acl_get_perm_np(perms, acl_perms[i].perm) != 0)
		    {
			if(!raosnprintf(&buf, &bufsize, len_p, "%s%s",
			    first++ ? "," : ":",
			    acl_perms[i].name))
			    return NULL;
		    }
		}
	    }
	}
	buf[(*len_p)++] = '\n';
	buf[(*len_p)] = 0;
	return buf;
}

ssize_t
acl_size(acl_t acl)
{
	_ACL_VALIDATE_ACL(acl);

	return(_ACL_HEADER_SIZE + acl->a_entries * _ACL_ENTRY_SIZE);
}
