! $Xorg: sparcsolstack.s,v 1.3 2000/08/17 19:55:20 cpqbld Exp $
	.seg	"text"
	.proc	16
	.globl	getStackPointer
getStackPointer:
	retl
	mov	%sp,%o0
	.globl	getFramePointer
getFramePointer:
	retl
	mov	%fp,%o0
