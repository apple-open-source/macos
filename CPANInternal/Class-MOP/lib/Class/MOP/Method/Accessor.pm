
package Class::MOP::Method::Accessor;

use strict;
use warnings;

use Carp         'confess';
use Scalar::Util 'blessed', 'weaken';

our $VERSION   = '0.98';
$VERSION = eval $VERSION;
our $AUTHORITY = 'cpan:STEVAN';

use base 'Class::MOP::Method::Generated';

sub new {
    my $class   = shift;
    my %options = @_;

    (exists $options{attribute})
        || confess "You must supply an attribute to construct with";

    (exists $options{accessor_type})
        || confess "You must supply an accessor_type to construct with";

    (blessed($options{attribute}) && $options{attribute}->isa('Class::MOP::Attribute'))
        || confess "You must supply an attribute which is a 'Class::MOP::Attribute' instance";

    ($options{package_name} && $options{name})
        || confess "You must supply the package_name and name parameters $Class::MOP::Method::UPGRADE_ERROR_TEXT";

    my $self = $class->_new(\%options);

    # we don't want this creating
    # a cycle in the code, if not
    # needed
    weaken($self->{'attribute'});

    $self->_initialize_body;

    return $self;
}

sub _new {
    my $class = shift;

    return Class::MOP::Class->initialize($class)->new_object(@_)
        if $class ne __PACKAGE__;

    my $params = @_ == 1 ? $_[0] : {@_};

    return bless {
        # inherited from Class::MOP::Method
        body                 => $params->{body},
        associated_metaclass => $params->{associated_metaclass},
        package_name         => $params->{package_name},
        name                 => $params->{name},
        original_method      => $params->{original_method},

        # inherit from Class::MOP::Generated
        is_inline            => $params->{is_inline} || 0,
        definition_context   => $params->{definition_context},

        # defined in this class
        attribute            => $params->{attribute},
        accessor_type        => $params->{accessor_type},
    } => $class;
}

## accessors

sub associated_attribute { (shift)->{'attribute'}     }
sub accessor_type        { (shift)->{'accessor_type'} }

## factory

sub _initialize_body {
    my $self = shift;

    my $method_name = join "_" => (
        '_generate',
        $self->accessor_type,
        'method',
        ($self->is_inline ? 'inline' : ())
    );

    $self->{'body'} = $self->$method_name();
}

## generators

sub _generate_accessor_method {
    my $attr = (shift)->associated_attribute;
    return sub {
        $attr->set_value($_[0], $_[1]) if scalar(@_) == 2;
        $attr->get_value($_[0]);
    };
}

sub _generate_reader_method {
    my $attr = (shift)->associated_attribute;
    return sub {
        confess "Cannot assign a value to a read-only accessor" if @_ > 1;
        $attr->get_value($_[0]);
    };
}


sub _generate_writer_method {
    my $attr = (shift)->associated_attribute;
    return sub {
        $attr->set_value($_[0], $_[1]);
    };
}

sub _generate_predicate_method {
    my $attr = (shift)->associated_attribute;
    return sub {
        $attr->has_value($_[0])
    };
}

sub _generate_clearer_method {
    my $attr = (shift)->associated_attribute;
    return sub {
        $attr->clear_value($_[0])
    };
}

## Inline methods

sub _generate_accessor_method_inline {
    my $self          = shift;
    my $attr          = $self->associated_attribute;
    my $attr_name     = $attr->name;
    my $meta_instance = $attr->associated_class->instance_metaclass;

    my ( $code, $e ) = $self->_eval_closure(
        {},
        'sub {'
        . $meta_instance->inline_set_slot_value('$_[0]', $attr_name, '$_[1]')
        . ' if scalar(@_) == 2; '
        . $meta_instance->inline_get_slot_value('$_[0]', $attr_name)
        . '}'
    );
    confess "Could not generate inline accessor because : $e" if $e;

    return $code;
}

