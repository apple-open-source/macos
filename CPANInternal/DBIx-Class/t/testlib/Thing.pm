package Thing;
use base 'DBIx::Class::Test::SQLite';

Thing->set_table("thing");
Thing->columns(All => qw(id that_thing));

sub create_sql {
    return qq{
        id              INTEGER,
        that_thing      INTEGER
    };
}

1;
