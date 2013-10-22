use Test::More tests => 33;
use strict;
use HTTP::Proxy::BodyFilter::lines;

my $filter;

# error checking
eval { $filter = HTTP::Proxy::BodyFilter::lines->new( undef ) };
like( $@, qr/slurp mode is not supported/, "No slurp mode" );

eval { $filter = HTTP::Proxy::BodyFilter::lines->new( \'foo' ) };
like( $@, qr/"foo" is not numeric/, "Records must be numeric" );

eval { $filter = HTTP::Proxy::BodyFilter::lines->new( \0 ) };
like( $@, qr/Records of size 0/, "Records must be != 0" );

# test the filter
for (
    HTTP::Proxy::BodyFilter::lines->new(),
    [ "\n\n\nfoo\n",   "",    "\n\n\nfoo\n",   "" ],
    [ "foo\nbar",      "",    "foo\n",         "bar" ],
    [ "foo\nbar\nbaz", "",    "foo\nbar\n",    "baz" ],
    [ "",              "",    "",              "" ],
    [ "foo\nbar\nbaz", undef, "foo\nbar\nbaz", undef ],
    HTTP::Proxy::BodyFilter::lines->new('%'),
    [ "\n\n%\nfoo\n",    "",    "\n\n%",         "\nfoo\n" ],
    [ "foo\%bar",        "",    'foo%',          "bar" ],
    [ "foo\n\%bar\nbaz", "",    "foo\n\%",       "bar\nbaz" ],
    [ "foo\nbar\%baz",   undef, "foo\nbar\%baz", undef ],
    HTTP::Proxy::BodyFilter::lines->new(""),
    [ "foo\nbar\n\nbaz", "",    "foo\nbar\n\n", "baz" ],
    [ "foo\nbar\n\n",    "",    "",             "foo\nbar\n\n" ],
    [ "foo\nbar\n\n",    undef, "foo\nbar\n\n", undef ],
    HTTP::Proxy::BodyFilter::lines->new( \10 ),
    [ '01234567890123', "",    "0123456789",     "0123" ],
    [ '0123456789' x 2, "",    "0123456789" x 2, "" ],
    [ '01234567890123', undef, "01234567890123", undef ],
  )
{
    $filter = $_, next if ref eq 'HTTP::Proxy::BodyFilter::lines';

    my ( $data, $buffer ) = @$_[ 0, 1 ];
    $filter->filter( \$data, undef, undef,
        ( defined $buffer ? \$buffer : undef ) );
    is( $data,   $_->[2], "Correct data" );
    is( $buffer, $_->[3], "Correct buffer" );
}

