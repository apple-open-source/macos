#! /bin/zsh
SCRIPT_PATH="${0:A:h}"
. "$SCRIPT_PATH/common.sh"

function do_acp {
  certgen -n $1
  esc_acp "$tmpdir/authorized_principals" no_groups | tee "$tmpdir/results"
  if [[ $? -ne 0 ]]; then
    echo not ok
    return
  fi
  grep -q "^$1\$" $tmpdir/results
}

function expect_success {
  if do_acp $1; then
    echo ok
  else
    echo not ok
  fi
}

function expect_failure {
  if do_acp $1; then
    echo not ok
  else
    echo ok
  fi
}

echo 1..4

cat > "$tmpdir/authorized_principals" <<EOF
j*@APPLECONNECT.APPLE.COM
xyzzy@APPLECONNECT.APPLE.COM
EOF

expect_success jvidrine@APPLECONNECT.APPLE.COM
expect_success janedoe@APPLECONNECT.APPLE.COM
expect_success xyzzy@APPLECONNECT.APPLE.COM
expect_failure tcook@APPLECONNECT.APPLE.COM
