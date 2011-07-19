use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

my $Is_Beta = 0;

WriteMakefile(
	      'NAME'	     => 'Expect',
	      'VERSION_FROM' => 'Expect.pm',
	      'PREREQ_PM'    => {
                                  'IO::Tty' => 1.03,
                                  'IO::Pty' => 1.03,
				},
	      'clean'        => { 'FILES' => '*.log' },
	      'dist'         => { COMPRESS => 'gzip -9f', SUFFIX => 'gz' },
	      );

print <<'__EOT__' if $Is_Beta;
**********************************************************************
WARNING: this is a BETA version.  If it works, good for you, if not,
tell me, <RGiersig@cpan.org> about it and I'll see what I can do.
**********************************************************************
__EOT__
