package DBIx::Class::Schema::Loader::DBI::ODBC;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI';
use mro 'c3';

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::ODBC - L<DBD::ODBC> proxy

=head1 DESCRIPTION

Reblesses into an C<::ODBC::> class when connecting via L<DBD::ODBC>.

Code stolen from the L<DBIx::Class> ODBC storage.

See L<DBIx::Class::Schema::Loader::Base> for usage information.

=cut

sub _rebless {
  my $self = shift;

  return if ref $self ne __PACKAGE__;

# stolen from DBIC ODBC storage
  my $dbh  = $self->schema->storage->dbh;
  my $dbtype = eval { $dbh->get_info(17) };
  unless ( $@ ) {
    # Translate the backend name into a perl identifier
    $dbtype =~ s/\W/_/gi;
    my $class = "DBIx::Class::Schema::Loader::DBI::ODBC::${dbtype}";
    if ($self->load_optional_class($class) && !$self->isa($class)) {
        bless $self, $class;
        $self->_rebless;
    }
  }
}

sub _tables_list {
    my ($self, $opts) = @_;

    return $self->next::method($opts, undef, undef);
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::ODBC::Microsoft_SQL_Server>,
L<DBIx::Class::Schema::Loader::DBI::ODBC::SQL_Anywhere>,
L<DBIx::Class::Schema::Loader::DBI::ODBC::Firebird>,
L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,
L<DBIx::Class::Schema::Loader::DBI>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
