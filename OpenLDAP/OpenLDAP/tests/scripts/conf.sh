#! /bin/sh
# $OpenLDAP: pkg/ldap/tests/scripts/conf.sh,v 1.5 2001/06/07 16:00:16 kurt Exp $
sed -e s/@BACKEND@/$BACKEND/ -e s/^#$BACKEND#//
