#! /bin/bash
# Copyright (C) 2007 Apple Inc. All rights reserved.

# Basic script to verify that printer sharing works correctly. Relies on the
# "printcap cache time" parameter being set low enough for us to notice changes
# in time for the next test.

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

if [ $# -lt 1 ]; then
cat <<EOF
Usage: printer_sharing.sh USERNAME PASSWORD
EOF
exit 1;
fi

SERVER=localhost # NOTE: this test is really only going to work on localhost
USERNAME=$1
PASSWORD=$2

ASROOT=sudo
TESTQUEUE=sambaqa
DEBUGLEVEL=1

SMBCLIENT="/usr/bin/smbclient -d$DEBUGLEVEL -N -U$USERNAME%$PASSWORD"
LPADMIN=/usr/sbin/lpadmin

CUPSCONFIG=/etc/cups/cupsd.conf
CUPSSAVED=/tmp/cupsd.conf.saved-$$

PSCONFIG=/Library/Preferences/com.apple.printservice.plist
PSSAVED=/tmp/com.apple.printservice.plist-saved-$$

failed=0

cleanup()
{
    # Restore cupsd.conf if we changed it.
    restore_config_file $CUPSCONFIG $(basename $0)

    # Restore the printservice plist if it was present and we changed it,
    # otherwise remove it.
    restore_config_file $PSCONFIG $(basename $0)

    restore_config_file /etc/smb.conf $(basename $0)

    $ASROOT killall -TERM cupsd
}

failtest()
{
	failed=`expr $failed + 1`
	submsg test FAILED
	exit
}

lpadmin()
{
    $LPADMIN "$@" 2>&1 | indent
}

switch_cups_browsing()
{
    value="$1"
    tmpfile=/tmp/cups.conf.$$

    # As per msweet, we need to toggle both Browsing and BrowseAddress to turn
    # CUPS printer sharing on and off. We also need to Listen on something
    # other than localhost.
    (
	    case $value in
		On)
			echo Browsing On
			echo BrowseAddress @LOCAL
			echo Listen $(hostname)
		;;
		Off) 
			echo Browsing Off
		;;
	    esac
	    grep -v -e Browsing -e BrowseAddress -e "Listen $(hostname)"
    ) < $CUPSSAVED > $tmpfile

    $ASROOT mv $tmpfile $CUPSCONFIG

    $ASROOT killall -TERM cupsd
    submsg switched CUPS browsing $value
}

create_local_printer()
{
    name="$1"
    submsg creating print queue named $name
    lpadmin -p "$name" -v "ipp://localhost/printer/$name" -E -m raw
}

destroy_local_printer()
{
    name="$1"
    submsg destroying print queue named $name
    lpadmin -x "$name"
}

cups_share_printer()
{
    name="$1"
    value="$2"

    case $value in
	yes)
	    submsg CUPS sharing print queue named $name
	    value="true"
	    ;;
	no)
	    submsg CUPS unsharing print queue named $name
	    value="false"
	    ;;
    esac

    lpadmin -p $name -o printer-is-shared=$value
}

ps_share_printer()
{
    name="$1"
    value="$2"
    shared=""

    case $value in
	yes)
	    submsg PrintService sharing print queue named $name
	    shared="<string>$name</string>"
	    ;;
	no)
	    submsg PrintService unsharing print queue named $name
	    ;;
    esac

    cat >$PSCONFIG <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
        <key>accessControlledQueues</key>
        <dict/>
        <key>serviceState</key>
        <true/>
        <key>smbSharedQueues</key>
        <array>
		$shared
        </array>
        <key>startTime</key>
        <date>2006-06-30T22:49:14Z</date>
</dict>
</plist>
EOF

    cat $PSCONFIG | indent | indent
}

printer_is_shared()
{
    queue="$1"
    output=/tmp/scratch.$$	

    vrun $SMBCLIENT -g -L $SERVER > $output 2>&1

    awk -F\| '$1 == "Printer"{print $2}' < $output | \
	grep "^$queue\$" > /dev/null 2>&1

    if [ "$?" = "0" ]; then
        submsg $SERVER IS sharing printer $queue
	cat $output | indent | indent
	true
    else
	submsg $SERVER IS NOT sharing printer $queue
	cat $output | indent | indent
	false
    fi
}

save_config_file $CUPSCONFIG $(basename $0)
save_config_file $PSCONFIG $(basename $0)
save_config_file /etc/smb.conf $(basename $0)

echo >> /etc/smb.conf <<EOF
# Added by $0
[global]
    printcap cache time = 0
EOF

cp $CUPSCONFIG $CUPSSAVED >/dev/null 2>&1
cp $PSCONFIG $PSSAVED >/dev/null 2>&1
register_cleanup_handler cleanup

echo Setting up a clean slate
destroy_local_printer $TESTQUEUE 
switch_cups_browsing Off
rm -f $PSCONFIG
printer_is_shared $TESTQUEUE && failtest

echo CHECKING that the browse policy controls the listing

echo Browse policy is Off, so $TESTQUEUE should be created unshared
create_local_printer $TESTQUEUE
printer_is_shared $TESTQUEUE && failtest

echo Browse policy is Off, so sharing $TESTQUEUE should have no effect
cups_share_printer $TESTQUEUE yes
printer_is_shared $TESTQUEUE && failtest

echo CHECKING that individual CUPS printers can be shared

echo Browse policy is On, so $TESTQUEUE should now be listed
switch_cups_browsing On
printer_is_shared $TESTQUEUE || failtest

echo Turn off CUPS sharing for $TESTQUEUE - should not be listed
cups_share_printer $TESTQUEUE no
printer_is_shared $TESTQUEUE && failtest

echo CHECKING that PrintServices policy overrides CUPS policy

echo Turn on PrintServices sharing for $TESTQUEUE - should be listed
ps_share_printer $TESTQUEUE yes
printer_is_shared $TESTQUEUE || failtest

echo Turn off PrintServices sharing for $TESTQUEUE - should not be listed
ps_share_printer $TESTQUEUE no
printer_is_shared $TESTQUEUE && failtest

echo Browse policy is Off - $TESTQUEUE should be listed
switch_cups_browsing Off
ps_share_printer $TESTQUEUE yes
printer_is_shared $TESTQUEUE || failtest

echo Turn off CUPS sharing for $TESTQUEUE - should be listed
cups_share_printer $TESTQUEUE no
printer_is_shared $TESTQUEUE || failtest

echo Turn off printer sharing
destroy_local_printer $TESTQUEUE

testok $0 $failed
