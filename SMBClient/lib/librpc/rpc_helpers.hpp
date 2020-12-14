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

#ifndef RPC_HELPERS_HPP_142C40C1_6E87_4460_9641_01748A09BDC3
#define RPC_HELPERS_HPP_142C40C1_6E87_4460_9641_01748A09BDC3

#include <vector>

extern "C" {
#include <dce/dcethread.h>
}

struct rpc_mempool
{
    typedef std::vector<void *> ptr_list_type;
	
    rpc_mempool();
    ~rpc_mempool();
	
    void * alloc(size_t sz);
    void free(void * ptr);
	
    static inline unsigned block_size() {
        return roundup(sizeof(struct rpc_mempool), 16);
    }
	
    static rpc_mempool * allocate(size_t);
    static void destroy(rpc_mempool *);
	
private:
    ptr_list_type ptrs;
};

template <typename T>
std::pair<rpc_mempool *, T *>
allocate_rpc_mempool(void)
{
    std::pair<rpc_mempool *, T *> ret;
	
    ret.first = rpc_mempool::allocate(sizeof(T));
    ret.second = (T *)(void *)((uint8_t *)ret.first + rpc_mempool::block_size());
	
    return ret;
}

idl_void_p_t
rpc_pool_allocate(
				  idl_void_p_t context,
				  idl_size_t sz
				  );

void
rpc_pool_free(
			  idl_void_p_t context,
			  idl_void_p_t ptr
			  );

struct rpc_binding
{
    rpc_binding() : binding_handle(0) {}
    rpc_binding(const rpc_binding&);
    ~rpc_binding();
	
    explicit rpc_binding(const char * string_binding);
	
    rpc_binding_handle_t get() const {
        return binding_handle;
    }
	
    void swap(rpc_binding& rhs) {
        std::swap(this->binding_handle, rhs.binding_handle);
    }
	
    rpc_binding& operator=(const rpc_binding& rhs) {
        rpc_binding tmp(rhs);
        this->swap(tmp);
        return *this;
    }
	
private:
    rpc_binding_handle_t binding_handle;
};

// Return an appropriate binding for the given ServerName.
rpc_binding
make_rpc_binding(
				 const char * ServerName,
				 const char * EndPoint
				 );

// Convert a DCE exception into an error code.
error_status_t
rpc_exception_status(
					 dcethread_exc *
					 );

#endif /* RPC_HELPERS_HPP_142C40C1_6E87_4460_9641_01748A09BDC3 */
/* vim: set sw=4 ts=4 tw=79 et: */