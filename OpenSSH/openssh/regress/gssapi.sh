#!/bin/sh

port=3007
realm="TEST.APPLE.COM"

service="test"
user="local"
pass="local"
server="localhost"

h3ldir="/usr/local/libexec/heimdal/bin"

tmp="/private/tmp/krb5_testing_$$"
kt_file="${tmp}/server.keytab"
cc_file="${tmp}/krb5ccache"
pw_file="${tmp}/password-file"
export  KRB5CCNAME="FILE:${cc_file}"
export KRB5_KTNAME="FILE:${kt_file}"
export KRB5_CONFIG="${tmp}/kdc.conf"

kinit="kinit -c ${KRB5CCNAME}"
kdestroy="kdestroy -c ${KRB5CCNAME}"
klist="klist -c ${KRB5CCNAME}"
kadmin="kadmin -l -r ${realm}"

mkdir -p "${tmp}"
echo "${pass}" > "${pw_file}"

#--------------------------#
# Configure and start kr5b #
#--------------------------#
cat "${h3ldir}/krb5.conf.in" |
	sed \
		-e "s,[@]realm[@],${realm},g" \
		-e "s,[@]objdir[@],${tmp},g" \
		-e "s,[@]port[@],${port},g" \
	> "${KRB5_CONFIG}"

${kadmin} init \
	--realm-max-ticket-life=1day \
	--realm-max-renewable-life=1month \
	"${realm}" || exit 1

${kadmin} add -p "${pass}" --use-defaults "${user}@${realm}" || exit 1
${kadmin} add -r --use-defaults "host/${server}@${realm}" || exit 1
${kadmin} ext_keytab "${user}@${realm}" || exit 1
${kadmin} ext_keytab "host/${server}@${realm}" || exit 1

"/System/Library/PrivateFrameworks/Heimdal.framework/Helpers/kdc" \
	--config-file="${KRB5_CONFIG}" \
	--addresses="localhost" \
	--ports="${port}" \
	--no-sandbox &
kdcpid=$!
trap "kill -9 ${kdcpid}; echo signal killing ${kdcpid}; exit 0;" EXIT
sleep 1

${kinit} --password-file="${pw_file}" "${user}@${realm}"

#----------------------#
# Starting ssh testing #
#----------------------#

cp $OBJ/sshd_config $OBJ/sshd_config.orig
cat >> $OBJ/sshd_config << __GSSAPI__
# GSSAPI options
GSSAPIAuthentication yes
GSSAPICleanupCredentials yes
GSSAPIStrictAcceptorCheck yes
GSSAPIKeyExchange yes
__GSSAPI__
start_sshd

# SSH to server
${SSH} -F $OBJ/ssh_config \
	-o "GSSAPIAuthentication=yes" \
	-o "GSSAPIDelegateCredentials=yes" \
	-o "GSSAPIKeyExchange=yes" \
	-o "GSSAPITrustDNS=no" \
	somehost true
if [ $? -ne 0 ]; then
	fail "ssh connect with gssapi failed"
fi

cp $OBJ/sshd_config.orig $OBJ/sshd_config

#--------------#
# Cleanup krb5 #
#--------------#
${kdestroy} --cache="${cc_file}" --all

trap - EXIT
kill $kdcpid

${kadmin} del "host/${server}@${realm}"
${kadmin} del "${user}@${realm}"

rm -rf "${tmp}"
