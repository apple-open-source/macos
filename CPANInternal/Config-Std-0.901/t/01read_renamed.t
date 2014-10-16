use Config::Std { read_config => 'get_config' };
use Test::More 'no_plan';

my $input_file = 'test.cfg';

my %data = (
    # Default section...
    '' => {
        'def1'     => 'def val 1',
        'def 2'    => 'def val 2',
        'def 3 ml' => "def val 3\nacross several\n   lines",
        'def 3'    => 'def val 3',
        'def 4'    => 'def val 4',
    },

    # Named section...
    'Named' => {
        'hi there' => q{What's your name???},
        'list'     => [qw(a list of values), 'all different'],
    },

    # Complex named section...
    'Complex named!!!' => {
        123456789 => 'zero',
        '%^$%$#%' => 'curses',
    },
);

my %config;

ok eval{ get_config $input_file => %config }    => 'Read succeeded';
diag( $@ ) if $@;

is_deeply \%data, \%config                       => 'Data correct';
