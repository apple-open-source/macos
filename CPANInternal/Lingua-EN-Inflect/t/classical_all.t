use Lingua::EN::Inflect qw(PL_N classical);
use Test::More 'no_plan';

# DEFAULT...

is PL_N('error', 0)    => 'errors';           # classical 'zero' not active
is PL_N('wildebeest')  => 'wildebeests';      # classical 'herd' not active
is PL_N('Sally')       => 'Sallys';           # classical 'names' active
is PL_N('brother')     => 'brothers';         # classical others not active
is PL_N('person')      => 'people';           # classical 'persons' not active
is PL_N('formula')     => 'formulas';         # classical 'ancient' not active

# CLASSICAL PLURALS ACTIVATED...

classical 'all';
is PL_N('error', 0)    => 'error';           # classical 'zero' active
is PL_N('wildebeest')  => 'wildebeest';      # classical 'herd' active
is PL_N('Sally')       => 'Sallys';          # classical 'names' active
is PL_N('brother')     => 'brethren';        # classical others active
is PL_N('person')      => 'persons';         # classical 'persons' active
is PL_N('formula')     => 'formulae';        # classical 'ancient' active


# CLASSICAL PLURALS DEACTIVATED...

classical all => 0;
is PL_N('error', 0)    => 'errors';           # classical 'zero' not active
is PL_N('wildebeest')  => 'wildebeests';      # classical 'herd' not active
is PL_N('Sally')       => 'Sallies';          # classical 'names' not active
is PL_N('brother')     => 'brothers';         # classical others not active
is PL_N('person')      => 'people';           # classical 'persons' not active
is PL_N('formula')     => 'formulas';         # classical 'ancient' not active


# CLASSICAL PLURALS REACTIVATED...

classical all => 1;
is PL_N('error', 0)    => 'error';           # classical 'zero' active
is PL_N('wildebeest')  => 'wildebeest';      # classical 'herd' active
is PL_N('Sally')       => 'Sallys';          # classical 'names' active
is PL_N('brother')     => 'brethren';        # classical others active
is PL_N('person')      => 'persons';         # classical 'persons' active
is PL_N('formula')     => 'formulae';        # classical 'ancient' active


# CLASSICAL PLURALS REDEACTIVATED...

classical 0;
is PL_N('error', 0)    => 'errors';           # classical 'zero' not active
is PL_N('wildebeest')  => 'wildebeests';      # classical 'herd' not active
is PL_N('Sally')       => 'Sallies';          # classical 'names' not active
is PL_N('brother')     => 'brothers';         # classical others not active
is PL_N('person')      => 'people';           # classical 'persons' not active
is PL_N('formula')     => 'formulas';         # classical 'ancient' not active


# CLASSICAL PLURALS REREACTIVATED...

classical 1;
is PL_N('error', 0)    => 'error';           # classical 'zero' active
is PL_N('wildebeest')  => 'wildebeest';      # classical 'herd' active
is PL_N('Sally')       => 'Sallys';          # classical 'names' active
is PL_N('brother')     => 'brethren';        # classical others active
is PL_N('person')      => 'persons';         # classical 'persons' active
is PL_N('formula')     => 'formulae';        # classical 'ancient' active


# CLASSICAL PLURALS REREDEACTIVATED...

classical 0;
is PL_N('error', 0)    => 'errors';           # classical 'zero' not active
is PL_N('wildebeest')  => 'wildebeests';      # classical 'herd' not active
is PL_N('Sally')       => 'Sallies';          # classical 'names' not active
is PL_N('brother')     => 'brothers';         # classical others not active
is PL_N('person')      => 'people';           # classical 'persons' not active
is PL_N('formula')     => 'formulas';         # classical 'ancient' not active


# CLASSICAL PLURALS REREREACTIVATED...

classical;
is PL_N('error', 0)    => 'error';           # classical 'zero' active
is PL_N('wildebeest')  => 'wildebeest';      # classical 'herd' active
is PL_N('Sally')       => 'Sallys';          # classical 'names' active
is PL_N('brother')     => 'brethren';        # classical others active
is PL_N('person')      => 'persons';         # classical 'persons' active
is PL_N('formula')     => 'formulae';        # classical 'ancient' active
