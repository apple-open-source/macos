use Test::More tests => 2;
use HTTP::Proxy;
use HTTP::Proxy::HeaderFilter;
use HTTP::Proxy::BodyFilter;

my $proxy = HTTP::Proxy->new( port => 0 );

my $filter = HTTP::Proxy::HeaderFilter->new;
$proxy->push_filter( request => $filter );
is( $filter->proxy, $proxy, "The HeaderFilter knows its proxy" );

$filter = HTTP::Proxy::BodyFilter->new;
$proxy->push_filter( response => $filter );
is( $filter->proxy, $proxy, "The BodyFilter knows its proxy" );

