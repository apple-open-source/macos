package DBICTest::Stats;
use strict;
use warnings;

use base qw/DBIx::Class::Storage::Statistics/;

sub txn_begin {
  my $self = shift;

  $self->{'TXN_BEGIN'}++;
  return $self->{'TXN_BEGIN'};
}

sub txn_rollback {
  my $self = shift;

  $self->{'TXN_ROLLBACK'}++;
  return $self->{'TXN_ROLLBACK'};
}

sub txn_commit {
  my $self = shift;

  $self->{'TXN_COMMIT'}++;
  return $self->{'TXN_COMMIT'};
}

sub svp_begin {
  my ($self, $name) = @_;

  $self->{'SVP_BEGIN'}++;
  return $self->{'SVP_BEGIN'};
}

sub svp_release {
  my ($self, $name) = @_;

  $self->{'SVP_RELEASE'}++;
  return $self->{'SVP_RELEASE'};
}

sub svp_rollback {
  my ($self, $name) = @_;

  $self->{'SVP_ROLLBACK'}++;
  return $self->{'SVP_ROLLBACK'};
}

sub query_start {
  my ($self, $string, @bind) = @_;

  $self->{'QUERY_START'}++;
  return $self->{'QUERY_START'};
}

sub query_end {
  my ($self, $string) = @_;

  $self->{'QUERY_END'}++;
  return $self->{'QUERY_START'};
}

1;
