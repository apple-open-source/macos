use strict;
use warnings;

use Test::Exception tests => 1;
use lib qw(t/lib);
use DBICTest;
use DBICTest::Schema;
use DBIx::Class::ResultSource::Table;

my $schema = DBICTest->init_schema();

my $foo = DBIx::Class::ResultSource::Table->new({ name => "foo" });
my $bar = DBIx::Class::ResultSource::Table->new({ name => "bar" });

lives_ok {
    $schema->register_source(foo => $foo);
    $schema->register_source(bar => $bar);
} 'multiple classless sources can be registered';
