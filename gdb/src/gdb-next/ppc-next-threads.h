#ifndef _PPC_NEXT_THREADS_H_
#define _PPC_NEXT_THREADS_H_

#define	REGS_STATE PPC_THREAD_STATE
#define REGS_STRUCT struct ppc_thread_state
#define REGS_COUNT PPC_THREAD_STATE_COUNT
#define PC_FIELD(s) ((s).srr0)

#define	FP_REGS_STATE PPC_FLOAT_STATE
#define FP_REGS_STRUCT struct ppc_float_state
#define FP_REGS_COUNT  PPC_FLOAT_STATE_COUNT

#define USER_REG_STATE PPC_THREAD_STATE
#define USER_REG_STRUCT struct ppc_thread_state
#define USER_REG_COUNT PPC_THREAD_STATE_COUNT
#define USER_REG(s) ((s).r1)

#define TRACE_STATE PPC_THREAD_STATE
#define TRACE_STRUCT struct ppc_thread_state
#define TRACE_COUNT PPC_THREAD_STATE_COUNT
#define TRACE_BIT_SET (s) ((s).srr1 & 0x400UL)
#define SET_TRACE_BIT (s) ((s).srr1 |= 0x400UL)
#define CLEAR_TRACE_BIT (s) ((s).srr1 &= 0x400UL)

#endif /* _PPC_NEXT_THREADS_H_ */
