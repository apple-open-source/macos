use warnings;
use strict;

use Test::More tests => 11;

require_ok "newgetopt.pl";

our($opt_foo, $opt_Foo, $opt_bar, $opt_baR);

@ARGV = qw(-Foo -baR --foo bar);
$newgetopt::ignorecase = 0;
$newgetopt::ignorecase = 0;
undef $opt_baR;
undef $opt_bar;
ok NGetOpt("foo", "Foo=s");
is $opt_foo, 1;
is $opt_Foo, "-baR";
is_deeply \@ARGV, [ "bar" ];
ok !defined($opt_baR);
ok !defined($opt_bar);

@ARGV = qw(--foo -- --bar j);
undef $opt_foo;
undef $opt_bar;
ok NGetOpt("foo", "bar");
is_deeply \@ARGV, [qw(--bar j)];
is $opt_foo, 1;
ok !defined($opt_bar);

1;
