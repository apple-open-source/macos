#!/bin/sh

[ "$UID" == 0 ] || { echo "must be root" ; exit 1 ; }

if [ -x /Users/Shared/Raft/raft ] ; then
    RAFT=/Users/Shared/Raft/raft
else
    RAFT=:
fi

#(ktutil list | grep host/) > /dev/null || { echo "list test failed"; exit 1; }

echo " = Testing gss ="
/usr/local/libexec/heimdal/bin/test-gss.sh || { echo "gss test failed"; exit 1; }

echo " = Testing kClient ="
/usr/local/libexec/heimdal/bin/test-kClient || { echo "kClient test failed"; exit 1; }

# don't test KLL since it does a UI popup and we can't test that
#echo " = Testing kll ="
#/usr/local/libexec/heimdal/bin/test-kll ktestuser@ADS.APPLE.COM || { echo "kll test failed"; exit 1; }

echo " = Testing kll2 ="
/usr/local/libexec/heimdal/bin/test-kll2 ktestuser@ADS.APPLE.COM foobar || { echo "kll2 test failed"; exit 1; }

echo " = Testing krb ="
/usr/local/libexec/heimdal/bin/test-krb || { echo "krb test failed"; exit 1; }

echo " = Testing krb4 ="
/usr/local/libexec/heimdal/bin/test-krb4 || { echo "krb4 test failed"; exit 1; }

echo " = Testing krb5 ="
/usr/local/libexec/heimdal/bin/test-krb5 || { echo "krb5 test failed"; exit 1; }

echo " = Testing principal ="
/usr/local/libexec/heimdal/bin/test-principal || { echo "princ test failed"; exit 1; }

echo " = Testing sd ="
/usr/local/libexec/heimdal/bin/test-sd || { echo "vnc test failed"; exit 1; }

#${RAFT} -f /usr/local/libexec/heimdal/raft/KLLAcquireCredUI/KLLAcquireCredUI.py

exit 0
