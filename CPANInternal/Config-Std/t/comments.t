use Config::Std;
use Test::More 'no_plan';

my $data = q{
# A comment
foo: bar   # Not a trailing comment. This is data for the 'foo' config var
};

# Read in the config file from Example 19-3...
read_config \$data => my %config;

write_config %config, \$results;
is $results, $data    => "Comments preserved on simple round-trip";

$config{""}{foo} = 'baz';
$data =~ s/bar.*/baz/;

write_config %config, \$results;
is $results, $data    => "Comments preserved on mutating round-trip";
