PAMDIR=		${.CURDIR}/../../../../contrib/libpam

LIB=		pam_nis
SHLIB_NAME=	pam_nis.so
SRCS=		pam_nis.c
CFLAGS+=	-Wall
CFLAGS+=	-I${PAMDIR}/libpam/include
CFLAGS+=	-I${.CURDIR}/../../libpam
DPADD+=		${LIBUTIL} ${LIBGCC_PIC} ${LIBCRYPT}
LDADD+=	
INTERNALLIB=	yes
INTERNALSTATICLIB=yes

.include <bsd.lib.mk>
