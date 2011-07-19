use strict;
use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan $@ ? (skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@")
          : (tests=> 5);
}

INIT {
    use lib 't/cdbi/testlib';
    use Film;
}

{
    Film->insert({
        Title     => "Breaking the Waves",
        Director  => 'Lars von Trier',
        Rating    => 'R'
    });

    my $film = Film->construct({
        Title     => "Breaking the Waves",
        Director  => 'Lars von Trier',
    });

    isa_ok $film, "Film";
    is $film->title,    "Breaking the Waves";
    is $film->director, "Lars von Trier";
    is $film->rating,   "R",
        "constructed objects can get missing data from the db";
}

{
    package Foo;
    use base qw(Film);
    Foo->columns( TEMP => qw(temp_thing) );
    my $film = Foo->construct({
        temp_thing  => 23
    });
    
    ::is $film->temp_thing, 23, "construct sets temp columns";
}
