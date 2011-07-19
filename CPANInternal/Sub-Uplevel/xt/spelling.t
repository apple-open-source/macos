use Test::More;
use IO::File;

my $min_tps = 0.11;
eval "use Test::Spelling $min_tps";
plan skip_all => "Test::Spelling $min_tps required for testing POD" if $@;
system( "ispell -v" ) and plan skip_all => "No ispell";

set_spell_cmd( "ispell -l" );

my $swf = IO::File->new('xt/stopwords.txt');
my @stopwords = grep { length } map { chomp; $_ } <$swf>;
add_stopwords( @stopwords );

all_pod_files_spelling_ok();
