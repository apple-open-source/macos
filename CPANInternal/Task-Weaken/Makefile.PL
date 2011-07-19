# Dear Distribution Packagers
#
# Hi there!
#
# Before we begin, I'm really really sorry for what you are about to
# encounter a bit lower down in this file.
#
# Trust me, I didn't want to WRITE it just as much as you are probably
# not going to want to READ it and try to work out how the hell it
# applies to your Linux/etc distribution.
#
# So lets try to shortcut the whole confusion thing and save us both
# some time.
#
# Some distributions have this problem where the Scalar::Util version
# in the Perl core gets built WITHOUT a C compiler available. When this happens,
# most of the functions in Scalar::Util that need the compiler are replaced
# with a Pure-Perl version that works identically...
#
# ...except for two function relating to "weak references", a reference
# that is intentionally not counted for the purposes of garbage collection.
#
# It's used to implement certain specific data structures, like circular
# references and parent links in tree structures, in a way that can safely
# be garbage collected and won't leak.
#
# So what you really should check FIRST, is that the Perl build in your
# distribution has the Scalar::Util::weaken function defined.
#
# If not, then your Perl build is probably broken, and your solution
# ultimately needs to be applied to Perl itself. Then the whole problem
# goes away.
#
# If Scalar::Util::weaken is defined in your core Perl distribution, then
# none of the edge cases described below apply, and everything will be
# just rosy.
#
# Best Regards
#
# Adam K

use strict;
use inc::Module::Install 0.91;

all_from      'lib/Task/Weaken.pm';
test_requires 'Test::More' => '0.42';
test_requires 'File::Spec' => '0.80';

check_scalar_util();

WriteAll;

sub check_scalar_util {
	# Try to load Scalar::Util
	eval {
		require Scalar::Util;
	};
	if ( $@ ) {
		# Scalar::Util not installed (really old Perl?)
		requires 'Scalar::Util' => '1.14';
		return;
	}

	# Is installed
	my $module_version = $Scalar::Util::VERSION;
	my $module_weaken  = !! defined &Scalar::Util::weaken;
	if ( $module_weaken ) {
		# Already have weaken support.
		# Make sure it is a new enough version
		requires 'Scalar::Util' => '1.14';
		return;
	}

	# Does NOT have weaken, so either a very old
	# Scalar::Util, or a broken one.
	if ( $module_version <= 1.01 ) {
		# Very old Scalar::Util, upgrade it
		requires 'Scalar::Util' => '1.14';
		return;
	}

	# Broken Scalar::Util
	# That is, it does NOT contain the weaken function
	# which means it was built without a compiler.
	unless ( can_cc() ) {
		# The user does not have a compiler
		# There is currently no way to resolve
		# this situation, but we should probably
		# try to upgrade Scalar::Util anyway, just
		# in case the author is able to come up
		# with a solution at some point in the
		# future.
		message1();
		requires 'Scalar::Util' => '1.19';
		return;
	}

	# User should be capable of installing an
	# upgraded version.
	# Can we be certain they WILL do the upgrade?
	if ( $module_version < 1.19 ) {
		# We should be able to do a straight
		# forward upgrade of the module
		requires 'Scalar::Util' => '1.19';
		return;
	}

	# They ALREADY have the newest version... and it is broken.
	# Very little we can do in this case, other than issue a
	# message to the user, and then add a high dependency in the
	# vague hope it does something.
	message2();
	requires 'Scalar::Util' => '1.19';
	return;
}

sub message1 {
	print "\n\n\n";
	print "    ERROR:\n\n";
	print "    A CPAN module critically requires a function\n";
	print "    (Scalar::Util::weaken) that can only be provided by\n";
	print "    upgrading your Scalar::Util module to a bew version,\n";
	print "    which will need a C compiler in order to install.\n\n";
	print "    Unfortunately, I can't seem to locate a C compiler on this\n";
	print "    computer.\n\n";
	print "    I'm going to try to continue anyway, but the most likely result\n";
	print "    is going to be an extremely noisy series of testing errors.\n\n";
	print "    If this happens, you will need to install a C compiler\n";
	print "    (such as gcc) and then try to install whatever it is you are\n";
	print "    installing again.\n\n";
	print "    During the second attempt, I should be able to find the C\n";
	print "    compiler and be able to build the needed function without\n";
	print "    having to bother you again.\n\n";
	print "    I'm going to wait for about a minute now in the hope you read\n";
	print "    this\n\n\n";
	sleep( 50 );
}

sub message2 {
	print "\n\n\n";
	print "    ERROR:\n\n";
	print "    A CPAN module critically requires a function\n";
	print "    (Scalar::Util::weaken) that should exist in your\n";
	print "    Scalar::Util module but doesn't.\n\n";
	print "    This probably happened because you are using a Perl\n";
	print "    provided by a binary package from a vendor, and this\n";
	print "    vendor has packaged Perl incorrectly.\n\n";
	print "    I have checked for a couple of potential workarounds\n";
	print "    but none of them appear to be usable in your\n";
	print "    situation.\n\n";
	print "    I will try a last-ditch option anyway, but the most\n";
	print "    likely result is a number of noisily failing tests\n\n";
	print "    If this happens, you will need to contact technical\n";
	print "    support for your vendor and report the broken Perl,\n";
	print "    so that they can repair it.\n\n";
	print "    Please refer them to the documentation for the\n";
	print "    'Task::Weaken' CPAN module, which explains the problem\n";
	print "    and how they can fix it.\n\n";
	print "    I'm going to wait for about a minute now so you have time\n";
	print "    to read this message\n\n\n";
	sleep( 50 );
}
