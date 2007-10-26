#ifndef __GDB_I386_MACOSX_THREAD_STATUS_H__
#define __GDB_I386_MACOSX_THREAD_STATUS_H__

#include <stdint.h>
#include <mach/mach_types.h>  /* For mach_msg_type_number_t, used below. */

#define GDB_i386_THREAD_STATE 1
#define GDB_i386_THREAD_FPSTATE 2 /* Equivalent to Mach's i386_FLOAT_STATE */
#define GDB_i386_FLOAT_STATE 2    /* Equivalent to Mach's i386_FLOAT_STATE */

#define GDB_i386_FP_NO 0        /* No floating point. */
#define GDB_i386_FP_SOFT 1      /* Software FP emulator. */
#define GDB_i386_FP_287 2       /* 80287 */
#define GDB_i386_FP_387 3       /* 80387 or 80486 */
#define GDB_i386_FP_SSE2 4      /* P4 Streaming SIMD 2 Extensions (includes MMX and SSE) - corresponds to Mach's FP_FXSR */


#define GDB_x86_THREAD_STATE32		1
#define GDB_x86_FLOAT_STATE32		2
#define GDB_x86_EXCEPTION_STATE32	3
#define GDB_x86_THREAD_STATE64		4
#define GDB_x86_FLOAT_STATE64		5
#define GDB_x86_EXCEPTION_STATE64	6
#define GDB_x86_THREAD_STATE		7
#define GDB_x86_FLOAT_STATE		8
#define GDB_x86_EXCEPTION_STATE		9
#define GDB_THREAD_STATE_NONE		THREAD_STATE_NONE



/*
 * VALID_THREAD_STATE_FLAVOR is a platform specific macro that when passed
 * an exception flavor will return if that is a defined flavor for that
 * platform. The macro must be manually updated to include all of the valid
 * exception flavors as defined above.
 */
#define GDB_VALID_THREAD_STATE_FLAVOR(x)       \
	((x == GDB_x86_THREAD_STATE32)	|| \
	 (x == GDB_x86_FLOAT_STATE32)	|| \
	 (x == GDB_x86_EXCEPTION_STATE32)	|| \
	 (x == GDB_x86_THREAD_STATE64)	|| \
	 (x == GDB_x86_FLOAT_STATE64)	|| \
	 (x == GDB_x86_EXCEPTION_STATE64)	|| \
	 (x == GDB_x86_THREAD_STATE)	|| \
	 (x == GDB_x86_FLOAT_STATE)		|| \
	 (x == GDB_x86_EXCEPTION_STATE)	|| \
	 (x == GDB_THREAD_STATE_NONE))


struct gdb_x86_state_hdr {
    int 	flavor;
    int		count;
};
typedef struct gdb_x86_state_hdr gdb_x86_state_hdr_t;


struct gdb_i386_thread_state {
    unsigned int	eax;
    unsigned int	ebx;
    unsigned int	ecx;
    unsigned int	edx;
    unsigned int	edi;
    unsigned int	esi;
    unsigned int	ebp;
    unsigned int	esp;
    unsigned int	ss;
    unsigned int	eflags;
    unsigned int	eip;
    unsigned int	cs;
    unsigned int	ds;
    unsigned int	es;
    unsigned int	fs;
    unsigned int	gs;
} ;

typedef struct gdb_i386_thread_state gdb_i386_thread_state_t;
#define GDB_i386_THREAD_STATE_COUNT	((mach_msg_type_number_t) \
    ( sizeof (gdb_i386_thread_state_t) / sizeof (int) ))

/* This structure is a copy of the struct i386_float_state definition
   in /usr/include/mach/i386/thread_status.h -- it must be identical for
   the call to thread_get_state and thread_set_state.  */

#define GDB_i386_FP_SSE2_STATE_SIZE 512

struct gdb_i386_thread_fpstate
{
  unsigned int fpkind;
  unsigned int initialized;
  unsigned char hw_fu_state[GDB_i386_FP_SSE2_STATE_SIZE];
  unsigned int exc_status;
};

typedef struct gdb_i386_thread_fpstate gdb_i386_thread_fpstate_t;

typedef struct gdb_i386_thread_state gdb_x86_thread_state32_t;
#define GDB_x86_THREAD_STATE32_COUNT ((mach_msg_type_number_t) \
    ( sizeof (gdb_x86_thread_state32_t) / sizeof (int) ))

#define GDB_i386_THREAD_FPSTATE_COUNT \
    (sizeof (struct gdb_i386_thread_fpstate) / sizeof (unsigned int))

