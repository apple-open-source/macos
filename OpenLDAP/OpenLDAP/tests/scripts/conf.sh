#! /bin/sh
# $OpenLDAP: pkg/ldap/tests/scripts/conf.sh,v 1.5.2.3 2003/02/10 18:43:11 kurt Exp $
if [ x"$MONITORDB" = x"yes" ] ; then
	MON=monitor
else
	MON=nomonitor
fi
sed -e "s/@BACKEND@/${BACKEND}/"	\
	-e "s/^#${BACKEND}#//"			\
	-e "s/^#${MON}#//"				\
	-e "s/@PORT@/${PORT}/"			\
	-e "s/@SLAVEPORT@/${SLAVEPORT}/"
