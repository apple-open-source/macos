package DBIx::Class::Storage::DBI;
# -*- mode: cperl; cperl-indent-level: 2 -*-

use base 'DBIx::Class::Storage';

use strict;    
use warnings;
use DBI;
use SQL::Abstract::Limit;
use DBIx::Class::Storage::DBI::Cursor;
use DBIx::Class::Storage::Statistics;
use Scalar::Util qw/blessed weaken/;

__PACKAGE__->mk_group_accessors('simple' =>
    qw/_connect_info _dbi_connect_info _dbh _sql_maker _sql_maker_opts
       _conn_pid _conn_tid disable_sth_caching on_connect_do
       on_disconnect_do transaction_depth unsafe _dbh_autocommit/
);

__PACKAGE__->cursor_class('DBIx::Class::Storage::DBI::Cursor');

__PACKAGE__->mk_group_accessors('inherited' => qw/sql_maker_class/);
__PACKAGE__->sql_maker_class('DBIC::SQL::Abstract');

BEGIN {

package DBIC::SQL::Abstract; # Would merge upstream, but nate doesn't reply :(

use base qw/SQL::Abstract::Limit/;

# This prevents the caching of $dbh in S::A::L, I believe
sub new {
  my $self = shift->SUPER::new(@_);

  # If limit_dialect is a ref (like a $dbh), go ahead and replace
  #   it with what it resolves to:
  $self->{limit_dialect} = $self->_find_syntax($self->{limit_dialect})
    if ref $self->{limit_dialect};

  $self;
}

sub _RowNumberOver {
  my ($self, $sql, $order, $rows, $offset ) = @_;

  $offset += 1;
  my $last = $rows + $offset;
  my ( $order_by ) = $self->_order_by( $order );

  $sql = <<"";
SELECT * FROM
(
   SELECT Q1.*, ROW_NUMBER() OVER( ) AS ROW_NUM FROM (
      $sql
      $order_by
   ) Q1
) Q2
WHERE ROW_NUM BETWEEN $offset AND $last

  return $sql;
}


# While we're at it, this should make LIMIT queries more efficient,
#  without digging into things too deeply
use Scalar::Util 'blessed';
sub _find_syntax {
  my ($self, $syntax) = @_;
  my $dbhname = blessed($syntax) ?  $syntax->{Driver}{Name} : $syntax;
  if(ref($self) && $dbhname && $dbhname eq 'DB2') {
    return 'RowNumberOver';
  }

  $self->{_cached_syntax} ||= $self->SUPER::_find_syntax($syntax);
}

sub select {
  my ($self, $table, $fields, $where, $order, @rest) = @_;
  $table = $self->_quote($table) unless ref($table);
  local $self->{rownum_hack_count} = 1
    if (defined $rest[0] && $self->{limit_dialect} eq 'RowNum');
  @rest = (-1) unless defined $rest[0];
  die "LIMIT 0 Does Not Compute" if $rest[0] == 0;
    # and anyway, SQL::Abstract::Limit will cause a barf if we don't first
  local $self->{having_bind} = [];
  my ($sql, @ret) = $self->SUPER::select(
    $table, $self->_recurse_fields($fields), $where, $order, @rest
  );
  $sql .= 
    $self->{for} ?
    (
      $self->{for} eq 'update' ? ' FOR UPDATE' :
      $self->{for} eq 'shared' ? ' FOR SHARE'  :
      ''
    ) :
    ''
  ;
  return wantarray ? ($sql, @ret, @{$self->{having_bind}}) : $sql;
}

sub insert {
  my $self = shift;
  my $table = shift;
  $table = $self->_quote($table) unless ref($table);
  $self->SUPER::insert($table, @_);
}

sub update {
  my $self = shift;
  my $table = shift;
  $table = $self->_quote($table) unless ref($table);
  $self->SUPER::update($table, @_);
}

sub delete {
  my $self = shift;
  my $table = shift;
  $table = $self->_quote($table) unless ref($table);
  $self->SUPER::delete($table, @_);
}

sub _emulate_limit {
  my $self = shift;
  if ($_[3] == -1) {
    return $_[1].$self->_order_by($_[2]);
  } else {
    return $self->SUPER::_emulate_limit(@_);
  }
}

sub _recurse_fields {
  my ($self, $fields, $params) = @_;
  my $ref = ref $fields;
  return $self->_quote($fields) unless $ref;
  return $$fields if $ref eq 'SCALAR';

  if ($ref eq 'ARRAY') {
    return join(', ', map {
      $self->_recurse_fields($_)
        .(exists $self->{rownum_hack_count} && !($params && $params->{no_rownum_hack})
          ? ' AS col'.$self->{rownum_hack_count}++
          : '')
      } @$fields);
  } elsif ($ref eq 'HASH') {
    foreach my $func (keys %$fields) {
      return $self->_sqlcase($func)
        .'( '.$self->_recurse_fields($fields->{$func}).' )';
    }
  }
}

sub _order_by {
  my $self = shift;
  my $ret = '';
  my @extra;
  if (ref $_[0] eq 'HASH') {
    if (defined $_[0]->{group_by}) {
      $ret = $self->_sqlcase(' group by ')
        .$self->_recurse_fields($_[0]->{group_by}, { no_rownum_hack => 1 });
    }
    if (defined $_[0]->{having}) {
      my $frag;
      ($frag, @extra) = $self->_recurse_where($_[0]->{having});
      push(@{$self->{having_bind}}, @extra);
      $ret .= $self->_sqlcase(' having ').$frag;
    }
    if (defined $_[0]->{order_by}) {
      $ret .= $self->_order_by($_[0]->{order_by});
    }
  } elsif (ref $_[0] eq 'SCALAR') {
    $ret = $self->_sqlcase(' order by ').${ $_[0] };
  } elsif (ref $_[0] eq 'ARRAY' && @{$_[0]}) {
    my @order = @{+shift};
    $ret = $self->_sqlcase(' order by ')
          .join(', ', map {
                        my $r = $self->_order_by($_, @_);
                        $r =~ s/^ ?ORDER BY //i;
                        $r;
                      } @order);
  } else {
    $ret = $self->SUPER::_order_by(@_);
  }
  return $ret;
}

sub _order_directions {
  my ($self, $order) = @_;
  $order = $order->{order_by} if ref $order eq 'HASH';
  return $self->SUPER::_order_directions($order);
}

sub _table {
  my ($self, $from) = @_;
  if (ref $from eq 'ARRAY') {
    return $self->_recurse_from(@$from);
  } elsif (ref $from eq 'HASH') {
    return $self->_make_as($from);
  } else {
    return $from; # would love to quote here but _table ends up getting called
                  # twice during an ->select without a limit clause due to
                  # the way S::A::Limit->select works. should maybe consider
                  # bypassing this and doing S::A::select($self, ...) in
                  # our select method above. meantime, quoting shims have
                  # been added to select/insert/update/delete here
  }
}

sub _recurse_from {
  my ($self, $from, @join) = @_;
  my @sqlf;
  push(@sqlf, $self->_make_as($from));
  foreach my $j (@join) {
    my ($to, $on) = @$j;

    # check whether a join type exists
    my $join_clause = '';
    my $to_jt = ref($to) eq 'ARRAY' ? $to->[0] : $to;
    if (ref($to_jt) eq 'HASH' and exists($to_jt->{-join_type})) {
      $join_clause = ' '.uc($to_jt->{-join_type}).' JOIN ';
    } else {
      $join_clause = ' JOIN ';
    }
    push(@sqlf, $join_clause);

    if (ref $to eq 'ARRAY') {
      push(@sqlf, '(', $self->_recurse_from(@$to), ')');
    } else {
      push(@sqlf, $self->_make_as($to));
    }
    push(@sqlf, ' ON ', $self->_join_condition($on));
  }
  return join('', @sqlf);
}

sub _make_as {
  my ($self, $from) = @_;
  return join(' ', map { (ref $_ eq 'SCALAR' ? $$_ : $self->_quote($_)) }
                     reverse each %{$self->_skip_options($from)});
}

sub _skip_options {
  my ($self, $hash) = @_;
  my $clean_hash = {};
  $clean_hash->{$_} = $hash->{$_}
    for grep {!/^-/} keys %$hash;
  return $clean_hash;
}

sub _join_condition {
  my ($self, $cond) = @_;
  if (ref $cond eq 'HASH') {
    my %j;
    for (keys %$cond) {
      my $v = $cond->{$_};
      if (ref $v) {
        # XXX no throw_exception() in this package and croak() fails with strange results
        Carp::croak(ref($v) . qq{ reference arguments are not supported in JOINS - try using \"..." instead'})
            if ref($v) ne 'SCALAR';
        $j{$_} = $v;
      }
      else {
        my $x = '= '.$self->_quote($v); $j{$_} = \$x;
      }
    };
    return scalar($self->_recurse_where(\%j));
  } elsif (ref $cond eq 'ARRAY') {
    return join(' OR ', map { $self->_join_condition($_) } @$cond);
  } else {
    die "Can't handle this yet!";
  }
}

sub _quote {
  my ($self, $label) = @_;
  return '' unless defined $label;
  return "*" if $label eq '*';
  return $label unless $self->{quote_char};
  if(ref $self->{quote_char} eq "ARRAY"){
    return $self->{quote_char}->[0] . $label . $self->{quote_char}->[1]
      if !defined $self->{name_sep};
    my $sep = $self->{name_sep};
    return join($self->{name_sep},
        map { $self->{quote_char}->[0] . $_ . $self->{quote_char}->[1]  }
       split(/\Q$sep\E/,$label));
  }
  return $self->SUPER::_quote($label);
}

sub limit_dialect {
    my $self = shift;
    $self->{limit_dialect} = shift if @_;
    return $self->{limit_dialect};
}

sub quote_char {
    my $self = shift;
    $self->{quote_char} = shift if @_;
    return $self->{quote_char};
}

sub name_sep {
    my $self = shift;
    $self->{name_sep} = shift if @_;
    return $self->{name_sep};
}

} # End of BEGIN block

=head1 NAME

DBIx::Class::Storage::DBI - DBI storage handler

=head1 SYNOPSIS

=head1 DESCRIPTION

This class represents the connection to an RDBMS via L<DBI>.  See
L<DBIx::Class::Storage> for general information.  This pod only
documents DBI-specific methods and behaviors.

=head1 METHODS

=cut

sub new {
  my $new = shift->next::method(@_);

  $new->transaction_depth(0);
  $new->_sql_maker_opts({});
  $new->{_in_dbh_do} = 0;
  $new->{_dbh_gen} = 0;

  $new;
}

=head2 connect_info

The arguments of C<connect_info> are always a single array reference.

This is normally accessed via L<DBIx::Class::Schema/connection>, which
encapsulates its argument list in an arrayref before calling
C<connect_info> here.

The arrayref can either contain the same set of arguments one would
normally pass to L<DBI/connect>, or a lone code reference which returns
a connected database handle.  Please note that the L<DBI> docs
recommend that you always explicitly set C<AutoCommit> to either
C<0> or C<1>.   L<DBIx::Class> further recommends that it be set
to C<1>, and that you perform transactions via our L</txn_do>
method.  L<DBIx::Class> will set it to C<1> if you do not do explicitly
set it to zero.  This is the default for most DBDs.  See below for more
details.

In either case, if the final argument in your connect_info happens
to be a hashref, C<connect_info> will look there for several
connection-specific options:

=over 4

=item on_connect_do

Specifies things to do immediately after connecting or re-connecting to
the database.  Its value may contain:

=over

=item an array reference

This contains SQL statements to execute in order.  Each element contains
a string or a code reference that returns a string.

=item a code reference

This contains some code to execute.  Unlike code references within an
array reference, its return value is ignored.

=back

=item on_disconnect_do

Takes arguments in the same form as L<on_connect_do> and executes them
immediately before disconnecting from the database.

Note, this only runs if you explicitly call L<disconnect> on the
storage object.

=item disable_sth_caching

If set to a true value, this option will disable the caching of
statement handles via L<DBI/prepare_cached>.

=item limit_dialect 

Sets the limit dialect. This is useful for JDBC-bridge among others
where the remote SQL-dialect cannot be determined by the name of the
driver alone.

=item quote_char

Specifies what characters to use to quote table and column names. If 
you use this you will want to specify L<name_sep> as well.

quote_char expects either a single character, in which case is it is placed
on either side of the table/column, or an arrayref of length 2 in which case the
table/column name is placed between the elements.

For example under MySQL you'd use C<quote_char =E<gt> '`'>, and user SQL Server you'd 
use C<quote_char =E<gt> [qw/[ ]/]>.

=item name_sep

This only needs to be used in conjunction with L<quote_char>, and is used to 
specify the charecter that seperates elements (schemas, tables, columns) from 
each other. In most cases this is simply a C<.>.

=item unsafe

This Storage driver normally installs its own C<HandleError>, sets
C<RaiseError> and C<ShowErrorStatement> on, and sets C<PrintError> off on
all database handles, including those supplied by a coderef.  It does this
so that it can have consistent and useful error behavior.

If you set this option to a true value, Storage will not do its usual
modifications to the database handle's attributes, and instead relies on
the settings in your connect_info DBI options (or the values you set in
your connection coderef, in the case that you are connecting via coderef).

Note that your custom settings can cause Storage to malfunction,
especially if you set a C<HandleError> handler that suppresses exceptions
and/or disable C<RaiseError>.

=back

These options can be mixed in with your other L<DBI> connection attributes,
or placed in a seperate hashref after all other normal L<DBI> connection
arguments.

Every time C<connect_info> is invoked, any previous settings for
these options will be cleared before setting the new ones, regardless of
whether any options are specified in the new C<connect_info>.

Another Important Note:

DBIC can do some wonderful magic with handling exceptions,
disconnections, and transactions when you use C<AutoCommit =&gt; 1>
combined with C<txn_do> for transaction support.

If you set C<AutoCommit =&gt; 0> in your connect info, then you are always
in an assumed transaction between commits, and you're telling us you'd
like to manage that manually.  A lot of DBIC's magic protections
go away.  We can't protect you from exceptions due to database
disconnects because we don't know anything about how to restart your
transactions.  You're on your own for handling all sorts of exceptional
cases if you choose the C<AutoCommit =&gt 0> path, just as you would
be with raw DBI.

Examples:

  # Simple SQLite connection
  ->connect_info([ 'dbi:SQLite:./foo.db' ]);

  # Connect via subref
  ->connect_info([ sub { DBI->connect(...) } ]);

  # A bit more complicated
  ->connect_info(
    [
      'dbi:Pg:dbname=foo',
      'postgres',
      'my_pg_password',
      { AutoCommit => 1 },
      { quote_char => q{"}, name_sep => q{.} },
    ]
  );

  # Equivalent to the previous example
  ->connect_info(
    [
      'dbi:Pg:dbname=foo',
      'postgres',
      'my_pg_password',
      { AutoCommit => 1, quote_char => q{"}, name_sep => q{.} },
    ]
  );

  # Subref + DBIC-specific connection options
  ->connect_info(
    [
      sub { DBI->connect(...) },
      {
          quote_char => q{`},
          name_sep => q{@},
          on_connect_do => ['SET search_path TO myschema,otherschema,public'],
          disable_sth_caching => 1,
      },
    ]
  );

