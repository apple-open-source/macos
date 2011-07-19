package DBIx::Class::Storage::DBI::MSSQL;

use strict;
use warnings;

use base qw/DBIx::Class::Storage::DBI/;
use mro 'c3';

use List::Util();

__PACKAGE__->mk_group_accessors(simple => qw/
  _identity _identity_method
/);

__PACKAGE__->sql_maker_class('DBIx::Class::SQLAHacks::MSSQL');

sub _set_identity_insert {
  my ($self, $table) = @_;

  my $sql = sprintf (
    'SET IDENTITY_INSERT %s ON',
    $self->sql_maker->_quote ($table),
  );

  my $dbh = $self->_get_dbh;
  eval { $dbh->do ($sql) };
  if ($@) {
    $self->throw_exception (sprintf "Error executing '%s': %s",
      $sql,
      $dbh->errstr,
    );
  }
}

sub _unset_identity_insert {
  my ($self, $table) = @_;

  my $sql = sprintf (
    'SET IDENTITY_INSERT %s OFF',
    $self->sql_maker->_quote ($table),
  );

  my $dbh = $self->_get_dbh;
  $dbh->do ($sql);
}

sub insert_bulk {
  my $self = shift;
  my ($source, $cols, $data) = @_;

  my $is_identity_insert = (List::Util::first
      { $source->column_info ($_)->{is_auto_increment} }
      (@{$cols})
  )
     ? 1
     : 0;

  if ($is_identity_insert) {
     $self->_set_identity_insert ($source->name);
  }

  $self->next::method(@_);

  if ($is_identity_insert) {
     $self->_unset_identity_insert ($source->name);
  }
}

# support MSSQL GUID column types

sub insert {
  my $self = shift;
  my ($source, $to_insert) = @_;

  my $supplied_col_info = $self->_resolve_column_info($source, [keys %$to_insert] );

  my %guid_cols;
  my @pk_cols = $source->primary_columns;
  my %pk_cols;
  @pk_cols{@pk_cols} = ();

  my @pk_guids = grep {
    $source->column_info($_)->{data_type}
    &&
    $source->column_info($_)->{data_type} =~ /^uniqueidentifier/i
  } @pk_cols;

  my @auto_guids = grep {
    $source->column_info($_)->{data_type}
    &&
    $source->column_info($_)->{data_type} =~ /^uniqueidentifier/i
    &&
    $source->column_info($_)->{auto_nextval}
  } grep { not exists $pk_cols{$_} } $source->columns;

  my @get_guids_for =
    grep { not exists $to_insert->{$_} } (@pk_guids, @auto_guids);

  my $updated_cols = {};

  for my $guid_col (@get_guids_for) {
    my ($new_guid) = $self->_get_dbh->selectrow_array('SELECT NEWID()');
    $updated_cols->{$guid_col} = $to_insert->{$guid_col} = $new_guid;
  }

  my $is_identity_insert = (List::Util::first { $_->{is_auto_increment} } (values %$supplied_col_info) )
     ? 1
     : 0;

  if ($is_identity_insert) {
     $self->_set_identity_insert ($source->name);
  }

  $updated_cols = { %$updated_cols, %{ $self->next::method(@_) } };

  if ($is_identity_insert) {
     $self->_unset_identity_insert ($source->name);
  }


  return $updated_cols;
}

sub _prep_for_execute {
  my $self = shift;
  my ($op, $extra_bind, $ident, $args) = @_;

# cast MONEY values properly
  if ($op eq 'insert' || $op eq 'update') {
    my $fields = $args->[0];

    for my $col (keys %$fields) {
      # $ident is a result source object with INSERT/UPDATE ops
      if ($ident->column_info ($col)->{data_type}
         &&
         $ident->column_info ($col)->{data_type} =~ /^money\z/i) {
        my $val = $fields->{$col};
        $fields->{$col} = \['CAST(? AS MONEY)', [ $col => $val ]];
      }
    }
  }

  my ($sql, $bind) = $self->next::method (@_);

  if ($op eq 'insert') {
    $sql .= ';SELECT SCOPE_IDENTITY()';

  }

  return ($sql, $bind);
}

sub _execute {
  my $self = shift;
  my ($op) = @_;

  my ($rv, $sth, @bind) = $self->dbh_do($self->can('_dbh_execute'), @_);

  if ($op eq 'insert') {

    # this should bring back the result of SELECT SCOPE_IDENTITY() we tacked
    # on in _prep_for_execute above
    my ($identity) = eval { $sth->fetchrow_array };

    # SCOPE_IDENTITY failed, but we can do something else
    if ( (! $identity) && $self->_identity_method) {
      ($identity) = $self->_dbh->selectrow_array(
        'select ' . $self->_identity_method
      );
    }

    $self->_identity($identity);
    $sth->finish;
  }

  return wantarray ? ($rv, $sth, @bind) : $rv;
}

sub last_insert_id { shift->_identity }

