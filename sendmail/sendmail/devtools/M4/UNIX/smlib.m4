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
#	$Id: smlib.m4,v 1.1.1.2 2002/03/12 17:59:59 zarzycki Exp $
#
divert(0)dnl

define(`confLIBEXT', `a')dnl

define(`bldPUSH_SMLIB',
	`bldPUSH_TARGET(bldABS_OBJ_DIR`/lib$1/lib$1.a')
bldPUSH_SMDEPLIB(bldABS_OBJ_DIR`/lib$1/lib$1.a')
PREPENDDEF(`confLIBS', bldABS_OBJ_DIR`/lib$1/lib$1.a')
divert(bldTARGETS_SECTION)
bldABS_OBJ_DIR/lib$1/lib$1.a:
	(cd ${SRCDIR}/lib$1; sh Build ${SENDMAIL_BUILD_FLAGS})
divert
')dnl
