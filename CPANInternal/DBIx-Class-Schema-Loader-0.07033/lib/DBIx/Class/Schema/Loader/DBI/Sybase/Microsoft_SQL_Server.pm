package DBIx::Class::Schema::Loader::DBI::Sybase::Microsoft_SQL_Server;

use strict;
use warnings;
use base 'DBIx::Class::Schema::Loader::DBI::MSSQL';
use mro 'c3';

our $VERSION = '0.07033';

=head1 NAME

DBIx::Class::Schema::Loader::DBI::Sybase::Microsoft_SQL_Server - Driver for
using Microsoft SQL Server through DBD::Sybase

=head1 DESCRIPTION

Subclasses L<DBIx::Class::Schema::Loader::DBI::MSSQL>.

See L<DBIx::Class::Schema::Loader> and L<DBIx::Class::Schema::Loader::Base>.

=head1 SEE ALSO

L<DBIx::Class::Schema::Loader::DBI::MSSQL>,
L<DBIx::Class::Schema::Loader::DBI::ODBC::Microsoft_SQL_Server>,
L<DBIx::Class::Schema::Loader::DBI::Sybase::Common>,
L<DBIx::Class::Schema::Loader::DBI>
L<DBIx::Class::Schema::Loader>, L<DBIx::Class::Schema::Loader::Base>,

=head1 AUTHOR

See L<DBIx::Class::Schema::Loader/AUTHOR> and L<DBIx::Class::Schema::Loader/CONTRIBUTORS>.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut

1;
