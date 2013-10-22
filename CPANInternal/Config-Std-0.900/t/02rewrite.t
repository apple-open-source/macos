use Config::Std;
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

ok eval{ read_config $input_file => %config }    => 'Read succeeded';
diag( $@ ) if $@;

is_deeply \%data, \%config                       => 'Data correct';

$config{Extra}{'key 1'} = 'extra key 1';
push @{$config{Named}{list}}, 'an extra line';

ok eval{ write_config %config }                  => 'Write succeeded';

ok open(my $fh, '<', $input_file)                => 'File opened';

ok my @config = <$fh>                            => 'File read';

ok my @extra = <DATA>                            => 'DATA loaded';

is_deeply [@extra[-4..-1]], [@config[-4..-1]]    => 'Extra content correct';

__DATA__
[Extra]

key 1: extra key 1

