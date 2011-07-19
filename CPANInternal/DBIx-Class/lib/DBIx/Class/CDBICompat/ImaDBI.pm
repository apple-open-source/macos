package # hide from PAUSE
    DBIx::Class::CDBICompat::ImaDBI;

use strict;
use warnings;
use DBIx::ContextualFetch;
use Sub::Name ();

use base qw(Class::Data::Inheritable);

__PACKAGE__->mk_classdata('sql_transformer_class' =>
                          'DBIx::Class::CDBICompat::SQLTransformer');

__PACKAGE__->mk_classdata('_transform_sql_handler_order'
                            => [ qw/TABLE ESSENTIAL JOIN IDENTIFIER/ ] );

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
        $class = $data ? $self->{_classes}{$data} : $class;
        return join(', ', $class->columns('Essential'));
      },
    'IDENTIFIER' =>
      sub {
        my ($self, $class, $data) = @_;
        $class = $data ? $self->{_classes}{$data} : $class;
        return join ' AND ', map  "$_ = ?", $class->primary_columns;
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
          $from_class->result_source_instance->_resolve_condition(
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
  my $sql_name = "sql_${name}";
  my $full_sql_name = join '::', $class, $sql_name;
  *$full_sql_name = Sub::Name::subname $full_sql_name,
    sub {
      my $sql = $sql;
      my $class = shift;
      return $class->storage->sth($class->transform_sql($sql, @_));
    };
  if ($sql =~ /select/i) {
    my $search_name = "search_${name}";
    my $full_search_name = join '::', $class, $search_name;
    *$full_search_name = Sub::Name::subname $full_search_name,
      sub {
        my ($class, @args) = @_;
        my $sth = $class->$sql_name;
        return $class->sth_to_objects($sth, \@args);
      };
  }
}

sub sth_to_objects {
  my ($class, $sth, $execute_args) = @_;

  $sth->execute(@$execute_args);

  my @ret;
  while (my $row = $sth->fetchrow_hashref) {
    push(@ret, $class->inflate_result($class->result_source_instance, $row));
  }

  return @ret;
}

sub transform_sql {
  my ($class, $sql, @args) = @_;

  my $tclass = $class->sql_transformer_class;
  $class->ensure_class_loaded($tclass);
  my $t = $tclass->new($class, $sql, @args);

  return sprintf($t->sql, $t->args);
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
