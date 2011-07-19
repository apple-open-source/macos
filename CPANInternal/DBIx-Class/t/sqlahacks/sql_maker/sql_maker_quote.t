use strict;
use warnings;

use Test::More;

use lib qw(t/lib);
use DBIC::SqlMakerTest;

use_ok('DBICTest');

my $schema = DBICTest->init_schema();

my $sql_maker = $schema->storage->sql_maker;

$sql_maker->quote_char('`');
$sql_maker->name_sep('.');

my ($sql, @bind) = $sql_maker->select(
          [
            {
              'me' => 'cd'
            },
            [
              {
                'artist' => 'artist',
                '-join_type' => ''
              },
              {
                'artist.artistid' => 'me.artist'
              }
            ],
            [
              {
                'tracks' => 'tracks',
                '-join_type' => 'left'
              },
              {
                'tracks.cd' => 'me.cdid'
              }
            ],
          ],
          [
            'me.cdid',
            { count => 'tracks.cd' },
            { min => 'me.year', -as => 'me.minyear' },
          ],
          {
            'artist.name' => 'Caterwauler McCrae',
            'me.year' => 2001
          },
          [],
          undef,
          undef
);

is_same_sql_bind(
  $sql, \@bind,
  q/
    SELECT `me`.`cdid`, COUNT( `tracks`.`cd` ), MIN( `me`.`year` ) AS `me`.`minyear`
      FROM `cd` `me`
      JOIN `artist` `artist` ON ( `artist`.`artistid` = `me`.`artist` )
      LEFT JOIN `tracks` `tracks` ON ( `tracks`.`cd` = `me`.`cdid` )
    WHERE ( `artist`.`name` = ? AND `me`.`year` = ? )
  /,
  [ ['artist.name' => 'Caterwauler McCrae'], ['me.year' => 2001] ],
  'got correct SQL and bind parameters for complex select query with quoting'
);


($sql, @bind) = $sql_maker->select(
          [
            {
              'me' => 'cd'
            }
          ],
          [
            'me.cdid',
            'me.artist',
            'me.title',
            'me.year'
          ],
          undef,
          'year DESC',
          undef,
          undef
);

is_same_sql_bind(
  $sql, \@bind,
  q/SELECT `me`.`cdid`, `me`.`artist`, `me`.`title`, `me`.`year` FROM `cd` `me` ORDER BY `year DESC`/, [],
  'scalar ORDER BY okay (single value)'
);


($sql, @bind) = $sql_maker->select(
          [
            {
              'me' => 'cd'
            }
          ],
          [
            'me.cdid',
            'me.artist',
            'me.title',
            'me.year'
          ],
          undef,
          [
            'year DESC',
            'title ASC'
          ],
          undef,
          undef
);

is_same_sql_bind(
  $sql, \@bind,
  q/SELECT `me`.`cdid`, `me`.`artist`, `me`.`title`, `me`.`year` FROM `cd` `me` ORDER BY `year DESC`, `title ASC`/, [],
  'scalar ORDER BY okay (multiple values)'
);

{
  ($sql, @bind) = $sql_maker->select(
            [
              {
                'me' => 'cd'
              }
            ],
            [
              'me.cdid',
              'me.artist',
              'me.title',
              'me.year'
            ],
            undef,
            { -desc => 'year' },
            undef,
            undef
  );

  is_same_sql_bind(
    $sql, \@bind,
    q/SELECT `me`.`cdid`, `me`.`artist`, `me`.`title`, `me`.`year` FROM `cd` `me` ORDER BY `year` DESC/, [],
    'hashref ORDER BY okay (single value)'
  );


  ($sql, @bind) = $sql_maker->select(
            [
              {
                'me' => 'cd'
              }
            ],
            [
              'me.cdid',
              'me.artist',
              'me.title',
              'me.year'
            ],
            undef,
            [
              { -desc => 'year' },
              { -asc => 'title' }
            ],
            undef,
            undef
  );

  is_same_sql_bind(
    $sql, \@bind,
    q/SELECT `me`.`cdid`, `me`.`artist`, `me`.`title`, `me`.`year` FROM `cd` `me` ORDER BY `year` DESC, `title` ASC/, [],
    'hashref ORDER BY okay (multiple values)'
  );

}


($sql, @bind) = $sql_maker->select(
          [
            {
              'me' => 'cd'
            }
          ],
          [
            'me.cdid',
            'me.artist',
            'me.title',
            'me.year'
          ],
          undef,
          \'year DESC',
          undef,
          undef
);