struct gdb_x86_thread_state64 {
    uint64_t		rax;
    uint64_t		rbx;
    uint64_t		rcx;
    uint64_t		rdx;
    uint64_t		rdi;
    uint64_t		rsi;
    uint64_t		rbp;
    uint64_t		rsp;
    uint64_t		r8;
    uint64_t		r9;
    uint64_t		r10;
    uint64_t		r11;
    uint64_t		r12;
    uint64_t		r13;
    uint64_t		r14;
    uint64_t		r15;
    uint64_t		rip;
    uint64_t		rflags;
    uint64_t		cs;
    uint64_t		fs;
    uint64_t		gs;
} ;


typedef struct gdb_x86_thread_state64 gdb_x86_thread_state64_t;
#define GDB_x86_THREAD_STATE64_COUNT	((mach_msg_type_number_t) \
    ( sizeof (gdb_x86_thread_state64_t) / sizeof (int) ))




struct gdb_x86_thread_state {
    gdb_x86_state_hdr_t		tsh;
    union {
        gdb_x86_thread_state32_t	ts32;
        gdb_x86_thread_state64_t	ts64;
    } uts;
} ;


typedef struct gdb_x86_thread_state gdb_x86_thread_state_t;
#define GDB_x86_THREAD_STATE_COUNT	((mach_msg_type_number_t) \
    ( sizeof (gdb_x86_thread_state_t) / sizeof (int) ))


typedef struct gdb_fp_control {
    unsigned short		invalid	:1,
    				denorm	:1,
				zdiv	:1,
				ovrfl	:1,
				undfl	:1,
				precis	:1,
					:2,
				pc	:2,
				rc	:2,
				/*inf*/	:1,
					:3;
} gdb_fp_control_t;

typedef struct gdb_fp_status {
    unsigned short		invalid	:1,
    				denorm	:1,
				zdiv	:1,
				ovrfl	:1,
				undfl	:1,
				precis	:1,
				stkflt	:1,
				errsumm	:1,
				c0	:1,
				c1	:1,
				c2	:1,
				tos	:3,
				c3	:1,
				busy	:1;
} gdb_fp_status_t;
				
struct gdb_mmst_reg {
	char	mmst_reg[10];
	char	mmst_rsrv[6];
};


struct gdb_xmm_reg {
	char		xmm_reg[16];
};

#define GDB_FP_STATE_BYTES		512

struct gdb_i386_float_state {
	int 			fpu_reserved[2];
	gdb_fp_control_t		fpu_fcw;
	gdb_fp_status_t		fpu_fsw;
	uint8_t			fpu_ftw;
	uint8_t			fpu_rsrv1;
	uint16_t		fpu_fop;
	uint32_t		fpu_ip;
	uint16_t		fpu_cs;
	uint16_t		fpu_rsrv2;
	uint32_t		fpu_dp;
	uint16_t		fpu_ds;
	uint16_t		fpu_rsrv3;
	uint32_t		fpu_mxcsr;
	uint32_t		fpu_mxcsrmask;
	struct gdb_mmst_reg	fpu_stmm0;
	struct gdb_mmst_reg	fpu_stmm1;
	struct gdb_mmst_reg	fpu_stmm2;
	struct gdb_mmst_reg	fpu_stmm3;
	struct gdb_mmst_reg	fpu_stmm4;
	struct gdb_mmst_reg	fpu_stmm5;
	struct gdb_mmst_reg	fpu_stmm6;
	struct gdb_mmst_reg	fpu_stmm7;
	struct gdb_xmm_reg	fpu_xmm0;
	struct gdb_xmm_reg	fpu_xmm1;
	struct gdb_xmm_reg	fpu_xmm2;
	struct gdb_xmm_reg	fpu_xmm3;
	struct gdb_xmm_reg	fpu_xmm4;
	struct gdb_xmm_reg	fpu_xmm5;
	struct gdb_xmm_reg	fpu_xmm6;
	struct gdb_xmm_reg	fpu_xmm7;
	char			fpu_rsrv4[14*16];
	int 			fpu_reserved1;
};

typedef struct gdb_i386_float_state gdb_i386_float_state_t;
#define GDB_i386_FLOAT_STATE_COUNT ((mach_msg_type_number_t) \
		(sizeof(gdb_i386_float_state_t)/sizeof(unsigned int)))
	 
typedef struct gdb_i386_float_state gdb_x86_float_state32_t;
#define GDB_x86_FLOAT_STATE32_COUNT ((mach_msg_type_number_t) \
		(sizeof(gdb_x86_float_state32_t)/sizeof(unsigned int)))
	 



