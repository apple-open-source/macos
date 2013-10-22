use Lingua::EN::Inflect qw( WORDLIST );
use Test::More 'no_plan'; 

my @words;

# Four words...
@words = qw(apple banana carrot tomato);

is WORDLIST(@words),
  "apple, banana, carrot, and tomato"
   => 'plain 4 words';

is WORDLIST(@words, {final_sep=>''}),
  "apple, banana, carrot and tomato"
   => '4 words, no final sep';

is WORDLIST(@words, {final_sep=>'...'}),
  "apple, banana, carrot... and tomato"
   => '4 words, different final sep';

is WORDLIST(@words, {final_sep=>'...', conj=>''}),
  "apple, banana, carrot... tomato"
   => '4 words, different final sep, no conjunction';

is WORDLIST(@words, {conj=>'or'}),
  "apple, banana, carrot, or tomato"
   => '4 words, different conjunction';

is WORDLIST(@words, {conj=>'&'}),
  "apple, banana, carrot, & tomato"
   => '4 words, different conjunction';

# Three words...
@words = qw(apple banana carrot);

is WORDLIST(@words),
   "apple, banana, and carrot"
    => 'plain 3 words';

is WORDLIST(@words, {final_sep=>''}),
   "apple, banana and carrot"
    => '3 words, no final sep';

is WORDLIST(@words, {final_sep=>'...'}),
   "apple, banana... and carrot"
    => '3 words, different final sep';

is WORDLIST(@words, {final_sep=>'...', conj=>''}),
   "apple, banana... carrot"
    => '3 words, different final sep, no conjunction';

is WORDLIST(@words, {conj=>'or'}),
   "apple, banana, or carrot"
    => '3 words, different conjunction';

is WORDLIST(@words, {conj=>'&'}),
   "apple, banana, & carrot"
    => '3 words, different conjunction';


# Three words with semicolons...
@words = ('apple,fuji' ,  'banana' , 'carrot');

is WORDLIST(@words),
   "apple,fuji; banana; and carrot"
    => 'comma-inclusive 3 words';

is WORDLIST(@words, {final_sep=>''}),
   "apple,fuji; banana and carrot"
    => 'comma-inclusive 3 words, no final sep';

is WORDLIST(@words, {final_sep=>'...'}),
   "apple,fuji; banana... and carrot"
    => 'comma-inclusive 3 words, different final sep';

is WORDLIST(@words, {final_sep=>'...', conj=>''}),
   "apple,fuji; banana... carrot"
    => 'comma-inclusive 3 words, different final sep, no conjunction';

is WORDLIST(@words, {conj=>'or'}),
   "apple,fuji; banana; or carrot"
    => 'comma-inclusive 3 words, different conjunction';

is WORDLIST(@words, {conj=>'&'}),
   "apple,fuji; banana; & carrot"
    => 'comma-inclusive 3 words, different conjunction';


# Two words...
@words = qw(apple carrot );

is WORDLIST(@words),
   "apple and carrot"
    => 'plain 2 words';

is WORDLIST(@words, {final_sep=>''}),
   "apple and carrot"
    => '2 words, no final sep';

is WORDLIST(@words, {final_sep=>'...'}),
   "apple and carrot"
    => '2 words, different final sep';

is WORDLIST(@words, {final_sep=>'...', conj=>''}),
   "applecarrot"
    => '2 words, different final sep, no conjunction';

is WORDLIST(@words, {conj=>'or'}),
   "apple or carrot"
    => '2 words, different conjunction';

is WORDLIST(@words, {conj=>'&'}),
   "apple & carrot"
    => '2 words, different conjunction';


# One word...
@words = qw(carrot );

is WORDLIST(@words),
   "carrot"
    => 'plain 1 word';

is WORDLIST(@words, {final_sep=>''}),
   "carrot"
    => '1 word, no final sep';

is WORDLIST(@words, {final_sep=>'...'}),
   "carrot"
    => '1 word, different final sep';

is WORDLIST(@words, {final_sep=>'...', conj=>''}),
   "carrot"
    => '1 word, different final sep, no conjunction';

is WORDLIST(@words, {conj=>'or'}),
   "carrot"
    => '1 word, different conjunction';

