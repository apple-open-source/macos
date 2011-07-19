use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

plan tests => 23;

# an insane multicreate 
# (should work, despite the fact that no one will probably use it this way)

my $schema = DBICTest->init_schema();

# first count how many rows do we initially have
my $counts;
$counts->{$_} = $schema->resultset($_)->count for qw/Artist CD Genre Producer Tag/;

# do the crazy create
eval {
  $schema->resultset('CD')->create ({
    artist => {
      name => 'james',
    },
    title => 'Greatest hits 1',
    year => '2012',
    genre => {
      name => '"Greatest" collections',
    },
    tags => [
      { tag => 'A' },
      { tag => 'B' },
    ],
    cd_to_producer => [
      {
        producer => {
          name => 'bob',
          producer_to_cd => [
            {
              cd => { 
                artist => {
                  name => 'lars',
                  cds => [
                    {
                      title => 'Greatest hits 2',
                      year => 2012,
                      genre => {
                        name => '"Greatest" collections',
                      },
                      tags => [
                        { tag => 'A' },
                        { tag => 'B' },
                      ],
                      # This cd is created via artist so it doesn't know about producers
                      cd_to_producer => [
                        { producer => { name => 'bob' } },
                        { producer => { name => 'paul' } },
                        { producer => {
                          name => 'flemming',
                          producer_to_cd => [
                            { cd => {
                              artist => {
                                name => 'kirk',
                                cds => [
                                  {
                                    title => 'Greatest hits 3',
                                    year => 2012,
                                    genre => {
                                      name => '"Greatest" collections',
                                    },
                                    tags => [
                                      { tag => 'A' },
                                      { tag => 'B' },
                                    ],
                                  },
                                  {
                                    title => 'Greatest hits 4',
                                    year => 2012,
                                    genre => {
                                      name => '"Greatest" collections2',
                                    },
                                    tags => [
                                      { tag => 'A' },
                                      { tag => 'B' },
                                    ],
                                  },
                                ],
                              },
                              title => 'Greatest hits 5',
                              year => 2013,
                              genre => {
                                name => '"Greatest" collections2',
                              },
                            }},
                          ],
                        }},
                      ],
                    },
                  ],
                },
                title => 'Greatest hits 6',
                year => 2012,
                genre => {
                  name => '"Greatest" collections',
                },
                tags => [
                  { tag => 'A' },
                  { tag => 'B' },
                ],
              },
            },
            {
              cd => { 
                artist => {
                  name => 'lars',    # should already exist
                  # even though the artist 'name' is not uniquely constrained
                  # find_or_create will arguably DWIM 
                },
                title => 'Greatest hits 7',
                year => 2013,
              },
            },
          ],
        },
      },
    ],
  });

  is ($schema->resultset ('Artist')->count, $counts->{Artist} + 3, '3 new artists created');
  is ($schema->resultset ('Genre')->count, $counts->{Genre} + 2, '2 additional genres created');
  is ($schema->resultset ('Producer')->count, $counts->{Producer} + 3, '3 new producer');
  is ($schema->resultset ('CD')->count, $counts->{CD} + 7, '7 new CDs');
  is ($schema->resultset ('Tag')->count, $counts->{Tag} + 10, '10 new Tags');

  my $cd_rs = $schema->resultset ('CD')
    ->search ({ title => { -like => 'Greatest hits %' }}, { order_by => 'title'} );
  is ($cd_rs->count, 7, '7 greatest hits created');

  my $cds_2012 = $cd_rs->search ({ year => 2012});
  is ($cds_2012->count, 5, '5 CDs created in 2012');

  is (
    $cds_2012->search(
      { 'tags.tag' => { -in => [qw/A B/] } },
      {
        join => 'tags',
        group_by => 'me.cdid',
        having => 'count(me.cdid) = 2',
      }
    ),
    5,
    'All 10 tags were pairwise distributed between 5 year-2012 CDs'
  );

  my $paul_prod = $cd_rs->search (
    { 'producer.name' => 'paul'},
    { join => { cd_to_producer => 'producer' } }
  );
  is ($paul_prod->count, 1, 'Paul had 1 production');
  my $pauls_cd = $paul_prod->single;
  is ($pauls_cd->cd_to_producer->count, 3, 'Paul had two co-producers');
  is (
    $pauls_cd->search_related ('cd_to_producer',
      { 'producer.name' => 'flemming'},
      { join => 'producer' }
    )->count,
    1,
    'The second producer is flemming',
  );

  my $kirk_cds = $cd_rs->search ({ 'artist.name' => 'kirk' }, { join => 'artist' });
  is ($kirk_cds, 3, 'Kirk had 3 CDs');
  is (
    $kirk_cds->search (
      { 'cd_to_producer.cd' => { '!=', undef } },
      { join => 'cd_to_producer' },
    ),
    1,
    'Kirk had a producer only on one cd',
  );

  my $lars_cds = $cd_rs->search ({ 'artist.name' => 'lars' }, { join => 'artist' });
  is ($lars_cds->count, 3, 'Lars had 3 CDs');
  is (
    $lars_cds->search (
      { 'cd_to_producer.cd' => undef },
      { join => 'cd_to_producer' },
    ),
    0,
    'Lars always had a producer',
  );
  is (
    $lars_cds->search_related ('cd_to_producer',
      { 'producer.name' => 'flemming'},
      { join => 'producer' }
    )->count,
    1,
    'Lars produced 1 CD with flemming',
  );
  is (
    $lars_cds->search_related ('cd_to_producer',
      { 'producer.name' => 'bob'},
      { join => 'producer' }
    )->count,
    3,
    'Lars produced 3 CDs with bob',
  );

  my $bob_prod = $cd_rs->search (
    { 'producer.name' => 'bob'},
    { join => { cd_to_producer => 'producer' } }
  );
  is ($bob_prod->count, 4, 'Bob produced a total of 4 CDs');
  ok ($bob_prod->find ({ title => 'Greatest hits 1'}), '1st Bob production name correct');
  ok ($bob_prod->find ({ title => 'Greatest hits 6'}), '2nd Bob production name correct');
  ok ($bob_prod->find ({ title => 'Greatest hits 2'}), '3rd Bob production name correct');
  ok ($bob_prod->find ({ title => 'Greatest hits 7'}), '4th Bob production name correct');

  is (
    $bob_prod->search ({ 'artist.name' => 'james' }, { join => 'artist' })->count,
    1,
    "Bob produced james' only CD",
  );
};
diag $@ if $@;

1;