#
# MSSQL is retarded wrt ordered subselects. One needs to add a TOP
# to *all* subqueries, but one also can't use TOP 100 PERCENT
# http://sqladvice.com/forums/permalink/18496/22931/ShowThread.aspx#22931
#
sub _select_args_to_query {
  my $self = shift;

  my ($sql, $prep_bind, @rest) = $self->next::method (@_);

  # see if this is an ordered subquery
  my $attrs = $_[3];
  if ( scalar $self->_parse_order_by ($attrs->{order_by}) ) {
    $self->throw_exception(
      'An ordered subselect encountered - this is not safe! Please see "Ordered Subselects" in DBIx::Class::Storage::DBI::MSSQL
    ') unless $attrs->{unsafe_subselect_ok};
    my $max = 2 ** 32;
    $sql =~ s/^ \s* SELECT \s/SELECT TOP $max /xi;
  }

  return wantarray
    ? ($sql, $prep_bind, @rest)
    : \[ "($sql)", @$prep_bind ]
  ;
}


# savepoint syntax is the same as in Sybase ASE

sub _svp_begin {
  my ($self, $name) = @_;

  $self->_get_dbh->do("SAVE TRANSACTION $name");
}

# A new SAVE TRANSACTION with the same name releases the previous one.
sub _svp_release { 1 }

sub _svp_rollback {
  my ($self, $name) = @_;

  $self->_get_dbh->do("ROLLBACK TRANSACTION $name");
}

sub build_datetime_parser {
  my $self = shift;
  my $type = "DateTime::Format::Strptime";
  eval "use ${type}";
  $self->throw_exception("Couldn't load ${type}: $@") if $@;
  return $type->new( pattern => '%Y-%m-%d %H:%M:%S' );  # %F %T
}

sub sqlt_type { 'SQLServer' }

sub _get_mssql_version {
  my $self = shift;

  my $data = $self->_get_dbh->selectrow_hashref('xp_msver ProductVersion');

  if ($data->{Character_Value} =~ /^(\d+)\./) {
    return $1;
  } else {
    $self->throw_exception(q{Your ProductVersion's Character_Value is missing or malformed!});
  }
}

sub sql_maker {
  my $self = shift;

  unless ($self->_sql_maker) {
    unless ($self->{_sql_maker_opts}{limit_dialect}) {
      my $version = eval { $self->_get_mssql_version; } || 0;

      $self->{_sql_maker_opts} = {
        limit_dialect => ($version >= 9 ? 'RowNumberOver' : 'Top'),
        %{$self->{_sql_maker_opts}||{}}
      };
    }

    my $maker = $self->next::method (@_);
  }

  return $self->_sql_maker;
}

1;

=head1 NAME

DBIx::Class::Storage::DBI::MSSQL - Base Class for Microsoft SQL Server support
in DBIx::Class

=head1 SYNOPSIS

This is the base class for Microsoft SQL Server support, used by
L<DBIx::Class::Storage::DBI::ODBC::Microsoft_SQL_Server> and
L<DBIx::Class::Storage::DBI::Sybase::Microsoft_SQL_Server>.

=head1 IMPLEMENTATION NOTES

=head2 IDENTITY information

Microsoft SQL Server supports three methods of retrieving the IDENTITY
value for inserted row: IDENT_CURRENT, @@IDENTITY, and SCOPE_IDENTITY().
SCOPE_IDENTITY is used here because it is the safest.  However, it must
be called is the same execute statement, not just the same connection.

So, this implementation appends a SELECT SCOPE_IDENTITY() statement
onto each INSERT to accommodate that requirement.

C<SELECT @@IDENTITY> can also be used by issuing:

  $self->_identity_method('@@identity');

it will only be used if SCOPE_IDENTITY() fails.

This is more dangerous, as inserting into a table with an on insert trigger that
inserts into another table with an identity will give erroneous results on
recent versions of SQL Server.

=head2 identity insert

Be aware that we have tried to make things as simple as possible for our users.
For MSSQL that means that when a user tries to create a row, while supplying an
explicit value for an autoincrementing column, we will try to issue the
appropriate database call to make this possible, namely C<SET IDENTITY_INSERT
$table_name ON>. Unfortunately this operation in MSSQL requires the
C<db_ddladmin> privilege, which is normally not included in the standard
write-permissions.

=head2 Ordered Subselects

If you attempted the following query (among many others) in Microsoft SQL
Server

 $rs->search ({}, {
  prefetch => 'relation',
  rows => 2,
  offset => 3,
 });

You may be surprised to receive an exception. The reason for this is a quirk
in the MSSQL engine itself, and sadly doesn't have a sensible workaround due
to the way DBIC is built. DBIC can do truly wonderful things with the aid of
subselects, and does so automatically when necessary. The list of situations
when a subselect is necessary is long and still changes often, so it can not
be exhaustively enumerated here. The general rule of thumb is a joined
L<has_many|DBIx::Class::Relationship/has_many> relationship with limit/group
applied to the left part of the join.

In its "pursuit of standards" Microsft SQL Server goes to great lengths to
forbid the use of ordered subselects. This breaks a very useful group of
searches like "Give me things number 4 to 6 (ordered by name), and prefetch
all their relations, no matter how many". While there is a hack which fools
the syntax checker, the optimizer may B<still elect to break the subselect>.
Testing has determined that while such breakage does occur (the test suite
contains an explicit test which demonstrates the problem), it is relative
rare. The benefits of ordered subselects are on the other hand too great to be
outright disabled for MSSQL.

Thus compromise between usability and perfection is the MSSQL-specific
L<resultset attribute|DBIx::Class::ResultSet/ATTRIBUTES> C<unsafe_subselect_ok>.
It is deliberately not possible to set this on the Storage level, as the user
should inspect (and preferably regression-test) the return of every such
ResultSet individually. The example above would work if written like:

 $rs->search ({}, {
  unsafe_subselect_ok => 1,
  prefetch => 'relation',
  rows => 2,
  offset => 3,
 });

If it is possible to rewrite the search() in a way that will avoid the need
for this flag - you are urged to do so. If DBIC internals insist that an
ordered subselect is necessary for an operation, and you believe there is a
different/better way to get the same result - please file a bugreport.

=head1 AUTHOR

See L<DBIx::Class/CONTRIBUTORS>.

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
