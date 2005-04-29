MACHINE_INSTHDRS += limits.h
MACHINE_INSTHDRS := ${MACHINE_INSTHDRS:S/^/${.CURDIR}\/include\/machine\//}
