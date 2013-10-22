package DBIx::Class::Schema::Loader::DBObject;

use strict;
use warnings;
use base 'Class::Accessor::Grouped';
use mro 'c3';
use Carp::Clan qw/^DBIx::Class/;
use Scalar::Util 'weaken';
use namespace::clean;

=head1 NAME

DBIx::Class::Schema::Loader::DBObject - Base Class for Database Objects Such as
Tables and Views in L<DBIx::Class::Schema::Loader>

=head1 METHODS

=head2 loader

The loader object this object is associated with, this is a required parameter
to L</new>.

=head2 name

Name of the object. The object stringifies to this value.

=cut

__PACKAGE__->mk_group_accessors(simple => qw/
    loader
    name
    _schema
    ignore_schema
/);

use overload
    '""' => sub { $_[0]->name },
    fallback => 1;

=head2 new

The constructor, takes L</loader>, L</name>, L</schema>, and L</ignore_schema>
as key-value parameters.

=cut

sub new {
    my $class = shift;

    my $self = { @_ };

    croak "loader is required" unless ref $self->{loader};

    weaken $self->{loader};

    $self->{_schema} = delete $self->{schema};

    return bless $self, $class;
}

=head2 clone

Make a shallow copy of the object.

=cut

sub clone {
    my $self = shift;

    return bless { %$self }, ref $self;
}

=head2 schema

The schema (or owner) of the object. Returns nothing if L</ignore_schema> is
true.

=head2 ignore_schema

Set to true to make L</schema> and L</sql_name> not use the defined L</schema>.
Does not affect L</dbic_name> (for
L<qualify_objects|DBIx::Class::Schema::Loader::Base/qualify_objects> testing on
SQLite.)

=cut

sub schema {
    my $self = shift;

    return $self->_schema(@_) unless $self->ignore_schema;

    return undef;
}

sub _quote {
    my ($self, $identifier) = @_;

    $identifier = '' if not defined $identifier;

    my $qt = $self->loader->quote_char || '';

    if (length $qt > 1) {
        my @qt = split //, $qt;
        return $qt[0] . $identifier . $qt[1];
    }

    return "${qt}${identifier}${qt}";
}

=head1 sql_name

Returns the properly quoted full identifier with L</schema> and L</name>.

=cut

sub sql_name {
    my $self = shift;

    my $name_sep = $self->loader->name_sep;

    if ($self->schema) {
        return $self->_quote($self->schema)
            . $name_sep
            . $self->_quote($self->name);
    }

    return $self->_quote($self->name);
}

=head1 dbic_name

Returns a value suitable for the C<< __PACKAGE__->table >> call in L<DBIx::Class> Result files.

=cut

sub dbic_name {
    my $self = shift;

    my $name_sep = $self->loader->name_sep;

    if ($self->loader->qualify_objects && $self->_schema) {
        if ($self->_schema =~ /\W/ || $self->name =~ /\W/) {
            return \ $self->sql_name;
        }

        return $self->_schema . $name_sep . $self->name;
    }

    if ($self->name =~ /\W/) {
        return \ $self->_quote($self->name);
    }

    return $self->name;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::Table>, L<DBIx::Class::Schema::Loader>,
L<DBIx::Class::Schema::Loader::Base>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
# vim:et sts=4 sw=4 tw=0:
