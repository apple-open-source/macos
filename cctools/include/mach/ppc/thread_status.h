/*
 * Copyright (c) 1999-2004, Apple Computer, Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * @OSF_COPYRIGHT@
 */

#ifndef	_MACH_PPC_THREAD_STATUS_H_
#define _MACH_PPC_THREAD_STATUS_H_

/*
 * ppc_thread_state is the structure that is exported to user threads for 
 * use in status/mutate calls.  This structure should never change.
 *
 * TODO : it'd be nice to provide 64 bit compatibility in this structure
 *        - need to make sure that it's understood by gdb though?
 */

#define PPC_THREAD_STATE        1
#define PPC_FLOAT_STATE         2
#define PPC_EXCEPTION_STATE	3
#define THREAD_STATE_NONE	7
	       
typedef struct ppc_thread_state {
	unsigned int srr0;      /* Instruction address register (PC) */
	unsigned int srr1;	/* Machine state register (supervisor) */
				/* srr1 may contain SRR_SYSCALL_VAL */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;
	unsigned int r13;
	unsigned int r14;
	unsigned int r15;
	unsigned int r16;
	unsigned int r17;
	unsigned int r18;
	unsigned int r19;
	unsigned int r20;
	unsigned int r21;
	unsigned int r22;
	unsigned int r23;
	unsigned int r24;
	unsigned int r25;
	unsigned int r26;
	unsigned int r27;
	unsigned int r28;
	unsigned int r29;
	unsigned int r30;
	unsigned int r31;

	unsigned int cr;        /* Condition register */
	unsigned int xer;	/* User's integer exception register */
	unsigned int lr;	/* Link register */
	unsigned int ctr;	/* Count register */
	unsigned int mq;	/* MQ register (601 only) */

	unsigned int pad;      /* structure TODO - check these! */
} ppc_thread_state_t;

/* This structure should be double-word aligned for performance */

typedef struct ppc_float_state {
	double  fpregs[32];

	unsigned int fpscr_pad; /* fpscr is 64 bits, 32 bits of rubbish */
	unsigned int fpscr;	/* floating point status register */
} ppc_float_state_t;

/*
 * saved state structure
 *
 * This structure corresponds to the state of the user registers as saved
 * on the stack upon kernel entry (saved in pcb). On interrupts and exceptions
 * we save all registers. On system calls we only save the registers not
 * saved by the caller.
 *
 * NB: If you change this structure you must also change the KGDB stub as well.
 * 
 * TODO : not sure about the save/restore regs. Does this need to be
 *        different to ppc_thread_state?
 */

typedef struct ppc_saved_state {
	unsigned int srr0;      /* Instruction address register (PC) */
	unsigned int srr1;	/* Machine state register (supervisor) */
				/* srr1 may contain SRR_SYSCALL_VAL */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;
	unsigned int r13;
	unsigned int r14;
	unsigned int r15;
	unsigned int r16;
	unsigned int r17;
	unsigned int r18;
	unsigned int r19;
	unsigned int r20;
	unsigned int r21;
	unsigned int r22;
	unsigned int r23;
	unsigned int r24;
	unsigned int r25;
	unsigned int r26;
	unsigned int r27;
	unsigned int r28;
	unsigned int r29;
	unsigned int r30;
	unsigned int r31;

	unsigned int cr;        /* Condition register */
	unsigned int xer;	/* User's integer exception register */
	unsigned int lr;	/* Link register */
	unsigned int ctr;	/* Count register */
	unsigned int mq;	/* MQ register (601 only) */
	unsigned int pad;	/* To mirror pcb_thread_state in pcb*/

/*	These are extra.  Remove them from the count */

	unsigned int sr_copyin; /* SR_COPYIN is used for remapping */
	unsigned int pad2;	/* struct alignment */
	unsigned int pad3;
	unsigned int pad4;
} ppc_saved_state_t;

/*
 * ppc_exception_state
 *
 * This structure corresponds to some additional state of the user
 * registers as saved in the PCB upon kernel entry. They are only
 * available if an exception is passed out of the kernel, and even
 * then not all are guaranteed to be updated.
 *
 * Some padding is included in this structure which allows space for
 * servers to store temporary values if need be, to maintain binary
 * compatiblity.
 */

typedef struct ppc_exception_state {
	unsigned long dar;	/* Fault registers for coredump */
	unsigned long dsisr;
	unsigned long exception;/* number of powerpc exception taken */
	unsigned long pad0;	/* align to 16 bytes */

	unsigned long pad1[4];	/* space in PCB "just in case" */
} ppc_exception_state_t;

/*
 * Save State Flags
 */

#define PPC_THREAD_STATE_COUNT \
   (sizeof(struct ppc_thread_state) / sizeof(int))

#define PPC_EXCEPTION_STATE_COUNT \
   (sizeof(struct ppc_exception_state) / sizeof(int))

#define PPC_FLOAT_STATE_COUNT \
   (sizeof(struct ppc_float_state) / sizeof(int))

/*
 * Machine-independent way for servers and Mach's exception mechanism to
 * choose the most efficient state flavor for exception RPC's:
 */
#define PPC_MACHINE_THREAD_STATE		PPC_THREAD_STATE
#define PPC_MACHINE_THREAD_STATE_COUNT	PPC_THREAD_STATE_COUNT

/*
 * Largest state on this machine:
 */
#define PPC_THREAD_MACHINE_STATE_MAX	PPC_FLOAT_STATE_COUNT

#endif /* _MACH_PPC_THREAD_STATUS_H_ */
