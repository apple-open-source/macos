use strict;
use Test::More;
use Test::Exception;
use lib 't/cdbi/testlib';

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@"
    if $@;
  plan skip_all => "DateTime required" unless eval { require DateTime };
  plan tests => 1;
}

{
    package Thing;

    use base 'DBIC::Test::SQLite';

    Thing->columns(All  => qw[thing_id this that date]);
}

my $thing = Thing->construct({ thing_id => 23, date => "01-02-1994" });
my $date = DateTime->now;
lives_ok {
  $thing->set( date => $date );
  $thing->set( date => $date );
};



$thing->discard_changes;
