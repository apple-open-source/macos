/*
 * Copyright (c) 2008 - 2010 Apple Inc. All rights reserved.
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

#include <smbclient/smbclient.h>
#include <smbclient/ntstatus.h>
#include <smbclient/smbclient_internal.h>

#include <algorithm>
#include <vector>
#include <cstdlib>
#include <assert.h>
#include <string>

#include "lmshare.h"
#include "memory.hpp"
#include "rpc_helpers.hpp"

extern "C" {
#include <dce/dcethread.h>
}

static idl_void_p_t
share_memalloc(idl_void_p_t context, idl_size_t sz)
{
    rpc_mempool * pool = (rpc_mempool *)context;
    return pool->alloc(sz);
}

static void
share_memfree(idl_void_p_t context, idl_void_p_t ptr)
{
    rpc_mempool * pool = (rpc_mempool *)context;
    return pool->free(ptr);
}

NET_API_STATUS
NetShareGetInfo(
        const char * ServerName,
        const char * NetName,
        uint32_t Level,
        PSHARE_INFO * ShareInfo)
{
	WCHAR * serverName = SMBConvertFromUTF8ToUTF16(ServerName, 1024, 0);
	WCHAR * netName = SMBConvertFromUTF8ToUTF16(NetName, 1024, 0);

    if (!serverName || !netName || !ShareInfo) {
		if (serverName) 
			free(serverName);
		if (netName) 
			free(netName);
		
        return ERROR_INVALID_PARAMETER;
    }
	
    rpc_binding binding(make_rpc_binding(ServerName, "srvsvc"));
    rpc_ss_allocator_t allocator;

    NET_API_STATUS api_status = NERR_Success;
    error_status_t rpc_status = rpc_s_ok;

	memset(&allocator, 0, sizeof(allocator));

    std::pair<rpc_mempool *, SHARE_INFO *> result(
            allocate_rpc_mempool<SHARE_INFO>());

    allocator.p_allocate = share_memalloc;
    allocator.p_free = share_memfree;
    allocator.p_context = (idl_void_p_t)result.first;

    rpc_ss_swap_client_alloc_free_ex(&allocator, &allocator);

    DCETHREAD_TRY
        api_status = NetrShareGetInfo(
                binding.get(),
                const_cast<WCHAR *>(serverName),
                const_cast<WCHAR *>(netName),
                Level, result.second, &rpc_status);
    DCETHREAD_CATCH_ALL(exc)
        /*
	 * Unmarshalling a response with an unknown level will throw a 
	 * rpc_x_invalid_tag exception since the unknown level is not defined as 
	 * a union discriminator.
	 */
        rpc_status = rpc_exception_status(exc);
    DCETHREAD_ENDTRY

	free(serverName);
	free(netName);
    rpc_ss_swap_client_alloc_free_ex(&allocator, &allocator);

    if (rpc_status != rpc_s_ok) {
        SMBLogInfo("RPC to srvsrvc gave error %#08x", ASL_LEVEL_ERR, rpc_status);
        NetApiBufferFree(result.second);
        return RPC_S_PROTOCOL_ERROR;
    }

    if (api_status == NERR_Success) {
        *ShareInfo = result.second;
    } else {
        NetApiBufferFree(result.second);
        *ShareInfo = NULL;
    }

    return api_status;
}

NET_API_STATUS
NetShareEnum(
        const char * ServerName,
        uint32_t Level,
        PSHARE_ENUM_STRUCT * InfoStruct)
{	
	WCHAR * serverName = SMBConvertFromUTF8ToUTF16(ServerName, 1024, 0);
    if (!serverName || !InfoStruct) {
		if ( serverName)
			free(serverName);
        return ERROR_INVALID_PARAMETER;
    }
	
    rpc_binding binding(make_rpc_binding(ServerName, "srvsvc"));
    rpc_ss_allocator_t allocator;

    NET_API_STATUS api_status = NERR_Success;
    error_status_t rpc_status = rpc_s_ok;

	memset(&allocator, 0, sizeof(allocator));

    std::pair<rpc_mempool *, SHARE_ENUM_STRUCT *> result(
            allocate_rpc_mempool<SHARE_ENUM_STRUCT>());

    DWORD entries = 0;
    DWORD resume = 0;

    allocator.p_allocate = share_memalloc;
    allocator.p_free = share_memfree;
    allocator.p_context = (idl_void_p_t)result.first;

    rpc_ss_swap_client_alloc_free_ex(&allocator, &allocator);

    result.second->Level = Level;

    /* 
	 * Windows requires a valid pointer for the ShareInfo union. It doesn't matter 
	 * which container type we choose here, since they all have the same binary 
	 * layout.
	 */
    result.second->ShareInfo.Level0 =
        (SHARE_INFO_0_CONTAINER *)result.first->alloc(
                                sizeof(SHARE_INFO_0_CONTAINER));
    result.second->ShareInfo.Level0->EntriesRead = 0;
    result.second->ShareInfo.Level0->Buffer = NULL;
	
	DCETHREAD_TRY
		api_status = NetrShareEnum(
                binding.get(),
                const_cast<WCHAR *>(serverName),
                result.second,
                0xffffffff,
                &entries,
                &resume,
                &rpc_status);
	DCETHREAD_CATCH_ALL(exc)
		rpc_status = rpc_exception_status(exc);
	DCETHREAD_ENDTRY

    rpc_ss_swap_client_alloc_free_ex(&allocator, &allocator);

	free(serverName);
    if (rpc_status != rpc_s_ok) {
        SMBLogInfo("RPC to srvsrvc gave error %#08x", ASL_LEVEL_ERR, rpc_status);
        NetApiBufferFree(result.second);
        return RPC_S_PROTOCOL_ERROR;
    }

    if (api_status == NERR_Success) {
        *InfoStruct = result.second;
    } else {
        NetApiBufferFree(result.second);
        *InfoStruct = NULL;
    }

    return api_status;
}

void
NetApiBufferFree(
        void * bufptr)
{
    rpc_mempool * pool;

    if (!bufptr) {
        return;
    }

    pool = (rpc_mempool *)(void *)((uint8_t *)bufptr - rpc_mempool::block_size());
    pool->~rpc_mempool();

    std::free(pool);
}

/* vim: set sw=4 ts=4 tw=79 et: */
