package # Hide from pause for now - till we get it working
  DBIx::Class::Storage::TxnScopeGuard;

use strict;
use warnings;

sub new {
  my ($class, $storage) = @_;

  $storage->txn_begin;
  bless [ 0, $storage ], ref $class || $class;
}

sub commit {
  my $self = shift;

  $self->[1]->txn_commit;
  $self->[0] = 1;
}

sub DESTROY {
  my ($dismiss, $storage) = @{$_[0]};

  return if $dismiss;

  my $exception = $@;

  $DB::single = 1;

  local $@;
  eval { $storage->txn_rollback };
  my $rollback_exception = $@;
  if($rollback_exception) {
    my $exception_class = "DBIx::Class::Storage::NESTED_ROLLBACK_EXCEPTION";

    $storage->throw_exception(
      "Transaction aborted: ${exception}. "
      . "Rollback failed: ${rollback_exception}"
    ) unless $rollback_exception =~ /$exception_class/;
  }
}

1;

__END__

=head1 NAME

DBIx::Class::Storage::TxnScopeGuard

=head1 SYNOPSIS

 sub foo {
   my ($self, $schema) = @_;

   my $guard = $schema->txn_scope_guard;

   # Multiple database operations here

   $guard->commit;
 }

=head1 DESCRIPTION

An object that behaves much like L<Scope::Guard>, but hardcoded to do the
right thing with transactions in DBIx::Class. 

=head1 METHODS

=head2 new

Creating an instance of this class will start a new transaction. Expects a
L<DBIx::Class::Storage> object as its only argument.

=head2 commit

Commit the transaction, and stop guarding the scope. If this method is not
called (i.e. an exception is thrown) and this object goes out of scope then
the transaction is rolled back.

=cut

=head1 SEE ALSO

L<DBIx::Class::Schema/txn_scope_guard>.

=head1 AUTHOR

Ash Berlin, 2008.

Insipred by L<Scope::Guard> by chocolateboy.

This module is free software. It may be used, redistributed and/or modified
under the same terms as Perl itself.

=cut
