use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'	=> 'Parse::Yapp',
    'VERSION_FROM' => 'lib/Parse/Yapp/Driver.pm', # finds $VERSION
    'MAN3PODS' => { 'lib/Parse/Yapp.pm' => '$(INST_MAN3DIR)/Parse::Yapp.3' },
    'PM' => {
              'lib/Parse/Yapp.pm' => '$(INST_LIBDIR)/Yapp.pm',
              'lib/Parse/Yapp/Options.pm' => '$(INST_LIBDIR)/Yapp/Options.pm',
              'lib/Parse/Yapp/Driver.pm' => '$(INST_LIBDIR)/Yapp/Driver.pm',
              'lib/Parse/Yapp/Grammar.pm' => '$(INST_LIBDIR)/Yapp/Grammar.pm',
              'lib/Parse/Yapp/Lalr.pm' => '$(INST_LIBDIR)/Yapp/Lalr.pm',
              'lib/Parse/Yapp/Output.pm' => '$(INST_LIBDIR)/Yapp/Output.pm',
              'lib/Parse/Yapp/Parse.pm' => '$(INST_LIBDIR)/Yapp/Parse.pm'
            },
	'EXE_FILES' => [ 'yapp' ],
);

sub MY::postamble {
<<'EOT';
YAPPPARSE = lib/Parse/Yapp/Parse.pm

$(YAPPPARSE) :: YappParse.yp
	$(PERL) -I./lib yapp -m 'Parse::Yapp::Parse' -o 'lib/Parse/Yapp/Parse.pm' YappParse.yp
EOT
}
