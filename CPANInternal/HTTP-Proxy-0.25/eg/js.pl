use HTTP::Proxy;
use HTTP::Proxy::BodyFilter::save;

my $proxy = HTTP::Proxy->new(@ARGV);

# save javascript files as we browse them
$proxy->push_filter(
    path => qr!/.js$!,
    response => HTTP::Proxy::BodyFilter::save->new(
        template => '%f',
        prefix   => 'javascript',
        multiple => 0,
        keep_old => 1,
    )
);

$proxy->start;

