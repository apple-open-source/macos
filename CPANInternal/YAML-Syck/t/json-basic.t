use strict;
use t::TestYAML ();
use Data::Dumper;
use Test::More;
use JSON::Syck;

BEGIN {
unless (defined &utf8::encode) {
    plan skip_all => 'No Unicode support';
    exit;
}
}

use Storable;

our $HAS_JSON = 0;
eval { require JSON; $HAS_JSON = 1 };

$Data::Dumper::Indent = 0;
$Data::Dumper::Terse  = 1;

my @tests = (
    '"foo"',
    '[1, 2, 3]',
    '[1, 2, 3]',
    '2',
    '2.1',
    '"foo\'bar"',
    '[1,2,3]',
    '[1.1, 2.2, 3.3]',
    '[1.1,2.2,3.3]',
    '{"foo": "bar"}',
    '{"foo":"bar"}',
    '[{"foo": 2}, {"foo": "bar"}]',
    qq("\xe5\xaa\xbe"),
    'null',
    '{"foo":null}',
    '""',
    '[null,null]',
    '["",null]',
    '{"foo":""}',
    '["\"://\""]',
    '"~foo"',
);

plan tests => scalar @tests * (2 + $HAS_JSON) * 2;

for my $single_quote (0, 1) {
for my $unicode (0, 1) {
    local $JSON::Syck::SingleQuote = $single_quote;
    local $JSON::Syck::ImplicitUnicode = $unicode;

    for my $test_orig (@tests) {
        my $test = $test_orig;
        if ($single_quote) {
            $test =~ s/'/\\'/g;
            $test =~ s/"/'/g;
        }

        my $data = eval { JSON::Syck::Load($test) };
        my $json = JSON::Syck::Dump($data);
        utf8::encode($json) if !ref($json) && $unicode;

        # don't bother white spaces
        for ($test, $json) {
            s/([,:]) /$1/eg;
        }

        my $desc = "roundtrip $test -> " . Dumper($data) . " -> $json";
        utf8::encode($desc);
        is $json, $test, $desc;

        # try parsing the data with JSON.pm
        if ($HAS_JSON and !$single_quote) {
            $SIG{__WARN__} = sub { 1 };
            utf8::encode($data) if defined($data) && !ref($data) && $unicode;
            my $data_pp = eval { JSON::jsonToObj($json) };
            is_deeply $data_pp, $data, "compatibility with JSON.pm $test";
        }
    }
}
}


