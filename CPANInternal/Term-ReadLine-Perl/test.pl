#! /usr/bin/perl -w
# Give an argument to use stdin, stdout instead of console
# If argument starts with /dev, use it as console
# If argument is '--no-print', do not print the result.

BEGIN{ $ENV{PERL_RL} = 'Perl' };	# Do not test TR::Gnu !
use Term::ReadLine;

use Carp;
$SIG{__WARN__} = sub { warn Carp::longmess(@_) };

if ($ENV{AUTOMATED_TESTING}) {
  print "1..0 # skip: \$ENV{AUTOMATED_TESTING} is TRUE\n";
  exit;
}

if (!@ARGV) {
  $term = new Term::ReadLine 'Simple Perl calc';
} elsif (@ARGV == 2) {
  open(IN,"<$ARGV[0]");
  open(OUT,">$ARGV[1]");
  $term = new Term::ReadLine 'Simple Perl calc', \*IN, \*OUT;
} elsif ($ARGV[0] =~ m|^/dev|) {
  open(IN,"<$ARGV[0]");
  open(OUT,">$ARGV[0]");
  $term = new Term::ReadLine 'Simple Perl calc', \*IN, \*OUT;
} else {
  $term = new Term::ReadLine 'Simple Perl calc', \*STDIN, \*STDOUT;
  $no_print = $ARGV[0] eq '--no-print';
}
$prompt = "Enter arithmetic or Perl expression: ";
$OUT = $term->OUT || STDOUT;
%features = %{ $term->Features };
if (%features) {
  @f = %features;
  print $OUT "Features present: @f\n";
  #$term->ornaments(1) if $features{ornaments};
} else {
  print $OUT "No additional features present.\n";
}
print $OUT "Flipping rl_default_selected each line.\n";
while ( defined ($_ = $term->readline($prompt, "exit")) ) {
  $res = eval($_);
  warn $@ if $@;
  print $OUT $res, "\n" unless $@ or $no_print;
  $term->addhistory($_) if /\S/ and !$features{autohistory};
  $readline::rl_default_selected = !$readline::rl_default_selected;
}
