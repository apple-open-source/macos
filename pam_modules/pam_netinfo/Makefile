PAMDIR=		${.CURDIR}/../../../../contrib/libpam

LIB=		pam_netinfo
SHLIB_NAME=	pam_netinfo.so
SRCS=		pam_netinfo.c
CFLAGS+=	-Wall
CFLAGS+=	-I${PAMDIR}/libpam/include
CFLAGS+=	-I${.CURDIR}/../../libpam
DPADD+=		${LIBUTIL} ${LIBGCC_PIC} ${LIBCRYPT}
LDADD+=		-lnetinfo
INTERNALLIB=	yes
INTERNALSTATICLIB=yes

.include <bsd.lib.mk>
