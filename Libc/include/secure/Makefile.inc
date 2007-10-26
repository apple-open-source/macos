SECURE_INSTHDRS += _common.h	\
		   _string.h	\
                   _stdio.h

SECURE_INSTHDRS := ${SECURE_INSTHDRS:S/^/${.CURDIR}\/include\/secure\//}
