# $FreeBSD: head/bin/sh/tests/invocation/Makefile 322455 2017-08-13 14:36:10Z jilles $

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/bin/sh/${.CURDIR:T}

.PATH: ${.CURDIR:H}
ATF_TESTS_SH=	functional_test

${PACKAGE}FILES+=	sh-ac1.0
${PACKAGE}FILES+=	sh-c-missing1.0
${PACKAGE}FILES+=	sh-c1.0
${PACKAGE}FILES+=	sh-ca1.0
${PACKAGE}FILES+=	sh-fca1.0

.include <bsd.test.mk>
