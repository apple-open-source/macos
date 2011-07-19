package DBIx::Class::Storage::DBI::Informix;
use strict;
use warnings;

use base qw/DBIx::Class::Storage::DBI/;

use mro 'c3';

__PACKAGE__->mk_group_accessors('simple' => '__last_insert_id');

sub _execute {
  my $self = shift;
  my ($op) = @_;
  my ($rv, $sth, @rest) = $self->next::method(@_);
  if ($op eq 'insert') {
    $self->__last_insert_id($sth->{ix_sqlerrd}[1]);
  }
  return (wantarray ? ($rv, $sth, @rest) : $rv);
}

sub last_insert_id {
  shift->__last_insert_id;
}

sub _sql_maker_opts {
  my ( $self, $opts ) = @_;

  if ( $opts ) {
    $self->{_sql_maker_opts} = { %$opts };
  }

  return { limit_dialect => 'SkipFirst', %{$self->{_sql_maker_opts}||{}} };
}

1;

__END__

=head1 NAME

DBIx::Class::Storage::DBI::Informix - Base Storage Class for INFORMIX Support

=head1 SYNOPSIS

=head1 DESCRIPTION

This class implements storage-specific support for Informix

=head1 AUTHORS

See L<DBIx::Class/CONTRIBUTORS>

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
