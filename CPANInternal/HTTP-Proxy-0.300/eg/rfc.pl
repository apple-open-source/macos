use HTTP::Proxy;
use HTTP::Proxy::BodyFilter::save;

my $proxy = HTTP::Proxy->new(@ARGV);

# save RFC files as we browse them
$proxy->push_filter(
    path => qr!/rfc\d+.txt!,
    mime => 'text/plain',
    response => HTTP::Proxy::BodyFilter::save->new(
        template => '%f',
        prefix   => 'rfc',
        multiple => 0,
        keep_old => 1,
    )
);

$proxy->start;

