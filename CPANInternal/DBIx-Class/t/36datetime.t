use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

eval { require DateTime::Format::MySQL };

plan $@ ? ( skip_all => 'Requires DateTime::Format::MySQL' )
        : ( tests => 3 );

my $schema = DBICTest->init_schema(
    no_deploy => 1, # Deploying would cause an early rebless
);

is(
    ref $schema->storage, 'DBIx::Class::Storage::DBI',
    'Starting with generic storage'
);

# Calling date_time_parser should cause the storage to be reblessed,
# so that we can pick up datetime_parser_type from subclasses

my $parser = $schema->storage->datetime_parser();

# We're currently expecting a MySQL parser. May change in future.
is($parser, 'DateTime::Format::MySQL', 'Got expected datetime_parser');

isa_ok($schema->storage, 'DBIx::Class::Storage::DBI::SQLite', 'storage');

