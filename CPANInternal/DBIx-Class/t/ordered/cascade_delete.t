use strict;
use warnings;  

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

use POSIX qw(ceil);

my $schema = DBICTest->init_schema();

plan tests => 1;

{
  my $artist = $schema->resultset ('Artist')->search ({}, { rows => 1})->single; # braindead sqlite
  my $cd = $schema->resultset ('CD')->create ({
    artist => $artist,
    title => 'Get in order',
    year => 2009,
    tracks => [
      { title => 'T1' },
      { title => 'T2' },
      { title => 'T3' },
    ],
  });

  lives_ok (sub { $cd->delete}, "Cascade delete on ordered has_many doesn't bomb");
}

1;
