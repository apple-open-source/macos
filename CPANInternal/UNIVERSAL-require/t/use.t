#!/usr/bin/perl -Tw

use Test::More tests => 10;
use_ok "UNIVERSAL::require";

use lib qw(t);

my $Filename = quotemeta $0;

is( Dummy->use, 23 );

is( Dummy->use("foo", "bar"), 1 );
is( foo(), 42 );
is( bar(), 23 );

ok( !Dummy->use(1) );
is( $UNIVERSAL::require::ERROR, $@ );

#line 23
ok( !Dont::Exist->use );
like( $@, qq[/^Can't locate Dont/Exist.pm in .* at $Filename line 23\./]  );
is( $UNIVERSAL::require::ERROR, $@ );
