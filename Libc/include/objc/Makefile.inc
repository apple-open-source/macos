OBJC_INSTHDRS += malloc.h zone.h
OBJC_INSTHDRS := ${OBJC_INSTHDRS:S/^/${.CURDIR}\/include\/objc\//}
