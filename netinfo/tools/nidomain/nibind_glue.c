/*
 * nibindd glue 
 * Copyright 1989-94, NeXT Computer Inc.
 */
#include <netinfo/ni.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>

/*
 * Initiate nibindd connection
 */
void *
nibind_new(
	   struct in_addr *addr
	   )
{
	struct sockaddr_in sin;
	int sock = RPC_ANYSOCK;

	sin.sin_port = 0;
	sin.sin_family = AF_INET;
	bzero(sin.sin_zero, sizeof(sin.sin_zero));
	sin.sin_addr = *addr;
	return ((void *)clnttcp_create(&sin, NIBIND_PROG, NIBIND_VERS, 
				       &sock, 0, 0));
}

/*
 * List registered netinfods
 */
ni_status
nibind_listreg(
	       void *nb,
	       nibind_registration **regvec,
	       unsigned *reglen
	       )
{
	nibind_listreg_res *res;

	res = nibind_listreg_1(NULL, nb);
	if (res == NULL) {
 		return (NI_FAILED);
	}
	if (res->status == NI_OK) {
		*regvec = res->nibind_listreg_res_u.regs.regs_val;
		*reglen = res->nibind_listreg_res_u.regs.regs_len;
	}
	return (res->status);
}

/*
 * Get registration
 */
ni_status
nibind_getregister(
	       void *nb,
		   ni_name tag,
	       nibind_addrinfo **addrs
	       )
{
	nibind_getregister_res *res;

	res = nibind_getregister_1(&tag, nb);
	if (res == NULL) {
 		return (NI_FAILED);
	}
	if (res->status == NI_OK) {
		*addrs = &res->nibind_getregister_res_u.addrs;
	}
	return (res->status);
}

/*
 * Register a server
 */
ni_status
nibind_register(
	       void *nb,
		   nibind_registration *reg
	       )
{
	ni_status *res;

	res = nibind_register_1(reg, nb);
	return (*res);
}

/*
 * Unregister a server
 */
ni_status
nibind_unregister(
	       void *nb,
		   ni_name tag
	       )
{
	ni_status *res;

	res = nibind_unregister_1(&tag, nb);
	return (*res);
}

/*
 * Create a master netinfod
 */
ni_status
nibind_createmaster(
		    void *nb,
		    ni_name tag
		    )
{
	ni_status *status;

	status = nibind_createmaster_1(&tag, nb);
	if (status == NULL) {
		return (NI_FAILED);
	}
	return (*status);
}
	
/*
 * Create a clone netinfod
 */
ni_status
nibind_createclone(
		   void *nb,
		   ni_name tag,
		   ni_name master_name,
		   struct in_addr *master_addr,
		   ni_name master_tag
		   )
{
	ni_status *status;
	nibind_clone_args args;

	args.tag = tag;
	args.master_name = master_name;
	args.master_addr = master_addr->s_addr;
	args.master_tag = master_tag;

	/* XDR will swap the master address if this is a little-endian system. */
	/* We swap it to host order first, so that XDR will swap it back to */
	/* network byte order. */
	args.master_addr = ntohl(args.master_addr);

	status = nibind_createclone_1(&args, nb);
	if (status == NULL) {
		return (NI_FAILED);
	}
	return (*status);
}

/*
 * Destroy a netinfod
 */
ni_status
nibind_destroydomain(
		     void *nb,
		     ni_name tag
		     )
{
	ni_status *status;

	status = nibind_destroydomain_1(&tag, nb);
	if (status == NULL) {
		return (NI_FAILED);
	}
	return (*status);
}
		   

/*
 * Free up connection to nibindd
 */
void
nibind_free(
	    void *nb
	    )
{
	clnt_destroy(((CLIENT *)nb));
}
