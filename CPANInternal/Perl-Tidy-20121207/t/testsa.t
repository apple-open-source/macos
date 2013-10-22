use strict;
use Test;
use Carp;
BEGIN {plan tests => 1}
use Perl::Tidy; 

#----------------------------------------------------------------------
## test string->array 
#     Also tests flags -ce and -l=60
#     Note that we have to use -npro to avoid using local .perltidyrc
#----------------------------------------------------------------------
my $source = <<'EOM';
$seqno = $type_sequence[$i];
if ($seqno) {
  if (tok =~/[\(\[\{]/) {
      $indentation{$seqno} = indentation
  }
}
elsif (tok =~/[\)\]\}]/) {
  $min_indentation = $indentation{$seqno};
  delete $indentation{$seqno};
  if ($indentation < $min_indentation) {$indentation = $min_indentation}
}
EOM

my @tidy_output;

Perl::Tidy::perltidy(
    source      => \$source,
    destination => \@tidy_output,
    perltidyrc  => undef,
    argv        => '-nsyn -ce -npro -l=60',
);

my @expected_output=<DATA>;
my $ok=1;
if (@expected_output == @tidy_output) {
        while ( $_ = pop @tidy_output ) {
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

# This is the expected result of 'perltidy -ce -l=60' on the above string:

__DATA__
$seqno = $type_sequence[$i];
if ($seqno) {
    if ( tok =~ /[\(\[\{]/ ) {
        $indentation{$seqno} = indentation;
    }
} elsif ( tok =~ /[\)\]\}]/ ) {
    $min_indentation = $indentation{$seqno};
    delete $indentation{$seqno};
    if ( $indentation < $min_indentation ) {
        $indentation = $min_indentation;
    }
}
