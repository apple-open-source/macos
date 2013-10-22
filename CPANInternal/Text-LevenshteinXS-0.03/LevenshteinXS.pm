package Text::LevenshteinXS;

use strict;
use warnings;
use Carp;

require Exporter;
require DynaLoader;
use AutoLoader;

our @ISA = qw(Exporter DynaLoader);

our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
distance
);
our $VERSION = '0.03';

bootstrap Text::LevenshteinXS $VERSION;

1;
__END__

=head1 NAME

Text::LevenshteinXS - An XS implementation of the Levenshtein edit distance

=head1 SYNOPSIS

 use Text::LevenshteinXS qw(distance);

 print distance("foo","four");
 # prints "2"

 print distance("foo","bar");
 # prints "3"


=head1 DESCRIPTION

This module implements the Levenshtein edit distance in a XS way.

The Levenshtein edit distance is a measure of the degree of proximity between two strings.
This distance is the number of substitutions, deletions or insertions ("edits") 
needed to transform one string into the other one (and vice versa).
When two strings have distance 0, they are the same.
A good point to start is: <http://www.merriampark.com/ld.htm>


=head1 CREDITS

All the credits go to Vladimir Levenshtein the author of the algorithm and to 
Lorenzo Seidenari who made the C implementation <http://www.merriampark.com/ldc.htm>


=head1 SEE ALSO

Text::Levenshtein , Text::WagnerFischer , Text::Brew , String::Approx


=head1 AUTHOR

Copyright 2003 Dree Mistrut <F<dree@friul.it>>
Modifications Copyright 2004 Josh Goldberg <F<josh@3io.com>>

This package is free software and is provided "as is" without express
or implied warranty.  You can redistribute it and/or modify it under 
the same terms as Perl itself.

=cut
