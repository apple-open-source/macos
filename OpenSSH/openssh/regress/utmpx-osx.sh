tid="utmpx mac os x"

if [ ! -f "/private/var/run/utmpx" ]
then
	fail "utmpx|utmpx does not exist"
fi
