package # hide from PAUSE
    DBIC::Test::SQLite;

=head1 NAME

DBIx::Class::Test::SQLite - Base class for running Class::DBI tests against DBIx::Class compat layer, shamelessly ripped from Class::DBI::Test::SQLite

=head1 SYNOPSIS

  use base 'DBIx::Class::Test::SQLite';

  __PACKAGE__->set_table('test');
  __PACKAGE__->columns(All => qw/id name film salary/);

  sub create_sql {
      return q{
          id     INTEGER PRIMARY KEY,
          name   CHAR(40),
          film   VARCHAR(255),
          salary INT
      }
  }
    
=head1 DESCRIPTION

This provides a simple base class for DBIx::Class::CDBICompat tests using
SQLite.  Each class for the test should inherit from this, provide a
create_sql() method which returns a string representing the SQL used to
create the table for the class, and then call set_table() to create the
table, and tie it to the class.

=cut

use strict;
use warnings;

use base qw/DBIx::Class/;

__PACKAGE__->load_components(qw/CDBICompat Core DB/);

use File::Temp qw/tempfile/;
my (undef, $DB) = tempfile();
END { unlink $DB if -e $DB }

my @DSN = ("dbi:SQLite:dbname=$DB", '', '', { AutoCommit => 1, RaiseError => 1 });

__PACKAGE__->connection(@DSN);
__PACKAGE__->set_sql(_table_pragma => 'PRAGMA table_info(__TABLE__)');
__PACKAGE__->set_sql(_create_me    => 'CREATE TABLE __TABLE__ (%s)');
__PACKAGE__->storage->dbh->do("PRAGMA synchronous = OFF");

=head1 METHODS

=head2 set_table

    __PACKAGE__->set_table('test');

This combines creating the table with the normal DBIx::Class table()
call.

=cut

sub set_table {
    my ($class, $table) = @_;
    $class->table($table);
    $class->_create_test_table;
}

sub _create_test_table {
    my $class = shift;
    my @vals  = $class->sql__table_pragma->select_row;
    $class->sql__create_me($class->create_sql)->execute unless @vals;
}

=head2 create_sql

This is an abstract method you must override.

  sub create_sql {
      return q{
          id     INTEGER PRIMARY KEY,
          name   CHAR(40),
          film   VARCHAR(255),
          salary INT
      }
  }

This should return, as a text string, the schema for the table represented
by this class.

=cut

sub create_sql { die "create_sql() not implemented by $_[0]\n" }

1;