=cut

sub connect_info {
  my ($self, $info_arg) = @_;

  return $self->_connect_info if !$info_arg;

  # Kill sql_maker/_sql_maker_opts, so we get a fresh one with only
  #  the new set of options
  $self->_sql_maker(undef);
  $self->_sql_maker_opts({});
  $self->_connect_info([@$info_arg]); # copy for _connect_info

  my $dbi_info = [@$info_arg]; # copy for _dbi_connect_info

  my $last_info = $dbi_info->[-1];
  if(ref $last_info eq 'HASH') {
    $last_info = { %$last_info }; # so delete is non-destructive
    my @storage_option = qw(
      on_connect_do on_disconnect_do disable_sth_caching unsafe cursor_class
    );
    for my $storage_opt (@storage_option) {
      if(my $value = delete $last_info->{$storage_opt}) {
        $self->$storage_opt($value);
      }
    }
    for my $sql_maker_opt (qw/limit_dialect quote_char name_sep/) {
      if(my $opt_val = delete $last_info->{$sql_maker_opt}) {
        $self->_sql_maker_opts->{$sql_maker_opt} = $opt_val;
      }
    }
    # re-insert modified hashref
    $dbi_info->[-1] = $last_info;

    # Get rid of any trailing empty hashref
    pop(@$dbi_info) if !keys %$last_info;
  }
  $self->_dbi_connect_info($dbi_info);

  $self->_connect_info;
}

