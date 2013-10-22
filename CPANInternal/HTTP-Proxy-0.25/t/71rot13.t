use Test::More tests => 4;
use HTTP::Proxy;
use HTTP::Proxy::BodyFilter::tags;
use HTTP::Proxy::BodyFilter::htmltext;
use t::Utils;
use strict;

# a very simple proxy
my $proxy = HTTP::Proxy->new( port => 0 );

$proxy->push_filter(
    mime     => 'text/html',
    response => HTTP::Proxy::BodyFilter::tags->new,
    response => HTTP::Proxy::BodyFilter::htmltext->new(
        sub { tr/a-zA-z/n-za-mN-ZA-M/ }
    )
);

# get and test the filter stack
my $stack = $proxy->_filter_stack(
    body => 'response',
    HTTP::Request->new( GET => 'http://foo.com/bar.html' ),
    HTTP::Response->new(
        200, "OK", HTTP::Headers->new( 'Content-Type' => 'text/html' )
    )
);

for (
    [ "<b>abc</b>",                     "<b>nop</b>" ],
    [ "<b>100</b> &euro; is expensive", "<b>100</b> &euro; vf rkcrafvir" ],
    [ "</b> <-- here </b>",             "</b> <-- urer </b>" ],
    [
qq'<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//FR" "http://www.w3.org/TR/html4/loose.dtd"\n<style><!--\nbody,td,a,p{font-family:arial;}\n//-->\n</style> foo',
qq'<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//FR" "http://www.w3.org/TR/html4/loose.dtd"\n<style><!--\nbody,td,a,p{font-family:arial;}\n//-->\n</style> sbb',
    ],
  )
{
    my $data = "$_->[0]";
    $stack->select_filters( $proxy->{response} );
    $stack->filter( \$data, $proxy->{response}, undef );
    is( $data, $_->[1], "Correct data transformation" );
}

