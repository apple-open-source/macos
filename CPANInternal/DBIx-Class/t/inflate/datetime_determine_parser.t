use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

eval { require DateTime::Format::SQLite };
plan $@ ? ( skip_all => 'Requires DateTime::Format::SQLite' )
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

is($parser, 'DateTime::Format::SQLite', 'Got expected storage-set datetime_parser');
isa_ok($schema->storage, 'DBIx::Class::Storage::DBI::SQLite', 'storage');

