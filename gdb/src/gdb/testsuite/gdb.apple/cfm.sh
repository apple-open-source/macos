#! /bin/sh

srcdir=`cd "$*"; pwd`

rm -rf /tmp/cfm
mkdir /tmp/cfm

(cd /tmp/cfm && uudecode "${srcdir}"/cfm-libs/*.uu)

for i in `yes | cat -n | head -64 | awk '{ print $1; }'`; do
    cp -p /tmp/cfm/template.cfm /tmp/cfm/$i.cfm
    cp -p /tmp/cfm/._template.cfm /tmp/cfm/._$i.cfm
done
