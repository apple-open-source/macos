use strict;
use warnings;  

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 12;

{
  my $cd_rc = $schema->resultset("CD")->result_class;
  
  throws_ok {
    $schema->resultset("Artist")
      ->search_rs({}, {result_class => "IWillExplode"})
  } qr/Can't locate IWillExplode/, 'nonexistant result_class exception';

# to make ensure_class_loaded happy, dies on inflate
  eval 'package IWillExplode; sub dummy {}';

  my $artist_rs = $schema->resultset("Artist")
    ->search_rs({}, {result_class => "IWillExplode"});
  is($artist_rs->result_class, 'IWillExplode', 'Correct artist result_class');

  throws_ok {
    $artist_rs->result_class('mtfnpy')
  } qr/Can't locate mtfnpy/,
  'nonexistant result_access exception (from accessor)';

  throws_ok {
    $artist_rs->first
  } qr/Can't locate object method "inflate_result" via package "IWillExplode"/,
  'IWillExplode explodes on inflate';

  my $cd_rs = $artist_rs->related_resultset('cds');
  is($cd_rs->result_class, $cd_rc, 'Correct cd result_class');

  my $cd_rs2 = $schema->resultset("Artist")->search_rs({})->related_resultset('cds');
  is($cd_rs->result_class, $cd_rc, 'Correct cd2 result_class');

  my $cd_rs3 = $schema->resultset("Artist")->search_rs({},{})->related_resultset('cds');
  is($cd_rs->result_class, $cd_rc, 'Correct cd3 result_class');
  
  isa_ok(eval{ $cd_rs->find(1) }, $cd_rc, 'Inflated into correct cd result_class');
}


{
  my $cd_rc = $schema->resultset("CD")->result_class;
  
  my $artist_rs = $schema->resultset("Artist")
    ->search_rs({}, {result_class => "IWillExplode"})->search({artistid => 1});
  is($artist_rs->result_class, 'IWillExplode', 'Correct artist result_class');
  
  my $cd_rs = $artist_rs->related_resultset('cds');
  is($cd_rs->result_class, $cd_rc, 'Correct cd result_class');
  
  isa_ok(eval{ $cd_rs->find(1) }, $cd_rc, 'Inflated into correct cd result_class');   
  isa_ok(eval{ $cd_rs->search({ cdid => 1 })->first }, $cd_rc, 'Inflated into correct cd result_class');
}
