
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

$PACKAGE = 'Class::DBI';
($PACKAGE_FILE) = $PACKAGE =~ /::([^:]+)/;
$LAST_API_CHANGE = 0.93;

require 5.006;

eval "require $PACKAGE";

unless ($@) { # Make sure we did find the module.
    print <<"CHANGE_WARN" if ${$PACKAGE.'::VERSION'} < $LAST_API_CHANGE;

NOTE: There have been API changes between this version and any older than
version $LAST_API_CHANGE!  Please check carefully if you are upgrading
from a version older than $LAST_API_CHANGE.

Significant changes:

  0.95 - Breaking the encapsulation of an object (e.g. assuming that 
         the value for the 'title' column will be stored internally in
         $object->{'title'} will stop working in a forthcoming release.
         Relying on this behaviour is a Bad Thing, and you should fix it 
         now! (This also applies to before_create triggers, where 
         previously it was the supported approach.)

  0.93 - Undocumented ordered_search() method no longer exists
       - Undocumented make_filter() method no longer exists
       - Undocumented option for add_constructor to do %s subitutions removed
       - single_value_select() no longer takes raw SQL fragments

  0.91 - on_setting() trigger point replaced by new individual column
          triggers (see documentation on constraints)
       - runtime database errors now throw exceptions
         - all errors standardised so on_failed_create no longer needed
       - objects now overload in string or boolean context

  0.90 - hasa is now deprecated in favour of has_a. Therefore
          has_many no longer creates reciprocal hasa relationship.

  0.85 - Class::DBI no longer supports pseudo-hash based objects
       - hasa_list is now deprecated in favour of has_many
       - has_many auto-creates a reciprocal hasa relationship

  0.32 - delete() now removes any foreign elements, to avoid orphans

CHANGE_WARN
}

sub MY::postamble { # DBD_PG_USER=tony make cover
	return <<''
cover:
	rm -rf cover_db
	PERL5OPT=-MDevel::Cover \$(MAKE) test || true
	cover cover_db -report html

}

WriteMakefile(
	NAME                => $PACKAGE,
	VERSION_FROM        => 'lib/Class/DBI.pm',
	PREREQ_PM           => {
		'Class::Accessor'          => '0.18',
		'Class::Data::Inheritable' => '0.02',
		'Class::Trigger'           => '0.07',
		'File::Temp'               => '0.12',
		'Ima::DBI'                 => '0.33',
		'List::Util'               => '1.00',
		'Scalar::Util'             => '1.08',
		'Clone'                    => 0,
		'Test::More'               => '0.47',
		'UNIVERSAL::moniker'       => '0.06',
		'version'                  => 0,
  },
	dist              => { 
		COMPRESS   => 'gzip -9',
		SUFFIX     => '.gz',
		DIST_DEFAULT   => 'all tardist',
	},
);
