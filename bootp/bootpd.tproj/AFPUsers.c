/*
 * AFPUsers.c
 * - create/maintain AFP logins
 */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <mach/boolean.h>
#include <sys/errno.h>
#include <limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>

#include "netinfo.h"
#include "NICache.h"
#include "NICachePrivate.h"
#include "AFPUsers.h"
#include "NetBootServer.h"

extern void
my_log(int priority, const char *message, ...);


#define	BSDPD_CREATOR		"bsdpd"
#define MAX_RETRY		5

char *
EncryptPassword(
    char	*szpPass,
    char	*szpCrypt)
{
    static char		*_szpSalts = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./" ;
    char		szSalt [9] ;

    if (!szpCrypt)
        return "*" ;
    if (!szpPass) {
        *szpCrypt = '*' ;
        szpCrypt[1] = '\0' ;
        return szpCrypt ;
    }

    // If the password is blank, don't hash it.
    if (!*szpPass) {
        *szpCrypt = '\0' ;
        return szpCrypt ;
    }

    // Generate a random salt. (Algorithm from passwd command.)
    szSalt[0] = _szpSalts[random() % strlen (_szpSalts)] ;
    szSalt[1] = _szpSalts[random() % strlen (_szpSalts)] ;
    szSalt[2] = '\0' ;

    strcpy (szpCrypt, crypt (szpPass, szSalt)) ;
    return szpCrypt ;
}

void
AFPUsers_free(AFPUsers_t * users)
{
    PLCache_free(&users->list);
    bzero(users, sizeof(*users));
}

static boolean_t
S_commit_mods(void * handle, PLCacheEntry_t * entry)
{
    int			i;
    ni_status 		status = NI_OK;

    for (i = 0; i < MAX_RETRY; i++) {
	status = ni_write(handle, &entry->dir, entry->pl);
	if (status == NI_STALE) { /* refresh and try again */
	    ni_self(handle, &entry->dir);
	    continue;
	}
	break;
    }
    if (status != NI_OK) {
	my_log(LOG_ERR, "AFPUsers: ni_write failed (attempts %d), %s",
	       i + 1, ni_error(status));
	return (FALSE);
    }
    return (TRUE);
}

boolean_t
AFPUsers_set_password(AFPUsers_t * users, PLCacheEntry_t * entry,
		      u_char * passwd)
{
    char 		crypted_passwd[256];	

    ni_set_prop(&entry->pl, NIPROP_PASSWD, 
		EncryptPassword(passwd, crypted_passwd), NULL);
    if (S_commit_mods(NIDomain_handle(users->domain), entry)
	== FALSE) {
	return (FALSE);
    }
    return (TRUE);
}

boolean_t
AFPUsers_init(AFPUsers_t * users, NIDomain_t * domain)
{
    int			i;
    ni_idlist		id_list;
    ni_status 		status;

    NI_INIT(&id_list);
    bzero(users, sizeof(*users));
    PLCache_init(&users->list);
#define ARBITRARILY_LARGE_NUMBER	(100 * 1024 * 1024)
    PLCache_set_max(&users->list, ARBITRARILY_LARGE_NUMBER);
    
    /* make sure the path is there already */
    status = ni_pathsearch(NIDomain_handle(domain), &users->dir,
			   NIDIR_USERS);
    if (status != NI_OK) {
	my_log(LOG_INFO, "bsdp: netinfo dir '%s', %s",
	       NIDIR_USERS, ni_error(status));
	goto failed;
    }

    users->domain = domain;
    status = ni_lookup(NIDomain_handle(domain), &users->dir,
		       NIPROP__CREATOR, BSDPD_CREATOR, &id_list);
    if (status != NI_OK) {
	/* no entries matched */
    }
    else {
	for (i = 0; i < id_list.niil_len; i++) {
	    ni_id		dir;
	    ni_proplist 	pl;
	    
	    NI_INIT(&pl);
	    dir.nii_object = id_list.niil_val[i];
	    status = ni_read(NIDomain_handle(domain), &dir, &pl);
	    if (status == NI_OK)
		PLCache_append(&users->list, PLCacheEntry_create(dir, pl));
	    ni_proplist_free(&pl);
	}
    }
    ni_idlist_free(&id_list);
    return (TRUE);
 failed:
    ni_idlist_free(&id_list);
    AFPUsers_free(users);
    return (FALSE);
}

