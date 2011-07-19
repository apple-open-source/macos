
package Class::MOP::Module;

use strict;
use warnings;

use Carp         'confess';
use Scalar::Util 'blessed';

our $VERSION   = '0.98';
$VERSION = eval $VERSION;
our $AUTHORITY = 'cpan:STEVAN';

use base 'Class::MOP::Package';

sub _new {
    my $class = shift;
    return Class::MOP::Class->initialize($class)->new_object(@_)
        if $class ne __PACKAGE__;

    my $params = @_ == 1 ? $_[0] : {@_};
    return bless {

        # from Class::MOP::Package
        package   => $params->{package},
        namespace => \undef,

        # attributes
        version   => \undef,
        authority => \undef
    } => $class;
}

sub version {  
    my $self = shift;
    ${$self->get_package_symbol({ sigil => '$', type => 'SCALAR', name => 'VERSION' })};
}

sub authority {  
    my $self = shift;
    ${$self->get_package_symbol({ sigil => '$', type => 'SCALAR', name => 'AUTHORITY' })};
}

sub identifier {
    my $self = shift;
    join '-' => (
        $self->name,
        ($self->version   || ()),
        ($self->authority || ()),
    );
}

sub create {
    confess "The Class::MOP::Module->create method has been made a private object method.\n";
}

sub _instantiate_module {
    my($self, $version, $authority) = @_;
    my $package_name = $self->name;

    Class::MOP::_is_valid_class_name($package_name)
        || confess "creation of $package_name failed: invalid package name";

    no strict 'refs';
    scalar %{ $package_name . '::' };    # touch the stash
    ${ $package_name . '::VERSION' }   = $version   if defined $version;
    ${ $package_name . '::AUTHORITY' } = $authority if defined $authority;

    return;
}

1;

__END__

=pod

=head1 NAME 

Class::MOP::Module - Module Meta Object

=head1 DESCRIPTION

A module is essentially a L<Class::MOP::Package> with metadata, in our
case the version and authority.

=head1 INHERITANCE

B<Class::MOP::Module> is a subclass of L<Class::MOP::Package>.

=head1 METHODS

=over 4

=item B<< $metamodule->version >>

This is a read-only attribute which returns the C<$VERSION> of the
package, if one exists.

=item B<< $metamodule->authority >>

This is a read-only attribute which returns the C<$AUTHORITY> of the
package, if one exists.

=item B<< $metamodule->identifier >>

This constructs a string which combines the name, version and
authority.

=item B<< Class::MOP::Module->meta >>

This will return a L<Class::MOP::Class> instance for this class.

=back

=head1 AUTHORS

Stevan Little E<lt>stevan@iinteractive.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2010 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
