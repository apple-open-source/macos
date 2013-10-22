package Log::Log4perl::Internal::Test;
use strict;
use warnings;

# We don't require any of these modules for testing, but if they're 
# installed, we require minimal versions.

our %MINVERSION = qw(
    DBI            1.607
    DBD::CSV       0.33
    SQL::Statement 1.20
);

1;

__END__

=head1 NAME

Log::Log4perl::Internal::Test - Internal Test Utilities for Log4perl

=head1 SYNOPSIS

    use Log::Log4perl::Internal::Test;

=head1 DESCRIPTION

Some general-purpose test routines and constants to be used in the Log4perl
test suite.

=head1 AUTHOR

Please contribute patches to the project on Github:

    http://github.com/mschilli/log4perl

Send bug reports or requests for enhancements to the authors via our

MAILING LIST (questions, bug reports, suggestions/patches): 
log4perl-devel@lists.sourceforge.net

Authors (please contact them via the list above, not directly):
Mike Schilli <m@perlmeister.com>,
Kevin Goess <cpan@goess.org>

Contributors (in alphabetical order):
Ateeq Altaf, Cory Bennett, Jens Berthold, Jeremy Bopp, Hutton
Davidson, Chris R. Donnelly, Matisse Enzer, Hugh Esco, Anthony
Foiani, James FitzGibbon, Carl Franks, Dennis Gregorovic, Andy
Grundman, Paul Harrington, David Hull, Robert Jacobson, Jason Kohles, 
Jeff Macdonald, Markus Peter, Brett Rann, Peter Rabbitson, Erik
Selberg, Aaron Straup Cope, Lars Thegler, David Viner, Mac Yang.

=head1 LICENSE

Copyright 2002-2012 by Mike Schilli E<lt>m@perlmeister.comE<gt> 
and Kevin Goess E<lt>cpan@goess.orgE<gt>.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

