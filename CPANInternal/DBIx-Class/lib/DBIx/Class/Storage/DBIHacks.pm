package   #hide from PAUSE
  DBIx::Class::Storage::DBIHacks;

#
# This module contains code that should never have seen the light of day,
# does not belong in the Storage, or is otherwise unfit for public
# display. The arrival of SQLA2 should immediately oboslete 90% of this
#

use strict;
use warnings;

use base 'DBIx::Class::Storage';
use mro 'c3';

use Carp::Clan qw/^DBIx::Class/;

#
# This code will remove non-selecting/non-restricting joins from
# {from} specs, aiding the RDBMS query optimizer
#
sub _prune_unused_joins {
  my ($self) = shift;

  my ($from, $select, $where, $attrs) = @_;

  if (ref $from ne 'ARRAY' || ref $from->[0] ne 'HASH' || ref $from->[1] ne 'ARRAY') {
    return $from;   # only standard {from} specs are supported
  }

  my $aliastypes = $self->_resolve_aliastypes_from_select_args(@_);

  # a grouped set will not be affected by amount of rows. Thus any
  # {multiplying} joins can go
  delete $aliastypes->{multiplying} if $attrs->{group_by};


  my @newfrom = $from->[0]; # FROM head is always present

  my %need_joins = (map { %{$_||{}} } (values %$aliastypes) );
  for my $j (@{$from}[1..$#$from]) {
    push @newfrom, $j if (
      (! $j->[0]{-alias}) # legacy crap
        ||
      $need_joins{$j->[0]{-alias}}
    );
  }

  return \@newfrom;
}

#
# This is the code producing joined subqueries like:
# SELECT me.*, other.* FROM ( SELECT me.* FROM ... ) JOIN other ON ... 
#
sub _adjust_select_args_for_complex_prefetch {
  my ($self, $from, $select, $where, $attrs) = @_;

  $self->throw_exception ('Nothing to prefetch... how did we get here?!')
    if not @{$attrs->{_prefetch_select}};

  $self->throw_exception ('Complex prefetches are not supported on resultsets with a custom from attribute')
    if (ref $from ne 'ARRAY' || ref $from->[0] ne 'HASH' || ref $from->[1] ne 'ARRAY');


  # generate inner/outer attribute lists, remove stuff that doesn't apply
  my $outer_attrs = { %$attrs };
  delete $outer_attrs->{$_} for qw/where bind rows offset group_by having/;

  my $inner_attrs = { %$attrs };
  delete $inner_attrs->{$_} for qw/for collapse _prefetch_select _collapse_order_by select as/;


  # bring over all non-collapse-induced order_by into the inner query (if any)
  # the outer one will have to keep them all
  delete $inner_attrs->{order_by};
  if (my $ord_cnt = @{$outer_attrs->{order_by}} - @{$outer_attrs->{_collapse_order_by}} ) {
    $inner_attrs->{order_by} = [
      @{$outer_attrs->{order_by}}[ 0 .. $ord_cnt - 1]
    ];
  }

  # generate the inner/outer select lists
  # for inside we consider only stuff *not* brought in by the prefetch
  # on the outside we substitute any function for its alias
  my $outer_select = [ @$select ];
  my $inner_select = [];
  for my $i (0 .. ( @$outer_select - @{$outer_attrs->{_prefetch_select}} - 1) ) {
    my $sel = $outer_select->[$i];

    if (ref $sel eq 'HASH' ) {
      $sel->{-as} ||= $attrs->{as}[$i];
      $outer_select->[$i] = join ('.', $attrs->{alias}, ($sel->{-as} || "inner_column_$i") );
    }

    push @$inner_select, $sel;
  }

  # construct the inner $from for the subquery
  # we need to prune first, because this will determine if we need a group_by below
  my $inner_from = $self->_prune_unused_joins ($from, $inner_select, $where, $inner_attrs);

  # if a multi-type join was needed in the subquery - add a group_by to simulate the
  # collapse in the subq
  $inner_attrs->{group_by} ||= $inner_select
    if List::Util::first
      { ! $_->[0]{-is_single} }
      (@{$inner_from}[1 .. $#$inner_from])
  ;

  # generate the subquery
  my $subq = $self->_select_args_to_query (
    $inner_from,
    $inner_select,
    $where,
    $inner_attrs,
  );

  my $subq_joinspec = {
    -alias => $attrs->{alias},
    -source_handle => $inner_from->[0]{-source_handle},
    $attrs->{alias} => $subq,
  };

  # Generate the outer from - this is relatively easy (really just replace
  # the join slot with the subquery), with a major caveat - we can not
  # join anything that is non-selecting (not part of the prefetch), but at
  # the same time is a multi-type relationship, as it will explode the result.
  #
  # There are two possibilities here
  # - either the join is non-restricting, in which case we simply throw it away
  # - it is part of the restrictions, in which case we need to collapse the outer
  #   result by tackling yet another group_by to the outside of the query

  # normalize a copy of $from, so it will be easier to work with further
  # down (i.e. promote the initial hashref to an AoH)
  $from = [ @$from ];
  $from->[0] = [ $from->[0] ];

  # so first generate the outer_from, up to the substitution point
  my @outer_from;
  while (my $j = shift @$from) {
    if ($j->[0]{-alias} eq $attrs->{alias}) { # time to swap
      push @outer_from, [
        $subq_joinspec,
        @{$j}[1 .. $#$j],
      ];
      last; # we'll take care of what's left in $from below
    }
    else {
      push @outer_from, $j;
    }
  }

  # scan the from spec against different attributes, and see which joins are needed
  # in what role
  my $outer_aliastypes =
    $self->_resolve_aliastypes_from_select_args( $from, $outer_select, $where, $outer_attrs );

  # see what's left - throw away if not selecting/restricting
  # also throw in a group_by if restricting to guard against
  # cross-join explosions
  #
  while (my $j = shift @$from) {
    my $alias = $j->[0]{-alias};

    if ($outer_aliastypes->{select}{$alias}) {
      push @outer_from, $j;
    }
    elsif ($outer_aliastypes->{restrict}{$alias}) {
      push @outer_from, $j;
      $outer_attrs->{group_by} ||= $outer_select unless $j->[0]{-is_single};
    }
  }

  # demote the outer_from head
  $outer_from[0] = $outer_from[0][0];

  # This is totally horrific - the $where ends up in both the inner and outer query
  # Unfortunately not much can be done until SQLA2 introspection arrives, and even
  # then if where conditions apply to the *right* side of the prefetch, you may have
  # to both filter the inner select (e.g. to apply a limit) and then have to re-filter
  # the outer select to exclude joins you didin't want in the first place
  #
  # OTOH it can be seen as a plus: <ash> (notes that this query would make a DBA cry ;)
  return (\@outer_from, $outer_select, $where, $outer_attrs);
}

# Due to a lack of SQLA2 we fall back to crude scans of all the
# select/where/order/group attributes, in order to determine what
# aliases are neded to fulfill the query. This information is used
# throughout the code to prune unnecessary JOINs from the queries
# in an attempt to reduce the execution time.
# Although the method is pretty horrific, the worst thing that can
# happen is for it to fail due to an unqualified column, which in
# turn will result in a vocal exception. Qualifying the column will
# invariably solve the problem.
sub _resolve_aliastypes_from_select_args {
  my ( $self, $from, $select, $where, $attrs ) = @_;

  $self->throw_exception ('Unable to analyze custom {from}')
    if ref $from ne 'ARRAY';

  # what we will return
  my $aliases_by_type;

  # see what aliases are there to work with
  my $alias_list;
  for (@$from) {
    my $j = $_;
    $j = $j->[0] if ref $j eq 'ARRAY';
    my $al = $j->{-alias}
      or next;

    $alias_list->{$al} = $j;
    $aliases_by_type->{multiplying}{$al} = 1
      unless $j->{-is_single};
  }

  # set up a botched SQLA
  my $sql_maker = $self->sql_maker;
  my $sep = quotemeta ($self->_sql_maker_opts->{name_sep} || '.');
  local $sql_maker->{quote_char}; # so that we can regex away


  my $select_sql = $sql_maker->_recurse_fields ($select);
  my $where_sql = $sql_maker->where ($where);
  my $group_by_sql = $sql_maker->_order_by({
    map { $_ => $attrs->{$_} } qw/group_by having/
  });
  my @order_by_chunks = ($self->_parse_order_by ($attrs->{order_by}) );

  # match every alias to the sql chunks above
  for my $alias (keys %$alias_list) {
    my $al_re = qr/\b $alias $sep/x;

    for my $piece ($where_sql, $group_by_sql) {
      $aliases_by_type->{restrict}{$alias} = 1 if ($piece =~ $al_re);
    }

    for my $piece ($select_sql, @order_by_chunks ) {
      $aliases_by_type->{select}{$alias} = 1 if ($piece =~ $al_re);
    }
  }

  # Add any non-left joins to the restriction list (such joins are indeed restrictions)
  for my $j (values %$alias_list) {
    my $alias = $j->{-alias} or next;
    $aliases_by_type->{restrict}{$alias} = 1 if (
      (not $j->{-join_type})
        or
      ($j->{-join_type} !~ /^left (?: \s+ outer)? $/xi)
    );
  }

  # mark all join parents as mentioned
  # (e.g.  join => { cds => 'tracks' } - tracks will need to bring cds too )
  for my $type (keys %$aliases_by_type) {
    for my $alias (keys %{$aliases_by_type->{$type}}) {
      $aliases_by_type->{$type}{$_} = 1
        for (map { keys %$_ } @{ $alias_list->{$alias}{-join_path} || [] });
    }
  }

  return $aliases_by_type;
}

sub _resolve_ident_sources {
  my ($self, $ident) = @_;

  my $alias2source = {};
  my $rs_alias;

  # the reason this is so contrived is that $ident may be a {from}
  # structure, specifying multiple tables to join
  if ( Scalar::Util::blessed($ident) && $ident->isa("DBIx::Class::ResultSource") ) {
    # this is compat mode for insert/update/delete which do not deal with aliases
    $alias2source->{me} = $ident;
    $rs_alias = 'me';
  }
  elsif (ref $ident eq 'ARRAY') {

    for (@$ident) {
      my $tabinfo;
      if (ref $_ eq 'HASH') {
        $tabinfo = $_;
        $rs_alias = $tabinfo->{-alias};
      }
      if (ref $_ eq 'ARRAY' and ref $_->[0] eq 'HASH') {
        $tabinfo = $_->[0];
      }

      $alias2source->{$tabinfo->{-alias}} = $tabinfo->{-source_handle}->resolve
        if ($tabinfo->{-source_handle});
    }
  }

  return ($alias2source, $rs_alias);
}

# Takes $ident, \@column_names
#
# returns { $column_name => \%column_info, ... }
# also note: this adds -result_source => $rsrc to the column info
#
# If no columns_names are supplied returns info about *all* columns
# for all sources
sub _resolve_column_info {
  my ($self, $ident, $colnames) = @_;
  my ($alias2src, $root_alias) = $self->_resolve_ident_sources($ident);

  my $sep = $self->_sql_maker_opts->{name_sep} || '.';
  my $qsep = quotemeta $sep;

  my (%return, %seen_cols, @auto_colnames);

  # compile a global list of column names, to be able to properly
  # disambiguate unqualified column names (if at all possible)
  for my $alias (keys %$alias2src) {
    my $rsrc = $alias2src->{$alias};
    for my $colname ($rsrc->columns) {
      push @{$seen_cols{$colname}}, $alias;
      push @auto_colnames, "$alias$sep$colname" unless $colnames;
    }
  }

  $colnames ||= [
    @auto_colnames,
    grep { @{$seen_cols{$_}} == 1 } (keys %seen_cols),
  ];

  COLUMN:
  foreach my $col (@$colnames) {
    my ($alias, $colname) = $col =~ m/^ (?: ([^$qsep]+) $qsep)? (.+) $/x;

    unless ($alias) {
      # see if the column was seen exactly once (so we know which rsrc it came from)
      if ($seen_cols{$colname} and @{$seen_cols{$colname}} == 1) {
        $alias = $seen_cols{$colname}[0];
      }
      else {
        next COLUMN;
      }
    }

    my $rsrc = $alias2src->{$alias};
    $return{$col} = $rsrc && {
      %{$rsrc->column_info($colname)},
      -result_source => $rsrc,
      -source_alias => $alias,
    };
  }

  return \%return;
}

# The DBIC relationship chaining implementation is pretty simple - every
# new related_relationship is pushed onto the {from} stack, and the {select}
# window simply slides further in. This means that when we count somewhere
# in the middle, we got to make sure that everything in the join chain is an
# actual inner join, otherwise the count will come back with unpredictable
# results (a resultset may be generated with _some_ rows regardless of if
# the relation which the $rs currently selects has rows or not). E.g.
# $artist_rs->cds->count - normally generates:
# SELECT COUNT( * ) FROM artist me LEFT JOIN cd cds ON cds.artist = me.artistid
# which actually returns the number of artists * (number of cds || 1)
#
# So what we do here is crawl {from}, determine if the current alias is at
# the top of the stack, and if not - make sure the chain is inner-joined down
# to the root.
#
sub _straight_join_to_node {
  my ($self, $from, $alias) = @_;

  # subqueries and other oddness are naturally not supported
  return $from if (
    ref $from ne 'ARRAY'
      ||
    @$from <= 1
      ||
    ref $from->[0] ne 'HASH'
      ||
    ! $from->[0]{-alias}
      ||
    $from->[0]{-alias} eq $alias  # this last bit means $alias is the head of $from - nothing to do
  );

  # find the current $alias in the $from structure
  my $switch_branch;
  JOINSCAN:
  for my $j (@{$from}[1 .. $#$from]) {
    if ($j->[0]{-alias} eq $alias) {
      $switch_branch = $j->[0]{-join_path};
      last JOINSCAN;
    }
  }

  # something else went quite wrong
  return $from unless $switch_branch;

  # So it looks like we will have to switch some stuff around.
  # local() is useless here as we will be leaving the scope
  # anyway, and deep cloning is just too fucking expensive
  # So replace the first hashref in the node arrayref manually 
  my @new_from = ($from->[0]);
  my $sw_idx = { map { values %$_ => 1 } @$switch_branch };

  for my $j (@{$from}[1 .. $#$from]) {
    my $jalias = $j->[0]{-alias};

    if ($sw_idx->{$jalias}) {
      my %attrs = %{$j->[0]};
      delete $attrs{-join_type};
      push @new_from, [
        \%attrs,
        @{$j}[ 1 .. $#$j ],
      ];
    }
    else {
      push @new_from, $j;
    }
  }

  return \@new_from;
}

# Most databases do not allow aliasing of tables in UPDATE/DELETE. Thus
# a condition containing 'me' or other table prefixes will not work
# at all. What this code tries to do (badly) is introspect the condition
# and remove all column qualifiers. If it bails out early (returns undef)
# the calling code should try another approach (e.g. a subquery)
sub _strip_cond_qualifiers {
  my ($self, $where) = @_;

  my $cond = {};

  # No-op. No condition, we're updating/deleting everything
  return $cond unless $where;

  if (ref $where eq 'ARRAY') {
    $cond = [
      map {
        my %hash;
        foreach my $key (keys %{$_}) {
          $key =~ /([^.]+)$/;
          $hash{$1} = $_->{$key};
        }
        \%hash;
      } @$where
    ];
  }
  elsif (ref $where eq 'HASH') {
    if ( (keys %$where) == 1 && ( (keys %{$where})[0] eq '-and' )) {
      $cond->{-and} = [];
      my @cond = @{$where->{-and}};
       for (my $i = 0; $i < @cond; $i++) {
        my $entry = $cond[$i];
        my $hash;
        my $ref = ref $entry;
        if ($ref eq 'HASH' or $ref eq 'ARRAY') {
          $hash = $self->_strip_cond_qualifiers($entry);
        }
        elsif (! $ref) {
          $entry =~ /([^.]+)$/;
          $hash->{$1} = $cond[++$i];
        }
        else {
          $self->throw_exception ("_strip_cond_qualifiers() is unable to handle a condition reftype $ref");
        }
        push @{$cond->{-and}}, $hash;
      }
    }
    else {
      foreach my $key (keys %$where) {
        $key =~ /([^.]+)$/;
        $cond->{$1} = $where->{$key};
      }
    }
  }
  else {
    return undef;
  }

  return $cond;
}

sub _parse_order_by {
  my ($self, $order_by) = @_;

  return scalar $self->sql_maker->_order_by_chunks ($order_by)
    unless wantarray;

  my $sql_maker = $self->sql_maker;
  local $sql_maker->{quote_char}; #disable quoting
  my @chunks;
  for my $chunk (map { ref $_ ? @$_ : $_ } ($sql_maker->_order_by_chunks ($order_by) ) ) {
    $chunk =~ s/\s+ (?: ASC|DESC ) \s* $//ix;
    push @chunks, $chunk;
  }

  return @chunks;
}

1;
