#ifndef _I386_NEXT_THREADS_H_
#define _I386_NEXT_THREADS_H_

#define	REGS_STATE i386_THREAD_STATE
#define REGS_STRUCT i386_thread_state_t
#define REGS_COUNT i386_THREAD_STATE_COUNT
#define PC_FIELD(s) ((s).eip)

#define	FP_REGS_STATE i386_THREAD_FPSTATE
#define FP_REGS_STRUCT i386_thread_fpstate_t
#define FP_REGS_COUNT  i386_THREAD_FPSTATE_COUNT

#define USER_REG_STATE i386_THREAD_CTHREADSTATE
#define USER_REG_STRUCT i386_thread_state
#define USER_REG_COUNT i386_THREAD_CTHREADSTATE_COUNT
#define USER_REG(s) ((s).self)

#define TRACE_STATE REGS_STATE
#define TRACE_STRUCT REGS_STRUCT
#define TRACE_COUNT REGS_COUNT
#define TRACE_BIT_SET(s) ((s).eflags & 0x100)
#define SET_TRACE_BIT(s) ((s).eflags |= 0x100)
#define CLEAR_TRACE_BIT(s) ((s).eflags &= ~0x100)

#endif /* _I386_NEXT_THREADS_H_ */
