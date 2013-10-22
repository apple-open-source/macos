package DBIx::Class::Schema::Loader::DBI::Sybase::Common;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI';
use mro 'c3';

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::Sybase::Common - Common methods for Sybase
and MSSQL

=head1 DESCRIPTION

See L<DBIx::Class::Schema::Loader> and L<DBIx::Class::Schema::Loader::Base>.

=cut

# DBD::Sybase doesn't implement get_info properly
sub _build_quote_char { '[]' }
sub _build_name_sep   { '.'  }

sub _setup {
    my $self = shift;

    $self->next::method(@_);

    $self->schema->storage->sql_maker->quote_char([qw/[ ]/]);
    $self->schema->storage->sql_maker->name_sep('.');
}

# remove 'IDENTITY' from column data_type
sub _columns_info_for {
    my $self   = shift;
    my $result = $self->next::method(@_);

    foreach my $col (keys %$result) {
        $result->{$col}->{data_type} =~ s/\s* identity \s*//ix;
    }

    return $result;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::Sybase>,
L<DBIx::Class::Schema::Loader::DBI::MSSQL>,
L<DBIx::Class::Schema::Loader::DBI::ODBC::Microsoft_SQL_Server>,
L<DBIx::Class::Schema::Loader::DBI::Sybase::Microsoft_SQL_Server>,
L<DBIx::Class::Schema::Loader::DBI>
L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
