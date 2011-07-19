use strict;
use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan $@ ? (skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@")
          : (tests=> 4);
}

INIT {
    use lib 't/cdbi/testlib';
}

{
    package # hide from PAUSE 
        MyFilm;

    use base 'DBIC::Test::SQLite';
    use strict;

    __PACKAGE__->set_table('Movies');
    __PACKAGE__->columns(All => qw(id title));

    sub create_sql {
        return qq{
                id              INTEGER PRIMARY KEY AUTOINCREMENT,
                title           VARCHAR(255)
        }
    }
}

my $film = MyFilm->create({ title => "For Your Eyes Only" });
ok $film->id;

my $new_film = $film->copy;
ok $new_film->id;
isnt $new_film->id, $film->id, "copy() gets new primary key";

$new_film = $film->copy(42);
is $new_film->id, 42, "copy() with new id";

