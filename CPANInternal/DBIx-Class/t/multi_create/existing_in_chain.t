use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

# For fully intuitive multicreate any relationships in a chain
# that do not exist for one reason or another should be created,
# even if the preceeding relationship already exists.
#
# To get this to work a minor rewrite of find() is necessary, and
# more importantly some sort of recursive_insert() call needs to 
# be available. The way things will work then is:
# *) while traversing the hierarchy code calls find_or_create()
# *) this in turn calls find(%\nested_dataset)
# *) this should return not only the existing object, but must
#    also attach all non-existing (in fact maybe existing) related
#    bits of data to it, with in_storage => 0
# *) then before returning the result of the succesful find(), we
#    simply call $obj->recursive_insert and all is dandy
#
# Since this will not be a very clean solution, todoifying for the
# time being until an actual need arises
#
# ribasushi

TODO: { my $f = __FILE__; local $TODO = "See comment at top of $f for discussion of the TODO";

{
  my $counts;
  $counts->{$_} = $schema->resultset($_)->count for qw/Track CD Genre/;

  lives_ok (sub {
    my $existing_nogen_cd = $schema->resultset('CD')->search (
      { 'genre.genreid' => undef },
      { join => 'genre' },
    )->first;

    $schema->resultset('Track')->create ({
      title => 'Sugar-coated',
      cd => {
        title => $existing_nogen_cd->title,
        genre => {
          name => 'sugar genre',
        }
      }
    });

    is ($schema->resultset('Track')->count, $counts->{Track} + 1, '1 new track');
    is ($schema->resultset('CD')->count, $counts->{CD}, 'No new cds');
    is ($schema->resultset('Genre')->count, $counts->{Genre} + 1, '1 new genre');

    is ($existing_nogen_cd->genre->title,  'sugar genre', 'Correct genre assigned to CD');
  }, 'create() did not throw');
}
{
  my $counts;
  $counts->{$_} = $schema->resultset($_)->count for qw/Artist CD Producer/;

  lives_ok (sub {
    my $artist = $schema->resultset('Artist')->first;
    my $producer = $schema->resultset('Producer')->create ({ name => 'the queen of england' });

    $schema->resultset('CD')->create ({
      artist => $artist,
      title => 'queen1',
      year => 2007,
      cd_to_producer => [
        {
          producer => {
          name => $producer->name,
            producer_to_cd => [
              {
                cd => {
                  title => 'queen2',
                  year => 2008,
                  artist => $artist,
                },
              },
            ],
          },
        },
      ],
    });

    is ($schema->resultset('Artist')->count, $counts->{Artist}, 'No new artists');
    is ($schema->resultset('Producer')->count, $counts->{Producer} + 1, '1 new producers');
    is ($schema->resultset('CD')->count, $counts->{CD} + 2, '2 new cds');

    is ($producer->cds->count, 2, 'CDs assigned to correct producer');
    is_deeply (
      [ $producer->cds->search ({}, { order_by => 'title' })->get_column('title')->all],
      [ qw/queen1 queen2/ ],
      'Correct cd names',
    );
  }, 'create() did not throw');
}

}

done_testing;
