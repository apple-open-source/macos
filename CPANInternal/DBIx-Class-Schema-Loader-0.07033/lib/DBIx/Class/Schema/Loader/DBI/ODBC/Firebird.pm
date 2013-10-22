package DBIx::Class::Schema::Loader::DBI::ODBC::Firebird;

use strict;
use warnings;
use base qw/
    DBIx::Class::Schema::Loader::DBI::ODBC
    DBIx::Class::Schema::Loader::DBI::InterBase
/;
use mro 'c3';

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::ODBC::Firebird - ODBC wrapper for
L<DBIx::Class::Schema::Loader::DBI::InterBase>

=head1 DESCRIPTION

Proxy for L<DBIx::Class::Schema::Loader::DBI::InterBase> when using L<DBD::ODBC>.

See L<DBIx::Class::Schema::Loader::Base> for usage information.

=cut

# Some (current) versions of the ODBC driver have a bug where ->type_info breaks
# with "data truncated". This "fixes" it, but some type names are truncated.
sub _dbh_type_info_type_name {
    my ($self, $type_num) = @_;

    my $dbh = $self->schema->storage->dbh;

    local $dbh->{LongReadLen} = 100_000;
    local $dbh->{LongTruncOk} = 1;

    my $type_info = $dbh->type_info($type_num);

    return undef if not $type_info;
    
    my $type_name = $type_info->{TYPE_NAME};

    # fix up truncated type names
    if ($type_name eq "VARCHAR(x) CHARACTER SET UNICODE_\0") {
        return 'VARCHAR(x) CHARACTER SET UNICODE_FSS';
    }
    elsif ($type_name eq "BLOB SUB_TYPE TEXT CHARACTER SET \0") {
        return 'BLOB SUB_TYPE TEXT CHARACTER SET UNICODE_FSS';
    }

    return $type_name;
}

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::InterBase>,
L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,
L<DBIx::Class::Schema::Loader::DBI>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
