#! /bin/sh
# $OpenLDAP: pkg/ldap/tests/scripts/acfilter.sh,v 1.6 2000/06/24 22:35:20 kurt Exp $
#
# Strip comments
#
egrep -iv '^#'
