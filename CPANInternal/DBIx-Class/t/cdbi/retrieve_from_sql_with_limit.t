use strict;
use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan $@ ? (skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@")
          : (tests=> 3);
}

INIT {
    use lib 't/cdbi/testlib';
    use Film;
}

for my $title ("Bad Taste", "Braindead", "Forgotten Silver") {
    Film->insert({ Title => $title, Director => 'Peter Jackson' });
}

Film->insert({ Title => "Transformers", Director => "Michael Bay"});

{
    my @films = Film->retrieve_from_sql(qq[director = "Peter Jackson" LIMIT 2]);
    is @films, 2, "retrieve_from_sql with LIMIT";
    is( $_->director, "Peter Jackson" ) for @films;
}
