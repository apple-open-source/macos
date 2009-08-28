use strict;
use warnings;

use Test::More qw(no_plan);
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

my $cd2 = $schema->resultset('CD')->create({ artist => 
                                   { name => 'Fred Bloggs' },
                                   title => 'Some CD',
                                   year => 1996
                                 });

is(ref $cd2->artist, 'DBICTest::Artist', 'Created CD and Artist object');
is($cd2->artist->name, 'Fred Bloggs', 'Artist created correctly');

my $artist = $schema->resultset('Artist')->create({ name => 'Fred 2',
                                                     cds => [
                                                             { title => 'Music to code by',
                                                               year => 2007,
                                                             },
                                                             ],
                                                     });
is(ref $artist->cds->first, 'DBICTest::CD', 'Created Artist with CDs');
is($artist->cds->first->title, 'Music to code by', 'CD created correctly');

# Add a new CD
$artist->update({cds => [ $artist->cds, 
                          { title => 'Yet another CD',
                            year => 2006,
                          },
                        ],
                });
is(($artist->cds->search({}, { order_by => 'year' }))[0]->title, 'Yet another CD', 'Updated and added another CD');

my $newartist = $schema->resultset('Artist')->find_or_create({ name => 'Fred 2'});

is($newartist->name, 'Fred 2', 'Retrieved the artist');


my $newartist2 = $schema->resultset('Artist')->find_or_create({ name => 'Fred 3',
                                                                cds => [
                                                                        { title => 'Noah Act',
                                                                          year => 2007,
                                                                        },
                                                                       ],

                                                              });

is($newartist2->name, 'Fred 3', 'Created new artist with cds via find_or_create');

my $artist2 = $schema->resultset('Artist')->create({ artistid => 1000,
                                                    name => 'Fred 3',
                                                     cds => [
                                                             { artist => 1000,
                                                               title => 'Music to code by',
                                                               year => 2007,
                                                             },
                                                             ],
                                                    cds_unordered => [
                                                             { artist => 1000,
                                                               title => 'Music to code by',
                                                               year => 2007,
                                                             },
                                                             ]
                                                     });

is($artist2->in_storage, 1, 'artist with duplicate rels inserted okay');

CREATE_RELATED1 :{

	my $artist = $schema->resultset('Artist')->first;
	
	my $cd_result = $artist->create_related('cds', {
	
		title => 'TestOneCD1',
		year => 2007,
		tracks => [
		
			{ position=>111,
			  title => 'TrackOne',
			},
			{ position=>112,
			  title => 'TrackTwo',
			}
		],

	});
	
	ok( $cd_result && ref $cd_result eq 'DBICTest::CD', "Got Good CD Class");
	ok( $cd_result->title eq "TestOneCD1", "Got Expected Title");
	
	my $tracks = $cd_result->tracks;
	
	ok( ref $tracks eq "DBIx::Class::ResultSet", "Got Expected Tracks ResultSet");
	
	foreach my $track ($tracks->all)
	{
		ok( $track && ref $track eq 'DBICTest::Track', 'Got Expected Track Class');
	}
}

CREATE_RELATED2 :{

	my $artist = $schema->resultset('Artist')->first;
	
	my $cd_result = $artist->create_related('cds', {
	
		title => 'TestOneCD2',
		year => 2007,
		tracks => [
		
			{ position=>111,
			  title => 'TrackOne',
			},
			{ position=>112,
			  title => 'TrackTwo',
			}
		],

    liner_notes => { notes => 'I can haz liner notes?' },

	});
	
	ok( $cd_result && ref $cd_result eq 'DBICTest::CD', "Got Good CD Class");
	ok( $cd_result->title eq "TestOneCD2", "Got Expected Title");
  ok( $cd_result->notes eq 'I can haz liner notes?', 'Liner notes');
	
	my $tracks = $cd_result->tracks;
	
	ok( ref $tracks eq "DBIx::Class::ResultSet", "Got Expected Tracks ResultSet");
	
	foreach my $track ($tracks->all)
	{
		ok( $track && ref $track eq 'DBICTest::Track', 'Got Expected Track Class');
	}
}

my $cdp = $schema->resultset('CD_to_Producer')->create({
            cd => { artist => 1, title => 'foo', year => 2000 },
            producer => { name => 'jorge' }
          });

ok($cdp, 'join table record created ok');

SPECIAL_CASE: {
  my $kurt_cobain = { name => 'Kurt Cobain' };

  my $in_utero = $schema->resultset('CD')->new({
      title => 'In Utero',
      year  => 1993
    });

  $kurt_cobain->{cds} = [ $in_utero ];


  $schema->resultset('Artist')->populate([ $kurt_cobain ]); # %)
  $a = $schema->resultset('Artist')->find({name => 'Kurt Cobain'});

  is($a->name, 'Kurt Cobain', 'Artist insertion ok');
  is($a->cds && $a->cds->first && $a->cds->first->title, 
		  'In Utero', 'CD insertion ok');
}

SPECIAL_CASE2: {
  my $pink_floyd = { name => 'Pink Floyd' };

  my $the_wall = { title => 'The Wall', year  => 1979 };

  $pink_floyd->{cds} = [ $the_wall ];


  $schema->resultset('Artist')->populate([ $pink_floyd ]); # %)
  $a = $schema->resultset('Artist')->find({name => 'Pink Floyd'});

  is($a->name, 'Pink Floyd', 'Artist insertion ok');
  is($a->cds && $a->cds->first->title, 'The Wall', 'CD insertion ok');
}

## Create foreign key col obj including PK
## See test 20 in 66relationships.t
my $new_cd_hashref = { 
              cdid => 27, 
               title => 'Boogie Woogie', 
              year => '2007', 
              artist => { artistid => 17, name => 'king luke' }
             };

my $cd = $schema->resultset("CD")->find(1);

is($cd->artist->id, 1, 'rel okay');

my $new_cd = $schema->resultset("CD")->create($new_cd_hashref);
is($new_cd->artist->id, 17, 'new id retained okay');
