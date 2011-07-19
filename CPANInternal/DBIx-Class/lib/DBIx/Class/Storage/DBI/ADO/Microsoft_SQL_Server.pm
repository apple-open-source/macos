package DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server;

use strict;
use warnings;

use base qw/
  DBIx::Class::Storage::DBI::ADO
  DBIx::Class::Storage::DBI::MSSQL
/;
use mro 'c3';

sub _rebless {
  my $self = shift;
  $self->_identity_method('@@identity');
}

sub source_bind_attributes {
  my $self = shift;
  my ($source) = @_;

  my $bind_attributes = $self->next::method(@_);

  foreach my $column ($source->columns) {
    $bind_attributes->{$column}{ado_size} ||= 8000; # max VARCHAR
  }

  return $bind_attributes;
}

sub bind_attribute_by_data_type {
  my ($self, $data_type) = @_;

  ($data_type = lc($data_type)) =~ s/\s+.*//;

  my $max_size =
    $self->_mssql_max_data_type_representation_size_in_bytes->{$data_type};

  my $res = {};
  $res->{ado_size} = $max_size if $max_size;

  return $res;
}

# approximate
# XXX needs to support varchar(max) and varbinary(max)
sub _mssql_max_data_type_representation_size_in_bytes {
  my $self = shift;

  my $blob_max = $self->_get_dbh->{LongReadLen} || 32768;

  return +{
# MSSQL types
    char => 8000,
    varchar => 8000,
    binary => 8000,
    varbinary => 8000,
    nchar => 8000,
    nvarchar => 8000,
    numeric => 100,
    smallint => 100,
    tinyint => 100,
    smallmoney => 100,
    bigint => 100,
    bit => 100,
    decimal => 100,
    integer => 100,
    int => 100,
    money => 100,
    float => 100,
    real => 100,
    uniqueidentifier => 100,
    ntext => $blob_max,
    text => $blob_max,
    image => $blob_max,
    date => 100,
    datetime => 100,
    datetime2 => 100,
    datetimeoffset => 100,
    smalldatetime => 100,
    time => 100,
    timestamp => 100,
    cursor => 100,
    hierarchyid => 100,
    sql_variant => 100,
    table => 100,
    xml => $blob_max, # ???

# some non-MSSQL types
    serial => 100,
    bigserial => 100,
    varchar2 => 8000,
    blob => $blob_max,
    clob => $blob_max,
  }
}

1;

=head1 NAME

DBIx::Class::Storage::DBI::ADO::Microsoft_SQL_Server - Support for Microsoft
SQL Server via DBD::ADO

=head1 SYNOPSIS

This subclass supports MSSQL server connections via L<DBD::ADO>.

=head1 DESCRIPTION

The MSSQL specific functionality is provided by
L<DBIx::Class::Storage::DBI::MSSQL>.

=head2 CAVEATS

=head3 identities

C<_identity_method> is set to C<@@identity>, as C<SCOPE_IDENTITY()> doesn't work
with L<DBD::ADO>. See L<DBIx::Class::Storage::DBI::MSSQL/IMPLEMENTATION NOTES>
for caveats regarding this.

=head3 truncation bug

There is a bug with MSSQL ADO providers where data gets truncated based on the
size of the bind sizes in the first prepare call:

L<https://rt.cpan.org/Ticket/Display.html?id=52048>

The C<ado_size> workaround is used (see L<DBD::ADO/"ADO Providers">) with the
approximate maximum size of the data_type of the bound column, or 8000 (maximum
VARCHAR size) if the data_type is not available.

This code is incomplete and may be buggy. Particularly, C<VARCHAR(MAX)> is not
supported yet. The data_type list for other DBs is also incomplete. Please
report problems (and send patches.)

=head1 AUTHOR

See L<DBIx::Class/CONTRIBUTORS>.

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
