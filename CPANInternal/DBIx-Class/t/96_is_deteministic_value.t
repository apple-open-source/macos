use strict;
use warnings;

# 6 tests

use Test::More;
use lib qw(t/lib);
use DBICTest;
plan skip_all => "DateTime required" unless eval { require DateTime };
eval "use DateTime::Format::Strptime";
plan skip_all => 'DateTime::Format::Strptime required' if $@;
plan 'no_plan';
use Test::Exception;

my $schema = DBICTest->init_schema();
my $artist_rs = $schema->resultset('Artist');
my $cd_rs = $schema->resultset('CD');

 {
   my $cd;
   lives_ok {
     $cd = $cd_rs->search({ year => {'=' => 1999}})->create
       ({
         artist => {name => 'Guillermo1'},
         title => 'Guillermo 1',
        });
   };
   is($cd->year, 1999);
 }

 {
   my $formatter = DateTime::Format::Strptime->new(pattern => '%Y');
   my $dt = DateTime->new(year => 2006, month => 06, day => 06,
                          formatter => $formatter );
   my $cd;
   lives_ok {
     $cd = $cd_rs->search({ year => $dt})->create
       ({
         artist => {name => 'Guillermo2'},
         title => 'Guillermo 2',
        });
   };
   is($cd->year, 2006);
 }


{
  my $artist;
  lives_ok {
    $artist = $artist_rs->search({ name => {'!=' => 'Killer'}})
      ->create({artistid => undef});
  };
  is($artist->name, undef);
}


{
  my $artist;
  lives_ok {
    $artist = $artist_rs->search({ name => [ q/ some stupid names here/]})
      ->create({artistid => undef});
  };
  is($artist->name, undef);
}


1;
