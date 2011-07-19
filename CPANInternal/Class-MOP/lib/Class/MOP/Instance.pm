
package Class::MOP::Instance;

use strict;
use warnings;

use Scalar::Util 'weaken', 'blessed';

our $VERSION   = '0.98';
$VERSION = eval $VERSION;
our $AUTHORITY = 'cpan:STEVAN';

use base 'Class::MOP::Object';

sub BUILDARGS {
    my ($class, @args) = @_;

    if ( @args == 1 ) {
        unshift @args, "associated_metaclass";
    } elsif ( @args >= 2 && blessed($args[0]) && $args[0]->isa("Class::MOP::Class") ) {
        # compat mode
        my ( $meta, @attrs ) = @args;
        @args = ( associated_metaclass => $meta, attributes => \@attrs );
    }

    my %options = @args;
    # FIXME lazy_build
    $options{slots} ||= [ map { $_->slots } @{ $options{attributes} || [] } ];
    $options{slot_hash} = { map { $_ => undef } @{ $options{slots} } }; # FIXME lazy_build

    return \%options;
}

sub new {
    my $class = shift;
    my $options = $class->BUILDARGS(@_);

    # FIXME replace with a proper constructor
    my $instance = $class->_new(%$options);

    # FIXME weak_ref => 1,
    weaken($instance->{'associated_metaclass'});

    return $instance;
}

sub _new {
    my $class = shift;
    return Class::MOP::Class->initialize($class)->new_object(@_)
      if $class ne __PACKAGE__;

    my $params = @_ == 1 ? $_[0] : {@_};
    return bless {
        # NOTE:
        # I am not sure that it makes
        # sense to pass in the meta
        # The ideal would be to just
        # pass in the class name, but
        # that is placing too much of
        # an assumption on bless(),
        # which is *probably* a safe
        # assumption,.. but you can
        # never tell <:)
        'associated_metaclass' => $params->{associated_metaclass},
        'attributes'           => $params->{attributes},
        'slots'                => $params->{slots},
        'slot_hash'            => $params->{slot_hash},
    } => $class;
}

sub _class_name { $_[0]->{_class_name} ||= $_[0]->associated_metaclass->name }

sub associated_metaclass { $_[0]{'associated_metaclass'} }

sub create_instance {
    my $self = shift;
    bless {}, $self->_class_name;
}

sub clone_instance {
    my ($self, $instance) = @_;
    bless { %$instance }, $self->_class_name;
}

# operations on meta instance

sub get_all_slots {
    my $self = shift;
    return @{$self->{'slots'}};
}

sub get_all_attributes {
    my $self = shift;
    return @{$self->{attributes}};
}

sub is_valid_slot {
    my ($self, $slot_name) = @_;
    exists $self->{'slot_hash'}->{$slot_name};
}

# operations on created instances

sub get_slot_value {
    my ($self, $instance, $slot_name) = @_;
    $instance->{$slot_name};
}

sub set_slot_value {
    my ($self, $instance, $slot_name, $value) = @_;
    $instance->{$slot_name} = $value;
}

sub initialize_slot {
    my ($self, $instance, $slot_name) = @_;
    return;
}

sub deinitialize_slot {
    my ( $self, $instance, $slot_name ) = @_;
    delete $instance->{$slot_name};
}

sub initialize_all_slots {
    my ($self, $instance) = @_;
    foreach my $slot_name ($self->get_all_slots) {
        $self->initialize_slot($instance, $slot_name);
    }
}

sub deinitialize_all_slots {
    my ($self, $instance) = @_;
    foreach my $slot_name ($self->get_all_slots) {
        $self->deinitialize_slot($instance, $slot_name);
    }
}

sub is_slot_initialized {
    my ($self, $instance, $slot_name, $value) = @_;
    exists $instance->{$slot_name};
}

sub weaken_slot_value {
    my ($self, $instance, $slot_name) = @_;
    weaken $instance->{$slot_name};
}

sub strengthen_slot_value {
    my ($self, $instance, $slot_name) = @_;
    $self->set_slot_value($instance, $slot_name, $self->get_slot_value($instance, $slot_name));
}

sub rebless_instance_structure {
    my ($self, $instance, $metaclass) = @_;

    # we use $_[1] here because of t/306_rebless_overload.t regressions on 5.8.8
    bless $_[1], $metaclass->name;
}

sub is_dependent_on_superclasses {
    return; # for meta instances that require updates on inherited slot changes
}

# inlinable operation snippets

sub is_inlinable { 1 }

sub inline_create_instance {
    my ($self, $class_variable) = @_;
    'bless {} => ' . $class_variable;
}

sub inline_slot_access {
    my ($self, $instance, $slot_name) = @_;
    sprintf q[%s->{"%s"}], $instance, quotemeta($slot_name);
}

sub inline_get_slot_value {
    my ($self, $instance, $slot_name) = @_;
    $self->inline_slot_access($instance, $slot_name);
}

sub inline_set_slot_value {
    my ($self, $instance, $slot_name, $value) = @_;
    $self->inline_slot_access($instance, $slot_name) . " = $value",
}

sub inline_initialize_slot {
    my ($self, $instance, $slot_name) = @_;
    return '';
}

sub inline_deinitialize_slot {
    my ($self, $instance, $slot_name) = @_;
    "delete " . $self->inline_slot_access($instance, $slot_name);
}
sub inline_is_slot_initialized {
    my ($self, $instance, $slot_name) = @_;
    "exists " . $self->inline_slot_access($instance, $slot_name);
}

