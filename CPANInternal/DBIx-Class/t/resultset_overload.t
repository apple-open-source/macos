use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 6;

{
  my $rs = $schema->resultset("CD")->search({});

  ok $rs->count;
  is $rs, $rs->count, "resultset as number with results";
  ok $rs,             "resultset as boolean always true";
}

{
  my $rs = $schema->resultset("CD")->search({ title => "Does not exist" });
  
  ok !$rs->count;
  is $rs, $rs->count, "resultset as number without results";
  ok $rs,             "resultset as boolean always true";
}