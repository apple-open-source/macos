#
REALRMAN=/usr/local/bin/rman.real
VOLLISTFILE=/etc/default/man

VOLLIST=`grep '^ORDER' $VOLLISTFILE | cut -f2 -d=`
exec $REALRMAN -V "=$VOLLIST" $*
