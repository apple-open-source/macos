SYS_INSTHDRS += acl.h statvfs.h

SYS_INSTHDRS := ${SYS_INSTHDRS:S/^/${.CURDIR}\/include\/sys\//}
