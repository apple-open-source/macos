package DBIx::Class::Storage::DBI::ODBC::SQL_Anywhere;

use strict;
use warnings;
use base qw/DBIx::Class::Storage::DBI::SQLAnywhere/;
use mro 'c3';

1;

=head1 NAME

DBIx::Class::Storage::DBI::ODBC::SQL_Anywhere - Driver for using Sybase SQL
Anywhere through ODBC

=head1 SYNOPSIS

All functionality is provided by L<DBIx::Class::Storage::DBI::SQLAnywhere>, see
that module for details.

=head1 AUTHOR

See L<DBIx::Class/AUTHOR> and L<DBIx::Class/CONTRIBUTORS>.

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
