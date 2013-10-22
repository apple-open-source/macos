package DBIx::Class::Schema::Loader::DBI::Sybase::Microsoft_SQL_Server;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI::MSSQL';
use Carp::Clan qw/^DBIx::Class/;
use Class::C3;

our $VERSION = '0.05003';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::Sybase::Microsoft_SQL_Server - Subclass for
using MSSQL through DBD::Sybase

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader::Base>.

Subclasses L<DBIx::Class::Schema::Loader::DBI::MSSQL>.

=cut

# Returns an array of table names
sub _tables_list { 
    my $self = shift;

    my ($table, $type) = @_ ? @_ : ('%', '%');

    my $dbh = $self->schema->storage->dbh;
    my @tables = $dbh->tables(undef, $self->db_schema, $table, $type);

    return $self->_filter_tables(@tables);
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::Sybase>,
L<DBIx::Class::Schema::Loader::DBI::MSSQL>,
L<DBIx::Class::Schema::Loader::DBI>
L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
