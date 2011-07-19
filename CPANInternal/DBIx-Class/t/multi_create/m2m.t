use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

plan tests => 4;

my $schema = DBICTest->init_schema();

lives_ok ( sub {

  my $prod_rs = $schema->resultset ('Producer');
  my $prod_count = $prod_rs->count;

  my $cd = $schema->resultset('CD')->first;
  $cd->add_to_producers ({name => 'new m2m producer'});

  is ($prod_rs->count, $prod_count + 1, 'New producer created');
  ok ($cd->producers->find ({name => 'new m2m producer'}), 'Producer created with correct name');

  my $cd2 = $schema->resultset('CD')->search ( { cdid => { '!=', $cd->cdid } }, {rows => 1} )->single;  # retrieve a cd different from the first
  $cd2->add_to_producers ({name => 'new m2m producer'});                                                # attach to an existing producer
  ok ($cd2->producers->find ({name => 'new m2m producer'}), 'Existing producer attached to existing cd');

}, 'Test far-end find_or_create over many_to_many');

1;
