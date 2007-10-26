use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

my $queries;
#$schema->storage->debugfh(IO::File->new('t/var/temp.trace', 'w'));
$schema->storage->debugcb( sub{ $queries++ } );

eval "use DBD::SQLite";
plan skip_all => 'needs DBD::SQLite for testing' if $@;
plan tests => 2;


my $cd = $schema->resultset("CD")->find(1);
$cd->title('test');

# SELECT count
$queries = 0;
$schema->storage->debug(1);

$cd->update;

is($queries, 1, 'liner_notes (might_have) not prefetched - do not load 
liner_notes on update');

$schema->storage->debug(0);


my $cd2 = $schema->resultset("CD")->find(2, {prefetch => 'liner_notes'});
$cd2->title('test2');

# SELECT count
$queries = 0;
$schema->storage->debug(1);

$cd2->update;

is($queries, 1, 'liner_notes (might_have) prefetched - do not load 
liner_notes on update');

$schema->storage->debug(0);

