#!/usr/bin/perl -I. -w

use strict;
use vars qw($TESTING);
$TESTING = 1;
use Test;

# use a BEGIN block so we print our plan before SQL::Abstract is loaded
BEGIN { plan tests => 60 }

use SQL::Abstract;

my @tests = (
      #1
      {
              func   => 'select',
              args   => ['test', '*'],
              stmt   => 'SELECT * FROM test',
              stmt_q => 'SELECT * FROM `test`',
              bind   => []
      },
      #2
      {
              func   => 'select',
              args   => ['test', [qw(one two three)]],
              stmt   => 'SELECT one, two, three FROM test',
              stmt_q => 'SELECT `one`, `two`, `three` FROM `test`',
              bind   => []
      },
      #3
      {
              func   => 'select',
              args   => ['test', '*', { a => 0 }, [qw/boom bada bing/]],
              stmt   => 'SELECT * FROM test WHERE ( a = ? ) ORDER BY boom, bada, bing',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` = ? ) ORDER BY `boom`, `bada`, `bing`',
              bind   => [0]
      },             
      #4             
      {              
              func   => 'select',
              args   => ['test', '*', [ { a => 5 }, { b => 6 } ]],
              stmt   => 'SELECT * FROM test WHERE ( ( a = ? ) OR ( b = ? ) )',
              stmt_q => 'SELECT * FROM `test` WHERE ( ( `a` = ? ) OR ( `b` = ? ) )',
              bind   => [5,6]
      },             
      #5             
      {              
              func   => 'select',
              args   => ['test', '*', undef, ['id']],
              stmt   => 'SELECT * FROM test ORDER BY id',
              stmt_q => 'SELECT * FROM `test` ORDER BY `id`',
              bind   => []
      },             
      #6             
      {              
              func   => 'select',
              args   => ['test', '*', { a => 'boom' } , ['id']],
              stmt   => 'SELECT * FROM test WHERE ( a = ? ) ORDER BY id',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` = ? ) ORDER BY `id`',
              bind   => ['boom']
      },             
      #7             
      {              
              func   => 'select',
              args   => ['test', '*', { a => ['boom', 'bang'] }],
              stmt   => 'SELECT * FROM test WHERE ( ( ( a = ? ) OR ( a = ? ) ) )',
              stmt_q => 'SELECT * FROM `test` WHERE ( ( ( `a` = ? ) OR ( `a` = ? ) ) )',
              bind   => ['boom', 'bang']
      },             
      #8             
      {              
              func   => 'select',
              args   => [[qw/test1 test2/], '*', { 'test1.a' => { 'In', ['boom', 'bang'] } }],
              stmt   => 'SELECT * FROM test1, test2 WHERE ( test1.a IN ( ?, ? ) )',
              stmt_q => 'SELECT * FROM `test1`, `test2` WHERE ( `test1`.`a` IN ( ?, ? ) )',
              bind   => ['boom', 'bang']
      },             
      #9             
      {              
              func   => 'select',
              args   => ['test', '*', { a => { 'between', ['boom', 'bang'] } }],
              stmt   => 'SELECT * FROM test WHERE ( a BETWEEN ? AND ? )',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` BETWEEN ? AND ? )',
              bind   => ['boom', 'bang']
      },             
      #10            
      {              
              func   => 'select',
              args   => ['test', '*', { a => { '!=', 'boom' } }],
              stmt   => 'SELECT * FROM test WHERE ( a != ? )',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` != ? )',
              bind   => ['boom']
      },             
      #11            
      {              
              func   => 'update',
              args   => ['test', {a => 'boom'}, {a => undef}],
              stmt   => 'UPDATE test SET a = ? WHERE ( a IS NULL )',
              stmt_q => 'UPDATE `test` SET `a` = ? WHERE ( `a` IS NULL )',
              bind   => ['boom']
      },             
      #12            
      {              
              func   => 'update',
              args   => ['test', {a => 'boom'}, { a => {'!=', "bang" }} ],
              stmt   => 'UPDATE test SET a = ? WHERE ( a != ? )',
              stmt_q => 'UPDATE `test` SET `a` = ? WHERE ( `a` != ? )',
              bind   => ['boom', 'bang']
      },             
      #13            
      {              
              func   => 'update',
              args   => ['test', {'a-funny-flavored-candy' => 'yummy', b => 'oops'}, { a42 => "bang" }],
              stmt   => 'UPDATE test SET a-funny-flavored-candy = ?, b = ? WHERE ( a42 = ? )',
              stmt_q => 'UPDATE `test` SET `a-funny-flavored-candy` = ?, `b` = ? WHERE ( `a42` = ? )',
              bind   => ['yummy', 'oops', 'bang']
      },             
      #14            
      {              
              func   => 'delete',
              args   => ['test', {requestor => undef}],
              stmt   => 'DELETE FROM test WHERE ( requestor IS NULL )',
              stmt_q => 'DELETE FROM `test` WHERE ( `requestor` IS NULL )',
              bind   => []
      },             
      #15            
      {              
              func   => 'delete',
              args   => [[qw/test1 test2 test3/],
                         { 'test1.field' => \'!= test2.field',
                            user => {'!=','nwiger'} },
                        ],
              stmt   => 'DELETE FROM test1, test2, test3 WHERE ( test1.field != test2.field AND user != ? )',
              stmt_q => 'DELETE FROM `test1`, `test2`, `test3` WHERE ( `test1`.`field` != test2.field AND `user` != ? )',  # test2.field is a literal value, cannnot be quoted.
              bind   => ['nwiger']
      },             
      #16            
      {              
              func   => 'insert',
              args   => ['test', {a => 1, b => 2, c => 3, d => 4, e => 5}],
              stmt   => 'INSERT INTO test (a, b, c, d, e) VALUES (?, ?, ?, ?, ?)',
              stmt_q => 'INSERT INTO `test` (`a`, `b`, `c`, `d`, `e`) VALUES (?, ?, ?, ?, ?)',
              bind   => [qw/1 2 3 4 5/],
      },             
      #17            
      {              
              func   => 'insert',
              args   => ['test', [qw/1 2 3 4 5/]],
              stmt   => 'INSERT INTO test VALUES (?, ?, ?, ?, ?)',
              stmt_q => 'INSERT INTO `test` VALUES (?, ?, ?, ?, ?)',
              bind   => [qw/1 2 3 4 5/],
      },             
      #18            
      {              
              func   => 'insert',
              args   => ['test', [qw/1 2 3 4 5/, undef]],
              stmt   => 'INSERT INTO test VALUES (?, ?, ?, ?, ?, ?)',
              stmt_q => 'INSERT INTO `test` VALUES (?, ?, ?, ?, ?, ?)',
              bind   => [qw/1 2 3 4 5/, undef],
      },             
      #19            
      {              
              func   => 'update',
              args   => ['test', {a => 1, b => 2, c => 3, d => 4, e => 5}],
              stmt   => 'UPDATE test SET a = ?, b = ?, c = ?, d = ?, e = ?',
              stmt_q => 'UPDATE `test` SET `a` = ?, `b` = ?, `c` = ?, `d` = ?, `e` = ?',
              bind   => [qw/1 2 3 4 5/],
      },             
      #20            
      {              
              func   => 'update',
              args   => ['test', {a => 1, b => 2, c => 3, d => 4, e => 5}, {a => {'in', [1..5]}}],
              stmt   => 'UPDATE test SET a = ?, b = ?, c = ?, d = ?, e = ? WHERE ( a IN ( ?, ?, ?, ?, ? ) )',
              stmt_q => 'UPDATE `test` SET `a` = ?, `b` = ?, `c` = ?, `d` = ?, `e` = ? WHERE ( `a` IN ( ?, ?, ?, ?, ? ) )',
              bind   => [qw/1 2 3 4 5 1 2 3 4 5/],
      },             
      #21            
      {              
              func   => 'update',
              args   => ['test', {a => 1, b => ["to_date(?, 'MM/DD/YY')", '02/02/02']}, {a => {'between', [1,2]}}],
              stmt   => 'UPDATE test SET a = ?, b = to_date(?, \'MM/DD/YY\') WHERE ( a BETWEEN ? AND ? )',
              stmt_q => 'UPDATE `test` SET `a` = ?, `b` = to_date(?, \'MM/DD/YY\') WHERE ( `a` BETWEEN ? AND ? )',
              bind   => [qw(1 02/02/02 1 2)],
      },             
      #22            
      {              
              func   => 'insert',
              args   => ['test.table', {high_limit => \'max(all_limits)', low_limit => 4} ],
              stmt   => 'INSERT INTO test.table (high_limit, low_limit) VALUES (max(all_limits), ?)',
              stmt_q => 'INSERT INTO `test`.`table` (`high_limit`, `low_limit`) VALUES (max(all_limits), ?)',
              bind   => ['4'],
      },             
      #23            
      {              
              func   => 'insert',
              new    => {bindtype => 'columns'},
              args   => ['test.table', {one => 2, three => 4, five => 6} ],
              stmt   => 'INSERT INTO test.table (five, one, three) VALUES (?, ?, ?)',
              stmt_q => 'INSERT INTO `test`.`table` (`five`, `one`, `three`) VALUES (?, ?, ?)',
              bind   => [['five', 6], ['one', 2], ['three', 4]],  # alpha order, man...
      },             
      #24            
      {              
              func   => 'select',
              new    => {bindtype => 'columns', case => 'lower'},
              args   => ['test.table', [qw/one two three/], {one => 2, three => 4, five => 6} ],
              stmt   => 'select one, two, three from test.table where ( five = ? and one = ? and three = ? )',
              stmt_q => 'select `one`, `two`, `three` from `test`.`table` where ( `five` = ? and `one` = ? and `three` = ? )',
              bind   => [['five', 6], ['one', 2], ['three', 4]],  # alpha order, man...
      },             
      #25            
      {              
              func   => 'update',
              new    => {bindtype => 'columns', cmp => 'like'},
              args   => ['testin.table2', {One => 22, Three => 44, FIVE => 66},
                                          {Beer => 'is', Yummy => '%YES%', IT => ['IS','REALLY','GOOD']}],
              stmt   => 'UPDATE testin.table2 SET FIVE = ?, One = ?, Three = ? WHERE '
                       . '( Beer LIKE ? AND ( ( IT LIKE ? ) OR ( IT LIKE ? ) OR ( IT LIKE ? ) ) AND Yummy LIKE ? )',
              stmt_q => 'UPDATE `testin`.`table2` SET `FIVE` = ?, `One` = ?, `Three` = ? WHERE '
                       . '( `Beer` LIKE ? AND ( ( `IT` LIKE ? ) OR ( `IT` LIKE ? ) OR ( `IT` LIKE ? ) ) AND `Yummy` LIKE ? )',
              bind   => [['FIVE', 66], ['One', 22], ['Three', 44], ['Beer','is'],
                         ['IT','IS'], ['IT','REALLY'], ['IT','GOOD'], ['Yummy','%YES%']],
      },             
      #26            
      {              
              func   => 'select',
              args   => ['test', '*', {priority => [ -and => {'!=', 2}, {'!=', 1} ]}],
              stmt   => 'SELECT * FROM test WHERE ( ( ( priority != ? ) AND ( priority != ? ) ) )',
              stmt_q => 'SELECT * FROM `test` WHERE ( ( ( `priority` != ? ) AND ( `priority` != ? ) ) )',
              bind   => [qw(2 1)],
      },             
      #27            
      {              
              func   => 'select',
              args   => ['Yo Momma', '*', { user => 'nwiger', 
                                       -nest => [ workhrs => {'>', 20}, geo => 'ASIA' ] }],
              stmt   => 'SELECT * FROM Yo Momma WHERE ( ( ( workhrs > ? ) OR ( geo = ? ) ) AND user = ? )',
              stmt_q => 'SELECT * FROM `Yo Momma` WHERE ( ( ( `workhrs` > ? ) OR ( `geo` = ? ) ) AND `user` = ? )',
              bind   => [qw(20 ASIA nwiger)],
      },             
      #28            
      {              
              func   => 'update',
              args   => ['taco_punches', { one => 2, three => 4 },
                                         { bland => [ -and => {'!=', 'yes'}, {'!=', 'YES'} ],
                                           tasty => { '!=', [qw(yes YES)] },
                                           -nest => [ face => [ -or => {'=', 'mr.happy'}, {'=', undef} ] ] },
                        ],
              stmt   => 'UPDATE taco_punches SET one = ?, three = ? WHERE ( ( ( ( ( face = ? ) OR ( face IS NULL ) ) ) )'
                      . ' AND ( ( bland != ? ) AND ( bland != ? ) ) AND ( ( tasty != ? ) OR ( tasty != ? ) ) )',
              stmt_q => 'UPDATE `taco_punches` SET `one` = ?, `three` = ? WHERE ( ( ( ( ( `face` = ? ) OR ( `face` IS NULL ) ) ) )'
                      . ' AND ( ( `bland` != ? ) AND ( `bland` != ? ) ) AND ( ( `tasty` != ? ) OR ( `tasty` != ? ) ) )',
              bind   => [qw(2 4 mr.happy yes YES yes YES)],
      },             
      #29            
      {              
              func   => 'select',
              args   => ['jeff', '*', { name => {'like', '%smith%', -not_in => ['Nate','Jim','Bob','Sally']},
                                       -nest => [ -or => [ -and => [age => { -between => [20,30] }, age => {'!=', 25} ],
                                                           yob => {'<', 1976} ] ] } ],
              stmt   => 'SELECT * FROM jeff WHERE ( ( ( ( ( ( ( age BETWEEN ? AND ? ) AND ( age != ? ) ) ) OR ( yob < ? ) ) ) )'
                      . ' AND name NOT IN ( ?, ?, ?, ? ) AND name LIKE ? )',
              stmt_q => 'SELECT * FROM `jeff` WHERE ( ( ( ( ( ( ( `age` BETWEEN ? AND ? ) AND ( `age` != ? ) ) ) OR ( `yob` < ? ) ) ) )'
                      . ' AND `name` NOT IN ( ?, ?, ?, ? ) AND `name` LIKE ? )',
              bind   => [qw(20 30 25 1976 Nate Jim Bob Sally %smith%)]
      },             
      #30            
      {              
              # The "-maybe" should be ignored, as it sits at the top level (bug?)
              func   => 'update',
              args   => ['fhole', {fpoles => 4}, [-maybe => {race => [-and => [qw(black white asian)]]},
                                                            {-nest => {firsttime => [-or => {'=','yes'}, undef]}},
                                                            [ -and => {firstname => {-not_like => 'candace'}}, {lastname => {-in => [qw(jugs canyon towers)]}} ] ] ],
              stmt   => 'UPDATE fhole SET fpoles = ? WHERE ( ( ( ( ( ( ( race = ? ) OR ( race = ? ) OR ( race = ? ) ) ) ) ) )'
                      . ' OR ( ( ( ( firsttime = ? ) OR ( firsttime IS NULL ) ) ) ) OR ( ( ( firstname NOT LIKE ? ) ) AND ( lastname IN ( ?, ?, ? ) ) ) )',
              stmt_q => 'UPDATE `fhole` SET `fpoles` = ? WHERE ( ( ( ( ( ( ( `race` = ? ) OR ( `race` = ? ) OR ( `race` = ? ) ) ) ) ) )'
                      . ' OR ( ( ( ( `firsttime` = ? ) OR ( `firsttime` IS NULL ) ) ) ) OR ( ( ( `firstname` NOT LIKE ? ) ) AND ( `lastname` IN ( ?, ?, ? ) ) ) )',
              bind   => [qw(4 black white asian yes candace jugs canyon towers)]
      },
);

