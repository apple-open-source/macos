

TESTS="tests/optionaltest tests/mantest tests/jqtest\
	tests/onigtest tests/shtest tests/utf8test tests/base64test"

for t in $TESTS
do
	$t
	echo $?
done
