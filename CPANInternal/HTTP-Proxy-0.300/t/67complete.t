use strict;
use warnings;
use Test::More;
use HTTP::Proxy;
use HTTP::Proxy::BodyFilter::complete;
use HTTP::Proxy::BodyFilter::simple;

my @data = (
    'miny hollers the let tiger catch meeny a he him',
    'joy beamish flame gyre o blade came callay jaws vorpal',
    'xvi vigor nvi Bvi trived Elvis levee viper e3 PVIC',
    'Wizzle Hunny_Bee Alexander_Beetle Owl Woozle Eeyore Backson',
    'necessitatibus lorem aperiam facere consequuntur incididunt similique'
);
my $full = join '', @data;

plan tests => 1 + @data;

# some variables
my $proxy = HTTP::Proxy->new( port => 0 );
$proxy->push_filter(
    response => HTTP::Proxy::BodyFilter::complete->new(),
    response => HTTP::Proxy::BodyFilter::simple->new(
        sub {
            my ( $self, $dataref, $message, $protocol, $buffer ) = @_;
            if ( defined $buffer ) {
                is( $$dataref, '', 'Empty chunk of data' );
            }
            else {
                is( $$dataref, $full, 'Full data in one big chunk' );
            }
        }
    ),
);

# set up a fake request/response set
my $res =
  HTTP::Response->new( 200, 'OK',
    HTTP::Headers->new( 'Content-Type' => 'text/html' ), 'dummy' );
$res->request( HTTP::Request->new( GET => 'http://www.example.com/' ) );
$proxy->request( $res->request );
$proxy->response($res);

# run the data through the filters
$proxy->{body}{response}->select_filters($res);

for my $data (@data) {
    $proxy->{body}{response}->filter( \$data, $res, '' );
}

# finalize
my $data = '';
$proxy->{body}{response}->filter_last( \$data, $res, '' );

