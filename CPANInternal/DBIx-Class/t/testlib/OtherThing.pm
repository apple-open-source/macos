package OtherThing;
use base 'DBIx::Class::Test::SQLite';

OtherThing->set_table("other_thing");
OtherThing->columns(All => qw(id));

sub create_sql {
    return qq{
        id              INTEGER
    };
}
