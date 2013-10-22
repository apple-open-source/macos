
BEGIN {
  unless ($ENV{RELEASE_TESTING}) {
    require Test::More;
    Test::More::plan(skip_all => 'these tests are for release candidate testing');
  }
}

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
set_spell_cmd('aspell list -l en');

# This prevents a weird segfault from the aspell command - see
# https://bugs.launchpad.net/ubuntu/+source/aspell/+bug/71322
local $ENV{LC_ALL} = 'C';
all_pod_files_spelling_ok;

__DATA__
API
CPAN
Cholet
Dumont
Goess
Manfredi
Miyagawa
PayPal
Pfeiffer
Rolsky
STDERR
STDOUT
Schilli
Straup
Swartz
Tatsuhiko
UTF
apache
appenders
auth
authpriv
autoflushed
classname
crit
emerg
filename
ident
kern
logopt
params
smtp
stderr
subclass's
subclasses
uucp
