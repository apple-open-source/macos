#!/usr/bin/perl -wn

# This program was posted on the MacPerl mailing list by 
# Charles Albrecht as one way to get perltidy to work as a filter
# under BBEdit.

use Perl::Tidy;

BEGIN { my $input_string = ""; my $output_string = ""; }

$input_string .= $_;

END {
    Perl::Tidy::perltidy(
        source      => \$input_string,
        destination => \$output_string
    );

    print "$output_string\n";
}

__END__

