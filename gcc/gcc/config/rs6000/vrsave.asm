	.file	"vrsave.asm"
	#include "ppc-asm.h"

	FUNC_START(_savev20)
	addi	r12,r0,-192
	stvx	v20,r12,r0	# save v20
	ENTRY_POINT(_savev21)
	addi	r12,r0,-176
	stvx	v21,r12,r0	# save v21
	ENTRY_POINT(_savev22)
	addi	r12,r0,-160
	stvx	v22,r12,r0	# save v22
	ENTRY_POINT(_savev23)
	addi	r12,r0,-144
	stvx	v23,r12,r0	# save v23
	ENTRY_POINT(_savev24)
	addi	r12,r0,-128
	stvx	v24,r12,r0	# save v24
	ENTRY_POINT(_savev25)
	addi	r12,r0,-112
	stvx	v25,r12,r0	# save v25
	ENTRY_POINT(_savev26)
	addi	r12,r0,-96
	stvx	v26,r12,r0	# save v26
	ENTRY_POINT(_savev27)
	addi	r12,r0,-80
	stvx	v27,r12,r0	# save v27
	ENTRY_POINT(_savev28)
	addi	r12,r0,-64
	stvx	v28,r12,r0	# save v28
	ENTRY_POINT(_savev29)
	addi	r12,r0,-48
	stvx	v29,r12,r0	# save v29
	ENTRY_POINT(_savev30)
	addi	r12,r0,-32
	stvx	v30,r12,r0	# save v30
	ENTRY_POINT(_savev31)
	addi	r12,r0,-16
	stvx	v31,r12,r0	# save v31
	blr			# return to prologue
	FUNC_END(_savev20)
	
	FUNC_START(_restv20)
	addi	r12,r0,-192
	lvx	v20,r12,r0	# restore v20
	ENTRY_POINT(_restv21)
	addi	r12,r0,-176
	lvx	v21,r12,r0	# restore v21
	ENTRY_POINT(_restv22)
	addi	r12,r0,-160
	lvx	v22,r12,r0	# restore v22
	ENTRY_POINT(_restv23)
	addi	r12,r0,-144
	lvx	v23,r12,r0	# restore v23
	ENTRY_POINT(_restv24)
	addi	r12,r0,-128
	lvx	v24,r12,r0	# restore v24
	ENTRY_POINT(_restv25)
	addi	r12,r0,-112
	lvx	v25,r12,r0	# restore v25
	ENTRY_POINT(_restv26)
	addi	r12,r0,-96
	lvx	v26,r12,r0	# restore v26
	ENTRY_POINT(_restv27)
	addi	r12,r0,-80
	lvx	v27,r12,r0	# restore v27
	ENTRY_POINT(_restv28)
	addi	r12,r0,-64
	lvx	v28,r12,r0	# restore v28
	ENTRY_POINT(_restv29)
	addi	r12,r0,-48
	lvx	v29,r12,r0	# restore v29
	ENTRY_POINT(_restv30)
	addi	r12,r0,-32
	lvx	v30,r12,r0	# restore v30
	ENTRY_POINT(_restv31)
	addi	r12,r0,-16
	lvx	v31,r12,r0	# restore v31
	blr		# return to prologue
	FUNC_END(_restv20)
