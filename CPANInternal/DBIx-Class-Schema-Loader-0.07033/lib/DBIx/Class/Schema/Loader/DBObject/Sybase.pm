package DBIx::Class::Schema::Loader::DBObject::Sybase;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBObject';
use mro 'c3';
use namespace::clean;

=head1 NAME

DBIx::Class::Schema::Loader::DBObject::Sybase - Class for Database Objects for
Sybase ASE and MSSQL Such as Tables and Views in L<DBIx::Class::Schema::Loader>

=head1 DESCRIPTION

This is a subclass of L<DBIx::Class::Schema::Loader::DBObject> that adds
support for fully qualified objects in Sybase ASE and MSSQL including both
L</database> and L<schema|DBIx::Class::Schema::Loader::DBObject/schema> of the
form:

    database.owner.object_name

=head1 METHODS

=head2 database

The database name this object belongs to.

Returns undef if
L<ignore_schema|DBIx::Class::Schema::Loader::DBObject/ignore_schema> is set.

=cut

__PACKAGE__->mk_group_accessors(simple => qw/
    _database
/);

sub new {
    my $class = shift;

    my $self = $class->next::method(@_);

    $self->{_database} = delete $self->{database};

    return $self;
}

sub database {
    my $self = shift;

    return $self->_database(@_) unless $self->ignore_schema;

    return undef;
}

=head1 sql_name

Returns the properly quoted full identifier with L</database>,
L<schema|DBIx::Class::Schema::Loader::DBObject/schema> and
L<name|DBIx::Class::Schema::Loader::DBObject/name>.

=cut

sub sql_name {
    my $self = shift;

    my $name_sep = $self->loader->name_sep;

    if ($self->database) {
        return $self->_quote($self->database)
            . $name_sep
            . $self->_quote($self->schema)
            . $name_sep
            . $self->_quote($self->name);
    }

    return $self->next::method(@_);
}

sub dbic_name {
    my $self = shift;

    my $name_sep = $self->loader->name_sep;

    if ($self->loader->qualify_objects && $self->_database) {
        if ($self->_database =~ /\W/
            || $self->_schema =~ /\W/ || $self->name =~ /\W/) {

            return \ $self->sql_name;
        }

        return $self->_database . $name_sep . $self->_schema . $name_sep . $self->name;
    }

    return $self->next::method(@_);
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::Table::Sybase>,
L<DBIx::Class::Schema::Loader::DBObject>,
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
