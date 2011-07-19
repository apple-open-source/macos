use strict;
use warnings;  

use Test::More;
use Test::Warn;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

my $queries;
$schema->storage->debugcb( sub{ $queries++ } );
my $sdebug = $schema->storage->debug;

my $cd = $schema->resultset("CD")->find(1);
$cd->title('test');

# SELECT count
$queries = 0;
$schema->storage->debug(1);

$cd->update;

is($queries, 1, 'liner_notes (might_have) not prefetched - do not load 
liner_notes on update');

$schema->storage->debug($sdebug);


my $cd2 = $schema->resultset("CD")->find(2, {prefetch => 'liner_notes'});
$cd2->title('test2');

# SELECT count
$queries = 0;
$schema->storage->debug(1);

$cd2->update;

is($queries, 1, 'liner_notes (might_have) prefetched - do not load 
liner_notes on update');

warning_like {
  DBICTest::Schema::Bookmark->might_have(
    linky => 'DBICTest::Schema::Link',
    { "foreign.id" => "self.link" },
  );
}
  qr{"might_have/has_one" must not be on columns with is_nullable set to true},
  'might_have should warn if the self.id column is nullable';

{
  local $ENV{DBIC_DONT_VALIDATE_RELS} = 1;
  warning_is { 
    DBICTest::Schema::Bookmark->might_have(
      slinky => 'DBICTest::Schema::Link',
      { "foreign.id" => "self.link" },
    );
  }
  undef,
  'Setting DBIC_DONT_VALIDATE_RELS suppresses nullable relation warnings';
}

$schema->storage->debug($sdebug);
done_testing();
