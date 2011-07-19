use strict;
use Test::More;
use HTTP::Proxy;
use HTTP::Proxy::BodyFilter::tags;
use HTTP::Proxy::BodyFilter::simple;
use HTTP::Proxy::BodyFilter::complete;
use HTTP::Proxy::BodyFilter::htmltext;
use HTTP::Proxy::BodyFilter::lines;
use HTTP::Proxy::BodyFilter::save;
use HTTP::Request;

my @idem_filters = qw( complete lines save tags );

plan tests => 2 + @idem_filters;

my $proxy = HTTP::Proxy->new( port => 0 );

my $req = HTTP::Request->new( GET => 'http://www.vronk.com/' );
my $res = HTTP::Response->new( 200 );
$res->request( $req );
$res->content_type( 'text/html' );
$proxy->request( $req );
$proxy->response( $res );

# basic values
for my $filter (@idem_filters) {
    $req->uri("http://www.$filter.com/");
    $proxy->push_filter(
        response => "HTTP::Proxy::BodyFilter::$filter"->new );

    $proxy->{body}{response}->select_filters($res);
    is( $proxy->{body}{response}->will_modify($res),
        0, qq{Filter $filter won't change a thing} );
}


# change the request info
$req->uri( 'http://www.zlonk.com/' );

# filters that don't modify anything
$proxy->push_filter(
    host     => 'zlonk.com',
    response => HTTP::Proxy::BodyFilter::tags->new(),
    response => HTTP::Proxy::BodyFilter::complete->new(),
);

$proxy->{body}{response}->select_filters( $res );
ok( !$proxy->{body}{response}->will_modify(),
    q{Filters won't change a thing}
);

# simulate end of connection
$proxy->{body}{response}->eod();

# add a filter that will change stuff
$proxy->push_filter(
    host     => 'zlonk.com',
    response => HTTP::Proxy::BodyFilter::simple->new( sub {} ),
);

$proxy->{body}{response}->select_filters( $res );
ok( $proxy->{body}{response}->will_modify( $res ),
    q{Filters admit they will change something}
);

unlink( 'www.zlonk.com' ); # cleanup file created by HPBF::save