sub inline_weaken_slot_value {
    my ($self, $instance, $slot_name) = @_;
    sprintf "Scalar::Util::weaken( %s )", $self->inline_slot_access($instance, $slot_name);
}

sub inline_strengthen_slot_value {
    my ($self, $instance, $slot_name) = @_;
    $self->inline_set_slot_value($instance, $slot_name, $self->inline_slot_access($instance, $slot_name));
}

sub inline_rebless_instance_structure {
    my ($self, $instance, $class_variable) = @_;
    "bless $instance => $class_variable";
}

1;

__END__

=pod

=head1 NAME

Class::MOP::Instance - Instance Meta Object

=head1 DESCRIPTION

The Instance Protocol controls the creation of object instances, and
the storage of attribute values in those instances.

Using this API directly in your own code violates encapsulation, and
we recommend that you use the appropriate APIs in L<Class::MOP::Class>
and L<Class::MOP::Attribute> instead. Those APIs in turn call the
methods in this class as appropriate.

This class also participates in generating inlined code by providing
snippets of code to access an object instance.

=head1 METHODS

=head2 Object construction

=over 4

=item B<< Class::MOP::Instance->new(%options) >>

This method creates a new meta-instance object.

It accepts the following keys in C<%options>:

=over 8

=item * associated_metaclass

The L<Class::MOP::Class> object for which instances will be created.

=item * attributes

An array reference of L<Class::MOP::Attribute> objects. These are the
attributes which can be stored in each instance.

=back

=back

=head2 Creating and altering instances

=over 4

=item B<< $metainstance->create_instance >>

This method returns a reference blessed into the associated
metaclass's class.

The default is to use a hash reference. Subclasses can override this.

=item B<< $metainstance->clone_instance($instance) >>

Given an instance, this method creates a new object by making
I<shallow> clone of the original.

=back

=head2 Introspection

=over 4

=item B<< $metainstance->associated_metaclass >>

This returns the L<Class::MOP::Class> object associated with the
meta-instance object.

=item B<< $metainstance->get_all_slots >>

This returns a list of slot names stored in object instances. In
almost all cases, slot names correspond directly attribute names.

=item B<< $metainstance->is_valid_slot($slot_name) >>

This will return true if C<$slot_name> is a valid slot name.

=item B<< $metainstance->get_all_attributes >>

This returns a list of attributes corresponding to the attributes
passed to the constructor.

=back

=head2 Operations on Instance Structures

It's important to understand that the meta-instance object is a
different entity from the actual instances it creates. For this
reason, any operations on the C<$instance_structure> always require
that the object instance be passed to the method.

=over 4

=item B<< $metainstance->get_slot_value($instance_structure, $slot_name) >>

=item B<< $metainstance->set_slot_value($instance_structure, $slot_name, $value) >>

=item B<< $metainstance->initialize_slot($instance_structure, $slot_name) >>

=item B<< $metainstance->deinitialize_slot($instance_structure, $slot_name) >>

=item B<< $metainstance->initialize_all_slots($instance_structure) >>

=item B<< $metainstance->deinitialize_all_slots($instance_structure) >>

=item B<< $metainstance->is_slot_initialized($instance_structure, $slot_name) >>

=item B<< $metainstance->weaken_slot_value($instance_structure, $slot_name) >>

=item B<< $metainstance->strengthen_slot_value($instance_structure, $slot_name) >>

=item B<< $metainstance->rebless_instance_structure($instance_structure, $new_metaclass) >>

The exact details of what each method does should be fairly obvious
from the method name.

=back

=head2 Inlinable Instance Operations

=over 4

=item B<< $metainstance->is_inlinable >>

This is a boolean that indicates whether or not slot access operations
can be inlined. By default it is true, but subclasses can override
this.

=item B<< $metainstance->inline_create_instance($class_variable) >>

This method expects a string that, I<when inlined>, will become a
class name. This would literally be something like C<'$class'>, not an
actual class name.

It returns a snippet of code that creates a new object for the
class. This is something like C< bless {}, $class_name >.

=item B<< $metainstance->inline_slot_access($instance_variable, $slot_name) >>

=item B<< $metainstance->inline_get_slot_value($instance_variable, $slot_name) >>

=item B<< $metainstance->inline_set_slot_value($instance_variable, $slot_name, $value) >>

=item B<< $metainstance->inline_initialize_slot($instance_variable, $slot_name) >>

=item B<< $metainstance->inline_deinitialize_slot($instance_variable, $slot_name) >>

=item B<< $metainstance->inline_is_slot_initialized($instance_variable, $slot_name) >>

=item B<< $metainstance->inline_weaken_slot_value($instance_variable, $slot_name) >>

=item B<< $metainstance->inline_strengthen_slot_value($instance_variable, $slot_name) >>

These methods all expect two arguments. The first is the name of a
variable, than when inlined, will represent the object
instance. Typically this will be a literal string like C<'$_[0]'>.

The second argument is a slot name.

The method returns a snippet of code that, when inlined, performs some
operation on the instance.

=item B<< $metainstance->inline_rebless_instance_structure($instance_variable, $class_variable) >>

This takes the name of a variable that will, when inlined, represent the object
instance, and the name of a variable that will represent the class to rebless
into, and returns code to rebless an instance into a class.

=back

=head2 Introspection

=over 4

=item B<< Class::MOP::Instance->meta >>

This will return a L<Class::MOP::Class> instance for this class.

It should also be noted that L<Class::MOP> will actually bootstrap
this module by installing a number of attribute meta-objects into its
metaclass.

=back

=head1 AUTHORS

Yuval Kogman E<lt>nothingmuch@woobling.comE<gt>

Stevan Little E<lt>stevan@iinteractive.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2010 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

