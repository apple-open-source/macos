#!/bin/sh

port=3007
realm="TEST.APPLE.COM"

service="test"
user="local"
pass="local"
server="localhost"

loc="/usr/local/libexec/heimdal/bin"

tmp="/private/tmp/krb5_testing_$$"
kt_file="${tmp}/server.keytab"
cc_file="${tmp}/krb5ccache"
pw_file="${tmp}/password-file"
export  KRB5CCNAME="FILE:${cc_file}"
export KRB5_KTNAME="FILE:${kt_file}"
export KRB5_CONFIG="${tmp}/kdc.conf"

kinit="kinit -c ${KRB5CCNAME}"
kdestroy="kdestory -c ${KRB5CCNAME}"
klist="klist -c ${KRB5CCNAME}"
kadmin="kadmin -l -r ${realm}"

echo "##### Starting #####"
echo "--------------------"
echo "hostname: ${server}"
echo "realm   : ${realm}"
echo "port    : ${port}"
echo "tmpdir  : ${tmpdir}"
echo ""

mkdir -p "${tmp}"

echo "${pass}" > "${pw_file}"

cat "${loc}/krb5.conf.in" |
	sed \
		-e "s,[@]realm[@],${realm},g" \
		-e "s,[@]objdir[@],${tmp},g" \
		-e "s,[@]port[@],${port},g" \
	> "${KRB5_CONFIG}"

echo "##### KRB5 CONFIG #####"
cat "${KRB5_CONFIG}"
echo "##########"
echo ""

echo "##### Configuring KDC #####"
${kadmin} init \
	--realm-max-ticket-life=1day \
	--realm-max-renewable-life=1month \
	"${realm}" || exit 1

${kadmin} add -p "${pass}" --use-defaults "${user}@${realm}" || exit 1
${kadmin} add -r --use-defaults "host/${server}@${realm}" || exit 1
${kadmin} add -r --use-defaults "${service}/${server}@${realm}" || exit 1

${kadmin} ext_keytab "${user}@${realm}" || exit 1
${kadmin} ext_keytab "host/${server}@${realm}" || exit 1
${kadmin} ext_keytab "${service}/${server}@${realm}" || exit 1

echo "##### Start the kdc #####"
"/System/Library/PrivateFrameworks/Heimdal.framework/Helpers/kdc" \
	--config-file="${KRB5_CONFIG}" \
	--addresses="localhost" \
	--ports="${port}" \
	--no-sandbox &
ret=$!
echo "Sleeping to let kdc start..."
sleep 1

trap "kill -9 ${ret}; echo signal killing ${ret}; exit 0;" EXIT

echo "##### Get tkt for ${user}@${realm} #####"
${kinit} --password-file="${pw_file}" "${user}@${realm}"

echo "##### Starting server/client for test one #####"
"${loc}/test-gss-server" --port ${port} --sname "${service}" &
"${loc}/test-gss-client" --server "${server}" --port ${port} \
		--sprinc "${service}/${server}@${realm}" \
		--cprinc "${user}@${realm}" || exit 100

echo "##### Removing ccache #####"
kdestroy -cache="${cc_file}"

echo "##### Removing setup #####"
${kadmin} del "${service}/${server}@${realm}"
${kadmin} del "host/${server}@${realm}"
${kadmin} del "${user}@${realm}"

rm -rf "${tmp}"

echo "##### DONE #####"
exit 0