static __inline__ boolean_t
S_uid_taken(ni_entrylist * id_list, uid_t uid)
{
    int 		i;

    for (i = 0; i < id_list->niel_len; i++) {
	ni_namelist * 	nl_p = id_list->niel_val[i].names;
	uid_t		user_id;

	if (nl_p == NULL || nl_p->ninl_len == 0)
	    continue;

	user_id = strtoul(nl_p->ninl_val[0], NULL, NULL);
	if (user_id == uid)
	    return (TRUE);
    }
    return (getpwuid(uid) != NULL);
}

boolean_t
AFPUsers_create(AFPUsers_t * users, gid_t gid,
		uid_t start, int count)
{
    char		buf[64];
    ni_entrylist	id_list;
    int			need;
    ni_proplist		pl;
    boolean_t		ret = FALSE;
    ni_status		status;
    uid_t		scan;

    if (PLCache_count(&users->list) >= count)
	return (TRUE); /* already sufficient users */

    need = count - PLCache_count(&users->list);

    NI_INIT(&id_list);
    NI_INIT(&pl);
    status = ni_list(NIDomain_handle(users->domain), &users->dir,
		     NIPROP_UID, &id_list);
    if (status != NI_OK) {
	my_log(LOG_INFO, "bsdp: couldn't get list of user ids, %s",
	       ni_error(status));
	goto failed;
    }
    ni_set_prop(&pl, NIPROP_SHELL, (ni_name)"/bin/false", NULL);
    snprintf(buf, sizeof(buf), "%d", gid);
    ni_set_prop(&pl, NIPROP_GID, buf, NULL);
    ni_set_prop(&pl, NIPROP_PASSWD, "*", NULL);
    ni_set_prop(&pl, NIPROP__CREATOR, BSDPD_CREATOR, NULL);

    for (scan = start; need > 0; scan++) {
	ni_id		child;
	char		user[256];

	if (S_uid_taken(&id_list, scan)) {
	    continue;
	}

	snprintf(buf, sizeof(buf), "%d", scan);
	ni_set_prop(&pl, NIPROP_UID, buf, NULL);
	snprintf(user, sizeof(user), NETBOOT_USER_PREFIX "%03d", scan);
	ni_set_prop(&pl, NIPROP_NAME, user, NULL);
	ni_set_prop(&pl, NIPROP_REALNAME, user, NULL);
	{
	    int		i;

	    for (i = 0; i < MAX_RETRY; i++) {
		status = ni_create(NIDomain_handle(users->domain), 
				   &users->dir, pl, &child, NI_INDEX_NULL);
		if (status == NI_STALE) {
		    ni_self(NIDomain_handle(users->domain), 
			    &users->dir);
		    continue;
		}
		break;
	    }
	}
	if (status != NI_OK) {
	    my_log(LOG_INFO, "AFPUsers_create: create %s failed, %s",
		   user, ni_error(status));
	    goto failed;
	}
	PLCache_append(&users->list, PLCacheEntry_create(child, pl));
	need--;
    }
    ret = TRUE;

 failed:
    ni_entrylist_free(&id_list);
    ni_proplist_free(&pl);
    return (ret);
}

void
AFPUsers_print(AFPUsers_t * users)
{
    PLCache_print(&users->list);
}

#ifdef TEST_AFPUSERS
int 
main(int argc, char * argv[])
{
    AFPUsers_t 		users;
    NIDomain_t *	domain;
    struct group *	group_ent_p;
    int			count;
    int			start;

    if (argc < 4) {
	printf("usage: AFPUsers domain user_count start\n");
	exit(1);
    }

    group_ent_p = getgrnam(NETBOOT_GROUP);
    if (group_ent_p == NULL) {
	printf("Group '%s' missing\n", NETBOOT_GROUP);
	exit(1);
    }

    count = strtol(argv[2], NULL, 0);
    if (count < 0 || count > 100) {
	printf("invalid user_count\n");
	exit(1);
    }
    start = strtol(argv[3], NULL, 0);
    if (start <= 0) {
	printf("invalid start\n");
	exit(1);
    }
    domain = NIDomain_init(argv[1]);
    if (domain == NULL) {
	fprintf(stderr, "open %s failed\n", argv[1]);
	exit(1);
    }
    AFPUsers_init(&users, domain);
    AFPUsers_print(&users);
    AFPUsers_create(&users, group_ent_p->gr_gid, start, count);
    AFPUsers_print(&users);
    AFPUsers_free(&users);
    exit(0);
    return (0);
}

void
my_log(int priority, const char *message, ...)
{
    va_list 		ap;

    va_start(ap, message);
    vprintf(message, ap);
    return;
}

#endif TEST_AFPUSERS
