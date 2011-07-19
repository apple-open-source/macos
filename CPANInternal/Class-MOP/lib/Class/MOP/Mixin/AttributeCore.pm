package Class::MOP::Mixin::AttributeCore;

use strict;
use warnings;

our $VERSION   = '0.98';
$VERSION = eval $VERSION;
our $AUTHORITY = 'cpan:STEVAN';

use Scalar::Util 'blessed';

use base 'Class::MOP::Mixin';

sub has_accessor        { defined $_[0]->{'accessor'} }
sub has_reader          { defined $_[0]->{'reader'} }
sub has_writer          { defined $_[0]->{'writer'} }
sub has_predicate       { defined $_[0]->{'predicate'} }
sub has_clearer         { defined $_[0]->{'clearer'} }
sub has_builder         { defined $_[0]->{'builder'} }
sub has_init_arg        { defined $_[0]->{'init_arg'} }
sub has_default         { defined $_[0]->{'default'} }
sub has_initializer     { defined $_[0]->{'initializer'} }
sub has_insertion_order { defined $_[0]->{'insertion_order'} }

sub accessor             { $_[0]->{'accessor'} }
sub reader               { $_[0]->{'reader'} }
sub writer               { $_[0]->{'writer'} }
sub predicate            { $_[0]->{'predicate'} }
sub clearer              { $_[0]->{'clearer'} }
sub builder              { $_[0]->{'builder'} }
sub init_arg             { $_[0]->{'init_arg'} }
sub initializer          { $_[0]->{'initializer'} }
sub definition_context   { $_[0]->{'definition_context'} }
sub insertion_order      { $_[0]->{'insertion_order'} }
sub _set_insertion_order { $_[0]->{'insertion_order'} = $_[1] }

sub has_read_method  { $_[0]->has_reader || $_[0]->has_accessor }
sub has_write_method { $_[0]->has_writer || $_[0]->has_accessor }

sub is_default_a_coderef {
    # Uber hack because it is called from CMOP::Attribute constructor as
    # $class->is_default_a_coderef(\%options)
    my ($value) = ref $_[0] ? $_[0]->{'default'} : $_[1]->{'default'};

    return unless ref($value);

    return ref($value) eq 'CODE'
        || ( blessed($value) && $value->isa('Class::MOP::Method') );
}

sub default {
    my ( $self, $instance ) = @_;
    if ( defined $instance && $self->is_default_a_coderef ) {
        # if the default is a CODE ref, then we pass in the instance and
        # default can return a value based on that instance. Somewhat crude,
        # but works.
        return $self->{'default'}->($instance);
    }
    $self->{'default'};
}

1;

__END__

=pod

=head1 NAME

Class::MOP::Mixin::AttributeCore - Core attributes shared by attribute metaclasses

=head1 DESCRIPTION

This class implements the core attributes (aka properties) shared by all
attributes. See the L<Class::MOP::Attribute> documentation for API details.

=head1 AUTHORS

Dave Rolsky E<lt>autarch@urth.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006-2010 by Infinity Interactive, Inc.

L<http://www.iinteractive.com>

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