sub _generate_reader_method_inline {
    my $self          = shift;
    my $attr          = $self->associated_attribute;
    my $attr_name     = $attr->name;
    my $meta_instance = $attr->associated_class->instance_metaclass;

     my ( $code, $e ) = $self->_eval_closure(
         {},
        'sub {'
        . 'confess "Cannot assign a value to a read-only accessor" if @_ > 1;'
        . $meta_instance->inline_get_slot_value('$_[0]', $attr_name)
        . '}'
    );
    confess "Could not generate inline reader because : $e" if $e;

    return $code;
}

sub _generate_writer_method_inline {
    my $self          = shift;
    my $attr          = $self->associated_attribute;
    my $attr_name     = $attr->name;
    my $meta_instance = $attr->associated_class->instance_metaclass;

    my ( $code, $e ) = $self->_eval_closure(
        {},
        'sub {'
        . $meta_instance->inline_set_slot_value('$_[0]', $attr_name, '$_[1]')
        . '}'
    );
    confess "Could not generate inline writer because : $e" if $e;

    return $code;
}

sub _generate_predicate_method_inline {
    my $self          = shift;
    my $attr          = $self->associated_attribute;
    my $attr_name     = $attr->name;
    my $meta_instance = $attr->associated_class->instance_metaclass;

    my ( $code, $e ) = $self->_eval_closure(
        {},
       'sub {'
       . $meta_instance->inline_is_slot_initialized('$_[0]', $attr_name)
       . '}'
    );
    confess "Could not generate inline predicate because : $e" if $e;

    return $code;
}

sub _generate_clearer_method_inline {
    my $self          = shift;
    my $attr          = $self->associated_attribute;
    my $attr_name     = $attr->name;
    my $meta_instance = $attr->associated_class->instance_metaclass;

    my ( $code, $e ) = $self->_eval_closure(
        {},
        'sub {'
        . $meta_instance->inline_deinitialize_slot('$_[0]', $attr_name)
        . '}'
    );
    confess "Could not generate inline clearer because : $e" if $e;

    return $code;
}

1;

__END__

=pod

=head1 NAME

Class::MOP::Method::Accessor - Method Meta Object for accessors

=head1 SYNOPSIS

    use Class::MOP::Method::Accessor;

    my $reader = Class::MOP::Method::Accessor->new(
        attribute     => $attribute,
        is_inline     => 1,
        accessor_type => 'reader',
    );

    $reader->body->execute($instance); # call the reader method

=head1 DESCRIPTION

This is a subclass of <Class::MOP::Method> which is used by
C<Class::MOP::Attribute> to generate accessor code. It handles
generation of readers, writers, predicates and clearers. For each type
of method, it can either create a subroutine reference, or actually
inline code by generating a string and C<eval>'ing it.

=head1 METHODS

=over 4

=item B<< Class::MOP::Method::Accessor->new(%options) >>

This returns a new C<Class::MOP::Method::Accessor> based on the
C<%options> provided.

=over 4

=item * attribute

This is the C<Class::MOP::Attribute> for which accessors are being
generated. This option is required.

=item * accessor_type

This is a string which should be one of "reader", "writer",
"accessor", "predicate", or "clearer". This is the type of method
being generated. This option is required.

=item * is_inline

This indicates whether or not the accessor should be inlined. This
defaults to false.

=item * name

The method name (without a package name). This is required.

=item * package_name

The package name for the method. This is required.

=back

=item B<< $metamethod->accessor_type >>

Returns the accessor type which was passed to C<new>.

=item B<< $metamethod->is_inline >>

Returns a boolean indicating whether or not the accessor is inlined.

=item B<< $metamethod->associated_attribute >>

This returns the L<Class::MOP::Attribute> object which was passed to
C<new>.

=item B<< $metamethod->body >>

The method itself is I<generated> when the accessor object is
constructed.

=back

=head1 AUTHORS

Stevan Little E<lt>stevan@iinteractive.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2010 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

