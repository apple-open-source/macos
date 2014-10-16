use strict;
use Test::More tests => 11;
use HTTP::Proxy;
use HTTP::Proxy::BodyFilter;
use HTTP::Proxy::HeaderFilter;

# test the basic filter methods
my $proxy = HTTP::Proxy->new( port => 0 );

# test the errors
eval { $proxy->push_filter( 1 ); };
like( $@, qr/^Odd number of arguments/, "Bad number of parameter" );

eval { $proxy->push_filter( response => 1 ); };
like( $@, qr/^Not a Filter reference for filter queue/, "Bad parameter" );

eval { $proxy->push_filter( typo => sub { } ); };
like( $@, qr/^'typo' is not a filter stack/, "Unknown filter stack" );

eval { $proxy->push_filter( mime => 'text', response => sub { } ); };
like( $@, qr/^Invalid MIME/, "Bad MIME type" );

eval { $proxy->push_filter( method => 'FOO', response => sub { } ); };
like( $@, qr/^Invalid method: FOO/, "Invalid method: " );

eval { $proxy->push_filter( scheme => 'rstp', response => sub { } ); };
like( $@, qr/^Unsupported scheme/, "Unsupported scheme" );

# test correct working
my $filter = HTTP::Proxy::HeaderFilter->new;
eval { $proxy->push_filter( response => $filter ); };
is( $@, '', "Accept a HeaderFilter");

{
  package Foo;
  use base qw( HTTP::Proxy::HeaderFilter );
}
$filter = Foo->new;
eval { $proxy->push_filter( response => $filter ); };
is( $@, '', "Accept an object derived from HeaderFilter");

# test multiple match criteria
eval {
    $proxy->push_filter(
        response => $filter,
        mime     => 'text/*',
        scheme   => 'http',
        method   => 'GET'
    );
};
is( $@, "", "Support several match criteria" );

# test pushing multiple filters at once
# this test breaks encapsulation
$proxy = HTTP::Proxy->new( port => 0 );
$filter = HTTP::Proxy::BodyFilter->new;
my $filter2 = HTTP::Proxy::BodyFilter->new;

$proxy->push_filter( response => $filter, response => $filter2 );
is( $proxy->{body}{response}{filters}[0][1], $filter, "First filter");
is( $proxy->{body}{response}{filters}[1][1], $filter2, "Second filter");

