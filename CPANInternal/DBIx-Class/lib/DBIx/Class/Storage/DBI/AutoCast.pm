package DBIx::Class::Storage::DBI::AutoCast;

use strict;
use warnings;

use base qw/DBIx::Class::Storage::DBI/;
use mro 'c3';

__PACKAGE__->mk_group_accessors('simple' => 'auto_cast' );

=head1 NAME

DBIx::Class::Storage::DBI::AutoCast - Storage component for RDBMS requiring explicit placeholder typing

=head1 SYNOPSIS

  $schema->storage->auto_cast(1);

=head1 DESCRIPTION

In some combinations of RDBMS and DBD drivers (e.g. FreeTDS and Sybase)
statements with values bound to columns or conditions that are not strings will
throw implicit type conversion errors.

As long as a column L<data_type|DBIx::Class::ResultSource/add_columns> is
defined and resolves to a base RDBMS native type via L</_native_data_type> as
defined in your Storage driver, the placeholder for this column will be
converted to:

  CAST(? as $mapped_type)

This option can also be enabled in L<DBIx::Class::Storage::DBI/connect_info> as:

  on_connect_call => ['set_auto_cast']

=cut

sub _prep_for_execute {
  my $self = shift;
  my ($op, $extra_bind, $ident, $args) = @_;

  my ($sql, $bind) = $self->next::method (@_);

# If we're using ::NoBindVars, there are no binds by this point so this code
# gets skippeed.
  if ($self->auto_cast && @$bind) {
    my $new_sql;
    my @sql_part = split /\?/, $sql;
    my $col_info = $self->_resolve_column_info($ident,[ map $_->[0], @$bind ]);

    foreach my $bound (@$bind) {
      my $col = $bound->[0];
      my $type = $self->_native_data_type($col_info->{$col}{data_type});

      foreach my $data (@{$bound}[1..$#$bound]) {
        $new_sql .= shift(@sql_part) .
          ($type ? "CAST(? AS $type)" : '?');
      }
    }
    $new_sql .= join '', @sql_part;
    $sql = $new_sql;
  }

  return ($sql, $bind);
}

=head2 connect_call_set_auto_cast

Executes:

  $schema->storage->auto_cast(1);

on connection.

Used as:

    on_connect_call => ['set_auto_cast']

in L<DBIx::Class::Storage::DBI/connect_info>.

=cut

sub connect_call_set_auto_cast {
  my $self = shift;
  $self->auto_cast(1);
}

=head1 AUTHOR

See L<DBIx::Class/CONTRIBUTORS>

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut

1;
