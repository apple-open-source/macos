use ExtUtils::MakeMaker;

WriteMakefile(
    'NAME'         => 'Apache::DBI',
    'VERSION_FROM' => 'lib/Apache/DBI.pm',
    'PREREQ_PM'    => { 
            'Test::More'   => 0,    # without tests we don't know fun.
			'DBI'          => 1.00, # no DBI can never be any fun.
            'Digest::SHA1' => 2.01, # SHA1 Digest for alternate encryption method fun.
            'Digest::MD5'  => 2.20  # MD5 Digest for even more fun.
		      },
);

# EOF
