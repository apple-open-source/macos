##
# Preprocessing script used to prepare OpenLDAP for Apple builds
##

##
# Some Netscape compatability routines
##

cp AppleExtras/apple_compat.c OpenLDAP/libraries/libldap
ex OpenLDAP/libraries/libldap/Makefile.in << EX_EOF > /dev/null
/SRCS
s/apple_compat.c //
s/=	/=	apple_compat.c /
/OBJS
s/apple_compat.lo //
s/=	/=	apple_compat.lo /
w
q
EX_EOF


##
# slapd.conf
##

cp AppleExtras/slapd.conf OpenLDAP/servers/slapd/slapd.conf


##
# Hardwire paths
##

cat OpenLDAP/build/man.mk |\
	sed -e 's/\$(sysconfdir)/\/etc\/openldap/g' \
		-e 's/\$(localstatedir)/\/var\/db\/openldap/g' \
		-e 's/\$(datadir)/\/usr\/share/g' \
		-e 's/\$(sbindir)/\/usr\/sbin/g' \
		-e 's/\$(bindir)/\/usr\/bin/g' \
		-e 's/\$(libdir)/\/usr\/lib/g' \
		-e 's/\$(libexecdir)/\/usr\/libexec/g' \
	> OpenLDAP/build/man.mk

cat OpenLDAP/include/ldap_config.h.in |\
	sed -e 's/%BINDIR%/\/usr\/bin/g' \
		-e 's/%SBINDIR%/\/usr\/sbin/g' \
		-e 's/%DATADIR%/\/usr\/share\/openldap/g' \
		-e 's/%SYSCONFDIR%/\/etc\/openldap/g' \
		-e 's/%LIBEXECDIR%/\/usr\/libexec/g' \
		-e 's/%RUNDIR%/\/var\/run/g' \
	> OpenLDAP/include/ldap_config.h.in
