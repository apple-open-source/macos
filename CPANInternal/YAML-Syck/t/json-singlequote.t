use strict;
use warnings;

use t::TestYAML ();
use Test::More tests => 5;
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

{
    my $url = 'http://www.pugscode.org';
    my $var = JSON::Syck::Load(<<"_EOC_");
    { url: '$url' }
_EOC_

    is $var->{url}, $url, "no extra space in the URL";
}
