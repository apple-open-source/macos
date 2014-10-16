use Test::More 'no_plan';
use Config::Std;

my $orig_contents = <<EOF;
[section2]

mutating: 0
EOF


my $tmp_file = 'tmp_file';
open my $fh, '>', $tmp_file or die;
print {$fh} $orig_contents;
close $fh;


read_config $tmp_file, my %config;
$config{section2}{mutating}++;
write_config %config;

open $fh, '<', $tmp_file;
my $contents = do {local $/; <$fh>};
close $fh;

ok $contents =~ m/mutating: 1/      =>  'Mutation via hash';

read_config $tmp_file, my $config_ref;
$config_ref->{section2}{mutating}++;
write_config $config_ref;

open $fh, '<', $tmp_file;
$contents = do {local $/; <$fh>};
close $fh;

ok $contents =~ m/mutating: 2/      =>  'Mutation via hash-ref';

unlink $tmp_file;
