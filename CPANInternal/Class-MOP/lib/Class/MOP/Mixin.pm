package Class::MOP::Mixin;

use strict;
use warnings;

our $VERSION   = '0.98';
$VERSION = eval $VERSION;
our $AUTHORITY = 'cpan:STEVAN';

use Scalar::Util 'blessed';

sub meta {
    require Class::MOP::Class;
    Class::MOP::Class->initialize( blessed( $_[0] ) || $_[0] );
}

1;

__END__

=pod

=head1 NAME

Class::MOP::Mixin - Base class for mixin classes

=head1 DESCRIPTION

This class provides a single method shared by all mixins

=head1 METHODS

This class provides a few methods which are useful in all metaclasses.

=over 4

=item B<< Class::MOP::Mixin->meta >>

This returns a L<Class::MOP::Class> object for the mixin class.

=back

=head1 AUTHORS

Dave Rolsky E<lt>autarch@urth.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2010 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
