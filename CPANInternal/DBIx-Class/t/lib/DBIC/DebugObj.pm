package DBIC::DebugObj;

use strict;
use warnings;

use Class::C3;

use base qw/DBIx::Class::Storage::Statistics Exporter Class::Accessor::Fast/;

__PACKAGE__->mk_accessors( qw/dbictest_sql_ref dbictest_bind_ref/ );


=head2 new(PKG, SQL_REF, BIND_REF, ...)

Creates a new instance that on subsequent queries will store
the generated SQL to the scalar pointed to by SQL_REF and bind
values to the array pointed to by BIND_REF.

=cut

sub new {
  my $pkg = shift;
  my $sql_ref = shift;
  my $bind_ref = shift;

  my $self = $pkg->SUPER::new(@_);

  $self->debugfh(undef);

  $self->dbictest_sql_ref($sql_ref);
  $self->dbictest_bind_ref($bind_ref || []);

  return $self;
}

sub query_start {
  my $self = shift;

  (${$self->dbictest_sql_ref}, @{$self->dbictest_bind_ref}) = @_;
}

sub query_end { }

sub txn_start { }

sub txn_commit { }

sub txn_rollback { }

1;