=head2 on_connect_do

This method is deprecated in favor of setting via L</connect_info>.

=head2 dbh_do

Arguments: $subref, @extra_coderef_args?

Execute the given subref using the new exception-based connection management.

The first two arguments will be the storage object that C<dbh_do> was called
on and a database handle to use.  Any additional arguments will be passed
verbatim to the called subref as arguments 2 and onwards.

Using this (instead of $self->_dbh or $self->dbh) ensures correct
exception handling and reconnection (or failover in future subclasses).

Your subref should have no side-effects outside of the database, as
there is the potential for your subref to be partially double-executed
if the database connection was stale/dysfunctional.

Example:

  my @stuff = $schema->storage->dbh_do(
    sub {
      my ($storage, $dbh, @cols) = @_;
      my $cols = join(q{, }, @cols);
      $dbh->selectrow_array("SELECT $cols FROM foo");
    },
    @column_list
  );

=cut

sub dbh_do {
  my $self = shift;
  my $coderef = shift;

  ref $coderef eq 'CODE' or $self->throw_exception
    ('$coderef must be a CODE reference');

  return $coderef->($self, $self->_dbh, @_) if $self->{_in_dbh_do}
      || $self->{transaction_depth};

  local $self->{_in_dbh_do} = 1;

  my @result;
  my $want_array = wantarray;

  eval {
    $self->_verify_pid if $self->_dbh;
    $self->_populate_dbh if !$self->_dbh;
    if($want_array) {
        @result = $coderef->($self, $self->_dbh, @_);
    }
    elsif(defined $want_array) {
        $result[0] = $coderef->($self, $self->_dbh, @_);
    }
    else {
        $coderef->($self, $self->_dbh, @_);
    }
  };

  my $exception = $@;
  if(!$exception) { return $want_array ? @result : $result[0] }

  $self->throw_exception($exception) if $self->connected;

  # We were not connected - reconnect and retry, but let any
  #  exception fall right through this time
  $self->_populate_dbh;
  $coderef->($self, $self->_dbh, @_);
}

