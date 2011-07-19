use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

my $art = $schema->resultset("Artist")->find(1);

isa_ok $art => 'DBICTest::Artist';

my $name = 'Caterwauler McCrae';

ok($art->name($name) eq $name, 'update');

{
  my @changed_keys = $art->is_changed;
  is( scalar (@changed_keys), 0, 'field changed but same value' );
}

$art->discard_changes;

ok($art->update({ artistid => 100 }), 'update allows pk mutation');

is($art->artistid, 100, 'pk mutation applied');

my $art_100 = $schema->resultset("Artist")->find(100);
$art_100->artistid(101);
ok($art_100->update(), 'update allows pk mutation via column accessor');

done_testing;