struct gdb_x86_float_state64 {
	int 			fpu_reserved[2];
	gdb_fp_control_t		fpu_fcw;
	gdb_fp_status_t		fpu_fsw;
	uint8_t			fpu_ftw;
	uint8_t			fpu_rsrv1;
	uint16_t		fpu_fop;
	uint32_t		fpu_ip;	
	uint16_t		fpu_cs;	
	uint16_t		fpu_rsrv2;
	uint32_t		fpu_dp;
	uint16_t		fpu_ds;
	uint16_t		fpu_rsrv3;
	uint32_t		fpu_mxcsr;
	uint32_t		fpu_mxcsrmask;
	struct gdb_mmst_reg	fpu_stmm0;
	struct gdb_mmst_reg	fpu_stmm1;
	struct gdb_mmst_reg	fpu_stmm2;
	struct gdb_mmst_reg	fpu_stmm3;
	struct gdb_mmst_reg	fpu_stmm4;
	struct gdb_mmst_reg	fpu_stmm5;
	struct gdb_mmst_reg	fpu_stmm6;
	struct gdb_mmst_reg	fpu_stmm7;
	struct gdb_xmm_reg	fpu_xmm0;
	struct gdb_xmm_reg	fpu_xmm1;
	struct gdb_xmm_reg	fpu_xmm2;
	struct gdb_xmm_reg	fpu_xmm3;
	struct gdb_xmm_reg	fpu_xmm4;
	struct gdb_xmm_reg	fpu_xmm5;
	struct gdb_xmm_reg	fpu_xmm6;
	struct gdb_xmm_reg	fpu_xmm7;
	struct gdb_xmm_reg	fpu_xmm8;
	struct gdb_xmm_reg	fpu_xmm9;
	struct gdb_xmm_reg	fpu_xmm10;
	struct gdb_xmm_reg	fpu_xmm11;
	struct gdb_xmm_reg	fpu_xmm12;
	struct gdb_xmm_reg	fpu_xmm13;
	struct gdb_xmm_reg	fpu_xmm14;
	struct gdb_xmm_reg	fpu_xmm15;
	char			fpu_rsrv4[6*16];
	int 			fpu_reserved1;
};

typedef struct gdb_x86_float_state64 gdb_x86_float_state64_t;
#define GDB_x86_FLOAT_STATE64_COUNT ((mach_msg_type_number_t) \
		(sizeof(gdb_x86_float_state64_t)/sizeof(unsigned int)))
		
	 


struct gdb_x86_float_state {
    gdb_x86_state_hdr_t		fsh;
    union {
        gdb_x86_float_state32_t	fs32;
        gdb_x86_float_state64_t	fs64;
    } ufs;
} ;


typedef struct gdb_x86_float_state gdb_x86_float_state_t;
#define GDB_x86_FLOAT_STATE_COUNT	((mach_msg_type_number_t) \
    ( sizeof (gdb_x86_float_state_t) / sizeof (int) ))



struct gdb_i386_exception_state {
    unsigned int	trapno;
    unsigned int	err;
    unsigned int	faultvaddr;
};

typedef struct gdb_i386_exception_state gdb_i386_exception_state_t;
#define GDB_i386_EXCEPTION_STATE_COUNT	((mach_msg_type_number_t) \
    ( sizeof (gdb_i386_exception_state_t) / sizeof (int) ))

#define GDB_I386_EXCEPTION_STATE_COUNT GDB_i386_EXCEPTION_STATE_COUNT

typedef struct gdb_i386_exception_state gdb_x86_exception_state32_t;
#define GDB_x86_EXCEPTION_STATE32_COUNT	((mach_msg_type_number_t) \
    ( sizeof (gdb_x86_exception_state32_t) / sizeof (int) ))




struct gdb_x86_exception_state64 {
    unsigned int	trapno;
    unsigned int	err;
    uint64_t		faultvaddr;
};

typedef struct gdb_x86_exception_state64 gdb_x86_exception_state64_t;
#define GDB_x86_EXCEPTION_STATE64_COUNT	((mach_msg_type_number_t) \
    ( sizeof (gdb_x86_exception_state64_t) / sizeof (int) ))





struct gdb_x86_exception_state {
    gdb_x86_state_hdr_t		esh;
    union {
        gdb_x86_exception_state32_t	es32;
        gdb_x86_exception_state64_t	es64;
    } ues;
} ;


typedef struct gdb_x86_exception_state gdb_x86_exception_state_t;
#define GDB_x86_EXCEPTION_STATE_COUNT	((mach_msg_type_number_t) \
    ( sizeof (gdb_x86_exception_state_t) / sizeof (int) ))




#endif /* __GDB_I386_MACOSX_THREAD_STATUS_H__ */

