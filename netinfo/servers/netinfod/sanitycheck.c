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
 * NetInfo clone sanity check
 * Copyright 1994, NeXT Computer Inc.
 */
#include <NetInfo/config.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinfo/ni.h>
#include <stdio.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <NetInfo/system_log.h>
#include <NetInfo/network.h>
#include "ni_globals.h"
#include "getstuff.h"
#include "sanitycheck.h"
#include <NetInfo/socket_lock.h>

#define READALL_TIMEOUT 60

ni_status checkserves(unsigned long, ni_name, unsigned long, ni_name);
extern unsigned long sys_address();

void sanitycheck(ni_name mytag)
{
	ni_name master, mastertag;
	unsigned long masteraddr;
	interface_list_t *l;
	int i;

	if (!getmaster(db_ni, &master, &mastertag))
	{
		/* no master! */
		system_log(LOG_ERR, "tag %s: can't determine master's host name", mytag);
		return;
	}

	masteraddr = getaddress(db_ni, master);
	if (masteraddr == 0)
	{
		/* no address for master */
		system_log(LOG_ERR, "tag %s: can't determine master's address", mytag);
		return;
	}

	l = sys_interfaces();
	if (l == NULL) return;

	for (i = 0; i < l->count; i++)
	{
		if ((l->interface[i].flags & IFF_UP) == 0) continue;
		if (l->interface[i].flags & IFF_LOOPBACK) continue;

		checkserves(masteraddr, mastertag, l->interface[i].addr.s_addr, mytag);
	}

	sys_interfaces_release(l);
}

