MALLOC_INSTHDRS += malloc.h
MALLOC_INSTHDRS := ${MALLOC_INSTHDRS:S/^/${.CURDIR}\/include\/malloc\//}
