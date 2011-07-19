use strict;
use Test::More;

#----------------------------------------------------------------------
# Make sure subclasses can be themselves subclassed
#----------------------------------------------------------------------

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  if ($@) {
    plan (skip_all => 'Class::Trigger and DBIx::ContextualFetch required');
    next;
  }
  eval "use DBD::SQLite";
  plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 6);
}

use lib 't/cdbi/testlib';
use Film;

INIT { @Film::Threat::ISA = qw/Film/; }

ok(Film::Threat->db_Main->ping, 'subclass db_Main()');
is_deeply [ sort Film::Threat->columns ], [ sort Film->columns ],
  'has the same columns';

my $bt = Film->create_test_film;
ok my $btaste = Film::Threat->retrieve('Bad Taste'), "subclass retrieve";
isa_ok $btaste => "Film::Threat";
isa_ok $btaste => "Film";
is $btaste->Title, 'Bad Taste', 'subclass get()';