ni_status checkserves(unsigned long masteraddr, ni_name mastertag, unsigned long myaddr, ni_name mytag)
{
	struct sockaddr_in mastersin;
	struct in_addr myinaddr;
	char myserves[MAXPATHLEN + 16];
	ni_index where;
	int status;
	int sock;
	CLIENT *cl;
	struct timeval tv;
	nibind_getregister_res res;
	ni_id_res id_res;
	ni_lookup_res lu_res;
	ni_lookup_args childdir;
	ni_namelist_res prop_res;
	ni_prop_args pa;

	mastersin.sin_addr.s_addr = masteraddr;
	mastersin.sin_port = 0;
	mastersin.sin_family = AF_INET;

	myinaddr.s_addr = myaddr;

	/* connect to master hosts's nibindd */
	sock = socket_connect(&mastersin, NIBIND_PROG, NIBIND_VERS);
	if (sock < 0)
	{
		system_log(LOG_WARNING,
			"sanitycheck can't connect to %s/%s - %m",
			inet_ntoa(mastersin.sin_addr), mastertag);
		return NI_FAILED;
	}
	FD_SET(sock, &clnt_fdset);	/* protect client socket */
	cl = clnttcp_create(&mastersin, NIBIND_PROG, NIBIND_VERS, &sock, 0, 0);
	if (cl == NULL) {
		socket_close(sock);
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		return NI_FAILED;
	}
	tv.tv_sec = READALL_TIMEOUT;
	tv.tv_usec = 0;

	/* get the ports for master's tag */
	bzero((char *)&res, sizeof(res));
	if (clnt_call(cl, NIBIND_GETREGISTER,
			xdr_ni_name, &mastertag, 
			xdr_nibind_getregister_res, &res, tv) != RPC_SUCCESS) {
		/* call failed */
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		return NI_FAILED;
	}
	socket_lock();
	clnt_destroy(cl);
	(void)close(sock);
	socket_unlock();
	FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */

	if (res.status != NI_OK) {
		/* no server for the master's tag */
		system_log(LOG_ERR,
			"tag %s: master's tag %s is not served at master's addresss",
			mytag, mastertag);
		return res.status;
	}

	/* connect to the master */
	mastersin.sin_port = htons(res.nibind_getregister_res_u.addrs.tcp_port);
	xdr_free(xdr_nibind_getregister_res, (void *)&res);
	sock = socket_connect(&mastersin, NI_PROG, NI_VERS);
	if (sock < 0) {
		system_log(LOG_WARNING, "sanitycheck can't connect to "
		       "%s/%s - %m",
		       inet_ntoa(mastersin.sin_addr), mastertag);
		return NI_FAILED;
	}
	FD_SET(sock, &clnt_fdset);	/* protect client socket */
	cl = clnttcp_create(&mastersin, NI_PROG, NI_VERS, &sock, 0, 0);
	if (cl == NULL) {
		/* can't connect to master */
		socket_close(sock);
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		return NI_FAILED;
	}
	FD_SET(sock, &clnt_fdset);	/* protect client socket */

	/* get root directory */
	bzero((char *)&id_res, sizeof(id_res));
	status = clnt_call(cl, _NI_ROOT,
		xdr_void, NULL, xdr_ni_id_res, &id_res, tv);
	if (status != NI_OK) {
		/* can't get root! */
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: can't get master's root directory",
			mytag);
		return status;
	}
	if (id_res.status != NI_OK) {
		/* can't get root! */
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: master has no root directory",
			mytag);
		return status;
	}

	childdir.key = malloc(16);
	childdir.value = malloc(16);

	/* get machines subdirectory */
	childdir.id = id_res.ni_id_res_u.id;
	xdr_free(xdr_ni_id_res, (void *)&id_res);
	strcpy(childdir.key,"name");
	strcpy(childdir.value,"machines");

	bzero((char *)&lu_res, sizeof(lu_res));
	status = clnt_call(cl, _NI_LOOKUP,
		xdr_ni_lookup_args, &childdir, xdr_ni_lookup_res, &lu_res, tv);
	if (status != NI_OK) {
		/* can't get /machines! */
		free(childdir.key);
		free(childdir.value);
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: can't get master's /machines directory",
			mytag);
		return status;
	}
	if (lu_res.status != NI_OK) {
		/* no /machines! */
		free(childdir.key);
		free(childdir.value);
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: can't get master's /machines directory",
			mytag);
		return lu_res.status;
	}
	if (lu_res.ni_lookup_res_u.stuff.idlist.ni_idlist_len == 0) {
		/* no /machines! */
		xdr_free(xdr_ni_lookup_res, (void *)&lu_res);
		free(childdir.key);
		free(childdir.value);
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: master has no /machines directory",
			mytag);
		return NI_NODIR;
	}

	/* get my subdirectory */
	childdir.id.nii_object = lu_res.ni_lookup_res_u.stuff.idlist.ni_idlist_val[0];
	xdr_free(xdr_ni_lookup_res, (void *)&lu_res);
	strcpy(childdir.key,"ip_address");
	strcpy(childdir.value,(char *)inet_ntoa(myinaddr));

	bzero((char *)&lu_res, sizeof(lu_res));
	status = clnt_call(cl, _NI_LOOKUP,
		xdr_ni_lookup_args, &childdir, xdr_ni_lookup_res, &lu_res, tv);
	if (status != NI_OK) {
		/* can't get /machines/ip_address=<myaddr>! */
		free(childdir.key);
		free(childdir.value);
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: can't get master's /machines/ip_address=%s directory",
			mytag,(char *)inet_ntoa(myinaddr));
		return status;
	}
	if (lu_res.status != NI_OK) {
		/* no /machines! */
		free(childdir.key);
		free(childdir.value);
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: can't get master's /machines/ip_address=%s directory",
			mytag,(char *)inet_ntoa(myinaddr));
		return lu_res.status;
	}
	if (lu_res.ni_lookup_res_u.stuff.idlist.ni_idlist_len == 0) {
		/* can't get /machines/ip_address=<myaddr>! */
		xdr_free(xdr_ni_lookup_res, (void *)&lu_res);
		free(childdir.key);
		free(childdir.value);
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: master has no /machines/ip_address=%s directory",
			mytag,(char *)inet_ntoa(myinaddr));
		return NI_NODIR;
	}

	/* list properties */
	prop_res.ni_namelist_res_u.stuff.values.ni_namelist_val = NULL;
	childdir.id.nii_object = lu_res.ni_lookup_res_u.stuff.idlist.ni_idlist_val[0];
	xdr_free(xdr_ni_lookup_res, (void *)&lu_res);
	bzero((char *)&prop_res, sizeof(prop_res));
	status = clnt_call(cl, _NI_LISTPROPS,
		xdr_ni_id, &childdir.id, xdr_ni_namelist_res, &prop_res, tv);
	if (status != NI_OK) {
		/* can't get proplist! */
		free(childdir.key);
		free(childdir.value);
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: can't get master's property list for /machines/ip_address=%s",
			mytag,(char *)inet_ntoa(myinaddr));
		return status;
	}
	free(childdir.key);
	free(childdir.value);

	if (prop_res.status != NI_OK) {
		/* no /machines! */
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: can't get master's property list for /machines/ip_address=%s",
			mytag,(char *)inet_ntoa(myinaddr));
		return prop_res.status;
	}
	if (prop_res.ni_namelist_res_u.stuff.values.ni_namelist_len == 0) {
		/* can't get proplist! */
		xdr_free(xdr_ni_namelist_res, (void *)&prop_res);
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: master has no property list for /machines/ip_address=%s",
			mytag,(char *)inet_ntoa(myinaddr));
		return NI_NOPROP;
	}

	/* find "serves" property */
	where = ni_namelist_match(prop_res.ni_namelist_res_u.stuff.values, "serves");
	xdr_free(xdr_ni_namelist_res, (void *)&prop_res);
	if (where == NI_INDEX_NULL) {
		/* no serves property! */
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: master has no serves property for /machines/ip_address=%s",
			mytag,(char *)inet_ntoa(myinaddr));
		return NI_NOPROP;
	}

	/* fetch serves property */
	pa.id = childdir.id;

	pa.prop_index = where;
	bzero((char *)&prop_res, sizeof(prop_res));
	status = clnt_call(cl, _NI_READPROP,
		xdr_ni_prop_args, &pa, xdr_ni_namelist_res, &prop_res, tv);
	if (status != NI_OK) {
		/* can't get proplist! */
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: can't get master's serves property for /machines/ip_address=%s",
			mytag,(char *)inet_ntoa(myinaddr));
		return status;
	}
	if (prop_res.status != NI_OK) {
		/* can't get proplist! */
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: can't get master's serves property for /machines/ip_address=%s",
			mytag,(char *)inet_ntoa(myinaddr));
		return prop_res.status;
	}
	if (prop_res.ni_namelist_res_u.stuff.values.ni_namelist_len == 0) {
		/* no values in serves property! */
		xdr_free(xdr_ni_namelist_res, (void *)&prop_res);
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: master has no values for serves property in /machines/ip_address=%s",
			mytag,(char *)inet_ntoa(myinaddr));
		return NI_NOPROP;
	}

	/* find my "serves" property */
	sprintf(myserves, "./%s", mytag);
	where = ni_namelist_match(prop_res.ni_namelist_res_u.stuff.values, myserves);
	xdr_free(xdr_ni_namelist_res, (void *)&prop_res);

	if (where == NI_INDEX_NULL) {
		/* no serves property! */
		socket_lock();
		clnt_destroy(cl);
		(void)close(sock);
		socket_unlock();
		FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
		system_log(LOG_ERR,
			"tag %s: master has no serves ./%s property in /machines/ip_address=%s",
			mytag,mytag,(char *)inet_ntoa(myinaddr));
		return NI_NONAME;
	}
	
	socket_lock();
	clnt_destroy(cl);
	(void)close(sock);
	socket_unlock();
	FD_CLR(sock, &clnt_fdset);	/* unprotect client socket */
	return NI_OK;
}
