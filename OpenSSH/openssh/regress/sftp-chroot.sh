#	$OpenBSD: sftp-chroot.sh,v 1.4 2014/01/20 00:00:30 dtucker Exp $
#	Placed in the Public Domain.

tid="sftp in chroot"

# Apple: /var/run is writable by group daemon; use /var/ssh-test
$SUDO mkdir -m 0755 /var/ssh-test
CHROOT=/var/ssh-test
FILENAME=testdata_${USER}
PRIVDATA=${CHROOT}/${FILENAME}

if [ -z "$SUDO" ]; then
  echo "skipped: need SUDO to create file in /var/run, test won't work without"
  exit 0
fi

$SUDO sh -c "echo mekmitastdigoat > $PRIVDATA" || \
	fatal "create $PRIVDATA failed"

start_sshd -oChrootDirectory=$CHROOT -oForceCommand="internal-sftp -d /"

verbose "test $tid: get"
${SFTP} -S "$SSH" -F $OBJ/ssh_config host:/${FILENAME} $COPY \
    >>$TEST_REGRESS_LOGFILE 2>&1 || \
	fatal "Fetch ${FILENAME} failed"
cmp $PRIVDATA $COPY || fail "$PRIVDATA $COPY differ"

$SUDO rm $PRIVDATA
#Apple:
$SUDO rm -rf /var/ssh-test