# This is basically a blend of dbh_do above and DBIx::Class::Storage::txn_do.
# It also informs dbh_do to bypass itself while under the direction of txn_do,
#  via $self->{_in_dbh_do} (this saves some redundant eval and errorcheck, etc)
sub txn_do {
  my $self = shift;
  my $coderef = shift;

  ref $coderef eq 'CODE' or $self->throw_exception
    ('$coderef must be a CODE reference');

  return $coderef->(@_) if $self->{transaction_depth};

  local $self->{_in_dbh_do} = 1;

  my @result;
  my $want_array = wantarray;

  my $tried = 0;
  while(1) {
    eval {
      $self->_verify_pid if $self->_dbh;
      $self->_populate_dbh if !$self->_dbh;

      $self->txn_begin;
      if($want_array) {
          @result = $coderef->(@_);
      }
      elsif(defined $want_array) {
          $result[0] = $coderef->(@_);
      }
      else {
          $coderef->(@_);
      }
      $self->txn_commit;
    };

    my $exception = $@;
    if(!$exception) { return $want_array ? @result : $result[0] }

    if($tried++ > 0 || $self->connected) {
      eval { $self->txn_rollback };
      my $rollback_exception = $@;
      if($rollback_exception) {
        my $exception_class = "DBIx::Class::Storage::NESTED_ROLLBACK_EXCEPTION";
        $self->throw_exception($exception)  # propagate nested rollback
          if $rollback_exception =~ /$exception_class/;

        $self->throw_exception(
          "Transaction aborted: ${exception}. "
          . "Rollback failed: ${rollback_exception}"
        );
      }
      $self->throw_exception($exception)
    }

    # We were not connected, and was first try - reconnect and retry
    # via the while loop
    $self->_populate_dbh;
  }
}

=head2 disconnect

Our C<disconnect> method also performs a rollback first if the
database is not in C<AutoCommit> mode.

=cut

sub disconnect {
  my ($self) = @_;

  if( $self->connected ) {
    my $connection_do = $self->on_disconnect_do;
    $self->_do_connection_actions($connection_do) if ref($connection_do);

    $self->_dbh->rollback unless $self->_dbh_autocommit;
    $self->_dbh->disconnect;
    $self->_dbh(undef);
    $self->{_dbh_gen}++;
  }
}

sub connected {
  my ($self) = @_;

  if(my $dbh = $self->_dbh) {
      if(defined $self->_conn_tid && $self->_conn_tid != threads->tid) {
          $self->_dbh(undef);
          $self->{_dbh_gen}++;
          return;
      }
      else {
          $self->_verify_pid;
          return 0 if !$self->_dbh;
      }
      return ($dbh->FETCH('Active') && $dbh->ping);
  }

  return 0;
}

# handle pid changes correctly
#  NOTE: assumes $self->_dbh is a valid $dbh
sub _verify_pid {
  my ($self) = @_;

  return if defined $self->_conn_pid && $self->_conn_pid == $$;

  $self->_dbh->{InactiveDestroy} = 1;
  $self->_dbh(undef);
  $self->{_dbh_gen}++;

  return;
}

sub ensure_connected {
  my ($self) = @_;

  unless ($self->connected) {
    $self->_populate_dbh;
  }
}

=head2 dbh

Returns the dbh - a data base handle of class L<DBI>.

=cut

sub dbh {
  my ($self) = @_;

  $self->ensure_connected;
  return $self->_dbh;
}

sub _sql_maker_args {
    my ($self) = @_;
    
    return ( bindtype=>'columns', limit_dialect => $self->dbh, %{$self->_sql_maker_opts} );
}

sub sql_maker {
  my ($self) = @_;
  unless ($self->_sql_maker) {
    my $sql_maker_class = $self->sql_maker_class;
    $self->_sql_maker($sql_maker_class->new( $self->_sql_maker_args ));
  }
  return $self->_sql_maker;
}

sub _populate_dbh {
  my ($self) = @_;
  my @info = @{$self->_dbi_connect_info || []};
  $self->_dbh($self->_connect(@info));

  # Always set the transaction depth on connect, since
  #  there is no transaction in progress by definition
  $self->{transaction_depth} = $self->_dbh_autocommit ? 0 : 1;

  if(ref $self eq 'DBIx::Class::Storage::DBI') {
    my $driver = $self->_dbh->{Driver}->{Name};
    if ($self->load_optional_class("DBIx::Class::Storage::DBI::${driver}")) {
      bless $self, "DBIx::Class::Storage::DBI::${driver}";
      $self->_rebless() if $self->can('_rebless');
    }
  }

  my $connection_do = $self->on_connect_do;
  $self->_do_connection_actions($connection_do) if ref($connection_do);

  $self->_conn_pid($$);
  $self->_conn_tid(threads->tid) if $INC{'threads.pm'};
}

