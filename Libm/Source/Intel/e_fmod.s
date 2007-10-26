/*
 * Adapted from original written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 *
 *	by Ian Ollmann, Apple Computer 2006
 */

#include <machine/asm.h>
#include "abi.h"


PRIVATE_ENTRY(__fmodl)							//private interface for single and double precision fmod
ENTRY(fmodl)
	fldt	SECOND_ARG_OFFSET(STACKP)
	fldt	FIRST_ARG_OFFSET(STACKP)
1:	fprem
	fstsw	%ax
	btw		$10,%ax
	jc		1b
	fstp	%st(1)
	ret

