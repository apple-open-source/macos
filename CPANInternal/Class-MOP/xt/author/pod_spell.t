use strict;
use warnings;

use Test::Spelling;

my @stopwords;
for (<DATA>) {
    chomp;
    push @stopwords, $_
        unless /\A (?: \# | \s* \z)/msx;    # skip comments, whitespace
}

add_stopwords(@stopwords);
local $ENV{LC_ALL} = 'C';
set_spell_cmd('aspell list -l en');
all_pod_files_spelling_ok;

__DATA__
## personal names
Aankhen
Aran
autarch
blblack
chansen
chromatic's
Debolaz
Deltac
dexter
ewilhelm
Goulah
gphat
groditi
Hardison
jrockway
Kinyon
Kinyon's
Kogman
kolibrie
konobi
lbr
McWhirter
merlyn
mst
nothingmuch
Pearcey
perigrin
phaylon
Prather
Ragwitz
rafl
Reis
rindolf
rlb
robkinyon
Rockway
Roditi
Rolsky
Roszatycki
sartak
Sedlacek
Shlomi
SL
stevan
Stevan
Vilain
wreis
Yuval
Goro

## proper names
AOP
CLOS
cpan
CPAN
OCaml
ohloh
SVN

## Class::MOP
CLR
JVM
MetaModel
metamodel
metaclass
metaclass's
BUILDARGS
MOPs
Metalevel

## computerese
API
APIs
Baz
clearers
continutation
datetimes
definedness
deinitialized
destructor
destructors
DWIM
eval'ing
hashrefs
Immutabilization
immutabilization
immutabilize
immutabilized
Inlinable
inline
invocant
invocant's
irc
IRC
isa
login
metadata
mixin
mixins
munge
namespace
namespaced
namespaces
namespace's
namespacing
OO
OOP
ORM
overridable
parameterizable
parameterize
parameterized
parameterizes
pluggable
prechecking
prepends
rebless
reblessing
runtime
sigil
sigils
stacktrace
subclassable
subtyping
TODO
unblessed
unexport
UNIMPORTING
uninitialize
Unported
unsets
unsettable
Whitelist

## other jargon
bey
gey

## neologisms
breakability
hackery
wrappee

## compound
# half-assed
assed
# role-ish, Ruby-ish, medium-to-large-ish
ish
# kool-aid
kool
# pre-5.10
pre
# vice versa
versa
# foo-ness
ness

## slang
C'mon
might've
Nuff

## things that should be in the dictionary, but are not
attribute's
declaratively
everyone's
human's
initializers
newfound
reimplements
reinitializes
specializer
unintrusive

## misspelt on purpose
emali
