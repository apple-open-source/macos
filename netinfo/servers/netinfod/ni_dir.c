/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * NetInfo directory (XXX.nidb) handling and database transfer
 * Copyright (C) 1989 by NeXT, Inc.
 *
 * There are three kinds of NetInfo directories. 
 * .nidb extension: A directory in use.
 * .temp extension: Temporary directory being loaded from master server.
 * .move extension: Old directory moved away to make room for new one.
 *
 * The transactional implications on startup are the following:
 * 	1. A ".temp" directory is assumed to be in a partial state of 
 *	   creation and cannot be used UNLESS there is also a ".move"
 *	   directory.
 *
 *	2. A ".move" directory is assumed to be in a partial state of deletion
 *	   and cannot be used. If there is a ".temp" directory, it should
 *	   be renamed ".nidb" and used.
 */
#include <NetInfo/config.h>
#include "ni_server.h"
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <rpc/xdr.h>
#include "ni_globals.h"
#include "ni_file.h"
#include <NetInfo/mm.h>
#include "getstuff.h"
#include <NetInfo/system.h>
#include <NetInfo/system_log.h>
#include <NetInfo/network.h>
#include "event.h"
#include <NetInfo/socket_lock.h>
#include <NetInfo/syslock.h>
#include <NetInfo/systhread.h>

#ifndef S_IRWXU
#define S_IRWXU 0000700
#endif

struct sockaddr_in readall_sin;
ni_name readall_tag = NULL;

#undef NFS_RM_BUG /* hasn't been fixed, just assume no NFS databases */
#define READALL_TIMEOUT 60

static void *new_ni;		/* NetInfo handle for database just received */

/*
 * Destroy a database directory
 */
static int
dir_destroy(char *dir)
{
	char path1[MAXPATHLEN + 1];
#ifdef NFS_RM_BUG
	char path2[MAXPATHLEN + 1];
#endif
	DIR *dp;
	struct direct *d;

	socket_lock();
	dp = opendir(dir);
	socket_unlock();
	if (dp == NULL) return (0);

	while (NULL != (d = readdir(dp)))
	{
		sprintf(path1, "%s/%.*s", dir, d->d_namlen, d->d_name);
#ifdef NFS_RM_BUG
		sprintf(path2, "./%.*s.tmp", d->d_namlen, d->d_name);

		/*
		 * rename, then unlink in case NFS leaves tmp files behind
		 * (.nfs* files, that is).
		 */
		if ((rename(path1, path2) != 0) || (unlink(path2) != 0))
		{
			/* ignore error: rmdir will catch ENOTEMPTY */
		}
#else
		(void)unlink(path1);
#endif
	}

	socket_lock();
	closedir(dp);
	socket_unlock();
	return (rmdir(dir));
}

static const char NI_SUFFIX_CUR[] = ".nidb";
static const char NI_SUFFIX_MOVED[] = ".move";
static const char NI_SUFFIX_TMP[] = ".temp";

/*
 * Returns the three kinds of directory names used by NetInfo
 */
void
dir_getnames(ni_name orig, ni_name *target, ni_name *moved, ni_name *tmp)
{
	if (target != NULL)
	{
		*target = malloc(strlen(orig) + strlen(NI_SUFFIX_CUR) + 1);
		sprintf(*target, "%s%s", orig, NI_SUFFIX_CUR);
	}

	if (moved != NULL)
	{
		*moved = malloc(strlen(orig) + strlen(NI_SUFFIX_MOVED) + 1);
		sprintf(*moved, "%s%s", orig, NI_SUFFIX_MOVED);
	}

	if (tmp != NULL)
	{
		*tmp = malloc(strlen(orig) + strlen(NI_SUFFIX_TMP) + 1);
		sprintf(*tmp, "%s%s", orig, NI_SUFFIX_TMP);
	}
}

/*
 * Switcharoo of directories
 *
 * new database is built in ".temp".
 * old database is moved to ".move".
 * ".temp" is renamed to ".nidb".
 * ".move" is destroyed.
 */
