use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;
use Class::Inspector;

BEGIN {
  package TestPackage::A;
  sub some_method {}
}

my $schema = DBICTest->init_schema();

plan tests => 28;

# Test ensure_class_found
ok( $schema->ensure_class_found('DBIx::Class::Schema'),
    'loaded package DBIx::Class::Schema was found' );
ok( !Class::Inspector->loaded('DBICTest::FakeComponent'),
    'DBICTest::FakeComponent not loaded yet' );
ok( $schema->ensure_class_found('DBICTest::FakeComponent'),
    'package DBICTest::FakeComponent was found' );
ok( !Class::Inspector->loaded('DBICTest::FakeComponent'),
    'DBICTest::FakeComponent not loaded by ensure_class_found()' );
ok( $schema->ensure_class_found('TestPackage::A'),
    'anonymous package TestPackage::A found' );
ok( !$schema->ensure_class_found('FAKE::WONT::BE::FOUND'),
        'fake package not found' );

# Test load_optional_class
my $retval = eval { $schema->load_optional_class('ANOTHER::FAKE::PACKAGE') };
ok( !$@, 'load_optional_class on a nonexistent class did not throw' );
ok( !$retval, 'nonexistent package not loaded' );
$retval = eval { $schema->load_optional_class('DBICTest::OptionalComponent') };
ok( !$@, 'load_optional_class on an existing class did not throw' );
ok( $retval, 'DBICTest::OptionalComponent loaded' );
eval { $schema->load_optional_class('DBICTest::ErrorComponent') };
like( $@, qr/did not return a true value/,
      'DBICTest::ErrorComponent threw ok' );

# Simulate a PAR environment
{
  my @code;
  local @INC = @INC;
  unshift @INC, sub {
    if ($_[1] eq 'VIRTUAL/PAR/PACKAGE.pm') {
      return (sub { return 0 unless @code; $_ = shift @code; 1; } );
    }
    else {
      return ();
    }
  };

  $retval = eval { $schema->load_optional_class('FAKE::PAR::PACKAGE') };
  ok( !$@, 'load_optional_class on a nonexistent PAR class did not throw' );
  ok( !$retval, 'nonexistent PAR package not loaded' );


  # simulate a class which does load but does not return true
  @code = (
    q/package VIRTUAL::PAR::PACKAGE;/,
    q/0;/,
  );

  $retval = eval { $schema->load_optional_class('VIRTUAL::PAR::PACKAGE') };
  ok( $@, 'load_optional_class of a no-true-returning PAR module did throw' );
  ok( !$retval, 'no-true-returning PAR package not loaded' );

  # simulate a normal class (no one adjusted %INC so it will be tried again
  @code = (
    q/package VIRTUAL::PAR::PACKAGE;/,
    q/1;/,
  );

  $retval = eval { $schema->load_optional_class('VIRTUAL::PAR::PACKAGE') };
  ok( !$@, 'load_optional_class of a PAR module did not throw' );
  ok( $retval, 'PAR package "loaded"' );

  # see if we can still load stuff with the coderef present
  $retval = eval { $schema->load_optional_class('DBIx::Class::ResultClass::HashRefInflator') };
  ok( !$@, 'load_optional_class did not throw' ) || diag $@;
  ok( $retval, 'DBIx::Class::ResultClass::HashRefInflator loaded' );
}

# Test ensure_class_loaded
ok( Class::Inspector->loaded('TestPackage::A'), 'anonymous package exists' );
eval { $schema->ensure_class_loaded('TestPackage::A'); };
ok( !$@, 'ensure_class_loaded detected an anon. class' );
eval { $schema->ensure_class_loaded('FakePackage::B'); };
like( $@, qr/Can't locate/,
     'ensure_class_loaded threw exception for nonexistent class' );
ok( !Class::Inspector->loaded('DBICTest::FakeComponent'),
   'DBICTest::FakeComponent not loaded yet' );
eval { $schema->ensure_class_loaded('DBICTest::FakeComponent'); };
ok( !$@, 'ensure_class_loaded detected an existing but non-loaded class' );
ok( Class::Inspector->loaded('DBICTest::FakeComponent'),
   'DBICTest::FakeComponent now loaded' );

{
  # Squash warnings about syntax errors in SytaxErrorComponent.pm
  local $SIG{__WARN__} = sub {
    my $warning = shift;
    warn $warning unless (
      $warning =~ /String found where operator expected/ or
      $warning =~ /Missing operator before/
    );
  };

  eval { $schema->ensure_class_loaded('DBICTest::SyntaxErrorComponent1') };
  like( $@, qr/syntax error/,
        'ensure_class_loaded(DBICTest::SyntaxErrorComponent1) threw ok' );
  eval { $schema->load_optional_class('DBICTest::SyntaxErrorComponent2') };
  like( $@, qr/syntax error/,
        'load_optional_class(DBICTest::SyntaxErrorComponent2) threw ok' );
}


eval {
  package Fake::ResultSet;

  use base 'DBIx::Class::ResultSet';

  __PACKAGE__->load_components('+DBICTest::SyntaxErrorComponent3');
};

# Make sure the errors in components of resultset classes are reported right.
like($@, qr!\Qsyntax error at t/lib/DBICTest/SyntaxErrorComponent3.pm!, "Errors from RS components reported right");

1;
