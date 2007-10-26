if [ $# != 1 ]; then
	echo "Usage: $0 [framework_dir]"
	exit 1
fi

TEST_SOURCE="/tmp/test_ruby.c"
TEST_BIN="/tmp/test_ruby"

cat > $TEST_SOURCE <<EOF || exit 1
#include <Ruby/ruby.h>

int
main (int argc, char **argv) 
{
	ruby_init ();
	return 0;
}
EOF

gcc $TEST_SOURCE -o $TEST_BIN -F$1 -framework Ruby || exit 1
$TEST_BIN || exit 1

exit 0