static int
dir_switch(void *ni)
{
	int res;
	int removeit;
	char *target;
	char *moved;
	char *tmp;
	ni_name tag;

	tag = ni_tagname(ni);
	/* close all files */
	ni_free(ni);

	/*
	 * target = tag.nidb
	 * moved = tag.move
	 * tmp = tag.temp
	 */
	dir_getnames(tag, &target, &moved, &tmp);
	ni_name_free(&tag);

	removeit = 1;
	if (access(target, F_OK) == 0)
	{
		/* tag.nidb exists.  rename it to tag.move */
		res = rename(target, moved) == 0;
		if (res < 0)
		{
			system_log(LOG_ERR, "can't rename %s to %s: %m", target, moved);
			goto cleanup;
		}
	}
	else
	{
		/*
		 * yikes!  tag.nidb doesn't exist!
		 */
		system_log(LOG_WARNING, "can't access %s: %m", target);
		removeit = 0;
	}

	/*
	 * now rename tag.temp to tag.nidb
	 */
	res = rename(tmp, target);
	if (res < 0)
	{
		system_log(LOG_ERR, "can't rename %s to %s: %m", tmp, target);
		goto cleanup;
	}
	
	/*
	 * final step: remove tag.move (if there is one)
	 */
	if (removeit)
	{
		res = dir_destroy(moved);
		if (res < 0)
		{
			system_log(LOG_ERR, "can't remove %s: %m", moved);
		}
	}

 cleanup:
	ni_name_free(&target);
	ni_name_free(&moved);
	ni_name_free(&tmp);

	return (res);
}

/*
 * Check to see if anything needs cleaned up (like maybe we crashed
 * while a switch was going on).
 *
 * If ".temp" exists but ".move" does not then destroy ".temp".
 *
 * If ".temp" and ".move" exist, then rename ".temp" to ".nidb" 
 * and destroy ".move". 
 *
 * If ".move" exists and ".temp" does not, then destroy ".move".
 *
 */
void
dir_cleanup(char *domain)
{
	ni_name target = NULL;
	ni_name tmp = NULL;
	ni_name moved = NULL;

	dir_getnames(domain, &target, &moved, &tmp);
	if (access(tmp, F_OK) == 0)
	{
		if (access(moved, F_OK) == 0)
		{
			if (rename(tmp, target) == 0)
			{
				dir_destroy(moved);
			}
		}
		else
		{
			dir_destroy(tmp);
		}
	}
	else if (access(moved, F_OK) == 0)
	{
		dir_destroy(moved);
	}

	ni_name_free(&target);
	ni_name_free(&tmp);
	ni_name_free(&moved);
}

/*
 * Log a failure for the given ID
 */
static void
log_failure(ni_index id, char *message)
{
	if (id == NI_INDEX_NULL)
		system_log(LOG_ERR, "Transfer failed at %s", message);
	else
		system_log(LOG_ERR, "Transfer failed at %s: id=%d", message, id);
}

/*
 * XDR routine to write out a new database.
 */
