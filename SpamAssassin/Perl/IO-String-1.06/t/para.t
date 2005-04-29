#!perl -w

use strict;
use Test qw(plan ok);

plan tests => 8;

use IO::String;

my $fh = IO::String->new(<<EOT);
a

a
b

a
b
c



a
b
c
d
EOT

$/ = "";

ok(<$fh>, "a\n\n");
ok(<$fh>, "a\nb\n\n");
ok(<$fh>, "a\nb\nc\n\n");
ok(<$fh>, "a\nb\nc\nd\n");
ok(<$fh>, undef);

$fh = IO::String->new(<<EOT);
a
b






EOT

ok(<$fh>, "a\nb\n\n");
ok(<$fh>, undef);
ok(<$fh>, undef);
