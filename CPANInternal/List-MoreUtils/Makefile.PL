# More or less identical with Scalar-List-Util's Makefile.PL

use 5.00503;
use ExtUtils::MakeMaker;
use Config;

for (@ARGV) {
    /^-pm/ and $no_xs = 1;
    /^-xs/ and $no_xs = 0;
}

sub init {
    my $hash = $_[1];
    if ($no_xs) {
	@{$hash}{XS,C} = ( {}, [] );
    }
    $hash;
}

sub no_cc {
    $no_xs = 1;
    print <<"EDQ";

I cannot determine if you have a C compiler
so I will install a perl-only implementation

You can force installation of the XS version with

perl Makefile.PL -xs

EDQ
    write_makefile();
    exit;
}

sub write_makefile {
    WriteMakefile(
	NAME		    => 'List::MoreUtils',
	VERSION_FROM	    => 'lib/List/MoreUtils.pm', 
	PREREQ_PM	    => {}, 
	($] >= 5.005 ?     
	  (ABSTRACT_FROM    => 'lib/List/MoreUtils.pm', 
	   AUTHOR	    => 'Tassilo von Parseval <tassilo.von.parseval@rwth-aachen.de>') : ()),
	DEFINE		    => '-DPERL_EXT',	# otherwise 'cxinc' isn't defined 
	CONFIGURE	    => \&init,
	clean		    => { FILES => 'test.c' }
    );
}

write_makefile();

exit if defined $no_xs;

print "Testing if you have a C compiler\n";

open F, ">test.c" or no_cc();
print F "int main() { return 0; }\n";
close F or no_cc();
system ("$Config{make} test$Config{obj_ext}") and no_cc();