static bool_t
xdr_writeall(XDR *xdr, char *dir)
{
	ni_status status;
	int more;
	void *fh;
	ni_object object;
	unsigned int highest_id;
	char *tag = NULL;
	unsigned given_checksum;	/* checksum given by master */
	unsigned db_checksum;		/* checksum of current database */

	if (mkdir(dir, S_IRWXU) < 0)
	{
		log_failure(NI_INDEX_NULL, "mkdir");
		return (FALSE);
	}

	status = file_init(dir, &fh);
	if (status != NI_OK)
	{
		log_failure(NI_INDEX_NULL, "file_init");
		return (FALSE);
	}

	if (!xdr_ni_status(xdr, &status))
	{
		log_failure(NI_INDEX_NULL, "status");
		file_free(fh);
		return (FALSE);
	}

	if (status != NI_OK)
	{
		if (status == NI_MASTERBUSY)
		{
			if (db_ni && (tag = ni_tagname(db_ni)))
			{
				system_log(LOG_ERR,
					"Master server for tag %s is busy; will "
					"retry transfer later", tag);
				ni_name_free(&tag);
			}
			else
			{
				system_log(LOG_ERR,
					"Master server is busy; will retry transfer later");
			}
		}
		else
		{
			log_failure(NI_INDEX_NULL, (char *)ni_error(status));
		}

		file_free(fh);
		return (FALSE);
	}

	if (!xdr_u_int(xdr, &given_checksum))
	{
		log_failure(NI_INDEX_NULL, "no checksum");
		file_free(fh);
		return (FALSE);
	}

	db_checksum = ni_getchecksum(db_ni);

	if (given_checksum == 0)
	{
		/*
		 * Database is already up to date
		 *
		 * XXX note assumption, above, that the checksum can never
		 * legitmately be 0.  If it overflows, it CAN be 0!
		 */
		system_log(LOG_DEBUG,
			"reading all from %s/%s: checksums match {%u}",
			inet_ntoa(readall_sin.sin_addr), readall_tag,
			db_checksum);
		file_free(fh);
		return (FALSE);
	}

	system_log(LOG_DEBUG,
		"reading all from %s/%s starting; "
		"old checksum %u, new checksum %u",
		inet_ntoa(readall_sin.sin_addr), readall_tag, db_checksum,
		given_checksum);

	if (!xdr_u_int(xdr, &highest_id))
	{
		log_failure(NI_INDEX_NULL, "no highest_id");
		file_free(fh);
		return (FALSE);
	}

	have_transferred++;
	for (;;)
	{
		if (!xdr_bool(xdr, &more))
		{
			log_failure(object.nio_id.nii_object, "no more");
			file_free(fh);
			return (FALSE);
		}

		if (!more)
		{
			file_free(fh);
			return (TRUE);
		}

		MM_ZERO(&object);
		if (!xdr_ni_object(xdr, &object))
		{
			log_failure(object.nio_id.nii_object, "no object");
			file_free(fh);
			return (FALSE);
		}

		if (file_writecopy(fh, &object) != NI_OK)
		{
			log_failure(object.nio_id.nii_object, "write");
			file_free(fh);
			return (FALSE);
		}

		xdr_free(xdr_ni_object, (char *)&object);
	}
}

/*
 * Reads a new database from the master and writes it out. Can be
 * short-circuited if it is detected that the master's copy is not
 * different than our current version (via checksums).
 */
static int
dir_transfer(struct sockaddr_in *sin, char *tag, char *dir, unsigned checksum)
{
	int status;
	int sock;
	CLIENT *cl;
	struct timeval tv;
	nibind_getregister_res res;

	/*
	 * Use UDP for this query of the binder daemon, if possible, to
	 * decrease network overhead.
	 */
	sock = socket_open(sin, NIBIND_PROG, NIBIND_VERS);
	if (sock < 0)
	{
		sock = socket_connect(sin, NIBIND_PROG, NIBIND_VERS);
		if (sock < 0) return (0);

		system_log(LOG_DEBUG, "dir_transfer using TCP to binder");
		FD_SET(sock, &clnt_fdset);	/* protect client socket */
		cl = clnttcp_create(sin, NIBIND_PROG, NIBIND_VERS, &sock, 0, 0);
	}
	else
	{
		tv.tv_sec = READALL_TIMEOUT;
		tv.tv_usec = 0;
		FD_SET(sock, &clnt_fdset);	/* protect client socket */
		cl = clntudp_create(sin, NIBIND_PROG, NIBIND_VERS, tv, &sock);
	}

	if (cl == NULL)
	{
		socket_close(sock);
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		return (0);
	}

	tv.tv_sec = READALL_TIMEOUT;
	tv.tv_usec = 0;
	if (clnt_call(cl, NIBIND_GETREGISTER, xdr_ni_name, &tag,
		  	xdr_nibind_getregister_res, &res, tv) != RPC_SUCCESS)
	{
		socket_lock();
		clnt_destroy(cl);
		socket_unlock();
		socket_close(sock);
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		return (0);
	}

	socket_lock();
	clnt_destroy(cl);
	socket_unlock();
	socket_close(sock);
	FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
	if (res.status != NI_OK) return (0);

	sin->sin_port = htons(res.nibind_getregister_res_u.addrs.tcp_port);
	sock = socket_connect(sin, NI_PROG, NI_VERS);
	if (sock < 0) return (0);

	FD_SET(sock, &clnt_fdset);	/* protect client socket */
	cl = clnttcp_create(sin, NI_PROG, NI_VERS, &sock, 0, 0);
	if (cl == NULL)
	{
		socket_close(sock);
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		return (0);
	}

	status = clnt_call(cl, _NI_READALL, xdr_u_int, &checksum, xdr_writeall, dir, tv);
	socket_lock();
	clnt_destroy(cl);
	socket_unlock();
	socket_close(sock);
	FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
	return (status == RPC_SUCCESS);
}

