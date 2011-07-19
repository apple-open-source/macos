#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

use SQL::Abstract::Test import => ['is_same_sql_bind'];

use SQL::Abstract;

my $sql = SQL::Abstract->new;

my (@tests, $sub_stmt, @sub_bind, $where);

#1
($sub_stmt, @sub_bind) = ("SELECT c1 FROM t1 WHERE c2 < ? AND c3 LIKE ?",
                          100, "foo%");
$where = {
    foo => 1234,
    bar => \["IN ($sub_stmt)" => @sub_bind],
  };
push @tests, {
  where => $where,
  stmt => " WHERE ( bar IN (SELECT c1 FROM t1 WHERE c2 < ? AND c3 LIKE ?) AND foo = ? )",
  bind => [100, "foo%", 1234],
};

#2
($sub_stmt, @sub_bind)
     = $sql->select("t1", "c1", {c2 => {"<" => 100}, 
                                 c3 => {-like => "foo%"}});
$where = {
    foo => 1234,
    bar => \["> ALL ($sub_stmt)" => @sub_bind],
  };
push @tests, {
  where => $where,
  stmt => " WHERE ( bar > ALL (SELECT c1 FROM t1 WHERE (( c2 < ? AND c3 LIKE ? )) ) AND foo = ? )",
  bind => [100, "foo%", 1234],
};

#3
($sub_stmt, @sub_bind) 
     = $sql->select("t1", "*", {c1 => 1, c2 => \"> t0.c0"});
$where = {
    foo                  => 1234,
    -nest => \["EXISTS ($sub_stmt)" => @sub_bind],
  };
push @tests, {
  where => $where,
  stmt => " WHERE ( EXISTS (SELECT * FROM t1 WHERE ( c1 = ? AND c2 > t0.c0 )) AND foo = ? )",
  bind => [1, 1234],
};

#4
$where = {
    -nest => \["MATCH (col1, col2) AGAINST (?)" => "apples"],
  };
push @tests, {
  where => $where,
  stmt => " WHERE ( MATCH (col1, col2) AGAINST (?) )",
  bind => ["apples"],
};


#5
($sub_stmt, @sub_bind) 
  = $sql->where({age => [{"<" => 10}, {">" => 20}]});
$sub_stmt =~ s/^ where //i; # don't want "WHERE" in the subclause
$where = {
    lname  => {-like => '%son%'},
    -nest  => \["NOT ( $sub_stmt )" => @sub_bind],
  };
push @tests, {
  where => $where,
  stmt => " WHERE ( NOT ( ( ( ( age < ? ) OR ( age > ? ) ) ) ) AND lname LIKE ? )",
  bind => [10, 20, '%son%'],
};

#6
($sub_stmt, @sub_bind) = ("SELECT c1 FROM t1 WHERE c2 < ? AND c3 LIKE ?",
                          100, "foo%");
$where = {
    foo => 1234,
    bar => { -in => \[$sub_stmt => @sub_bind] },
  };
push @tests, {
  where => $where,
  stmt => " WHERE ( bar IN (SELECT c1 FROM t1 WHERE c2 < ? AND c3 LIKE ?) AND foo = ? )",
  bind => [100, "foo%", 1234],
};


plan tests => scalar(@tests);

for (@tests) {

  my($stmt, @bind) = $sql->where($_->{where}, $_->{order});
  is_same_sql_bind($stmt, \@bind, $_->{stmt}, $_->{bind});
}





