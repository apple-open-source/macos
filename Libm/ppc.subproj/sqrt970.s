#import	<architecture/ppc/asm_help.h>
#import	<architecture/ppc/pseudo_inst.h>

.section __DATA,__tuned_text,regular,pure_instructions

.globl _sqrt
_sqrt:
	BRANCH_EXTERN(___sqrt)
        
.text

.private_extern _hw_sqrt
_hw_sqrt:
       fsqrt f1, f1
       blr
Lhw_sqrt_end:
.set Lhw_sqrt_len, Lhw_sqrt_end - _hw_sqrt

.const
.private_extern _hw_sqrt_len
_hw_sqrt_len:
       .long Lhw_sqrt_len