/*
 * Information needed by transfer thread
 */
typedef struct transfer_info
{
	struct sockaddr_in sin;
	unsigned checksum;
	ni_name tag;
} transfer_info;

/*
 * For locking
 */
static syslock *transfer_syslock;
static volatile int transfer_inprogress;

/*
 * The transfer thread: Let the server continue serving while the transfer
 * is going on.
 */
static void
transfer(transfer_info *info)
{
	ni_name tmp = NULL;
	ni_name tag = NULL;
	ni_status status;

	tag = ni_tagname(db_ni);
	dir_getnames(tag, NULL, NULL, &tmp);
	reading_all = TRUE;
	readall_sin = info->sin;
	if (readall_tag) ni_name_free(&readall_tag);
	readall_tag = info->tag;
	if (dir_transfer(&info->sin, info->tag, tmp, info->checksum))
	{
		/*
		 * Make sure everything is sunc to disk. 
 		 * XXX: sync() just starts the process - no way to
		 * know when everything is actually sunc. We hope
		 * the database initialization takes longer than
		 * a filsystem sync.
		 */
		sync();

		/*
		 * Init the new database to see if it transferred
		 * correctly.
		 */
		status = ni_init(tmp, &new_ni);
		if (status != NI_OK)
		{
			new_ni = NULL;
			system_log(LOG_ERR, "cannot init new database");
		}
		else
		{
			/*
			 * Successfully initialized
			 */
			event_post();
		}
	}
	else
	{
		new_ni = NULL;
		dir_destroy(tmp);
	}

	ni_name_free(&tag);
	ni_name_free(&tmp);
	MM_FREE(info);

	if (new_ni == NULL)
	{
		/*
		 * If we failed the transfer, unlock now. Otherwise
		 * do not unlock - wait for the main event loop to do
		 * it.
		 */
		syslock_lock(transfer_syslock);
		transfer_inprogress = 0;
		syslock_unlock(transfer_syslock);
	}

	reading_all = FALSE;

	systhread_exit();
}

/*
 * Callback routine to switch to the new database. The svc_run() event
 * checker will call us back.
 */
static void
cb_switch(void)
{
	ni_name dbname = NULL;
	ni_name tag = NULL;

	/*
	 * OK, it seems to work. Let's commit to it now.
	 */
	tag = ni_tagname(db_ni);
	dir_getnames(tag, &dbname, NULL, NULL);
	ni_name_free(&tag);
	if (dir_switch(db_ni) < 0)
	{
		ni_free(new_ni);
		system_log(LOG_ALERT, "couldn't switch directories");
                system_log(LOG_ALERT, "Aborting!");
                abort();
	}
	else
	{
		db_ni = new_ni;
		ni_renamedir(db_ni, dbname);
		ni_name_free(&dbname);
	}

	syslock_lock(transfer_syslock);
	transfer_inprogress = 0;
	syslock_unlock(transfer_syslock);
}


/*
 * For clone, check to see if we are out of date wrt the master.
 */
