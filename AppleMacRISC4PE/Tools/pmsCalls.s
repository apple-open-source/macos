			.globl	_pmsCall
			.align	5
			
_pmsCall:	li		r0,0x600D			; Get the Power Manager Stepper control call
			sc							; Call the kernel
			blr

			.globl	_gtb
			.align	5
			
_gtb:		mftbu	r3
			mftb	r4
			mftbu	r5
			cmplw	r3,r5
			beqlr+
			b		_gtb
