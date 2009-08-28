use strict;
use Test;
use Carp;
BEGIN {plan tests => 1}
use Perl::Tidy; 


#----------------------------------------------------------------------
## test string->string 
#----------------------------------------------------------------------
my $source = <<'EOM';
%height=("letter",27.9, "legal",35.6, "arche",121.9, "archd",91.4, "archc",61,
 "archb",45.7, "archa",30.5, "flsa",33, "flse",33, "halfletter",21.6,
 "11x17",43.2, "ledger",27.9);
%width=("letter",21.6, "legal",21.6, "arche",91.4, "archd",61, "archc",45.7,
 "archb",30.5, "archa",22.9, "flsa",21.6, "flse",21.6, "halfletter",14,
 "11x17",27.9, "ledger",43.2);
EOM

my $perltidyrc = <<'EOM';
-gnu
EOM

my $output;

Perl::Tidy::perltidy(
    source      => \$source,
    destination => \$output,
    perltidyrc  => \$perltidyrc,
    argv        => '-nsyn',
);

my $expected_output=<<'EOM';
%height = (
           "letter",     27.9, "legal", 35.6, "arche",  121.9,
           "archd",      91.4, "archc", 61,   "archb",  45.7,
           "archa",      30.5, "flsa",  33,   "flse",   33,
           "halfletter", 21.6, "11x17", 43.2, "ledger", 27.9
          );
%width = (
          "letter",     21.6, "legal", 21.6, "arche",  91.4,
          "archd",      61,   "archc", 45.7, "archb",  30.5,
          "archa",      22.9, "flsa",  21.6, "flse",   21.6,
          "halfletter", 14,   "11x17", 27.9, "ledger", 43.2
         );
EOM
ok($output, $expected_output);
