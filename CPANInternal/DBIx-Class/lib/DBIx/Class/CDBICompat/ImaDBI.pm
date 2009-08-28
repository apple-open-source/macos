package # hide from PAUSE
    DBIx::Class::CDBICompat::ImaDBI;

use strict;
use warnings;
use DBIx::ContextualFetch;

use base qw/DBIx::Class/;

__PACKAGE__->mk_classdata('_transform_sql_handler_order'
                            => [ qw/TABLE ESSENTIAL JOIN/ ] );

__PACKAGE__->mk_classdata('_transform_sql_handlers' =>
  {
    'TABLE' =>
      sub {
        my ($self, $class, $data) = @_;
        return $class->result_source_instance->name unless $data;
        my ($f_class, $alias) = split(/=/, $data);
        $f_class ||= $class;
        $self->{_classes}{$alias} = $f_class;
        return $f_class->result_source_instance->name." ${alias}";
      },
    'ESSENTIAL' =>
      sub {
        my ($self, $class, $data) = @_;
        return join(' ', $class->columns('Essential')) unless $data;
        return join(' ', $self->{_classes}{$data}->columns('Essential'));
      },
    'JOIN' =>
      sub {
        my ($self, $class, $data) = @_;
        my ($from, $to) = split(/ /, $data);
        my ($from_class, $to_class) = @{$self->{_classes}}{$from, $to};
        my ($rel_obj) = grep { $_->{class} && $_->{class} eq $to_class }
                          map { $from_class->relationship_info($_) }
                            $from_class->relationships;
        unless ($rel_obj) {
          ($from, $to) = ($to, $from);
          ($from_class, $to_class) = ($to_class, $from_class);
          ($rel_obj) = grep { $_->{class} && $_->{class} eq $to_class }
                          map { $from_class->relationship_info($_) }
                            $from_class->relationships;
        }
        $self->throw_exception( "No relationship to JOIN from ${from_class} to ${to_class}" )
          unless $rel_obj;
        my $join = $from_class->storage->sql_maker->_join_condition(
          $from_class->result_source_instance->resolve_condition(
            $rel_obj->{cond}, $to, $from) );
        return $join;
      }
        
  } );

sub db_Main {
  return $_[0]->storage->dbh;
}

sub connection {
  my ($class, @info) = @_;
  $info[3] = { %{ $info[3] || {}} };
  $info[3]->{RootClass} = 'DBIx::ContextualFetch';
  return $class->next::method(@info);
}

sub __driver {
  return $_[0]->storage->dbh->{Driver}->{Name};
}

sub set_sql {
  my ($class, $name, $sql) = @_;
  no strict 'refs';
  *{"${class}::sql_${name}"} =
    sub {
      my $sql = $sql;
      my $class = shift;
      return $class->storage->sth($class->transform_sql($sql, @_));
    };
  if ($sql =~ /select/i) {
    my $meth = "sql_${name}";
    *{"${class}::search_${name}"} =
      sub {
        my ($class, @args) = @_;
        my $sth = $class->$meth;
        $sth->execute(@args);
        return $class->sth_to_objects($sth);
      };
  }
}

sub sth_to_objects {
  my ($class, $sth) = @_;
  my @ret;
  while (my $row = $sth->fetchrow_hashref) {
    push(@ret, $class->inflate_result($class->result_source_instance, $row));
  }
  return @ret;
}

sub transform_sql {
  my ($class, $sql, @args) = @_;
  my $attrs = { };
  foreach my $key (@{$class->_transform_sql_handler_order}) {
    my $h = $class->_transform_sql_handlers->{$key};
    $sql =~ s/__$key(?:\(([^\)]+)\))?__/$h->($attrs, $class, $1)/eg;
  }
  #warn $sql;
  return sprintf($sql, @args);
}

package
  DBIx::ContextualFetch::st; # HIDE FROM PAUSE THIS IS NOT OUR CLASS

no warnings 'redefine';

sub _untaint_execute {
  my $sth = shift;
  my $old_value = $sth->{Taint};
  $sth->{Taint} = 0;
  my $ret;
  {
    no warnings 'uninitialized';
    $ret = $sth->SUPER::execute(@_);
  }
  $sth->{Taint} = $old_value;
  return $ret;
}

1;
