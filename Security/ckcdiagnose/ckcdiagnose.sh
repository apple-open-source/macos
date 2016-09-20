#!/bin/sh

# Poor man's option parsing.
# Replace with shift/case once more options come along.
SHORT=0
if [ "$1" == "-s" ]; then
    SHORT=1
fi

PRODUCT_NAME=$(sw_vers -productName)
PRODUCT_VERSION=$(sw_vers -buildVersion)
HOSTNAME=$(hostname -s)
NOW=$(date "+%Y%m%d%H%M%S")

case $PRODUCT_NAME in
    *"OS X")
        PROD=OSX
        secd=secd
        secexec=security2
        OUTPUTPARENT=/var/tmp
        CRASHDIR=/Library/Logs/DiagnosticReports
        CSDIR=$HOME/Library/Logs/CloudServices
        SECLOGPATH=/var/log/module/com.apple.securityd
        syd=/System/Library/PrivateFrameworks/SyncedDefaults.framework/Support/syncdefaultsd
        kvsutil=/AppleInternal/Applications/kvsutil
        ;;
    *)
        PROD=IOS
        secd=securityd
        secexec=security
        OUTPUTPARENT=/Library/Logs/CrashReporter
        CRASHDIR=/var/mobile/Library/Logs/CrashReporter
        CSDIR=$CRASHDIR/DiagnosticLogs/CloudServices
        SECLOGPATH=/var/mobile/Library/Logs/CrashReporter/DiagnosticLogs
        syd=/System/Library/PrivateFrameworks/SyncedDefaults.framework/Support/syncdefaultsd
        kvsutil=/usr/local/bin/kvsutil
        ;;
esac

if (( ! $SHORT )); then
    OUTPUTBASE=ckcdiagnose_${HOSTNAME}_${PRODUCT_VERSION}_${NOW}
else
    OUTPUTBASE=ckcdiagnose_snapshot_${HOSTNAME}_${PRODUCT_VERSION}_${NOW}
fi
OUTPUT=$OUTPUTPARENT/$OUTPUTBASE

mkdir $OUTPUT

if [ "$PROD" = "IOS" ]; then
    while !(/usr/local/bin/profilectl cpstate | grep -Eq 'Unlocked|Disabled'); do
        echo Please ensure that your device is unlocked and press Enter. >&2
        read enter
    done
fi

