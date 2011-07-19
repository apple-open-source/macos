#! /usr/bin/perl -Tw

use strict;
use warnings;
use Test::Builder;
use Test::Harness;
use Test::Builder::Tester tests => 13;
use Test::More;

BEGIN { use_ok( 'Test::Exception' ) };

sub div {
   my ($a, $b) = @_;
   return( $a / $b );
};

my $filename = sub { return (caller)[1] }->();

{
    my $ok = dies_ok { div(1, 0) } 'dies_ok passed on die';
    ok($ok, 'dies_ok returned true when block dies');
}


{
    test_out('not ok 1 - dies_ok failed');
    test_fail( +1 );
    my $ok = dies_ok { div(1, 1) } 'dies_ok failed';
    test_test('dies_ok fails when code does not die');

    ok(!$ok, 'dies_ok returned false on failure');
}


{
    my $ok = throws_ok { div(1, 0) } '/./', 'throws_ok succeeded';
    ok($ok, 'throws_ok returned true on success');
}

{
    test_out('not ok 1 - throws_ok failed');
    test_fail(+3);
    test_err('# expecting: /./');
    test_err('# found: normal exit');
    my $ok = throws_ok { div(1, 1) } '/./', 'throws_ok failed';
    test_test('throws_ok fails when appropriate');

    ok(!$ok, 'throws_ok returned false on failure');
}

{
    my $ok = lives_ok { div(1, 1) } 'lives_ok succeeded';
    ok($ok, 'lives_ok returned true on success');
}

{
    test_out('not ok 1 - lives_ok failed');
    test_fail(+2);
    test_err("# died: Illegal division by zero at $filename line 14.");
    my $ok = lives_ok { div(1, 0) } 'lives_ok failed';
    test_test("dies_ok fails"); 

    ok(!$ok, 'lives_ok returned false on failure');
}
