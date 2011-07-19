use t::TestYAML;
use Test::More;
use FindBin '$RealBin';

chdir $RealBin;

unless (-w $RealBin) {
    plan skip_all => "Can't write to $RealBin";
    exit;
}

plan tests => 6;

*::DumpFile = *YAML::Syck::DumpFile;

sub file_contents_is {
    my ($fn, $expected, $test_name) = @_;
    local *FH;
    open FH, $fn or die $!;
    my $contents = do { local $/; <FH> };
    is($contents, $expected, $test_name);
    close FH;
}

my $scalar = 'a simple scalar';
my $expected_yaml = <<YAML;
--- a simple scalar
YAML

# using file name
{
    DumpFile('dumpfile.yml', $scalar);
    file_contents_is('dumpfile.yml', $expected_yaml, 'DumpFile works with filenames');
    unlink 'dumpfile.yml' or die $!;
}

# dump to IO::File
{
    require IO::File;
    my $h = IO::File->new('>dumpfile.yml');
    DumpFile($h, $scalar);
    close $h;
    file_contents_is('dumpfile.yml', $expected_yaml, 'DumpFile works with IO::File');
    unlink 'dumpfile.yml' or die $!;
}

# dump to indirect file handles
SKIP: {
    skip "indirect file handles require 5.6 or later", 1 unless $] >= 5.006000; eval q[

    open(my $h, '>', 'dumpfile.yml');
    DumpFile($h, $scalar);
    close $h;
    file_contents_is('dumpfile.yml', $expected_yaml, 'DumpFile works with indirect file handles');
    unlink 'dumpfile.yml' or die $!;

] }

# dump to ordinary filehandles
{
    local *H;
    open(H, '>dumpfile.yml');
    DumpFile(*H, $scalar);
    close(H);
    file_contents_is('dumpfile.yml', $expected_yaml, 'DumpFile works with ordinary file handles');
    unlink 'dumpfile.yml' or die $!;
}

# dump to ordinary filehandles (refs)
{
    local *H;
    open(H, '>dumpfile.yml');
    DumpFile(\*H, $scalar);
    close(H);
    file_contents_is('dumpfile.yml', $expected_yaml, 'DumpFile works with glob refs');
    unlink 'dumpfile.yml' or die $!;
}

# dump to "in memory" file
SKIP : {
    skip "in-memory files require 5.8 or later", 1 unless $] >= 5.00800; eval q[

    open(my $h, '>', \my $s);
    DumpFile($h, $scalar);
    close($h);
    is($s, $expected_yaml, 'DumpFile works with in-memory files');

] }
