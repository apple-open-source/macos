use Test::More tests => 64;
use Lingua::EN::Inflect qw( :ALL );

# No commas...
is NO('cat', 12        ), '12 cats'        => 'No commas: 12';
is NO('cat', 123       ), '123 cats'       => 'No commas: 123';
is NO('cat', 1234      ), '1234 cats'      => 'No commas: 1234';
is NO('cat', 12345     ), '12345 cats'     => 'No commas: 12345';
is NO('cat', 123456    ), '123456 cats'    => 'No commas: 123456';
is NO('cat', 1234567   ), '1234567 cats'   => 'No commas: 1234567';
is NO('cat', 12345678  ), '12345678 cats'  => 'No commas: 12345678';
is NO('cat', 123456789 ), '123456789 cats' => 'No commas: 123456789';

# Std commas...
for my $comma ( qw< , . ' _ > ) {
    is NO('cat', 12       , { comma => $comma }), "12 cats"                        => "$comma as comma: 12";
    is NO('cat', 123      , { comma => $comma }), "123 cats"                       => "$comma as comma: 123";
    is NO('cat', 1234     , { comma => $comma }), "1${comma}234 cats"              => "$comma as comma: 1234";
    is NO('cat', 12345    , { comma => $comma }), "12${comma}345 cats"             => "$comma as comma: 12345";
    is NO('cat', 123456   , { comma => $comma }), "123${comma}456 cats"            => "$comma as comma: 123456";
    is NO('cat', 1234567  , { comma => $comma }), "1${comma}234${comma}567 cats"   => "$comma as comma: 1234567";
    is NO('cat', 12345678 , { comma => $comma }), "12${comma}345${comma}678 cats"  => "$comma as comma: 12345678";
    is NO('cat', 123456789, { comma => $comma }), "123${comma}456${comma}789 cats" => "$comma as comma: 123456789";
}

# Comma flag...
    is NO('cat', 12       , { comma => 1 }), "12 cats"          => "$comma as comma: 12";
    is NO('cat', 123      , { comma => 1 }), "123 cats"         => "$comma as comma: 123";
    is NO('cat', 1234     , { comma => 1 }), "1,234 cats"       => "$comma as comma: 1234";
    is NO('cat', 12345    , { comma => 1 }), "12,345 cats"      => "$comma as comma: 12345";
    is NO('cat', 123456   , { comma => 1 }), "123,456 cats"     => "$comma as comma: 123456";
    is NO('cat', 1234567  , { comma => 1 }), "1,234,567 cats"   => "$comma as comma: 1234567";
    is NO('cat', 12345678 , { comma => 1 }), "12,345,678 cats"  => "$comma as comma: 12345678";
    is NO('cat', 123456789, { comma => 1 }), "123,456,789 cats" => "$comma as comma: 123456789";

# Comma every 2...
is NO('cat', 12       , {comma_every=>2}), '12 cats'            => 'Comma every 2: 12';
is NO('cat', 123      , {comma_every=>2}), '1,23 cats'          => 'Comma every 2: 123';
is NO('cat', 1234     , {comma_every=>2}), '12,34 cats'         => 'Comma every 2: 1234';
is NO('cat', 12345    , {comma_every=>2}), '1,23,45 cats'       => 'Comma every 2: 12345';
is NO('cat', 123456   , {comma_every=>2}), '12,34,56 cats'      => 'Comma every 2: 123456';
is NO('cat', 1234567  , {comma_every=>2}), '1,23,45,67 cats'    => 'Comma every 2: 1234567';
is NO('cat', 12345678 , {comma_every=>2}), '12,34,56,78 cats'   => 'Comma every 2: 12345678';
is NO('cat', 123456789, {comma_every=>2}), '1,23,45,67,89 cats' => 'Comma every 2: 123456789';

# Eurocomma every 4...
is NO('cat', 12       , {comma_every=>4, comma=>'.'}), '12 cats'          => '. as comma every 4: 12';
is NO('cat', 123      , {comma_every=>4, comma=>'.'}), '123 cats'         => '. as comma every 4: 123';
is NO('cat', 1234     , {comma_every=>4, comma=>'.'}), '1234 cats'        => '. as comma every 4: 1234';
is NO('cat', 12345    , {comma_every=>4, comma=>'.'}), '1.2345 cats'      => '. as comma every 4: 12345';
is NO('cat', 123456   , {comma_every=>4, comma=>'.'}), '12.3456 cats'     => '. as comma every 4: 123456';
is NO('cat', 1234567  , {comma_every=>4, comma=>'.'}), '123.4567 cats'    => '. as comma every 4: 1234567';
is NO('cat', 12345678 , {comma_every=>4, comma=>'.'}), '1234.5678 cats'   => '. as comma every 4: 12345678';
is NO('cat', 123456789, {comma_every=>4, comma=>'.'}), '1.2345.6789 cats' => '. as comma every 4: 123456789';

