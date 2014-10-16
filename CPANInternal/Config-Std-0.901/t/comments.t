use Config::Std;
use Test::More 'no_plan';

my $data = q{
# A comment
foo: bar   # Not a trailing comment. This is data for the 'foo' config var

; Another comment
baz: qux   ; Not a trailing comment. This is data for the 'qux' config var
};

# Read in the config file from Example 19-3...
read_config \$data => my %config;

write_config %config, \$results;
is $results, $data    => "Comments preserved on simple round-trip";

$config{""}{foo} = 'baz';
$config{""}{baz} = 'foo';
$data =~ s/bar.*/baz/;
$data =~ s/qux.*/foo/;

write_config %config, \$results;
is $results, $data    => "Comments preserved on mutating round-trip";
