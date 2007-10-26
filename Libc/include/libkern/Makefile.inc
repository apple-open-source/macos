LIBKERN_INSTHDRS += OSAtomic.h	OSCacheControl.h
LIBKERN_INSTHDRS := ${LIBKERN_INSTHDRS:S/^/${.CURDIR}\/include\/libkern\//}
