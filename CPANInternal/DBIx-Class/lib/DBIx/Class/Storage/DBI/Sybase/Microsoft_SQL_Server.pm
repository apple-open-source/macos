package DBIx::Class::Storage::DBI::Sybase::Microsoft_SQL_Server;

use strict;
use warnings;

use base qw/
  DBIx::Class::Storage::DBI::Sybase
  DBIx::Class::Storage::DBI::MSSQL
/;
use mro 'c3';

sub _rebless {
  my $self = shift;
  my $dbh  = $self->_get_dbh;

  return if ref $self ne __PACKAGE__;

  if (not $self->_typeless_placeholders_supported) {
    require
      DBIx::Class::Storage::DBI::Sybase::Microsoft_SQL_Server::NoBindVars;
    bless $self,
      'DBIx::Class::Storage::DBI::Sybase::Microsoft_SQL_Server::NoBindVars';
    $self->_rebless;
  }
}

sub _run_connection_actions {
  my $self = shift;

  # LongReadLen doesn't work with MSSQL through DBD::Sybase, and the default is
  # huge on some versions of SQL server and can cause memory problems, so we
  # fix it up here (see ::DBI::Sybase.pm)
  $self->set_textsize;

  $self->next::method(@_);
}

sub _dbh_begin_work {
  my $self = shift;

  $self->_get_dbh->do('BEGIN TRAN');
}

sub _dbh_commit {
  my $self = shift;
  my $dbh  = $self->_dbh
    or $self->throw_exception('cannot COMMIT on a disconnected handle');
  $dbh->do('COMMIT');
}

sub _dbh_rollback {
  my $self = shift;
  my $dbh  = $self->_dbh
    or $self->throw_exception('cannot ROLLBACK on a disconnected handle');
  $dbh->do('ROLLBACK');
}

1;

=head1 NAME

DBIx::Class::Storage::DBI::Sybase::Microsoft_SQL_Server - Support for Microsoft
SQL Server via DBD::Sybase

=head1 SYNOPSIS

This subclass supports MSSQL server connections via L<DBD::Sybase>.

=head1 DESCRIPTION

This driver tries to determine whether your version of L<DBD::Sybase> and
supporting libraries (usually FreeTDS) support using placeholders, if not the
storage will be reblessed to
L<DBIx::Class::Storage::DBI::Sybase::Microsoft_SQL_Server::NoBindVars>.

The MSSQL specific functionality is provided by
L<DBIx::Class::Storage::DBI::MSSQL>.

=head1 AUTHOR

See L<DBIx::Class/CONTRIBUTORS>.

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
