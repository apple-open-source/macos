package DBIx::Class::Schema::Loader::DBI::ODBC::Microsoft_SQL_Server;

use strict;
use warnings;
use base qw/
    DBIx::Class::Schema::Loader::DBI::ODBC
    DBIx::Class::Schema::Loader::DBI::MSSQL
/;
use mro 'c3';

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::ODBC::Microsoft_SQL_Server - ODBC wrapper for
L<DBIx::Class::Schema::Loader::DBI::MSSQL>

=head1 DESCRIPTION

Proxy for L<DBIx::Class::Schema::Loader::DBI::MSSQL> when using L<DBD::ODBC>.

See L<DBIx::Class::Schema::Loader::Base> for usage information.

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::MSSQL>,
L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,
L<DBIx::Class::Schema::Loader::DBI>

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
