use strict;
use Test;
use Carp;
BEGIN {plan tests => 1}
use Perl::Tidy; 

#----------------------------------------------------------------------
## test file->file through arg list
#   Also tests:
#     passing parameters names through 'argv=>'
#     -o flag
#
#     NOTE: This will read file 'lextest' and create file 'lextest.out'
#     (for portability, 8.3 filenames are best)
#     NOTE: must use -npro flag to avoid using local perltidyrc flags
#----------------------------------------------------------------------

my $input_file = "lextest";
my $output_file = "lextest.out";

Perl::Tidy::perltidy(
    source      => undef,
    destination => undef,
    perltidyrc  => undef,
    argv        => "-nsyn -npro $input_file -o $output_file",
);

open FILE, "< $output_file";
my @output=<FILE>;
my @expected_output=<DATA>;
my $ok=1;
if (@expected_output == @output) {
        while ( $_ = pop @output ) {
            my $expect = pop @expected_output;
            if ( $expect ne $_ ) {
                print STDERR "got:$_";
                print STDERR "---\n";
                print STDERR "expected_output:$expect";
                $ok=0;
                last;
            }
        }
}
else {
        print STDERR "Line Counts differ\n";
        $ok=0;
}
ok ($ok,1);

# This is the expected result of 'perltidy lextest':

__DATA__
# This is a simple testfile to demonstrate perltidy, from perlop(1).
# One way (of several) to run perltidy is as follows:
#
#   perl ./perltidy lextest
#
# The output will be "lextest.tdy"
$_ = <<'EOL';
   $url = new URI::URL "http://www/";   die if $url eq "xXx";
EOL
LOOP: {
    print(" digits"),       redo LOOP if /\G\d+\b[,.;]?\s*/gc;
    print(" lowercase"),    redo LOOP if /\G[a-z]+\b[,.;]?\s*/gc;
    print(" UPPERCASE"),    redo LOOP if /\G[A-Z]+\b[,.;]?\s*/gc;
    print(" Capitalized"),  redo LOOP if /\G[A-Z][a-z]+\b[,.;]?\s*/gc;
    print(" MiXeD"),        redo LOOP if /\G[A-Za-z]+\b[,.;]?\s*/gc;
    print(" alphanumeric"), redo LOOP if /\G[A-Za-z0-9]+\b[,.;]?\s*/gc;
    print(" line-noise"),   redo LOOP if /\G[^A-Za-z0-9]+/gc;
    print ". That's all!\n";
}
