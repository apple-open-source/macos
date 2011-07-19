package Class::MOP::Mixin::HasAttributes;

use strict;
use warnings;

our $VERSION   = '0.98';
$VERSION = eval $VERSION;
our $AUTHORITY = 'cpan:STEVAN';

use Carp         'confess';
use Scalar::Util 'blessed';

use base 'Class::MOP::Mixin';

sub _attribute_map      { $_[0]->{'attributes'} }
sub attribute_metaclass { $_[0]->{'attribute_metaclass'} }

sub add_attribute {
    my $self = shift;

    my $attribute
        = blessed( $_[0] ) ? $_[0] : $self->attribute_metaclass->new(@_);

    ( $attribute->isa('Class::MOP::Mixin::AttributeCore') )
        || confess
        "Your attribute must be an instance of Class::MOP::Mixin::AttributeCore (or a subclass)";

    $self->_attach_attribute($attribute);

    my $attr_name = $attribute->name;

    $self->remove_attribute($attr_name)
        if $self->has_attribute($attr_name);

    my $order = ( scalar keys %{ $self->_attribute_map } );
    $attribute->_set_insertion_order($order);

    $self->_attribute_map->{$attr_name} = $attribute;

    # This method is called to allow for installing accessors. Ideally, we'd
    # use method overriding, but then the subclass would be responsible for
    # making the attribute, which would end up with lots of code
    # duplication. Even more ideally, we'd use augment/inner, but this is
    # Class::MOP!
    $self->_post_add_attribute($attribute)
        if $self->can('_post_add_attribute');

    return $attribute;
}

sub has_attribute {
    my ( $self, $attribute_name ) = @_;

    ( defined $attribute_name )
        || confess "You must define an attribute name";

    exists $self->_attribute_map->{$attribute_name};
}

sub get_attribute {
    my ( $self, $attribute_name ) = @_;

    ( defined $attribute_name )
        || confess "You must define an attribute name";

    return $self->_attribute_map->{$attribute_name};
}

sub remove_attribute {
    my ( $self, $attribute_name ) = @_;

    ( defined $attribute_name )
        || confess "You must define an attribute name";

    my $removed_attribute = $self->_attribute_map->{$attribute_name};
    return unless defined $removed_attribute;

    delete $self->_attribute_map->{$attribute_name};

    return $removed_attribute;
}

sub get_attribute_list {
    my $self = shift;
    keys %{ $self->_attribute_map };
}

1;

__END__

=pod

=head1 NAME

Class::MOP::Mixin::HasMethods - Methods for metaclasses which have attributes

=head1 DESCRIPTION

This class implements methods for metaclasses which have attributes
(L<Class::MOP::Class> and L<Moose::Meta::Role>). See L<Class::MOP::Class> for
API details.

=head1 AUTHORS

Dave Rolsky E<lt>autarch@urth.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2010 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
