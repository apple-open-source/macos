#!/usr/bin/perl

use strict;
use warnings;
use Test::More;
use Test::Warn;
use Test::Exception;

use SQL::Abstract::Test import => ['is_same_sql_bind'];

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
              args   => ['test', {a => 1, b => \["to_date(?, 'MM/DD/YY')", '02/02/02']}, {a => {'between', [1,2]}}],
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
      {              
              func   => 'insert',
              args   => ['test.table', [ \'max(all_limits)', 4 ] ],
              stmt   => 'INSERT INTO test.table VALUES (max(all_limits), ?)',
              stmt_q => 'INSERT INTO `test`.`table` VALUES (max(all_limits), ?)',
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
              func   => 'update',
# LDNOTE : removed the "-maybe", because we no longer admit unknown ops
#
# acked by RIBASUSHI
#              args   => ['fhole', {fpoles => 4}, [-maybe => {race => [-and => [qw(black white asian)]]},
              args   => ['fhole', {fpoles => 4}, [          {race => [-and => [qw(black white asian)]]},
                                                            {-nest => {firsttime => [-or => {'=','yes'}, undef]}},
                                                            [ -and => {firstname => {-not_like => 'candace'}}, {lastname => {-in => [qw(jugs canyon towers)]}} ] ] ],
              stmt   => 'UPDATE fhole SET fpoles = ? WHERE ( ( ( ( ( ( ( race = ? ) OR ( race = ? ) OR ( race = ? ) ) ) ) ) )'
                      . ' OR ( ( ( ( firsttime = ? ) OR ( firsttime IS NULL ) ) ) ) OR ( ( ( firstname NOT LIKE ? ) ) AND ( lastname IN ( ?, ?, ? ) ) ) )',
              stmt_q => 'UPDATE `fhole` SET `fpoles` = ? WHERE ( ( ( ( ( ( ( `race` = ? ) OR ( `race` = ? ) OR ( `race` = ? ) ) ) ) ) )'
                      . ' OR ( ( ( ( `firsttime` = ? ) OR ( `firsttime` IS NULL ) ) ) ) OR ( ( ( `firstname` NOT LIKE ? ) ) AND ( `lastname` IN ( ?, ?, ? ) ) ) )',
              bind   => [qw(4 black white asian yes candace jugs canyon towers)]
      },
      #31
      {
              func   => 'insert',
              args   => ['test', {a => 1, b => \["to_date(?, 'MM/DD/YY')", '02/02/02']}],
              stmt   => 'INSERT INTO test (a, b) VALUES (?, to_date(?, \'MM/DD/YY\'))',
              stmt_q => 'INSERT INTO `test` (`a`, `b`) VALUES (?, to_date(?, \'MM/DD/YY\'))',
              bind   => [qw(1 02/02/02)],
      },
      #32
      {
              func   => 'select',
# LDNOTE: modified test below because we agreed with MST that literal SQL
#         should not automatically insert a '='; the user has to do it
#
# acked by MSTROUT
#              args   => ['test', '*', { a => \["to_date(?, 'MM/DD/YY')", '02/02/02']}],
              args   => ['test', '*', { a => \["= to_date(?, 'MM/DD/YY')", '02/02/02']}],
              stmt   => q{SELECT * FROM test WHERE ( a = to_date(?, 'MM/DD/YY') )},
              stmt_q => q{SELECT * FROM `test` WHERE ( `a` = to_date(?, 'MM/DD/YY') )},
              bind   => ['02/02/02'],
      },
      #33
      {
              func   => 'insert',
              new    => {array_datatypes => 1},
              args   => ['test', {a => 1, b => [1, 1, 2, 3, 5, 8]}],
              stmt   => 'INSERT INTO test (a, b) VALUES (?, ?)',
              stmt_q => 'INSERT INTO `test` (`a`, `b`) VALUES (?, ?)',
              bind   => [1, [1, 1, 2, 3, 5, 8]],
      },
      #34
      {
              func   => 'insert',
              new    => {bindtype => 'columns', array_datatypes => 1},
              args   => ['test', {a => 1, b => [1, 1, 2, 3, 5, 8]}],
              stmt   => 'INSERT INTO test (a, b) VALUES (?, ?)',
              stmt_q => 'INSERT INTO `test` (`a`, `b`) VALUES (?, ?)',
              bind   => [[a => 1], [b => [1, 1, 2, 3, 5, 8]]],
      },
      #35
      {
              func   => 'update',
              new    => {array_datatypes => 1},
              args   => ['test', {a => 1, b => [1, 1, 2, 3, 5, 8]}],
              stmt   => 'UPDATE test SET a = ?, b = ?',
              stmt_q => 'UPDATE `test` SET `a` = ?, `b` = ?',
              bind   => [1, [1, 1, 2, 3, 5, 8]],
      },
      #36
      {
              func   => 'update',
              new    => {bindtype => 'columns', array_datatypes => 1},
              args   => ['test', {a => 1, b => [1, 1, 2, 3, 5, 8]}],
              stmt   => 'UPDATE test SET a = ?, b = ?',
              stmt_q => 'UPDATE `test` SET `a` = ?, `b` = ?',
              bind   => [[a => 1], [b => [1, 1, 2, 3, 5, 8]]],
      },
      #37
      {
              func   => 'select',
              args   => ['test', '*', { a => {'>', \'1 + 1'}, b => 8 }],
              stmt   => 'SELECT * FROM test WHERE ( a > 1 + 1 AND b = ? )',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` > 1 + 1 AND `b` = ? )',
              bind   => [8],
      },             
      #38
      {
              func   => 'select',
              args   => ['test', '*', { a => {'<' => \["to_date(?, 'MM/DD/YY')", '02/02/02']}, b => 8 }],
              stmt   => 'SELECT * FROM test WHERE ( a < to_date(?, \'MM/DD/YY\') AND b = ? )',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` < to_date(?, \'MM/DD/YY\') AND `b` = ? )',
              bind   => ['02/02/02', 8],
      },             
      #39            
      { #TODO in SQLA >= 2.0 it will die instead (we kept this just because old SQLA passed it through)
              func   => 'insert',
              args   => ['test', {a => 1, b => 2, c => 3, d => 4, e => { answer => 42 }}],
              stmt   => 'INSERT INTO test (a, b, c, d, e) VALUES (?, ?, ?, ?, ?)',
              stmt_q => 'INSERT INTO `test` (`a`, `b`, `c`, `d`, `e`) VALUES (?, ?, ?, ?, ?)',
              bind   => [qw/1 2 3 4/, { answer => 42}],
              warning_like => qr/HASH ref as bind value in insert is not supported/i,
      },             
      #40            
      {              
              func   => 'update',
              args   => ['test', {a => 1, b => \["42"]}, {a => {'between', [1,2]}}],
              stmt   => 'UPDATE test SET a = ?, b = 42 WHERE ( a BETWEEN ? AND ? )',
              stmt_q => 'UPDATE `test` SET `a` = ?, `b` = 42 WHERE ( `a` BETWEEN ? AND ? )',
              bind   => [qw(1 1 2)],
      },             
      #41
      {
              func   => 'insert',
              args   => ['test', {a => 1, b => \["42"]}],
              stmt   => 'INSERT INTO test (a, b) VALUES (?, 42)',
              stmt_q => 'INSERT INTO `test` (`a`, `b`) VALUES (?, 42)',
              bind   => [qw(1)],
      },
      #42
      {
              func   => 'select',
              args   => ['test', '*', { a => \["= 42"], b => 1}],
              stmt   => q{SELECT * FROM test WHERE ( a = 42 ) AND (b = ? )},
              stmt_q => q{SELECT * FROM `test` WHERE ( `a` = 42 ) AND ( `b` = ? )},
              bind   => [qw(1)],
      },
      #43
      {
              func   => 'select',
              args   => ['test', '*', { a => {'<' => \["42"]}, b => 8 }],
              stmt   => 'SELECT * FROM test WHERE ( a < 42 AND b = ? )',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` < 42 AND `b` = ? )',
              bind   => [qw(8)],
      },             
      #44
      {
              func   => 'insert',
              new    => {bindtype => 'columns'},
              args   => ['test', {a => 1, b => \["to_date(?, 'MM/DD/YY')", [dummy => '02/02/02']]}],
              stmt   => 'INSERT INTO test (a, b) VALUES (?, to_date(?, \'MM/DD/YY\'))',
              stmt_q => 'INSERT INTO `test` (`a`, `b`) VALUES (?, to_date(?, \'MM/DD/YY\'))',
              bind   => [[a => '1'], [dummy => '02/02/02']],
      },
      #45
      {              
              func   => 'update',
              new    => {bindtype => 'columns'},
              args   => ['test', {a => 1, b => \["to_date(?, 'MM/DD/YY')", [dummy => '02/02/02']]}, {a => {'between', [1,2]}}],
              stmt   => 'UPDATE test SET a = ?, b = to_date(?, \'MM/DD/YY\') WHERE ( a BETWEEN ? AND ? )',
              stmt_q => 'UPDATE `test` SET `a` = ?, `b` = to_date(?, \'MM/DD/YY\') WHERE ( `a` BETWEEN ? AND ? )',
              bind   => [[a => '1'], [dummy => '02/02/02'], [a => '1'], [a => '2']],
      },             
      #46
      {
              func   => 'select',
              new    => {bindtype => 'columns'},
              args   => ['test', '*', { a => \["= to_date(?, 'MM/DD/YY')", [dummy => '02/02/02']]}],
              stmt   => q{SELECT * FROM test WHERE ( a = to_date(?, 'MM/DD/YY') )},
              stmt_q => q{SELECT * FROM `test` WHERE ( `a` = to_date(?, 'MM/DD/YY') )},
              bind   => [[dummy => '02/02/02']],
      },
      #47
      {
              func   => 'select',
              new    => {bindtype => 'columns'},
              args   => ['test', '*', { a => {'<' => \["to_date(?, 'MM/DD/YY')", [dummy => '02/02/02']]}, b => 8 }],
              stmt   => 'SELECT * FROM test WHERE ( a < to_date(?, \'MM/DD/YY\') AND b = ? )',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` < to_date(?, \'MM/DD/YY\') AND `b` = ? )',
              bind   => [[dummy => '02/02/02'], [b => 8]],
      },             
      #48
      {
              func   => 'insert',
              new    => {bindtype => 'columns'},
              args   => ['test', {a => 1, b => \["to_date(?, 'MM/DD/YY')", '02/02/02']}],
              exception_like => qr/bindtype 'columns' selected, you need to pass: \[column_name => bind_value\]/,
      },
      #49
      {              
              func   => 'update',
              new    => {bindtype => 'columns'},
              args   => ['test', {a => 1, b => \["to_date(?, 'MM/DD/YY')", '02/02/02']}, {a => {'between', [1,2]}}],
              exception_like => qr/bindtype 'columns' selected, you need to pass: \[column_name => bind_value\]/,
      },             
      #49
      {
              func   => 'select',
              new    => {bindtype => 'columns'},
              args   => ['test', '*', { a => \["= to_date(?, 'MM/DD/YY')", '02/02/02']}],
              exception_like => qr/bindtype 'columns' selected, you need to pass: \[column_name => bind_value\]/,
      },
      #50
      {
              func   => 'select',
              new    => {bindtype => 'columns'},
              args   => ['test', '*', { a => {'<' => \["to_date(?, 'MM/DD/YY')", '02/02/02']}, b => 8 }],
              exception_like => qr/bindtype 'columns' selected, you need to pass: \[column_name => bind_value\]/,
      },             
      #51
      {
              func   => 'select',
              new    => {bindtype => 'columns'},
              args   => ['test', '*', { a => {-in => \["(SELECT d FROM to_date(?, 'MM/DD/YY') AS d)", [dummy => '02/02/02']]}, b => 8 }],
              stmt   => 'SELECT * FROM test WHERE ( a IN (SELECT d FROM to_date(?, \'MM/DD/YY\') AS d) AND b = ? )',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` IN (SELECT d FROM to_date(?, \'MM/DD/YY\') AS d) AND `b` = ? )',
              bind   => [[dummy => '02/02/02'], [b => 8]],
      },             
      #52
      {
              func   => 'select',
              new    => {bindtype => 'columns'},
              args   => ['test', '*', { a => {-in => \["(SELECT d FROM to_date(?, 'MM/DD/YY') AS d)", '02/02/02']}, b => 8 }],
              exception_like => qr/bindtype 'columns' selected, you need to pass: \[column_name => bind_value\]/,
      },             
      #53
      {
              func   => 'insert',
              new    => {bindtype => 'columns'},
              args   => ['test', {a => 1, b => \["to_date(?, 'MM/DD/YY')", [{dummy => 1} => '02/02/02']]}],
              stmt   => 'INSERT INTO test (a, b) VALUES (?, to_date(?, \'MM/DD/YY\'))',
              stmt_q => 'INSERT INTO `test` (`a`, `b`) VALUES (?, to_date(?, \'MM/DD/YY\'))',
              bind   => [[a => '1'], [{dummy => 1} => '02/02/02']],
      },
      #54
      {              
              func   => 'update',
              new    => {bindtype => 'columns'},
              args   => ['test', {a => 1, b => \["to_date(?, 'MM/DD/YY')", [{dummy => 1} => '02/02/02']]}, {a => {'between', [1,2]}}],
              stmt   => 'UPDATE test SET a = ?, b = to_date(?, \'MM/DD/YY\') WHERE ( a BETWEEN ? AND ? )',
              stmt_q => 'UPDATE `test` SET `a` = ?, `b` = to_date(?, \'MM/DD/YY\') WHERE ( `a` BETWEEN ? AND ? )',
              bind   => [[a => '1'], [{dummy => 1} => '02/02/02'], [a => '1'], [a => '2']],
      },             
      #55
      {
              func   => 'select',
              new    => {bindtype => 'columns'},
              args   => ['test', '*', { a => \["= to_date(?, 'MM/DD/YY')", [{dummy => 1} => '02/02/02']]}],
              stmt   => q{SELECT * FROM test WHERE ( a = to_date(?, 'MM/DD/YY') )},
              stmt_q => q{SELECT * FROM `test` WHERE ( `a` = to_date(?, 'MM/DD/YY') )},
              bind   => [[{dummy => 1} => '02/02/02']],
      },
      #56
      {
              func   => 'select',
              new    => {bindtype => 'columns'},
              args   => ['test', '*', { a => {'<' => \["to_date(?, 'MM/DD/YY')", [{dummy => 1} => '02/02/02']]}, b => 8 }],
              stmt   => 'SELECT * FROM test WHERE ( a < to_date(?, \'MM/DD/YY\') AND b = ? )',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` < to_date(?, \'MM/DD/YY\') AND `b` = ? )',
              bind   => [[{dummy => 1} => '02/02/02'], [b => 8]],
      },
      #57
      {
              func   => 'select',
              new    => {bindtype => 'columns'},
              args   => ['test', '*', { -or => [ -and => [ a => 'a', b => 'b' ], -and => [ c => 'c', d => 'd' ]  ]  }],
              stmt   => 'SELECT * FROM test WHERE ( a = ? AND b = ? ) OR ( c = ? AND d = ?  )',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` = ? AND `b` = ?  ) OR ( `c` = ? AND `d` = ? )',
              bind   => [[a => 'a'], [b => 'b'], [ c => 'c'],[ d => 'd']],
      },
      #58
      {
              func   => 'select',
              new    => {bindtype => 'columns'},
              args   => ['test', '*', [ { a => 1, b => 1}, [ a => 2, b => 2] ] ],
              stmt   => 'SELECT * FROM test WHERE ( a = ? AND b = ? ) OR ( a = ? OR b = ? )',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` = ? AND `b` = ? ) OR ( `a` = ? OR `b` = ? )',
              bind   => [[a => 1], [b => 1], [ a => 2], [ b => 2]],
      },
      #59
      {
              func   => 'select',
              new    => {bindtype => 'columns'},
              args   => ['test', '*', [ [ a => 1, b => 1], { a => 2, b => 2 } ] ],
              stmt   => 'SELECT * FROM test WHERE ( a = ? OR b = ? ) OR ( a = ? AND b = ? )',
              stmt_q => 'SELECT * FROM `test` WHERE ( `a` = ? OR `b` = ? ) OR ( `a` = ? AND `b` = ? )',
              bind   => [[a => 1], [b => 1], [ a => 2], [ b => 2]],
      },
      #60
      {
              func   => 'insert',
              args   => ['test', [qw/1 2 3 4 5/], { returning => 'id' }],
              stmt   => 'INSERT INTO test VALUES (?, ?, ?, ?, ?) RETURNING id',
              stmt_q => 'INSERT INTO `test` VALUES (?, ?, ?, ?, ?) RETURNING `id`',
              bind   => [qw/1 2 3 4 5/],
      },
      #60
      {
              func   => 'insert',
              args   => ['test', [qw/1 2 3 4 5/], { returning => 'id, foo, bar' }],
              stmt   => 'INSERT INTO test VALUES (?, ?, ?, ?, ?) RETURNING id, foo, bar',
              stmt_q => 'INSERT INTO `test` VALUES (?, ?, ?, ?, ?) RETURNING `id, foo, bar`',
              bind   => [qw/1 2 3 4 5/],
      },
      #61
      {
              func   => 'insert',
              args   => ['test', [qw/1 2 3 4 5/], { returning => [qw(id  foo  bar) ] }],
              stmt   => 'INSERT INTO test VALUES (?, ?, ?, ?, ?) RETURNING id, foo, bar',
              stmt_q => 'INSERT INTO `test` VALUES (?, ?, ?, ?, ?) RETURNING `id`, `foo`, `bar`',
              bind   => [qw/1 2 3 4 5/],
      },
      #62
      {
              func   => 'insert',
              args   => ['test', [qw/1 2 3 4 5/], { returning => \'id, foo, bar' }],
              stmt   => 'INSERT INTO test VALUES (?, ?, ?, ?, ?) RETURNING id, foo, bar',
              stmt_q => 'INSERT INTO `test` VALUES (?, ?, ?, ?, ?) RETURNING id, foo, bar',
              bind   => [qw/1 2 3 4 5/],
      },
      #63
      {
              func   => 'insert',
              args   => ['test', [qw/1 2 3 4 5/], { returning => \'id' }],
              stmt   => 'INSERT INTO test VALUES (?, ?, ?, ?, ?) RETURNING id',
              stmt_q => 'INSERT INTO `test` VALUES (?, ?, ?, ?, ?) RETURNING id',
              bind   => [qw/1 2 3 4 5/],
      },
);


plan tests => scalar(grep { !$_->{warning_like} } @tests) * 2
            + scalar(grep { $_->{warning_like} } @tests) * 4;

for (@tests) {
  local $"=', ';

  my $new = $_->{new} || {};
  $new->{debug} = $ENV{DEBUG} || 0;

  # test without quoting labels
  {
    my $sql = SQL::Abstract->new(%$new);

    my $func = $_->{func};
    my($stmt, @bind);
    my $test = sub {
      ($stmt, @bind) = $sql->$func(@{$_->{args}})
    };
    if ($_->{exception_like}) {
      throws_ok { &$test } $_->{exception_like}, "throws the expected exception ($_->{exception_like})";
    } else {
      if ($_->{warning_like}) {
        warning_like { &$test } $_->{warning_like}, "throws the expected warning ($_->{warning_like})";
      } else {
        &$test;
      }
      is_same_sql_bind($stmt, \@bind, $_->{stmt}, $_->{bind});
    }
  }

  # test with quoted labels
  {
    my $sql_q = SQL::Abstract->new(%$new, quote_char => '`', name_sep => '.');

    my $func_q = $_->{func};
    my($stmt_q, @bind_q);
    my $test = sub {
      ($stmt_q, @bind_q) = $sql_q->$func_q(@{$_->{args}})
    };
    if ($_->{exception_like}) {
      throws_ok { &$test } $_->{exception_like}, "throws the expected exception ($_->{exception_like})";
    } else {
      if ($_->{warning_like}) {
        warning_like { &$test } $_->{warning_like}, "throws the expected warning ($_->{warning_like})";
      } else {
        &$test;
      }

      is_same_sql_bind($stmt_q, \@bind_q, $_->{stmt_q}, $_->{bind});
    }
  }
}
