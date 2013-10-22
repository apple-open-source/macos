# vim:ts=4:sw=4:tw=78
# $Id: Build.PL 156 2010-07-02 10:08:59Z trevor $

use 5.006;

use strict;
use Module::Build;
use vars qw($build);
use FindBin;
use lib $FindBin::Bin . '/lib';

$@ = undef;
my @fs;
eval {
    require Sys::Filesystem;
    my $fs = Sys::Filesystem->new();
    @fs = $fs->filesystems();
};
if ($@)
{
    printf( STDERR "Can't be used ($@) - OS not supported or bad maintained?" );
    exit(0);
}

$build = Module::Build->new(
    module_name        => 'Sys::Filesystem',
    license            => 'apache',
    dist_author        => 'Jens Rehsack <rehsack@cpan.org>',
    create_makefile_pl => 'passthrough',
    create_readme      => 1,
    create_packlist    => 1,
    sign               => 0,

    requires => {
                  'perl'              => '5.008',
                  'Carp'              => 0,
                  'FindBin'           => 0,
                  'IO'                => 0,
                  'Module::Pluggable' => '3.9',
                  'Params::Util'      => '1.00',
                  ( $^O eq 'MSWin32' ? ( 'Win32::DriveInfo' => 0, ) : () ),
                },

    build_requires => { 'Test::More' => 0.90, },
    recommends     => {
                    'perl'                => '5.012001',
                    'Test::Pod'           => 0,
                    'Test::Pod::Coverage' => 0,
                  },
);

$build->create_build_script();

# Send perl and module version information home if we've been given
# permission to do so by a human being - default to not send for automated
# testing environments, of if the user does not respond within 20 seconds.

#my $url = $ENV{AUTOMATED_TESTING} ? undef : may_send_version_information();
#if ($url) {
#	my @resp = ();
#	eval {
#		local $SIG{ALRM} = sub { die; };
#		alarm 10;
#		my $ua = LWP::UserAgent->new(
#				agent => 'Build.PL $Revision: 380 $',
#				timeout => 9,
#				max_size => 500,
#			);
#		$ua->env_proxy;
#		my $response = $ua->get($url);
#		if ($response->is_success()) {
#			for (split(/\s*\n+\s*/, $response->content())) {
#				push @resp, $_ if $_;
#			}
#		}
#		alarm 0;
#	};
#	print substr($resp[0],0,79) || "Thank you for sending this information.";
#	print "\n\n";
#}
#
#sub may_send_version_information {
#	eval {
#		require Config;
#		require LWP::UserAgent;
#	};
#	return undef if $@;
#
#	my $str = sprintf('%s?%s=%s&%s=%s&%s=%s&%s=%s&%s=%s&%s=%s',
#			'http://perlgirl.org.uk/lib/usage.cgi',
#			'name',     $build->dist_name(),
#			'version',  $build->dist_version(),
#			'osname',   $Config::Config{osname},
#			'archname', $Config::Config{archname},
#			'osver',    $^O,
#			'perlver',  $]
#		);
#
#	print "\nThank you for downloading ".$build->dist_name()."\n\n";
#	print "I would like to find out how many people are using this software,\n";
#	print "and on what operating systems and Perl versions. If you have an\n";
#	print "internet connection, may I transmit the following information:\n\n";
#	print "$str\n\n";
#
#	my $send = 0;
#	eval {
#		local $SIG{ALRM} = sub { die; };
#		alarm 20;
#		$send = $build->y_n('Send this anonymous information?','n');
#		alarm 0;
#	};
#
#	return $send ? $str : undef;
#}

1;

