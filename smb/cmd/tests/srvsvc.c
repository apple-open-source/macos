/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <sysexits.h>
#include <sys/cdefs.h>
#include <SMBClient/smbclient.h>
#include <ndl/ndrtypes.ndl>
#include <ndl/srvsvc.ndl>
#include <libmlrpc/mlrpc.h>
#include <libmlrpc/mlsvc_util.h>

#define ZERO(x) memset(&(x), 0, sizeof(x))
#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

#define CHECK(rpc_err, nt_status) do { \
    if (rpc_err) {fprintf(stderr, "%s: RPC error %#x\n", __func__, rpc_err);} \
    if (nt_status) {fprintf(stderr, "%s: API status error %#x\n", __func__, \
	    nt_status);} \
    if ((rpc_err) || (nt_status)) { exit(EX_PROTOCOL); } \
} while(0);

typedef union
{
    int int_arg;
    const char * string_arg;
} rpc_arg_t;

typedef void (*rpc_call_t) (mlsvc_handle_t *, mlrpc_heapref_t *, rpc_arg_t);

typedef struct rpc_call_entry
{
    const char * rpc_name;
    rpc_call_t	rpc_call;
    rpc_arg_t	rpc_arg;
} rpc_call_entry_t;

static ssize_t
srvsvc_transact(
    void *	smbctx,
    int		fid,
    const uint8_t *	out_buf,
    size_t	out_len,
    uint8_t *	in_buf,
    size_t	in_len)
{
    NTSTATUS status;
    off_t nbytes;

    status = SMBTransactNamedPipe(smbctx, fid,
		out_buf, out_len, in_buf, in_len,
		&nbytes);
    if (!NT_SUCCESS(status)) {
	return -1;
    }

    return nbytes;
}

static ssize_t
srvsvc_read(
    void *	smbctx,
    int		fid,
    uint8_t *	buf,
    size_t	buflen)
{
    NTSTATUS status;
    off_t nbytes;

    status = SMBReadFile(smbctx, fid, buf,
		0 /* offset */, buflen, &nbytes);
    if (!NT_SUCCESS(status)) {
	return -1;
    }

    return nbytes;
}

static int
srvsvc_open_pipe(
    SMBHANDLE  smbctx,
    const char * pipe_path,
    int *   fid)
{
    NTSTATUS status;
    SMBFID hFile;

    status = SMBCreateFile(smbctx, pipe_path,
	    0x0007, /* dwDesiredAccess: Read|Write|Append */
	    0x0003, /* dwShareMode: SHARE_READ|SHARE_WRITE */
	    NULL,   /* lpSecurityAttributes */
	    1,	    /* dwCreateDisposition: FILE_OPEN */
	    0,	    /* dwFlagsAndAttributes: 0 */
	    &hFile);
    if (!NT_SUCCESS(status)) {
	return -1;
    }

    *fid = hFile;
    return 0;
}


static const char *
server_name_from_handle(mlsvc_handle_t * handle)
{
    char * unc = NULL;

    /* XXX We should have an API to query the connected server name
     * from the SMBHANDLE.
     */
    asprintf(&unc, "\\\\%s", "*SMBSERVER");

    return unc; /* XXX memory leak */
}

static void init_pipe(
	mlrpc_heapref_t * heapref,
	mlsvc_handle_t * handle,
	SMBHANDLE smb,
	const char * pipe)
{
    int err;
    int fid;
    struct mlrpc_smb_client smbcli;

    if (mlsvc_rpc_init(heapref) != 0) {
	fprintf(stderr, "mlsvc_rpc_init failed\n");
	exit(1);
    }

    /* OK, we have an IPC$ tree connected, let's try to open the pipe. */
    err = srvsvc_open_pipe(smb, pipe, &fid);
    if (err) {
	fprintf(stderr, "srvsvc_open_pipe: %s\n", strerror(err));
	exit(1);
    }

    smbcli.smb_context = smb;
    smbcli.smb_transact = srvsvc_transact;
    smbcli.smb_readx = srvsvc_read;

    // XXX remove need to cast pipe.
    err = mlsvc_rpc_bind(handle, &smbcli, fid, (char *)pipe);
    if (err != 0) {
	fprintf(stderr, "mlsvc_rpc_bind: %s\n", strerror(err));
	exit(1);
    }
}

