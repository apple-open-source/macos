use strict;
use warnings;

use t::TestYAML ();
use Test::More tests => 4;
use JSON::Syck;

{
    $JSON::Syck::SingleQuote = 1;

    my $dump;

    $dump = JSON::Syck::Dump(q{Some string});
    is($dump, q{'Some string'});

    #Test escaping
    my $thing = q{I'm sorry, Dave.};
    $dump = JSON::Syck::Dump($thing);
    is(JSON::Syck::Load($dump), $thing);
}

{
    $JSON::Syck::SingleQuote = 0;

    my $dump;

    $dump = JSON::Syck::Dump(q{Some string});
    is($dump, q{"Some string"});

    #Test escaping
    my $thing = q{I'm sorry, Dave.};
    $dump = JSON::Syck::Dump($thing);
    is(JSON::Syck::Load($dump), $thing);
}
