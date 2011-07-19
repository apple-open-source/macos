use strict;
use Test::More;
$| = 1;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  if ($@) {
    plan (skip_all => 'Class::Trigger and DBIx::ContextualFetch required');
  }
  
  eval "use DBD::SQLite";
  plan skip_all => 'needs DBD::SQLite for testing' if $@;
}

INIT {
    use lib 't/cdbi/testlib';
    use Film;
}

plan skip_all => "Object cache is turned off"
    if Film->isa("DBIx::Class::CDBICompat::NoObjectIndex");

plan tests => 5;


ok +Film->create({
    Title       => 'This Is Spinal Tap',
    Director    => 'Rob Reiner',
    Rating      => 'R',
});

{
    my $film1 = Film->retrieve( "This Is Spinal Tap" );
    my $film2 = Film->retrieve( "This Is Spinal Tap" );

    $film1->Director("Marty DiBergi");
    is $film2->Director, "Marty DiBergi", 'retrieve returns the same object';

    $film1->discard_changes;
}

{
    Film->nocache(1);
    
    my $film1 = Film->retrieve( "This Is Spinal Tap" );
    my $film2 = Film->retrieve( "This Is Spinal Tap" );

    $film1->Director("Marty DiBergi");
    is $film2->Director, "Rob Reiner",
       'caching turned off';
    
    $film1->discard_changes;
}

{
    Film->nocache(0);

    my $film1 = Film->retrieve( "This Is Spinal Tap" );
    my $film2 = Film->retrieve( "This Is Spinal Tap" );

    $film1->Director("Marty DiBergi");
    is $film2->Director, "Marty DiBergi",
       'caching back on';

    $film1->discard_changes;
}


{
    Film->nocache(1);

    local $Class::DBI::Weaken_Is_Available = 0;

    my $film1 = Film->retrieve( "This Is Spinal Tap" );
    my $film2 = Film->retrieve( "This Is Spinal Tap" );

    $film1->Director("Marty DiBergi");
    is $film2->Director, "Rob Reiner",
       'CDBI::Weaken_Is_Available turns off all caching';

    $film1->discard_changes;
}