is_same_sql_bind(
  $sql, \@bind,
  q/SELECT `me`.`cdid`, `me`.`artist`, `me`.`title`, `me`.`year` FROM `cd` `me` ORDER BY year DESC/, [],
  'did not quote ORDER BY with scalarref (single value)'
);


($sql, @bind) = $sql_maker->select(
          [
            {
              'me' => 'cd'
            }
          ],
          [
            'me.cdid',
            'me.artist',
            'me.title',
            'me.year'
          ],
          undef,
          [
            \'year DESC',
            \'title ASC'
          ],
          undef,
          undef
);

is_same_sql_bind(
  $sql, \@bind,
  q/SELECT `me`.`cdid`, `me`.`artist`, `me`.`title`, `me`.`year` FROM `cd` `me` ORDER BY year DESC, title ASC/, [],
  'did not quote ORDER BY with scalarref (multiple values)'
);


($sql, @bind) = $sql_maker->select(
  [ { me => 'cd' }                  ],
  [qw/ me.cdid me.artist me.title  /],
  { cdid => \['rlike ?', [cdid => 'X'] ]       },
  { group_by => 'title', having => \['count(me.artist) > ?', [ cnt => 2] ] },
);

is_same_sql_bind(
  $sql, \@bind,
  q/SELECT `me`.`cdid`, `me`.`artist`, `me`.`title` FROM `cd` `me` WHERE ( `cdid` rlike ? ) GROUP BY `title` HAVING count(me.artist) > ?/,
  [ [ cdid => 'X'], ['cnt' => '2'] ],
  'Quoting works with where/having arrayrefsrefs',
);


($sql, @bind) = $sql_maker->select(
  [ { me => 'cd' }                  ],
  [qw/ me.cdid me.artist me.title  /],
  { cdid => \'rlike X'              },
  { group_by => 'title', having => \'count(me.artist) > 2' },
);

is_same_sql_bind(
  $sql, \@bind,
  q/SELECT `me`.`cdid`, `me`.`artist`, `me`.`title` FROM `cd` `me` WHERE ( `cdid` rlike X ) GROUP BY `title` HAVING count(me.artist) > 2/,
  [],
  'Quoting works with where/having scalarrefs',
);


($sql, @bind) = $sql_maker->update(
          'group',
          {
            'order' => '12',
            'name' => 'Bill'
          }
);

is_same_sql_bind(
  $sql, \@bind,
  q/UPDATE `group` SET `name` = ?, `order` = ?/, [ ['name' => 'Bill'], ['order' => '12'] ],
  'quoted table names for UPDATE'
);

{
  ($sql, @bind) = $sql_maker->select(
        [
          {
            'me' => 'cd'
          }
        ],
        [
          'me.*'
        ],
        undef,
        [],
        undef,
        undef    
  );

  is_same_sql_bind(
    $sql, \@bind,
    q/SELECT `me`.* FROM `cd` `me`/, [],
    'select attr with me.* is right'
  );
}


$sql_maker->quote_char([qw/[ ]/]);

($sql, @bind) = $sql_maker->select(
          [
            {
              'me' => 'cd'
            },
            [
              {
                'artist' => 'artist',
                '-join_type' => ''
              },
              {
                'artist.artistid' => 'me.artist'
              }
            ]
          ],
          [
            {
              max => 'rank',
              -as => 'max_rank',
            },
            'rank',
            {
              'count' => '*',
              -as => 'cnt',
            }
          ],
          {
            'artist.name' => 'Caterwauler McCrae',
            'me.year' => 2001
          },
          [],
          undef,
          undef
);

is_same_sql_bind(
  $sql, \@bind,
  q/SELECT MAX ( [rank] ) AS [max_rank], [rank], COUNT( * ) AS [cnt] FROM [cd] [me]  JOIN [artist] [artist] ON ( [artist].[artistid] = [me].[artist] ) WHERE ( [artist].[name] = ? AND [me].[year] = ? )/, [ ['artist.name' => 'Caterwauler McCrae'], ['me.year' => 2001] ],
  'got correct SQL and bind parameters for count query with bracket quoting'
);


($sql, @bind) = $sql_maker->update(
          'group',
          {
            'order' => '12',
            'name' => 'Bill'
          }
);

is_same_sql_bind(
  $sql, \@bind,
  q/UPDATE [group] SET [name] = ?, [order] = ?/, [ ['name' => 'Bill'], ['order' => '12'] ],
  'bracket quoted table names for UPDATE'
);

done_testing;
