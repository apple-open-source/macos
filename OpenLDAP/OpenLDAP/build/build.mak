# $OpenLDAP: pkg/ldap/build/build.mak,v 1.3 2002/01/04 20:17:29 kurt Exp $
#
# Copyright 1998-2002 The OpenLDAP Foundation, All Rights Reserved.
# COPYING RESTRICTIONS APPLY, see COPYRIGHT file
#

all: build.txt

build.txt: version
	copy version build.txt
