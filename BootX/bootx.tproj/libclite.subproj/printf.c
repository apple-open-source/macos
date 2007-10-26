/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  printf.c - printf
 *
 *  Copyright (c) 1998-2002 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <libclite.h>

int printf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    prf(format, (unsigned int *)ap, putchar, 0);
    va_end(ap);
    return 0;
}


#if DEBUG
#include <stdint.h>

#define BTLEN 10
// ctx and id optional
void dump_backtrace(char *ctx, int id)
{
	void *bt[BTLEN];
	int cnt;

	if (ctx)
		printf("%s ", ctx);
	if (id)
		printf("(%d)", id);
	if (ctx || id)
		printf(":\n");

	cnt = OSBacktrace_ppc(bt, BTLEN);
	while(cnt <= BTLEN && cnt--)
		printf("bt[%d]: %x\n", cnt, bt[cnt]);
}

// ported from xnu/libkern/gen/OSDebug.cpp
// (non-__ppc__ #if branches and min/maxstackaddr checks omitted)
unsigned OSBacktrace_ppc(void **bt, unsigned maxAddrs)  
{
    unsigned frame;

#if __ppc__
    uint32_t stackptr, stackptr_prev;
    const uint32_t * const mem = (uint32_t *) 0;
    unsigned i = 0;

    __asm__ volatile("mflr %0" : "=r" (stackptr)); 
    bt[i++] = (void *) stackptr;

    __asm__ volatile("mr %0,r1" : "=r" (stackptr)); 
    for ( ; i < maxAddrs; i++) {
    // Validate we have a reasonable stackptr
    if ( /* !(minstackaddr <= stackptr && stackptr < maxstackaddr)
			|| */ (stackptr & 3))
        break;

    stackptr_prev = stackptr;
    stackptr = mem[stackptr_prev >> 2];
    if ((stackptr_prev ^ stackptr) > 8 * 1024)  // Sanity check
        break;

    uint32_t addr = mem[(stackptr >> 2) + 2]; 
    if ((addr & 3) || (addr < 0x8000))  // More sanity checks
        break;
    bt[i] = (void *) addr;
    }
    frame = i;

    for ( ; i < maxAddrs; i++)
        bt[i] = (void *) 0;
#else
#warning "BootX's OSBacktrace_ppc() not intended for other architectures"
#endif

    return frame;
}
#endif	// DEBUG
