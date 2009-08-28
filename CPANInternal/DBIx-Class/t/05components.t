#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

use lib qw(t/lib);
use DBICTest::ForeignComponent;

plan tests => 6;

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

# Test for a warning with incorrect order in load_components
my @warnings = ();
{
  package A::Test;
  our @ISA = 'DBIx::Class';
  {
    local $SIG{__WARN__} = sub { push @warnings, shift};
    __PACKAGE__->load_components(qw(Core UTF8Columns));
  }
}
like( $warnings[0], qr/Core loaded before UTF8Columns/,
      'warning issued for incorrect order in load_components()' );
is( scalar @warnings, 1,
    'only one warning issued for incorrect load_components call' );

# Test that no warning is issued for the correct order in load_components
{
  @warnings = ();
  package B::Test;
  our @ISA = 'DBIx::Class';
  {
    local $SIG{__WARN__} = sub { push @warnings, shift };
    __PACKAGE__->load_components(qw(UTF8Columns Core));
  }
}
is( scalar @warnings, 0,
    'warning not issued for correct order in load_components()' );

use_ok('DBIx::Class::AccessorGroup');
