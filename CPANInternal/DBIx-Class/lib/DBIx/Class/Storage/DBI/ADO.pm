package # hide from PAUSE
    DBIx::Class::Storage::DBI::ADO;

use base 'DBIx::Class::Storage::DBI';

sub _rebless {
  my $self = shift;

# check for MSSQL
# XXX This should be using an OpenSchema method of some sort, but I don't know
# how.
# Current version is stolen from Sybase.pm
  my $dbtype = eval {
    @{$self->_get_dbh
      ->selectrow_arrayref(qq{sp_server_info \@attribute_id=1})
    }[2]
  };

  unless ($@) {
    $dbtype =~ s/\W/_/gi;
    my $subclass = "DBIx::Class::Storage::DBI::ADO::${dbtype}";
    if ($self->load_optional_class($subclass) && !$self->isa($subclass)) {
      bless $self, $subclass;
      $self->_rebless;
    }
  }
}

# Here I was just experimenting with ADO cursor types, left in as a comment in
# case you want to as well. See the DBD::ADO docs.
#sub _dbh_sth {
#  my ($self, $dbh, $sql) = @_;
#
#  my $sth = $self->disable_sth_caching
#    ? $dbh->prepare($sql, { CursorType => 'adOpenStatic' })
#    : $dbh->prepare_cached($sql, { CursorType => 'adOpenStatic' }, 3);
#
#  $self->throw_exception($dbh->errstr) if !$sth;
#
#  $sth;
#}

1;