(
echo Outputting to $OUTPUT
set -x

sw_vers > $OUTPUT/sw_vers.log

$secexec sync -D > $OUTPUT/syncD.log
$secexec sync -i > $OUTPUT/synci.log
$secexec sync -L > $OUTPUT/syncL.log

(( $SHORT )) || ([ -x $kvsutil ] && $kvsutil show com.apple.security.cloudkeychainproxy3 > $OUTPUT/kvsutil_show.txt 2>&1)

if [ "$PROD" == "OSX" ]; then
    $secexec item -g class=genp,nleg=1,svce="iCloud Keychain Account Meta-data" > $OUTPUT/ickcmetadata.log
    $secexec item -g class=genp,nleg=1,acct=engine-state > $OUTPUT/engine-state.log
elif [ "$PROD" == "IOS" ]; then
    $secexec item -g class=genp,svce="iCloud Keychain Account Meta-data" > $OUTPUT/ickcmetadata.log
    $secexec item -g class=genp,acct=engine-state > $OUTPUT/engine-state.log
fi

# In preparation, before getting any of the logs, query all classes,
# just in order to excercise the decryption and corruption
# verification for all items. This will log errors and simulated crashes
# if any of the items should turn out corrupted.
# The items are NOT saved in the diagnostic log, because they potentially
# contain very private items.
for class in genp inet cert keys; do
    for sync in 0 1; do
        for tomb in 0 1; do

            echo class=${class},sync=${sync},tomb=${tomb},u_AuthUI=u_AuthUIS: >> $OUTPUT/keychain-state.log
            ${secexec} item -q class=${class},sync=${sync},tomb=${tomb},u_AuthUI=u_AuthUIS | grep '^acct'|wc -l 2>&1 >> $OUTPUT/keychain-state.log
        done
    done
done

if (( ! $SHORT )); then
    syslog -k Sender Seq syncdefaults > $OUTPUT/syslog_syncdefaults.log
    syslog -k Sender Seq $secd > $OUTPUT/syslog_secd.log
    syslog -k Sender Seq CloudKeychain > $OUTPUT/syslog_cloudkeychain.log
fi

(( $SHORT )) || (sbdtool status > $OUTPUT/sbdtool_status.log 2>&1)

if [ "$PROD" == "OSX" ]; then
(( $SHORT )) || plutil -p $HOME/Library/SyncedPreferences/com.apple.sbd.plist > $OUTPUT/sbd_kvs.txt
elif [ "$PROD" == "IOS" ]; then
(( $SHORT )) || plutil -p /var/mobile/Library/SyncedPreferences/com.apple.sbd.plist > $OUTPUT/sbd_kvs.txt
fi

$syd status > $OUTPUT/syd_status.txt 2>&1
$syd lastrequest > $OUTPUT/syd_lastrequest.txt 2>&1
$syd serverlimits > $OUTPUT/syd_serverlimits.txt 2>&1

# Compare kvsutil and sync -D state, shows if store diverged from on-device state.
if (( ! $SHORT )); then
    if [ -f $OUTPUT/kvsutil_show.txt ]; then
        cat $OUTPUT/kvsutil_show.txt | grep -E '^    "?[o-]?ak.* = ' | sed -E 's/^    "?([^"]*)"? = \<.* (.*) (.*)\>.*$/\1 \2\3/g;s/^(.*) [0-9a-f]*([0-9a-f]{8})/\1 \2/g' | sort > $OUTPUT/kvs_keys.txt
        cat $OUTPUT/syncD.log | grep -E 'contents = "?[o-]?ak' | sed -E 's/^.*contents = "?([^"]*)"?\} = .*bytes = .* ... [0-9a-f]+([0-9a-f]{8})\}/\1 \2/g' | sort > $OUTPUT/syncD_keys.txt
        diff -u $OUTPUT/kvs_keys.txt $OUTPUT/syncD_keys.txt > $OUTPUT/kvs_syncD_diff.txt
    fi
fi

if [ "$PROD" = "IOS" ]; then
    cp /private/var/preferences/com.apple.security.cloudkeychainproxy3.keysToRegister.plist $OUTPUT/
    cp /var/mobile/Library/SyncedPreferences/com.apple.security.cloudkeychainproxy3.plist $OUTPUT/
else
    cp ~/Library/Preferences/com.apple.security.cloudkeychainproxy3.keysToRegister.plist $OUTPUT/
    cp ~/Library/SyncedPreferences/com.apple.security.cloudkeychainproxy3.plist $OUTPUT/
fi

if (( ! $SHORT )); then
    cp $SECLOGPATH/security.log* $OUTPUT/

    cp $CRASHDIR/*${secd}* $OUTPUT/
    cp $CRASHDIR/*syncdefaults* $OUTPUT/
    cp $CRASHDIR/*CloudKeychain* $OUTPUT/

    (cd $CSDIR && for x in *_*.asl; do syslog -f "$x" > "$OUTPUT/${x%%.asl}.log"; done)

    (cd  $SECLOGPATH; gzcat -c -f security.log*) > $OUTPUT/security-complete.log
    
    # potential problems
    (cd  $SECLOGPATH; gzcat -c security.log.*.gz; cat security.log.*Z) | grep -E -- 'Invalid date.|-26275|[cC]orrupt|[cC]rash|Public Key not available' > $OUTPUT/problems.log
    (cd  $SECLOGPATH; gzcat -c security.log.*.gz; cat security.log.*Z) | cut -d ' ' -f 6- | sort |uniq -c | sort -n > $OUTPUT/security-sorted.log
fi

) > $OUTPUT/ckcdiagnose.log 2>&1

tar czf $OUTPUT.tgz -C $OUTPUTPARENT $OUTPUTBASE

rm -r $OUTPUT

if (( ! $SHORT )); then
    echo
    echo "The file containing the diagnostic information is "
    echo "        $OUTPUT.tgz"
    echo 'Please attach it to a Radar in "Security / iCloud Keychain"'
    echo

    [ "$PROD" = "OSX" ] && open $OUTPUTPARENT
else
    echo $OUTPUT.tgz
fi


