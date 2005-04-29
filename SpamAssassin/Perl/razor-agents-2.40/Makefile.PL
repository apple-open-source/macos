#!/usr/bin/perl -s
##
## Makefile for Vipul's Razor v2.
##
## Copyright (c) 1998-2002
## This code is free software; you can redistribute it and/or modify
## it under the same terms as Perl itself.
##
## $Id: Makefile.PL,v 1.1 2004/04/19 17:50:30 dasenbro Exp $

use ExtUtils::MakeMaker;

WriteMakefile (

    DISTNAME      => 'razor-agents',
    NAME          => 'razor-agents',
    AUTHOR        => 'Vipul Ved Prakash <mail@vipul.net>',
    ABSTRACT      => 'Collaborative, content-based spam filtering network.',
    DISTNAME      => 'razor-agents', 
    VERSION_FROM  => 'lib/Razor2/Client/Version.pm', 
    EXE_FILES     => [ qw( bin/razor-client ) ], 
    PREREQ_PM     => { 
                       'Net::Ping'        => 0,
                       'Net::DNS'         => 0,
                       'Time::HiRes'      => 0,
                       'Digest::SHA1'     => 0,
                       'Getopt::Long'     => 0,
                       'File::Copy'       => 0,
                       'URI::Escape'      => 0,
                       'File::Spec'       => 0,
                     },
	MAN1PODS      => { 
                       'docs/razor-check.pod'    => '$(INST_MAN1DIR)/razor-check.1',
                       'docs/razor-report.pod'   => '$(INST_MAN1DIR)/razor-report.1',
                       'docs/razor-admin.pod'    => '$(INST_MAN1DIR)/razor-admin.1',
                       'docs/razor-revoke.pod'   => '$(INST_MAN1DIR)/razor-revoke.1',
                     },
);

{
package MY;

sub constants {

    my $self = shift;
    my $inherited = $self->SUPER::constants(@_);

    my $prefix = $ExtUtils::MakeMaker::VERSION >= 6.18 ? 'SITEPREFIX' : 
                                                         'PREFIX';

    my $man5 = sprintf <<'MAKE', $prefix;

# begin razor-agents
MAN5PODS = docs/razor-agent.conf.pod \
	   docs/razor-whitelist.pod  \
	   docs/razor-agents.pod
INST_MAN5DIR = blib/man5
INSTALLMAN5DIR = $(%s)/man/man5
DESTINSTALLMAN5DIR = $(DESTDIR)$(INSTALLMAN5DIR)
MAN5EXT = 5
# end razor-agents

MAKE

    return $inherited . $man5;

}


sub MY::top_targets {

    use Config;

    my $self = shift;
    my $inherited = $self->SUPER::top_targets(@_);

    my ($perlh) = $self->catfile($Config{archlibexp}, 'CORE', "perl.h");

    my $noecho = q{NOECHO = @} unless $inherited =~ /NOECHO/;

    my $man5 = sprintf <<'MAKE_FRAG', $perlh, $perlh;

# begin razor-agents
config :: $(INST_MAN5DIR)/.exists
	@$(NOOP)

$(INST_MAN5DIR)/.exists :: %s
	@$(MKPATH) $(INST_MAN5DIR)
	@$(EQUALIZE_TIMESTAMP) %s $(INST_MAN5DIR)/.exists
	-@$(CHMOD) $(PERM_RWX) $(INST_MAN5DIR)
# end razor-agents

MAKE_FRAG

    return $inherited . $man5;
}


sub MY::manifypods {
    my $self = shift;
    my $inherited = $self->SUPER::manifypods(@_);

    $inherited =~ s{^(manifypods : .*)\\}{$1 manifypods-razor \\}m;

    # MakeMaker 6.06_x through 6.13 eliminated the POD2MAN macro.
    my $pod2man = $inherited =~ /^POD2MAN\b/m ? ''
                                              : 'POD2MAN = $(POD2MAN_EXE)';

    my $manifypods_razor = sprintf <<'MAKE_FRAG', $pod2man;

# begin razor-agents
%s

manifypods-razor : docs/razor-agent.conf.pod \
	docs/razor-agents.pod                \
	docs/razor-whitelist.pod
	$(POD2MAN) \
	docs/razor-agent.conf.pod \
	$(INST_MAN5DIR)/razor-agent.conf.$(MAN5EXT) \
	docs/razor-agents.pod \
	$(INST_MAN5DIR)/razor-agents.$(MAN5EXT) \
	docs/razor-whitelist.pod \
	$(INST_MAN5DIR)/razor-whitelist.$(MAN5EXT)

#end razor-agents
MAKE_FRAG

    return $inherited . "\n" . $manifypods_razor;

}


sub MY::install {

  my $self = shift;
  my $inherited = $self->SUPER::install(@_);

  $inherited =~ s{^(install :: .*)}{$1 install_razor_agents}m;

  my $install_razor_agents = q{

# begin razor-agents
install_razor_agents ::
	$(INST_SCRIPT)/razor-client
# end razor-agents

};

  my $man5 = q{ \\
		$(INST_MAN5DIR) $(DESTINSTALLMAN5DIR)};

  $inherited =~ s/(\$\((?:DEST)?INSTALL\w*MAN1DIR\))/$1$man5/gm;

  return $inherited . $install_razor_agents;
}

}