use Data::Dumper;

for (@tests) {
      local $"=', ';

      my $new = $_->{new} || {};
      $new->{debug} = $ENV{DEBUG} || 0;
      my $sql = SQL::Abstract->new(%$new);

      #print "testing with args (@{$_->{args}}): ";
      my $func = $_->{func};
      my($stmt, @bind) = $sql->$func(@{$_->{args}});
      ok($stmt eq $_->{stmt} && equal(\@bind, $_->{bind})) or
              print "got\n",
                    "[$stmt] [",Dumper(\@bind),"]\n",
                    "instead of\n",
                    "[$_->{stmt}] [",Dumper($_->{bind}),"]\n\n";

      # test with quoted labels
      my $sql_q = SQL::Abstract->new(%$new, quote_char => '`', name_sep => '.');

      my $func_q = $_->{func};
      my($stmt_q, @bind_q) = $sql_q->$func_q(@{$_->{args}});
      ok($stmt_q eq $_->{stmt_q} && equal(\@bind_q, $_->{bind})) or
              print "got\n",
                    "[$stmt_q] [",Dumper(\@bind_q),"]\n",
                    "instead of\n",
                    "[$_->{stmt_q}] [",Dumper($_->{bind}),"]\n\n";
}

sub equal {
      my ($a, $b) = @_;
      return 0 if @$a != @$b;
      for (my $i = 0; $i < $#{$a}; $i++) {
              next if (! defined($a->[$i])) && (! defined($b->[$i]));
              if (ref $a->[$i] && ref $b->[$i]) {
                  return 0 if $a->[$i][0] ne $b->[$i][0]
                           || $a->[$i][1] ne $b->[$i][1];
              } else {
                  return 0 if $a->[$i] ne $b->[$i];
              }
      }
      return 1;
}


