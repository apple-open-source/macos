# $OpenLDAP: pkg/ldap/build/build.mak,v 1.3.2.1 2003/03/03 17:10:01 kurt Exp $
#
# Copyright 1998-2003 The OpenLDAP Foundation, All Rights Reserved.
# COPYING RESTRICTIONS APPLY, see COPYRIGHT file
#

all: build.txt

build.txt: version
	copy version build.txt
