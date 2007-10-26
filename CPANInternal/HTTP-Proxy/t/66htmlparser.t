use strict;
use Test::More tests => 5;
use HTTP::Proxy;
use HTTP::Proxy::BodyFilter::htmlparser;
use HTML::Parser;

my @results = (
    [
        '<h1>Test</h1>\n<p align="left">foo<br> <i>bar</i></p>',
        '<h1>Test</h1>\n<p align="left">foo<br> <i>bar</i></p>',
        { start => 4, end => 3 }
    ],
    [
        '<h1>Test</h1>\n<p align="left">foo<br> <i>bar</i></p>',
        '<h1></h1><p align="left"><br><i></i></p>',
        { start => 4, end => 3 }
    ],
);

my $filter;
my $count;

# bad initialisation
eval { $filter = HTTP::Proxy::BodyFilter::htmlparser->new("foo"); };
like( $@, qr/^First parameter must be a HTML::Parser/, "Test constructor" );

my $p = HTML::Parser->new;
$p->handler( start          => \&start,          "self,text" );
$p->handler( end            => \&end,            "self,text" );
$p->handler( start_document => \&start_document, "" );

# the handlers
sub start_document { $count = {} }
sub start { $count->{start}++; $_[0]->{output} .= $_[1] }
sub end   { $count->{end}++;   $_[0]->{output} .= $_[1] }

# read-only filter
my $data = shift @results;
$filter = HTTP::Proxy::BodyFilter::htmlparser->new($p);
$filter->filter( \$data->[0], undef, undef, undef );
is_deeply( $data->[0], $data->[1], "Data not modified" );
is_deeply( $data->[2], $count, "Correct number of start and end events" );

# read-write filter (yeah, it's the same)
$data = shift @results;
$filter = HTTP::Proxy::BodyFilter::htmlparser->new( $p, rw => 1 );
$filter->filter( \$data->[0], undef, undef, undef );
is_deeply( $data->[0], $data->[1], "Data modified" );
is_deeply( $data->[2], $count, "Correct number of start and end events" );

