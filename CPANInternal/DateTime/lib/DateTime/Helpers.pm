package DateTime::Helpers;

use strict;
use warnings;

our $VERSION = '0.53';

use Scalar::Util ();


sub can
{
    my $object = shift;
    my $method = shift;

    return unless Scalar::Util::blessed($object);
    return $object->can($method);
}

sub isa
{
    my $object = shift;
    my $method = shift;

    return unless Scalar::Util::blessed($object);
    return $object->isa($method);
}


1;

__END__

=head1 NAME

DateTime::Helpers - Helper functions for other DateTime modules

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

However, please see the CREDITS file for more details on who I really
stole all the code from.

=head1 COPYRIGHT

Copyright (c) 2003-2009 David Rolsky.  All rights reserved.  This
program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut
