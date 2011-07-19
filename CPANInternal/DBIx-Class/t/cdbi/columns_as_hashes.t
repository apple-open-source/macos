use strict;
use Test::More;
use Test::Warn;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan $@ ? (skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@")
          : ('no_plan');
}

use lib 't/cdbi/testlib';
use Film;

my $waves = Film->insert({
    Title     => "Breaking the Waves",
    Director  => 'Lars von Trier',
    Rating    => 'R'
});

local $ENV{DBIC_CDBICOMPAT_HASH_WARN} = 0;

{
    local $ENV{DBIC_CDBICOMPAT_HASH_WARN} = 1;

    warnings_like {
        my $rating = $waves->{rating};
        $waves->Rating("PG");
        is $rating, "R", 'evaluation of column value is not deferred';
    } qr{^Column 'rating' of 'Film/$waves' was fetched as a hash at \Q$0};

    warnings_like {
        is $waves->{title}, $waves->Title, "columns can be accessed as hashes";
    } qr{^Column 'title' of 'Film/$waves' was fetched as a hash at\b};

    $waves->Rating("G");

    warnings_like {
        is $waves->{rating}, "G", "updating via the accessor updates the hash";
    } qr{^Column 'rating' of 'Film/$waves' was fetched as a hash at\b};


    warnings_like {
        $waves->{rating} = "PG";
    } qr{^Column 'rating' of 'Film/$waves' was stored as a hash at\b};

    $waves->update;
    my @films = Film->search( Rating => "PG", Title => "Breaking the Waves" );
    is @films, 1, "column updated as hash was saved";
}

warning_is {
    $waves->{rating}
} '', 'DBIC_CDBICOMPAT_HASH_WARN controls warnings';


{    
    $waves->rating("R");
    $waves->update;
    
    no warnings 'redefine';
    local *Film::rating = sub {
        return "wibble";
    };
    
    is $waves->{rating}, "R";
}


{
    no warnings 'redefine';
    no warnings 'once';
    local *Actor::accessor_name_for = sub {
        my($class, $col) = @_;
        return "movie" if lc $col eq "film";
        return $col;
    };
    
    require Actor;
    Actor->has_a( film => "Film" );

    my $actor = Actor->insert({
        name    => 'Emily Watson',
        film    => $waves,
    });
    
    ok !eval { $actor->film };
    is $actor->{film}->id, $waves->id,
       'hash access still works despite lack of accessor';
}


# Emulate that Class::DBI inflates immediately
SKIP: {
    skip "Need MySQL to run this test", 3 unless eval { require MyFoo };
    
    my $foo = MyFoo->insert({
        name    => 'Whatever',
        tdate   => '1949-02-01',
    });
    isa_ok $foo, 'MyFoo';
    
    isa_ok $foo->{tdate}, 'Date::Simple';
    is $foo->{tdate}->year, 1949;
}