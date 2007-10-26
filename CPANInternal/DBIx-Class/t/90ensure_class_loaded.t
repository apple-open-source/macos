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

plan tests => 19;

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

1;
