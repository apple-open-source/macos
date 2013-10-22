use warnings;
use strict;

use Test::More tests => 11;

require_ok "shellwords.pl";

my $unmatched_quote;

$SIG{__WARN__} = sub {
	if($_[0] =~ /\AUnmatched double quote/) {
		$unmatched_quote = 1;
	} else {
		die "WARNING: $_[0]";
	}
};

$unmatched_quote = 0;
is_deeply [ &shellwords(qq(foo "bar quiz" zoo)) ], [ "foo", "bar quiz", "zoo" ];
ok !$unmatched_quote;

# Now test error return
$unmatched_quote = 0;
is_deeply [ &shellwords('foo bar baz"bach blech boop') ], [];
ok $unmatched_quote;

# missing quote after matching regex used to hang after change #22997
$unmatched_quote = 0;
"1234" =~ /(1)(2)(3)(4)/;
is_deeply [ &shellwords(qq{"missing quote}) ], [];
ok $unmatched_quote;

# make sure shellwords strips out leading whitespace and trailng undefs
# from parse_line, so it's behavior is more like /bin/sh
$unmatched_quote = 0;
is_deeply [ &shellwords(" aa \\  \\ bb ", " \\  ", "cc dd ee\\ ") ],
	[ "aa", " ", " bb", " ", "cc", "dd", "ee " ];
ok !$unmatched_quote;

$unmatched_quote = 0;
is_deeply [ &shellwords("foo\\") ], [ "foo" ];
ok !$unmatched_quote;

1;
