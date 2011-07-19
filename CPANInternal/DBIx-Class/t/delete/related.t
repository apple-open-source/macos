use Test::More;
use strict;
use warnings;
use lib qw(t/lib);
use DBICTest;

plan tests => 4;

my $schema = DBICTest->init_schema();

my $ars = $schema->resultset('Artist');
my $cdrs = $schema->resultset('CD');
my $cd2pr_rs = $schema->resultset('CD_to_Producer');

# create some custom entries
$ars->populate ([
  [qw/artistid  name/],
  [qw/71        a1/],
  [qw/72        a2/],
  [qw/73        a3/],
]);

$cdrs->populate ([
  [qw/cdid artist title   year/],
  [qw/70   71     delete0 2005/],
  [qw/71   72     delete1 2005/],
  [qw/72   72     delete2 2005/],
  [qw/73   72     delete3 2006/],
  [qw/74   72     delete4 2007/],
  [qw/75   73     delete5 2008/],
]);

my $prod = $schema->resultset('Producer')->create ({ name => 'deleter' });
my $prod_cd = $cdrs->find (70);
my $cd2pr = $cd2pr_rs->create ({
  producer => $prod,
  cd => $prod_cd,
});

my $total_cds = $cdrs->count;

# test that delete_related w/o conditions deletes all related records only
$ars->search ({name => 'a3' })->search_related ('cds')->delete;
is ($cdrs->count, $total_cds -= 1, 'related delete ok');

my $a2_cds = $ars->search ({ name => 'a2' })->search_related ('cds');

# test that related deletion w/conditions deletes just the matched related records only
$a2_cds->search ({ year => 2005 })->delete;
is ($cdrs->count, $total_cds -= 2, 'related + condition delete ok');

# test that related deletion with limit condition works
$a2_cds->search ({}, { rows => 1})->delete;
is ($cdrs->count, $total_cds -= 1, 'related + limit delete ok');

TODO: {
  local $TODO = 'delete_related is based on search_related which is based on search which does not understand object arguments';
  my $cd2pr_count = $cd2pr_rs->count;
  $prod_cd->delete_related('cd_to_producer', { producer => $prod } );
  is ($cd2pr_rs->count, $cd2pr_count -= 1, 'm2m link deleted succesfully');
}
