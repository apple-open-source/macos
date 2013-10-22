use ExtUtils::MakeMaker;

require 5.005;

use Getopt::Long;

my $renum;

GetOptions('renumber' => \$renum);

do 'util/renum.pl' if $renum;

my %PREREQ_PM =
    (
    'List::Util'	=> 0,
    'Scalar::Util'	=> 0,
    'Math::Complex'	=> 0,
    'Test::More'	=> 0
    );

if ($] >= 5.008) {
    $PREREQ_PM{'Storable'} = '2.05';
    $PREREQ_PM{'Safe'	 } = 0,
}

WriteMakefile(
	      NAME            => 'Graph',
	      VERSION_FROM    => 'lib/Graph.pm',
	      PREREQ_PM	      => \%PREREQ_PM,
              AUTHOR => 'Jarkko Hietaniemi <jhi@iki.fi>',
              ($ExtUtils::MakeMaker::VERSION >= 6.3002 ?
	       ('LICENSE'=> 'perl', ) : ()),
	      );

# Some Debian distributions have a broken List::Util (see rt.cpan.org #9568)
eval 'require Scalar::Util; import Scalar::Util qw(weaken)';
if ($@) {
    die <<__EOF__;
$@
You do not have Scalar::Util::weaken, cannot continue, aborting.
__EOF__
}