sub _do_connection_actions {
  my $self = shift;
  my $connection_do = shift;

  if (ref $connection_do eq 'ARRAY') {
    $self->_do_query($_) foreach @$connection_do;
  }
  elsif (ref $connection_do eq 'CODE') {
    $connection_do->();
  }

  return $self;
}

sub _do_query {
  my ($self, $action) = @_;

  if (ref $action eq 'CODE') {
    $action = $action->($self);
    $self->_do_query($_) foreach @$action;
  }
  else {
    my @to_run = (ref $action eq 'ARRAY') ? (@$action) : ($action);
    $self->_query_start(@to_run);
    $self->_dbh->do(@to_run);
    $self->_query_end(@to_run);
  }

  return $self;
}

sub _connect {
  my ($self, @info) = @_;

  $self->throw_exception("You failed to provide any connection info")
    if !@info;

  my ($old_connect_via, $dbh);

  if ($INC{'Apache/DBI.pm'} && $ENV{MOD_PERL}) {
    $old_connect_via = $DBI::connect_via;
    $DBI::connect_via = 'connect';
  }

  eval {
    if(ref $info[0] eq 'CODE') {
       $dbh = &{$info[0]}
    }
    else {
       $dbh = DBI->connect(@info);
    }

    if($dbh && !$self->unsafe) {
      my $weak_self = $self;
      weaken($weak_self);
      $dbh->{HandleError} = sub {
          $weak_self->throw_exception("DBI Exception: $_[0]")
      };
      $dbh->{ShowErrorStatement} = 1;
      $dbh->{RaiseError} = 1;
      $dbh->{PrintError} = 0;
    }
  };

  $DBI::connect_via = $old_connect_via if $old_connect_via;

  $self->throw_exception("DBI Connection failed: " . ($@||$DBI::errstr))
    if !$dbh || $@;

  $self->_dbh_autocommit($dbh->{AutoCommit});

  $dbh;
}


sub txn_begin {
  my $self = shift;
  $self->ensure_connected();
  if($self->{transaction_depth} == 0) {
    $self->debugobj->txn_begin()
      if $self->debug;
    # this isn't ->_dbh-> because
    #  we should reconnect on begin_work
    #  for AutoCommit users
    $self->dbh->begin_work;
  }
  $self->{transaction_depth}++;
}

sub txn_commit {
  my $self = shift;
  if ($self->{transaction_depth} == 1) {
    my $dbh = $self->_dbh;
    $self->debugobj->txn_commit()
      if ($self->debug);
    $dbh->commit;
    $self->{transaction_depth} = 0
      if $self->_dbh_autocommit;
  }
  elsif($self->{transaction_depth} > 1) {
    $self->{transaction_depth}--
  }
}

sub txn_rollback {
  my $self = shift;
  my $dbh = $self->_dbh;
  eval {
    if ($self->{transaction_depth} == 1) {
      $self->debugobj->txn_rollback()
        if ($self->debug);
      $self->{transaction_depth} = 0
        if $self->_dbh_autocommit;
      $dbh->rollback;
    }
    elsif($self->{transaction_depth} > 1) {
      $self->{transaction_depth}--;
    }
    else {
      die DBIx::Class::Storage::NESTED_ROLLBACK_EXCEPTION->new;
    }
  };
  if ($@) {
    my $error = $@;
    my $exception_class = "DBIx::Class::Storage::NESTED_ROLLBACK_EXCEPTION";
    $error =~ /$exception_class/ and $self->throw_exception($error);
    # ensure that a failed rollback resets the transaction depth
    $self->{transaction_depth} = $self->_dbh_autocommit ? 0 : 1;
    $self->throw_exception($error);
  }
}

# This used to be the top-half of _execute.  It was split out to make it
#  easier to override in NoBindVars without duping the rest.  It takes up
#  all of _execute's args, and emits $sql, @bind.
sub _prep_for_execute {
  my ($self, $op, $extra_bind, $ident, $args) = @_;

  my ($sql, @bind) = $self->sql_maker->$op($ident, @$args);
  unshift(@bind,
    map { ref $_ eq 'ARRAY' ? $_ : [ '!!dummy', $_ ] } @$extra_bind)
      if $extra_bind;

  return ($sql, \@bind);
}

