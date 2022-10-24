#!/bin/sh

exitcode=0
passed=`cat .passed`
failed=`cat .failed`

# NFLOG support depends on both DLT_NFLOG and working <pcap/nflog.h>

if grep '^#define HAVE_PCAP_NFLOG_H 1$' ../config.h &>/dev/null
then
	if TESTonce.sh nflog-e nflog.pcap flog-e.out '-e'
	then
		passed=`expr $passed + 1`
		echo $passed >.passed
	else
		failed=`expr $failed + 1`
		echo $failed >.failed
		exitcode=1
	fi
else
	printf '    %-35s: TEST SKIPPED (compiled w/o NFLOG)\n' 'nflog-e'
fi

exit $exitcode
