package DBIx::Class::Schema::Loader::DBI::ADO;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI';
use mro 'c3';

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::ADO - L<DBD::ADO> proxy

=head1 DESCRIPTION

Reblesses into an C<::ADO::> class when connecting via L<DBD::ADO>.

See L<DBIx::Class::Schema::Loader::Base> for usage information.

=cut

sub _rebless {
  my $self = shift;

  return if ref $self ne __PACKAGE__;

  my $dbh  = $self->schema->storage->dbh;
  my $dbtype = eval { $dbh->get_info(17) };
  unless ( $@ ) {
    # Translate the backend name into a perl identifier
    $dbtype =~ s/\W/_/gi;
    my $class = "DBIx::Class::Schema::Loader::DBI::ADO::${dbtype}";
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

sub _filter_tables {
    my $self = shift;

    local $^W = 0; # turn off exception printing from Win32::OLE

    $self->next::method(@_);
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::ADO::Microsoft_SQL_Server>,
L<DBIx::Class::Schema::Loader::DBI::ADO::MS_Jet>,
L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,
L<DBIx::Class::Schema::Loader::DBI>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
