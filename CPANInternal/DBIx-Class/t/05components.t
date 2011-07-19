#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

use lib qw(t/lib);
use DBICTest::ForeignComponent;

#   Tests if foreign component was loaded by calling foreign's method
ok( DBICTest::ForeignComponent->foreign_test_method, 'foreign component' );

#   Test for inject_base to filter out duplicates
{   package DBICTest::_InjectBaseTest;
    use base qw/ DBIx::Class /;
    package DBICTest::_InjectBaseTest::A;
    package DBICTest::_InjectBaseTest::B;
    package DBICTest::_InjectBaseTest::C;
}
DBICTest::_InjectBaseTest->inject_base( 'DBICTest::_InjectBaseTest', qw/
    DBICTest::_InjectBaseTest::A
    DBICTest::_InjectBaseTest::B
    DBICTest::_InjectBaseTest::B
    DBICTest::_InjectBaseTest::C
/);
is_deeply( \@DBICTest::_InjectBaseTest::ISA,
    [qw/
        DBICTest::_InjectBaseTest::A
        DBICTest::_InjectBaseTest::B
        DBICTest::_InjectBaseTest::C
        DBIx::Class
    /],
    'inject_base filters duplicates'
);

use_ok('DBIx::Class::AccessorGroup');
use_ok('DBIx::Class::Componentised');

done_testing;
