#define REGISTER_TYPE unsigned int
#define FP_REGISTER_TYPE double
#define VP_REGISTER_TYPE float

#define	GP0_REGNUM 0		/* GPR register 0 */

#define FP0_REGNUM 32		/* FPR (Floating point) register 0 */

#define VP0_REGNUM 64		/* AltiVec register 0 */

#define VSCR_REGNUM 96		/* AltiVec status register */

#define PC_REGNUM 97		/* Program counter (instruction address %iar) */
#define PS_REGNUM 98		/* Processor (or machine) status (%msr) */
#define	CR_REGNUM 99		/* Condition register */
#define	LR_REGNUM 100		/* Link register */
#define	CTR_REGNUM 101		/* Count register */
#define	XER_REGNUM 102		/* Fixed point exception registers */
#define	MQ_REGNUM 103		/* Multiply/quotient register */
#define FPSCR_REGNUM 104	/* Floating-point status register */
#define VRSAVE_REGNUM 105	/* AltiVec save register */

#define FP_REGNUM 1		/* Contains address of executing stack frame */
#define SP_REGNUM 1		/* Contains address of top of stack */
#define	TOC_REGNUM 2		/* TOC register */
#define RV_REGNUM 3		/* Contains simple return values */
#define SRA_REGNUM RV_REGNUM	/* Contains address of struct return values */
#define FPRV_REGNUM FP0_REGNUM

#define FIRST_GP_REGNUM 0
#define LAST_GP_REGNUM 31
#define NUM_GP_REGS (LAST_GP_REGNUM - FIRST_GP_REGNUM + 1)
#define SIZE_GP_REGS (NUM_GP_REGS * 4)

#define FIRST_FP_REGNUM 32
#define LAST_FP_REGNUM 63
#define NUM_FP_REGS (LAST_FP_REGNUM - FIRST_FP_REGNUM + 1)
#define SIZE_FP_REGS (NUM_FP_REGS * 8)

#define FIRST_VP_REGNUM 64
#define LAST_VP_REGNUM 96
#define NUM_VP_REGS (LAST_VP_REGNUM - FIRST_VP_REGNUM + 1)
#define SIZE_VP_REGS (NUM_VP_REGS * 16)

#define FIRST_GSP_REGNUM 97
#define LAST_GSP_REGNUM 103
#define NUM_GSP_REGS (LAST_GSP_REGNUM - FIRST_GSP_REGNUM + 1)

#define FIRST_FSP_REGNUM 104
#define LAST_FSP_REGNUM 104
#define NUM_FSP_REGS (LAST_FSP_REGNUM - FIRST_FSP_REGNUM + 1)

#define FIRST_VSP_REGNUM 105
#define LAST_VSP_REGNUM 105
#define NUM_VSP_REGS (LAST_VSP_REGNUM - FIRST_VSP_REGNUM + 1)

#define FIRST_SP_REGNUM FIRST_GSP_REGNUM
#define LAST_SP_REGNUM LAST_VSP_REGNUM
#define NUM_SP_REGS (LAST_SP_REGNUM - FIRST_SP_REGNUM + 1)
#define SIZE_SP_REGS (NUM_SP_REGS * 4)

#define NUM_REGS (NUM_GP_REGS + NUM_FP_REGS + NUM_VP_REGS + NUM_SP_REGS)

#define REGISTER_BYTES (SIZE_GP_REGS + SIZE_FP_REGS + SIZE_VP_REGS + SIZE_SP_REGS)

