use Test::More tests => 14;
use strict;
use HTTP::Proxy::BodyFilter::tags;

my $filter = HTTP::Proxy::BodyFilter::tags->new();

# test the filter
for (
    [ '<b><i>foo</i></b> bar', '', '<b><i>foo</i></b> bar', '' ],
    [ '<b><i>foo</i></',       '', '<b><i>foo</i>',         '</' ],
    [ '>',                     '', '>',                     '' ],
    [ '><b>foo',               '', '><b>foo',               '' ],
    [ '><b>foo<i',             '', '><b>foo',               '<i' ],
    [ '><b>foo<i',             undef, '><b>foo<i',               undef ],
    [ '<!-- b> <b> --> > <>><', '', '<!-- b> <b> --> > <>>', '<'],
    # the following fails because of the implementation of the tags.pm
    # a stronger implementation requires parsing
    # [ 'x<a href="http://foo/>"', '', 'x', '<a href="http://foo/>"' ],
  )
{
    my ( $data, $buffer ) = @$_[ 0, 1 ];
    $filter->filter( \$data, undef, undef,
        ( defined $buffer ? \$buffer : undef ) );
    is( $data,   $_->[2], "Correct data" );
    is( $buffer, $_->[3], "Correct buffer" );
}

