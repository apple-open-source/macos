
package Class::MOP::Object;

use strict;
use warnings;

use Scalar::Util 'blessed';

our $VERSION   = '0.98';
$VERSION = eval $VERSION;
our $AUTHORITY = 'cpan:STEVAN';

# introspection

sub meta { 
    require Class::MOP::Class;
    Class::MOP::Class->initialize(blessed($_[0]) || $_[0]);
}

sub _new {
    Class::MOP::class_of(shift)->new_object(@_);
}

# RANT:
# Cmon, how many times have you written 
# the following code while debugging:
# 
#  use Data::Dumper; 
#  warn Dumper $obj;
#
# It can get seriously annoying, so why 
# not just do this ...
sub dump { 
    my $self = shift;
    require Data::Dumper;
    local $Data::Dumper::Maxdepth = shift || 1;
    Data::Dumper::Dumper $self;
}

1;

__END__

=pod

=head1 NAME 

Class::MOP::Object - Base class for metaclasses

=head1 DESCRIPTION

This class is a very minimal base class for metaclasses.

=head1 METHODS

This class provides a few methods which are useful in all metaclasses.

=over 4

=item B<< Class::MOP::???->meta >>

This returns a L<Class::MOP::Class> object.

=item B<< $metaobject->dump($max_depth) >>

This method uses L<Data::Dumper> to dump the object. You can pass an
optional maximum depth, which will set C<$Data::Dumper::Maxdepth>. The
default maximum depth is 1.

=back

=head1 AUTHORS

Stevan Little E<lt>stevan@iinteractive.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2010 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