static void srvsvc_NetShareEnum(
	mlsvc_handle_t * handle,
	mlrpc_heapref_t * heap,
	rpc_arg_t level)
{
    struct mslm_NetShareEnum arg;
    struct mslm_infonres infonres;
    int error;

    ZERO(arg);
    ZERO(infonres);

    arg.servername = (uint8_t *)server_name_from_handle(handle);
    arg.level = level.int_arg;
    arg.prefmaxlen = UINT32_MAX;
    arg.result.level = level.int_arg;
    arg.result.bufptr.p = &infonres;

    error = mlsvc_rpc_call(handle->context, SRVSVC_OPNUM_NetShareEnum,
		&arg, heap);
    CHECK(error, arg.status);

    printf("\tentriesread=%d\n", arg.totalentries);

    switch (level.int_arg) {
    case 1:
	for (int i = 0; i < arg.totalentries; ++i) {
	    struct mslm_SHARE_INFO_1 * info1;
	    info1 = &(arg.result.bufptr.bufptr1->entries[i]);

	    printf("\n\ttype=%#x\n\tnetname=%s\n\tremark=%s\n",
		info1->shi1_type, info1->shi1_netname, info1->shi1_remark);
	}

	break;

    case 2:
	/* Even with an admin account, Windows servers return
	 * WERR_ACCESS_DENIED for level 2.
	 */
	for (int i = 0; i < arg.totalentries; ++i) {
	    struct mslm_SHARE_INFO_2 * info2;
	    info2 = &(arg.result.bufptr.bufptr2->entries[i]);

	    printf("\n\tnetname=%s\n\ttype=%#x\n\tremark=%s"
		   "\n\tpermissions=%#x\n\tmax_uses=%d\n\tcurrent_uses=%d"
		   "\n\tpath=%s\n\tpasswd=%s\n",
		info2->shi2_netname, info2->shi2_type, info2->shi2_remark,
		info2->shi2_permissions, info2->shi2_max_uses,
		    info2->shi2_current_uses,
		info2->shi2_path, info2->shi2_passwd);
	}
    }

}

static void srvsvc_NetServerGetInfo(
	mlsvc_handle_t * handle,
	mlrpc_heapref_t * heap,
	rpc_arg_t level)
{
    struct mslm_NetServerGetInfo arg;
    int error;

    ZERO(arg);

    arg.level = level.int_arg;
    arg.servername = (uint8_t *)server_name_from_handle(handle);

    error = mlsvc_rpc_call(handle->context, SRVSVC_OPNUM_NetServerGetInfo,
		&arg, heap);

}

static mlrpc_service_t srvsvc_service = {
        "srvsvc",                       /* name */
        "Server services",              /* desc */
        "\\srvsvc",                     /* endpoint */
        "\\PIPE\\ntsvcs",                    /* sec_addr_port */
        "4b324fc8-1670-01d3-12785a47bf6ee188", 3,       /* abstract */
        "8a885d04-1ceb-11c9-9fe808002b104860", 2,       /* transfer */
        0,                              /* no bind_instance_size */
        0,                              /* no bind_req() */
        0,                              /* no unbind_and_close() */
        0,                              /* use generic_call_stub() */
        &TYPEINFO(srvsvc_interface),    /* interface ti */
        NULL               /* stub_table */
};

const rpc_call_entry_t rpc_table[] =
{
    {
	.rpc_name = "NetServerGetInfo_102",
	.rpc_call = srvsvc_NetServerGetInfo,
	.rpc_arg.int_arg = 102
    },
    {
	.rpc_name = "NetShareEnum_1",
	.rpc_call = srvsvc_NetShareEnum,
	.rpc_arg.int_arg = 1
    },
    {
	.rpc_name = "NetShareEnum_2",
	.rpc_call = srvsvc_NetShareEnum,
	.rpc_arg.int_arg = 2
    }
};

static void rpc_call_by_name(const char * name,
	mlsvc_handle_t * handle,
	mlrpc_heapref_t * heap)
{
    for (int i = 0; i < ARRAYSIZE(rpc_table); ++i) {
	if (strcasecmp(name, rpc_table[i].rpc_name) != 0) {
	    continue;
	}

	printf("calling %s...\n", rpc_table[i].rpc_name);
	rpc_table[i].rpc_call(handle, heap, rpc_table[i].rpc_arg);
	return;
    }

    printf("unknown RPC name %s\n", name);
}

static void usage(void)
{
    int i;
    const char * usage_message =
    "srvsvc smb://[domain;][user[:password]@]server CALL [CALL...]\n"
    "\n"
    "Valid calls are:\n";

    printf("%s", usage_message);

    for (i = 0; i < ARRAYSIZE(rpc_table); ++i) {
	printf("        %s\n", rpc_table[i].rpc_name);
    }

    exit(EX_USAGE);
}

int main(int argc, const char ** argv)
{
    mlrpc_heapref_t heap;
    mlsvc_handle_t  handle;
    NTSTATUS status;
    SMBHANDLE smb = NULL;

    ZERO(heap);
    ZERO(handle);

    if (argc < 3) {
	usage();
    }

    status = SMBOpenServer(argv[1], &smb);
    if (!NT_SUCCESS(status)) {
	fprintf(stderr, "failed to connect to %s: NTSTATUS %#08X\n",
		argv[1], status);
	usage();
    }

    mlrpc_register_service(&srvsvc_service);
    init_pipe(&heap, &handle, smb, "srvsvc");

    for (int i = 2; i < argc; ++i) {
	rpc_call_by_name(argv[i], &handle, &heap);
    }

    SMBReleaseServer(smb);
    return EX_OK;
}

/* vim: set ts=4 et tw=79 */
