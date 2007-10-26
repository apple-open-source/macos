#!/usr/bin/perl -Tw

use Test::More tests => 11;
use_ok "UNIVERSAL::require";

use lib qw(t);


is( Dummy->require,               23,           'require()' );
is( $UNIVERSAL::require::ERROR,   '',           '  $ERROR empty' );
ok( $Dummy::VERSION,                            '  $VERSION ok' );

{
    $SIG{__WARN__} = sub { warn @_ 
                             unless $_[0] =~ /^Subroutine \w+ redefined/ };
    delete $INC{'Dummy.pm'};
    is( Dummy->require(0.4), 23,                  'require($version)' );
    is( $UNIVERSAL::require::ERROR, '',           '  $ERROR empty' );

    delete $INC{'Dummy.pm'};
    ok( !Dummy->require(1.0),                       'require($version) fail' );
    like( $UNIVERSAL::require::ERROR,
          '/^Dummy version 1.* required--this is only version 0.5/' );
}

{
    my $warning = '';
    local $SIG{__WARN__} = sub { $warning = join '', @_ };
    eval 'use UNIVERSAL';
    is( $warning, '',     'use UNIVERSAL doesnt interfere' );
}


my $evil = "Dummy; Test::More::fail('this should never be called');";
ok !$evil->require;
isnt $@, '';
