use Config::Std {def_sep => '='};
use Test::More 'no_plan';

my $input_file = 'test.cfg';

my %config;

ok eval{ read_config $input_file => %config }    => 'Read succeeded';
diag( $@ ) if $@;

$config{'Extra Eq'}{'key 1'} = 'extra key 1';
push @{$config{Named}{list}}, 'an extra line';

ok eval{ write_config %config }                  => 'Write succeeded';

ok open(my $fh, '<', $input_file)                => 'File opened';

ok my @config = <$fh>                            => 'File read';

ok my @extra = <DATA>                            => 'DATA loaded';

is_deeply [@config[-4..-1]], [@extra[-4..-1]]    => 'Extra content correct';

__DATA__

[Extra Eq]

key 1 = extra key 1