void
dir_clonecheck(void)
{
	transfer_info *info;
	ni_name tag;
	systhread *t;

	if (have_transferred)
	{
		/*
		 * Do not transfer if already have done so in last
		 * cleanup period.
		 */
		system_log(LOG_INFO,
			"dir_clonecheck: have transferred recently; "
			"doing readall anyway");
	}
	if (transfer_syslock == NULL) transfer_syslock = syslock_new(0);

	/*
	 * Do not check if already transferring
	 */
	syslock_lock(transfer_syslock);
	if (transfer_inprogress)
	{
		syslock_unlock(transfer_syslock);
		return;
	}
	syslock_unlock(transfer_syslock);

	master_addr = getmasteraddr(db_ni, &tag);
	if (master_addr == 0)
	{
		system_log(LOG_ERR, "cannot locate master - transfer failed");
		system_log(LOG_ERR, "Forcing local-only writability");
		master_addr = INADDR_LOOPBACK;
		return;
	}

	event_init(cb_switch);
	MM_ALLOC(info);
	info->checksum = ni_getchecksum(db_ni);
	info->sin.sin_family = AF_INET;
	info->sin.sin_port = 0;
	MM_ZERO(info->sin.sin_zero);
	info->sin.sin_addr.s_addr = master_addr;
	info->tag = tag;

	transfer_inprogress = 1; /* no thread yet, so no need to lock */
	t = systhread_new();
	systhread_set_name(t, "transfer");
	systhread_run(t, (void (*)(void *))transfer, (void *)info);
}

/*
 * Useful routine for insert the given (key, val) pair as a new property
 * the the given property list.
 */
static void
pl_insert(ni_proplist *props, ni_name_const key, ni_name_const val)
{
	ni_property prop;

	MM_ZERO(&prop);
	prop.nip_name = ni_name_dup(key);
	ni_namelist_insert(&prop.nip_val, val, NI_INDEX_NULL);
	ni_proplist_insert(props, prop, NI_INDEX_NULL);
	ni_prop_free(&prop);
}

/*
 * Create a master server
 */
ni_status
dir_mastercreate(char *domain)
{
	ni_status status;
	ni_name dbname = NULL;
	void *ni;
	ni_property prop;
	ni_name masterloc;
	ni_name serves;
	ni_proplist props;
	ni_id mach_id;
	ni_id mast_id;
	ni_name myname;
	ni_id id;
	interface_list_t *l;
	int i;

	dir_getnames(domain, &dbname, NULL, NULL);
	if (mkdir(dbname, S_IRWXU) < 0)
	{
		ni_name_free(&dbname);
		return (NI_SYSTEMERR);
	}

	status = ni_init(dbname, &ni);
	if (status != NI_OK)
	{
		dir_destroy(dbname);
		ni_name_free(&dbname);
		return (status);
	}
	ni_setuser(ni, ACCESS_USER_SUPER);
	ni_name_free(&dbname);

	/*
	 * Initialize the master property to self
	 */
	myname = (ni_name)sys_hostname();
	masterloc = malloc(strlen(myname) + strlen(domain) + 2);
	sprintf(masterloc, "%s/%s", myname, domain);

	status = ni_root(ni, &id);
	if (status != NI_OK)
	{
		goto cleanup;
	}

	MM_ZERO(&prop);
	prop.nip_name = ni_name_dup(NAME_MASTER);
	ni_namelist_insert(&prop.nip_val, masterloc, NI_INDEX_NULL);
	status = ni_createprop(ni, &id, prop, NI_INDEX_NULL);
	ni_prop_free(&prop);
	if (status != NI_OK)
	{
		goto cleanup;
	}

	/*
	 * Create a "machines" directory
	 */
	MM_ZERO(&props);
	pl_insert(&props, NAME_NAME, NAME_MACHINES);
	mach_id.nii_object = NI_INDEX_NULL;
	status = ni_create(ni, &id, props, &mach_id, NI_INDEX_NULL);
	ni_proplist_free(&props);
	if (status != NI_OK)
	{
		goto cleanup;
	}

	/*
	 * Insert self into "machines" directory
	 */
	MM_ZERO(&props);
	pl_insert(&props, NAME_NAME, myname);

	l = sys_interfaces();
	if (l == NULL)
	{
		status = NI_SYSTEMERR;
		goto cleanup;
	}
	
	if (l->count == 0)
	{
		status = NI_SYSTEMERR;
		sys_interfaces_release(l);
		goto cleanup;
	}

	MM_ZERO(&prop);
	prop.nip_name = ni_name_dup(NAME_IP_ADDRESS);

	for (i = 0; i < l->count; i++)
	{
		if ((l->interface[i].flags & IFF_UP) == 0) continue;
		if (l->interface[i].flags & IFF_LOOPBACK) continue;
		ni_namelist_insert(&prop.nip_val, inet_ntoa(l->interface[i].addr), NI_INDEX_NULL);
	}
	
	sys_interfaces_release(l);

	ni_proplist_insert(&props, prop, NI_INDEX_NULL);
	ni_prop_free(&prop);

	serves = malloc(strlen(NAME_DOT) + strlen(domain) + 2);
	sprintf(serves, "%s/%s", NAME_DOT, domain);
	pl_insert(&props, NAME_SERVES, serves);
	ni_name_free(&serves);

	mast_id.nii_object = NI_INDEX_NULL;
	status = ni_create(ni, &mach_id, props, &mast_id, NI_INDEX_NULL);
	ni_proplist_free(&props);
	if (status != NI_OK)
	{
		goto cleanup;
	}

 cleanup:
	ni_free(ni);
	if (status != NI_OK)
	{
		dir_destroy(dbname);
	}
	ni_name_free(&dbname);
	return (status);
}

