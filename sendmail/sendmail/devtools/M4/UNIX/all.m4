divert(-1)
#
# Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	$Id: all.m4,v 1.1.1.2 2002/03/12 17:59:58 zarzycki Exp $
#
divert(0)dnl
ALL=${BEFORE} ${LINKS} bldTARGETS

all: ${ALL}

clean: bldCLEAN_TARGETS

define(`bldADD_SRC', ${$1SRCS} )dnl
SRCS=bldFOREACH(`bldADD_SRC(', bldC_PRODUCTS)
define(`bldADD_OBJS', ${$1OBJS} )dnl
OBJS=bldFOREACH(`bldADD_OBJS(', bldC_PRODUCTS)

ifdef(`bldNO_INSTALL', `divert(-1)')
install: bldINSTALL_TARGETS

install-strip: bldINSTALL_TARGETS ifdef(`bldSTRIP_TARGETS', `bldSTRIP_TARGETS')
ifdef(`bldNO_INSTALL', `divert(0)')

ifdef(`confREQUIRE_LIBSM',`
ifdef(`confSM_OS_HEADER',
`sm_os.h: ${SRCDIR}/inc`'lude/sm/os/confSM_OS_HEADER.h
	${RM} ${RMOPTS} sm_os.h
	${LN} ${LNOPTS} ${SRCDIR}/inc`'lude/sm/os/confSM_OS_HEADER.h sm_os.h',
`sm_os.h:
	${CP} /dev/null sm_os.h')')

divert(bldDEPENDENCY_SECTION)
################  Dependency scripts
include(confBUILDTOOLSDIR/M4/depend/ifdef(`confDEPEND_TYPE', `confDEPEND_TYPE',
`generic').m4)dnl
################  End of dependency scripts
divert(0)