sub _fix_bind_params {
    my ($self, @bind) = @_;

    ### Turn @bind from something like this:
    ###   ( [ "artist", 1 ], [ "cdid", 1, 3 ] )
    ### to this:
    ###   ( "'1'", "'1'", "'3'" )
    return
        map {
            if ( defined( $_ && $_->[1] ) ) {
                map { qq{'$_'}; } @{$_}[ 1 .. $#$_ ];
            }
            else { q{'NULL'}; }
        } @bind;
}

sub _query_start {
    my ( $self, $sql, @bind ) = @_;

    if ( $self->debug ) {
        @bind = $self->_fix_bind_params(@bind);
        $self->debugobj->query_start( $sql, @bind );
    }
}

sub _query_end {
    my ( $self, $sql, @bind ) = @_;

    if ( $self->debug ) {
        @bind = $self->_fix_bind_params(@bind);
        $self->debugobj->query_end( $sql, @bind );
    }
}

sub _dbh_execute {
  my ($self, $dbh, $op, $extra_bind, $ident, $bind_attributes, @args) = @_;
  
  if( blessed($ident) && $ident->isa("DBIx::Class::ResultSource") ) {
    $ident = $ident->from();
  }

  my ($sql, $bind) = $self->_prep_for_execute($op, $extra_bind, $ident, \@args);

  $self->_query_start( $sql, @$bind );

  my $sth = $self->sth($sql,$op);

  my $placeholder_index = 1; 

  foreach my $bound (@$bind) {
    my $attributes = {};
    my($column_name, @data) = @$bound;

    if ($bind_attributes) {
      $attributes = $bind_attributes->{$column_name}
      if defined $bind_attributes->{$column_name};
    }

    foreach my $data (@data) {
      $data = ref $data ? ''.$data : $data; # stringify args

      $sth->bind_param($placeholder_index, $data, $attributes);
      $placeholder_index++;
    }
  }

  # Can this fail without throwing an exception anyways???
  my $rv = $sth->execute();
  $self->throw_exception($sth->errstr) if !$rv;

  $self->_query_end( $sql, @$bind );

  return (wantarray ? ($rv, $sth, @$bind) : $rv);
}

sub _execute {
    my $self = shift;
    $self->dbh_do($self->can('_dbh_execute'), @_)
}

sub insert {
  my ($self, $source, $to_insert) = @_;
  
  my $ident = $source->from; 
  my $bind_attributes = $self->source_bind_attributes($source);

  $self->_execute('insert' => [], $source, $bind_attributes, $to_insert);

  return $to_insert;
}

## Still not quite perfect, and EXPERIMENTAL
## Currently it is assumed that all values passed will be "normal", i.e. not 
## scalar refs, or at least, all the same type as the first set, the statement is
## only prepped once.
sub insert_bulk {
  my ($self, $source, $cols, $data) = @_;
  my %colvalues;
  my $table = $source->from;
  @colvalues{@$cols} = (0..$#$cols);
  my ($sql, @bind) = $self->sql_maker->insert($table, \%colvalues);
  
  $self->_query_start( $sql, @bind );
  my $sth = $self->sth($sql);

#  @bind = map { ref $_ ? ''.$_ : $_ } @bind; # stringify args

  ## This must be an arrayref, else nothing works!
  
  my $tuple_status = [];
  
  ##use Data::Dumper;
  ##print STDERR Dumper( $data, $sql, [@bind] );

  my $time = time();

  ## Get the bind_attributes, if any exist
  my $bind_attributes = $self->source_bind_attributes($source);

  ## Bind the values and execute
  my $placeholder_index = 1; 

  foreach my $bound (@bind) {

    my $attributes = {};
    my ($column_name, $data_index) = @$bound;

    if( $bind_attributes ) {
      $attributes = $bind_attributes->{$column_name}
      if defined $bind_attributes->{$column_name};
    }

    my @data = map { $_->[$data_index] } @$data;

    $sth->bind_param_array( $placeholder_index, [@data], $attributes );
    $placeholder_index++;
  }
  my $rv = $sth->execute_array({ArrayTupleStatus => $tuple_status});
  $self->throw_exception($sth->errstr) if !$rv;

  $self->_query_end( $sql, @bind );
  return (wantarray ? ($rv, $sth, @bind) : $rv);
}

sub update {
  my $self = shift @_;
  my $source = shift @_;
  my $bind_attributes = $self->source_bind_attributes($source);
  
  return $self->_execute('update' => [], $source, $bind_attributes, @_);
}


sub delete {
  my $self = shift @_;
  my $source = shift @_;
  
  my $bind_attrs = {}; ## If ever it's needed...
  
  return $self->_execute('delete' => [], $source, $bind_attrs, @_);
}

sub _select {
  my ($self, $ident, $select, $condition, $attrs) = @_;
  my $order = $attrs->{order_by};

  if (ref $condition eq 'SCALAR') {
    $order = $1 if $$condition =~ s/ORDER BY (.*)$//i;
  }

  my $for = delete $attrs->{for};
  my $sql_maker = $self->sql_maker;
  local $sql_maker->{for} = $for;

  if (exists $attrs->{group_by} || $attrs->{having}) {
    $order = {
      group_by => $attrs->{group_by},
      having => $attrs->{having},
      ($order ? (order_by => $order) : ())
    };
  }
  my $bind_attrs = {}; ## Future support
  my @args = ('select', $attrs->{bind}, $ident, $bind_attrs, $select, $condition, $order);
  if ($attrs->{software_limit} ||
      $self->sql_maker->_default_limit_syntax eq "GenericSubQ") {
        $attrs->{software_limit} = 1;
  } else {
    $self->throw_exception("rows attribute must be positive if present")
      if (defined($attrs->{rows}) && !($attrs->{rows} > 0));
    push @args, $attrs->{rows}, $attrs->{offset};
  }

  return $self->_execute(@args);
}

sub source_bind_attributes {
  my ($self, $source) = @_;
  
  my $bind_attributes;
  foreach my $column ($source->columns) {
  
    my $data_type = $source->column_info($column)->{data_type} || '';
    $bind_attributes->{$column} = $self->bind_attribute_by_data_type($data_type)
     if $data_type;
  }

  return $bind_attributes;
}

=head2 select

=over 4

=item Arguments: $ident, $select, $condition, $attrs

=back

Handle a SQL select statement.

=cut

sub select {
  my $self = shift;
  my ($ident, $select, $condition, $attrs) = @_;
  return $self->cursor_class->new($self, \@_, $attrs);
}

sub select_single {
  my $self = shift;
  my ($rv, $sth, @bind) = $self->_select(@_);
  my @row = $sth->fetchrow_array;
  # Need to call finish() to work round broken DBDs
  $sth->finish();
  return @row;
}

=head2 sth

=over 4

=item Arguments: $sql

=back

Returns a L<DBI> sth (statement handle) for the supplied SQL.

=cut

sub _dbh_sth {
  my ($self, $dbh, $sql) = @_;

  # 3 is the if_active parameter which avoids active sth re-use
  my $sth = $self->disable_sth_caching
    ? $dbh->prepare($sql)
    : $dbh->prepare_cached($sql, {}, 3);

  # XXX You would think RaiseError would make this impossible,
  #  but apparently that's not true :(
  $self->throw_exception($dbh->errstr) if !$sth;

  $sth;
}

sub sth {
  my ($self, $sql) = @_;
  $self->dbh_do($self->can('_dbh_sth'), $sql);
}

sub _dbh_columns_info_for {
  my ($self, $dbh, $table) = @_;

  if ($dbh->can('column_info')) {
    my %result;
    eval {
      my ($schema,$tab) = $table =~ /^(.+?)\.(.+)$/ ? ($1,$2) : (undef,$table);
      my $sth = $dbh->column_info( undef,$schema, $tab, '%' );
      $sth->execute();
      while ( my $info = $sth->fetchrow_hashref() ){
        my %column_info;
        $column_info{data_type}   = $info->{TYPE_NAME};
        $column_info{size}      = $info->{COLUMN_SIZE};
        $column_info{is_nullable}   = $info->{NULLABLE} ? 1 : 0;
        $column_info{default_value} = $info->{COLUMN_DEF};
        my $col_name = $info->{COLUMN_NAME};
        $col_name =~ s/^\"(.*)\"$/$1/;

        $result{$col_name} = \%column_info;
      }
    };
    return \%result if !$@ && scalar keys %result;
  }

  my %result;
  my $sth = $dbh->prepare($self->sql_maker->select($table, undef, \'1 = 0'));
  $sth->execute;
  my @columns = @{$sth->{NAME_lc}};
  for my $i ( 0 .. $#columns ){
    my %column_info;
    $column_info{data_type} = $sth->{TYPE}->[$i];
    $column_info{size} = $sth->{PRECISION}->[$i];
    $column_info{is_nullable} = $sth->{NULLABLE}->[$i] ? 1 : 0;

    if ($column_info{data_type} =~ m/^(.*?)\((.*?)\)$/) {
      $column_info{data_type} = $1;
      $column_info{size}    = $2;
    }

    $result{$columns[$i]} = \%column_info;
  }
  $sth->finish;

  foreach my $col (keys %result) {
    my $colinfo = $result{$col};
    my $type_num = $colinfo->{data_type};
    my $type_name;
    if(defined $type_num && $dbh->can('type_info')) {
      my $type_info = $dbh->type_info($type_num);
      $type_name = $type_info->{TYPE_NAME} if $type_info;
      $colinfo->{data_type} = $type_name if $type_name;
    }
  }

  return \%result;
}

sub columns_info_for {
  my ($self, $table) = @_;
  $self->dbh_do($self->can('_dbh_columns_info_for'), $table);
}

=head2 last_insert_id

Return the row id of the last insert.

=cut

sub _dbh_last_insert_id {
    my ($self, $dbh, $source, $col) = @_;
    # XXX This is a SQLite-ism as a default... is there a DBI-generic way?
    $dbh->func('last_insert_rowid');
}

sub last_insert_id {
  my $self = shift;
  $self->dbh_do($self->can('_dbh_last_insert_id'), @_);
}

=head2 sqlt_type

Returns the database driver name.

=cut

sub sqlt_type { shift->dbh->{Driver}->{Name} }

=head2 bind_attribute_by_data_type

Given a datatype from column info, returns a database specific bind attribute for
$dbh->bind_param($val,$attribute) or nothing if we will let the database planner
just handle it.

Generally only needed for special case column types, like bytea in postgres.

=cut

sub bind_attribute_by_data_type {
    return;
}

=head2 create_ddl_dir

=over 4

=item Arguments: $schema \@databases, $version, $directory, $preversion, $sqlt_args

=back

Creates a SQL file based on the Schema, for each of the specified
database types, in the given directory.

=cut

sub create_ddl_dir
{
  my ($self, $schema, $databases, $version, $dir, $preversion, $sqltargs) = @_;

  if(!$dir || !-d $dir)
  {
    warn "No directory given, using ./\n";
    $dir = "./";
  }
  $databases ||= ['MySQL', 'SQLite', 'PostgreSQL'];
  $databases = [ $databases ] if(ref($databases) ne 'ARRAY');
  $version ||= $schema->VERSION || '1.x';
  $sqltargs = { ( add_drop_table => 1 ), %{$sqltargs || {}} };

  $self->throw_exception(q{Can't create a ddl file without SQL::Translator 0.08: '}
      . $self->_check_sqlt_message . q{'})
          if !$self->_check_sqlt_version;

  my $sqlt = SQL::Translator->new({
#      debug => 1,
      add_drop_table => 1,
  });
  foreach my $db (@$databases)
  {
    $sqlt->reset();
    $sqlt->parser('SQL::Translator::Parser::DBIx::Class');
#    $sqlt->parser_args({'DBIx::Class' => $schema);
    $sqlt = $self->configure_sqlt($sqlt, $db);
    $sqlt->data($schema);
    $sqlt->producer($db);

    my $file;
    my $filename = $schema->ddl_filename($db, $dir, $version);
    if(-e $filename)
    {
      warn("$filename already exists, skipping $db");
      next;
    }

    my $output = $sqlt->translate;
    if(!$output)
    {
      warn("Failed to translate to $db, skipping. (" . $sqlt->error . ")");
      next;
    }
    if(!open($file, ">$filename"))
    {
        $self->throw_exception("Can't open $filename for writing ($!)");
        next;
    }
    print $file $output;
    close($file);

    if($preversion)
    {
      require SQL::Translator::Diff;

      my $prefilename = $schema->ddl_filename($db, $dir, $preversion);
#      print "Previous version $prefilename\n";
      if(!-e $prefilename)
      {
        warn("No previous schema file found ($prefilename)");
        next;
      }
      #### We need to reparse the SQLite file we just wrote, so that 
      ##   Diff doesnt get all confoosed, and Diff is *very* confused.
      ##   FIXME: rip Diff to pieces!
#      my $target_schema = $sqlt->schema;
#      unless ( $target_schema->name ) {
#        $target_schema->name( $filename );
#      }
      my @input;
      push @input, {file => $prefilename, parser => $db};
      push @input, {file => $filename, parser => $db};
      my ( $source_schema, $source_db, $target_schema, $target_db ) = map {
        my $file   = $_->{'file'};
        my $parser = $_->{'parser'};

        my $t = SQL::Translator->new;
        $t->debug( 0 );
        $t->trace( 0 );
        $t->parser( $parser )            or die $t->error;
        my $out = $t->translate( $file ) or die $t->error;
        my $schema = $t->schema;
        unless ( $schema->name ) {
          $schema->name( $file );
        }
        ($schema, $parser);
      } @input;

      my $diff = SQL::Translator::Diff::schema_diff($source_schema, $db,
                                                    $target_schema, $db,
                                                    {}
                                                   );
      my $difffile = $schema->ddl_filename($db, $dir, $version, $preversion);
      print STDERR "Diff: $difffile: $db, $dir, $version, $preversion \n";
      if(-e $difffile)
      {
        warn("$difffile already exists, skipping");
        next;
      }
      if(!open $file, ">$difffile")
      { 
        $self->throw_exception("Can't write to $difffile ($!)");
        next;
      }
      print $file $diff;
      close($file);
    }
  }
}

sub configure_sqlt() {
  my $self = shift;
  my $tr = shift;
  my $db = shift || $self->sqlt_type;
  if ($db eq 'PostgreSQL') {
    $tr->quote_table_names(0);
    $tr->quote_field_names(0);
  }
  return $tr;
}

=head2 deployment_statements

=over 4

=item Arguments: $schema, $type, $version, $directory, $sqlt_args

=back

Returns the statements used by L</deploy> and L<DBIx::Class::Schema/deploy>.
The database driver name is given by C<$type>, though the value from
L</sqlt_type> is used if it is not specified.

C<$directory> is used to return statements from files in a previously created
L</create_ddl_dir> directory and is optional. The filenames are constructed
from L<DBIx::Class::Schema/ddl_filename>, the schema name and the C<$version>.

If no C<$directory> is specified then the statements are constructed on the
fly using L<SQL::Translator> and C<$version> is ignored.

See L<SQL::Translator/METHODS> for a list of values for C<$sqlt_args>.

=cut

sub deployment_statements {
  my ($self, $schema, $type, $version, $dir, $sqltargs) = @_;
  # Need to be connected to get the correct sqlt_type
  $self->ensure_connected() unless $type;
  $type ||= $self->sqlt_type;
  $version ||= $schema->VERSION || '1.x';
  $dir ||= './';
  my $filename = $schema->ddl_filename($type, $dir, $version);
  if(-f $filename)
  {
      my $file;
      open($file, "<$filename") 
        or $self->throw_exception("Can't open $filename ($!)");
      my @rows = <$file>;
      close($file);
      return join('', @rows);
  }

  $self->throw_exception(q{Can't deploy without SQL::Translator 0.08: '}
      . $self->_check_sqlt_message . q{'})
          if !$self->_check_sqlt_version;

  require SQL::Translator::Parser::DBIx::Class;
  eval qq{use SQL::Translator::Producer::${type}};
  $self->throw_exception($@) if $@;

  # sources needs to be a parser arg, but for simplicty allow at top level 
  # coming in
  $sqltargs->{parser_args}{sources} = delete $sqltargs->{sources}
      if exists $sqltargs->{sources};

  my $tr = SQL::Translator->new(%$sqltargs);
  SQL::Translator::Parser::DBIx::Class::parse( $tr, $schema );
  return "SQL::Translator::Producer::${type}"->can('produce')->($tr);

  return;

}

sub deploy {
  my ($self, $schema, $type, $sqltargs, $dir) = @_;
  foreach my $statement ( $self->deployment_statements($schema, $type, undef, $dir, { no_comments => 1, %{ $sqltargs || {} } } ) ) {
    foreach my $line ( split(";\n", $statement)) {
      next if($line =~ /^--/);
      next if(!$line);
#      next if($line =~ /^DROP/m);
      next if($line =~ /^BEGIN TRANSACTION/m);
      next if($line =~ /^COMMIT/m);
      next if $line =~ /^\s+$/; # skip whitespace only
      $self->_query_start($line);
      eval {
        $self->dbh->do($line); # shouldn't be using ->dbh ?
      };
      if ($@) {
        warn qq{$@ (running "${line}")};
      }
      $self->_query_end($line);
    }
  }
}

=head2 datetime_parser

Returns the datetime parser class

=cut

sub datetime_parser {
  my $self = shift;
  return $self->{datetime_parser} ||= do {
    $self->ensure_connected;
    $self->build_datetime_parser(@_);
  };
}

=head2 datetime_parser_type

Defines (returns) the datetime parser class - currently hardwired to
L<DateTime::Format::MySQL>

=cut

sub datetime_parser_type { "DateTime::Format::MySQL"; }

=head2 build_datetime_parser

See L</datetime_parser>

=cut

sub build_datetime_parser {
  my $self = shift;
  my $type = $self->datetime_parser_type(@_);
  eval "use ${type}";
  $self->throw_exception("Couldn't load ${type}: $@") if $@;
  return $type;
}

{
    my $_check_sqlt_version; # private
    my $_check_sqlt_message; # private
    sub _check_sqlt_version {
        return $_check_sqlt_version if defined $_check_sqlt_version;
        eval 'use SQL::Translator 0.08';
        $_check_sqlt_message = $@ ? $@ : '';
        $_check_sqlt_version = $@ ? 0 : 1;
    }

    sub _check_sqlt_message {
        _check_sqlt_version if !defined $_check_sqlt_message;
        $_check_sqlt_message;
    }
}

sub DESTROY {
  my $self = shift;
  return if !$self->_dbh;
  $self->_verify_pid;
  $self->_dbh(undef);
}

1;

=head1 SQL METHODS

The module defines a set of methods within the DBIC::SQL::Abstract
namespace.  These build on L<SQL::Abstract::Limit> to provide the
SQL query functions.

The following methods are extended:-

=over 4

=item delete

=item insert

=item select

=item update

=item limit_dialect

See L</connect_info> for details.
For setting, this method is deprecated in favor of L</connect_info>.

=item quote_char

See L</connect_info> for details.
For setting, this method is deprecated in favor of L</connect_info>.

=item name_sep

See L</connect_info> for details.
For setting, this method is deprecated in favor of L</connect_info>.

=back

=head1 AUTHORS

Matt S. Trout <mst@shadowcatsystems.co.uk>

Andy Grundman <andy@hybridized.org>

=head1 LICENSE

You may distribute this code under the same terms as Perl itself.

=cut
