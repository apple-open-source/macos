use Lingua::EN::Inflect qw(PL_N classical);
use Test::More 'no_plan';

# DEFAULT...

is PL_N('person')      => 'people';          # classical 'persons' not active

# "person" PLURALS ACTIVATED...

classical 'persons';
is PL_N('person')      => 'persons';          # classical 'persons' active

# OTHER CLASSICALS NOT ACTIVATED...

is PL_N('wildebeest')  => 'wildebeests';      # classical 'herd' not active
is PL_N('formula')     => 'formulas';         # classical 'ancient' not active
is PL_N('error', 0)    => 'errors';           # classical 'zero' not active
is PL_N('Sally')       => 'Sallys';           # classical 'names' active
is PL_N('brother')     => 'brothers';         # classical 'all' not active
