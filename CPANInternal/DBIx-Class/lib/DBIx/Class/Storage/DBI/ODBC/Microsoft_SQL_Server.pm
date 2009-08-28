package DBIx::Class::Storage::DBI::ODBC::Microsoft_SQL_Server;
use strict;
use warnings;

use base qw/DBIx::Class::Storage::DBI/;

sub _prep_for_execute {
    my $self = shift;
    my ($op, $extra_bind, $ident, $args) = @_;

    my ($sql, $bind) = $self->SUPER::_prep_for_execute(@_);
    $sql .= ';SELECT SCOPE_IDENTITY()' if $op eq 'insert';

    return ($sql, $bind);
}

sub insert {
    my ($self, $source, $to_insert) = @_;

    my $bind_attributes = $self->source_bind_attributes($source);
    my (undef, $sth) = $self->_execute( 'insert' => [], $source, $bind_attributes, $to_insert);
    $self->{_scope_identity} = $sth->fetchrow_array;

    return $to_insert;
}

sub last_insert_id { shift->{_scope_identity} }

sub sqlt_type { 'SQLServer' }

sub _sql_maker_opts {
    my ( $self, $opts ) = @_;

    if ( $opts ) {
        $self->{_sql_maker_opts} = { %$opts };
    }

    return { limit_dialect => 'Top', %{$self->{_sql_maker_opts}||{}} };
}

sub build_datetime_parser {
  my $self = shift;
  my $type = "DateTime::Format::Strptime";
  eval "use ${type}";
  $self->throw_exception("Couldn't load ${type}: $@") if $@;
  return $type->new( pattern => '%F %T' );
}

1;

__END__

=head1 NAME

DBIx::Class::Storage::ODBC::Microsoft_SQL_Server - Support specific to
Microsoft SQL Server over ODBC

=head1 DESCRIPTION

This class implements support specific to Microsoft SQL Server over ODBC,
including auto-increment primary keys and SQL::Abstract::Limit dialect.  It
is loaded automatically by by DBIx::Class::Storage::DBI::ODBC when it
detects a MSSQL back-end.

=head1 IMPLEMENTATION NOTES

Microsoft SQL Server supports three methods of retrieving the IDENTITY
value for inserted row: IDENT_CURRENT, @@IDENTITY, and SCOPE_IDENTITY().
SCOPE_IDENTITY is used here because it is the safest.  However, it must
be called is the same execute statement, not just the same connection.

So, this implementation appends a SELECT SCOPE_IDENTITY() statement
onto each INSERT to accommodate that requirement.

=head1 METHODS

=head2 insert

=head2 last_insert_id

=head2 sqlt_type

=head2 build_datetime_parser

The resulting parser handles the MSSQL C<DATETIME> type, but is almost
certainly not sufficient for the other MSSQL 2008 date/time types.

=head1 AUTHORS

Marc Mims C<< <marc@questright.com> >>

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
