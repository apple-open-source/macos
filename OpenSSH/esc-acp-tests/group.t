#! /bin/zsh
SCRIPT_PATH="${0:A:h}"
. "$SCRIPT_PATH/common.sh"

princ=jvidrine@APPLECONNECT.APPLE.COM

function do_acp {
  esc_acp no_princs "$tmpdir/authorized_groups" | tee "$tmpdir/results"
  grep -q "^$princ\$" $tmpdir/results
}
function do_gen_acp {
  certgen -n $princ "$@"
  do_acp
} 


echo 1..9

echo tribeca > "$tmpdir/authorized_groups"

if do_gen_acp; then 
  echo not ok
else
  echo ok
fi

if do_gen_acp -O extension:groups@corp.apple.com=tribeca; then
  echo ok
else
  echo not ok
fi

if do_gen_acp -O extension:groups=tribeca; then
  echo ok
else
  echo not ok
fi

if do_gen_acp -O extension:groups@corp.apple.com=group1,group2,tribeca; then
  echo ok
else
  echo not ok
fi

echo 'tri*' > "$tmpdir/authorized_groups"

if do_acp; then
  echo ok
else
  echo not ok
fi

cat > "$tmpdir/authorized_groups" <<EOF
# Comment 1
  # Comment 2
    tribeca 
# That has leading and trailing spaces above
group1
    group2 # Trailing comments not supported yet
EOF

if do_gen_acp -O extension:groups@corp.apple.com=group2; then
  echo not ok
else
  echo ok
fi

if do_gen_acp -O extension:groups@corp.apple.com=tribeca; then
  echo ok
else
  echo not ok
fi

if do_gen_acp -O extension:groups@corp.apple.com=group1; then
  echo ok
else
  echo not ok
fi

if do_gen_acp -O extension:groups@corp.apple.com=,,,,group1; then
  echo ok
else
  echo not ok
fi

