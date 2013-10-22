/*
 * Copyright (c) 2008 - 2011 Apple Inc. All rights reserved.
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

#define MEMPOOL_DEBUG 0

struct free_function : public std::unary_function<void *, void>
{
    void operator()(void * ptr) const {
#if MEMPOOL_DEBUG
        SMBLogInfo("rpc_mempool(%llu): freeing ptr %p", ASL_LEVEL_DEBUG
				  (unsigned long long)pthread_self(), ptr);
#endif
        std::free(ptr);
    }
};

#ifndef __clang_analyzer__
/* <12135199> Clang static analyzer does not understand the below code */

rpc_mempool *
rpc_mempool::allocate(
					  size_t payload_size)
{
    void * ptr;
	
    ptr = std::calloc(1, rpc_mempool::block_size() + payload_size);
    if (!ptr) {
        return NULL;
    }
	
    return new (ptr) rpc_mempool();
}
#endif

void
rpc_mempool::destroy(
					 rpc_mempool * pool)
{
    pool->~rpc_mempool();
    std::free(pool);
}

rpc_mempool::rpc_mempool()
: ptrs(10)
{
#if MEMPOOL_DEBUG
    SMBLogInfo("constructing rpc_mempool at %p", ASL_LEVEL_DEBUG, this);
#endif
    ptrs.resize(0);
}

rpc_mempool::~rpc_mempool()
{
#if MEMPOOL_DEBUG
    SMBLogInfo("destroying rpc_mempool at %p", ASL_LEVEL_DEBUG, this);
#endif
    std::for_each(ptrs.begin(), ptrs.end(), free_function());
}

void *
rpc_mempool::alloc(
				   size_t sz)
{
    void * ptr = platform::allocate(NULL, sz);
    if (ptr) {
        ptrs.push_back(ptr);
    }
	
#if MEMPOOL_DEBUG
    SMBLogInfo("rpc_mempool(%llu): allocated ptr %p for %u bytes", ASL_LEVEL_DEBUG,
			  (unsigned long long)pthread_self(), ptr, (unsigned)sz);
#endif
    return ptr;
}

void
rpc_mempool::free(
				  void * ptr)
{
    ptr_list_type::iterator which =
	std::find(ptrs.begin(), ptrs.end(), ptr);
	
    assert(which != ptrs.end());
    ptrs.erase(which);
	
#if MEMPOOL_DEBUG
    SMBLogInfo("rpc_mempool(%llu): freed ptr %p", ASL_LEVEL_DEBUG,
			  (unsigned long long)pthread_self(), ptr);
#endif
    free_function f; f(ptr);
}

idl_void_p_t
rpc_pool_allocate(idl_void_p_t context, idl_size_t sz)
{
    rpc_mempool * pool = (rpc_mempool *)context;
    return pool->alloc(sz);
}

void
rpc_pool_free(idl_void_p_t context, idl_void_p_t ptr)
{
    rpc_mempool * pool = (rpc_mempool *)context;
    return pool->free(ptr);
}

rpc_binding::rpc_binding(const rpc_binding& src)
{
    unsigned32 status;
    rpc_binding_copy(src.get(), &binding_handle, &status);
    assert(status == rpc_s_ok);
}

rpc_binding::rpc_binding(const char * string_binding)
{
    error_status_t status;
	
    rpc_binding_from_string_binding((idl_char *)string_binding,
									&binding_handle, &status);
    if (status != rpc_s_ok) {
        SMBLogInfo("rpc_binding_from_string_binding failed on <%s> status %#08x", 
                   ASL_LEVEL_ERR, 
                   string_binding == NULL ? "nullstr" : string_binding, 
                   status);
        binding_handle = NULL;
    }
}

rpc_binding::~rpc_binding()
{
    if (binding_handle) {
        error_status_t status;
	
        rpc_network_close(binding_handle, &status);
        rpc_binding_free(&binding_handle, &status);
    }
}

// Return an appropriate binding for the given ServerName.
rpc_binding
make_rpc_binding(
				 const char * ServerName,
				 const char * EndPoint)
{
    char * binding_string = NULL;
    char * endpoint_string = NULL;
    uint32_t status;
	
    if (!EndPoint) {
        return rpc_binding();
    }
	
    if (ServerName) {
        /* add in the "\pipe\" to the endpoint */
        asprintf(&endpoint_string, "\\pipe\\%s]", EndPoint);
        
        rpc_string_binding_compose(NULL, 
                                   (idl_char *) "ncacn_np", 
                                   (idl_char *) ServerName, 
                                   (idl_char *) endpoint_string, 
                                   NULL,
                                   (idl_char **) &binding_string, 
                                   &status);
        if (status != 0) {
            SMBLogInfo("rpc_string_binding_compose failed on <%s>:<%s> status %#08x", 
                       ASL_LEVEL_ERR, 
                       ServerName == NULL ? "nullstr" : ServerName, 
                       EndPoint == NULL ? "nullstr" : EndPoint, 
                       status);
            
            /* fallback to the old way instead */
            asprintf(&binding_string, "ncacn_np:%s[%s]",
                     ServerName, endpoint_string);
        }
        
        if (endpoint_string != NULL) {
            free(endpoint_string);
        }
    } 
    else {
        rpc_string_binding_compose(NULL, 
                                   (idl_char *) "ncalrpc", 
                                   NULL, 
                                   (idl_char *) EndPoint, 
                                   NULL, 
                                   (idl_char **) &binding_string, 
                                   &status);
        if (status != 0) {
            SMBLogInfo("rpc_string_binding_compose failed on <%s> status %#08x", 
                       ASL_LEVEL_ERR, 
                       EndPoint == NULL ? "nullstr" : EndPoint, 
                       status);
            
            /* fallback to the old way instead */
            asprintf(&binding_string, "ncalrpc:[%s]", EndPoint);
        }
    }
	
    if (!binding_string) {
        platform::invoke_new_handler();
    }
	
    rpc_binding binding(binding_string);
    
    if (status == 0) {
        /* rpc_string_binding_compose worked */
        if (binding_string != NULL) {
            rpc_string_free((idl_char **) &binding_string, &status);
            if (status != 0) {
                SMBLogInfo("rpc_string_free failed on <%s> status %#08x", 
                           ASL_LEVEL_ERR, 
                           binding_string == NULL ? "nullstr" : binding_string, 
                           status);
            }
        }
    }
    else {
        /* must have used asprintf */
        if (binding_string != NULL) {
            free(binding_string);
        }
    }
    
    return binding;
}

error_status_t
rpc_exception_status(
					 dcethread_exc * exc)
{
    int status;
	
    status = dcethread_exc_getstatus(exc);
    if (status == -1) {
        SMBLogInfo("unexpected RPC exception: kind=%#x address=%p %s", ASL_LEVEL_DEBUG,
				  exc->kind, exc->match.address, exc->name);
        return rpc_m_unexpected_exc;
    }
	
    return status;
}

/* vim: set sw=4 ts=4 tw=79 et: */
