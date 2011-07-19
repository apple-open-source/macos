#!perl -T

# the above line forces Test::Harness into taint-mode

use strict;
use warnings;

use Test::More;
BEGIN { plan tests => 7 }

package DBICTest::Taint::Classes;

use Test::More;
use Test::Exception;

use lib qw(t/lib);
use base qw/DBIx::Class::Schema/;

lives_ok (sub {
  __PACKAGE__->load_classes(qw/Manual/);
  ok( __PACKAGE__->source('Manual'), 'The Classes::Manual source has been registered' );
  __PACKAGE__->_unregister_source (qw/Manual/);
}, 'Loading classes with explicit load_classes worked in taint mode' );

lives_ok (sub {
  __PACKAGE__->load_classes();
  ok( __PACKAGE__->source('Auto'), 'The Classes::Auto source has been registered' );
  ok( __PACKAGE__->source('Auto'), 'The Classes::Manual source has been re-registered' );
}, 'Loading classes with Module::Find/load_classes worked in taint mode' );


package DBICTest::Taint::Namespaces;

use Test::More;
use Test::Exception;

use lib qw(t/lib);
use base qw/DBIx::Class::Schema/;

lives_ok (sub {
  __PACKAGE__->load_namespaces();
  ok( __PACKAGE__->source('Test'), 'The Namespaces::Test source has been registered' );
}, 'Loading classes with Module::Find/load_namespaces worked in taint mode' );

1;