/*
 * Create a clone server
 */
ni_status
dir_clonecreate(char *domain, char *master_name, char *master_addr,  char *master_domain)
{
	ni_status status;
	ni_name dbname = NULL;
	void *ni;
	ni_property prop;
	ni_name masterloc;
	ni_name serves;
	ni_proplist props;
	ni_id mach_id;
	ni_id mast_id;
	ni_id id;

	dir_getnames(domain, &dbname, NULL, NULL);
	if (mkdir(dbname, S_IRWXU) < 0)
	{
		ni_name_free(&dbname);
		return (NI_SYSTEMERR);
	}

	status = ni_init(dbname, &ni);
	if (status != NI_OK)
	{
		dir_destroy(dbname);
		ni_name_free(&dbname);
		return (status);
	}

	ni_setuser(ni, ACCESS_USER_SUPER);
	ni_name_free(&dbname);

	/*
	 * Initialize master property to real master
	 */
	masterloc = malloc(strlen(master_name) + strlen(master_domain) + 2);
	sprintf(masterloc, "%s/%s", master_name, master_domain);


	status = ni_root(ni, &id);
	if (status != NI_OK)
	{
		goto cleanup;
	}

	MM_ZERO(&prop);
	prop.nip_name = ni_name_dup(NAME_MASTER);
	ni_namelist_insert(&prop.nip_val, masterloc, NI_INDEX_NULL);
	status = ni_createprop(ni, &id, prop, NI_INDEX_NULL);
	ni_prop_free(&prop);
	if (status != NI_OK) {
		goto cleanup;
	}

	/*
	 * Create "machines" directory
	 */
	MM_ZERO(&props);
	pl_insert(&props, NAME_NAME, NAME_MACHINES);
	mach_id.nii_object = NI_INDEX_NULL;
	status = ni_create(ni, &id, props, &mach_id, NI_INDEX_NULL);
	ni_proplist_free(&props);
	if (status != NI_OK)
	{
		goto cleanup;
	}

	/*
	 * Put master in "machines" directory
	 */
	MM_ZERO(&props);
	pl_insert(&props, NAME_NAME, master_name);
	pl_insert(&props, NAME_IP_ADDRESS, master_addr);
	serves = malloc(strlen(NAME_DOT) + strlen(master_domain) + 2);
	sprintf(serves, "%s/%s", NAME_DOT, master_domain);
	pl_insert(&props, NAME_SERVES, serves);
	ni_name_free(&serves);
	mast_id.nii_object = NI_INDEX_NULL;
	status = ni_create(ni, &mach_id, props, &mast_id, NI_INDEX_NULL);
	ni_proplist_free(&props);
	if (status != NI_OK)
	{
		goto cleanup;
	}

 cleanup:
	ni_free(ni);
	if (status != NI_OK) {
		dir_destroy(dbname);
	}
	ni_name_free(&dbname);
	return (status);
}
