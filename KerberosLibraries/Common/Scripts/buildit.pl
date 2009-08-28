#!/usr/bin/perl -U
# rcbintool

# push plib onto search path
BEGIN {
    if ($ENV{'PERL_LIB'}) {
	push(@INC, $ENV{'PERL_LIB'});
    } else {
        my($plib) = `echo ~rcdev/xbs-Current/plib`;
	chomp($plib);
	push(@INC, $plib);
    }
}

# required libraries
use File::Basename;
use Getopt::Long;
use Cwd;
#require 'hostname.pl';

# flush output after every write 
select((select(STDOUT), $| = 1)[0]);
select((select(STDERR), $| = 1)[0]);

if ( $> ) {
    my ($args) = join(' ', @ARGV);
    ($progname = $0) =~ s|^.*/||;
    print "\n";
    print "*** WARNING: buildit should be run as root. We recommend using the\n";
    print "*** following command to run this buildit attempt as the root user:\n";
    print "***\n";
    print "***     sudo $progname $args\n"; 
    sleep 10;
#} elsif ( $ENV{'GROUP'} ne "wheel") {
#    my ($args) = join(' ', @ARGV);
#    ($progname = $0) =~ s|^.*/||;
#    print "\n";
#    print "*** WARNING: this root account isn't properly configured. (Perhaps you\n";
#    print "*** typed \"su\" instead of \"su -\" to become root?)\n";
#    print "*** We recommend using the following command to run this buildit\n";
#    print "*** attempt as the root user:\n";
#    print "***\n";
#    print "***     sudo $progname $args\n"; 
#    sleep 15;
}

# set group to wheel
$ENV{GROUP} = 'wheel' if $> == 0;

# set environment variables
$ENV{'PATH'} = '/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin';
$ENV{'RC_OBJCUNIQUE_SILENT'} = 'YES';
$ENV{'INSTALLED_PRODUCT_ASIDES'} = 'YES';
$ENV{'SEPARATE_STRIP'} = 'YES';

# set umask
umask(022);

# define list of supported architectures
@archList = (
    'hppa',
    'i386',
    'm68k',
    'ppc',
    'ppc64',
    'sparc'
);

# set host name
chomp($hostname = `hostname`);
#$hostname = &hostname();

# get file name of this program 
($buildit = $0) =~ s|^.*/(.*)$|$1|;

# execute and exit
&main();


################################ SUBROUTINES ################################

sub usage
{
    print "@_\n" if @_;
    print <<END_OF_USAGE;
Usage: $buildit <projectSourceDirectory> 
	[-release <release name>]
	[-arch <archName>]*
	[-noclean]
	[-noinstall]
	[-noinstallhdrs]
	[-noinstallsrc]
	[-noverify]
	[-nosum]
	[-noprebind]
	[-noperms]
	[-notilde]
	[-merge <mergeDir>]
	[-target <buildTarget>]
	[-buildstyle <buildStyle>]
	[-project <projectName>]
	[-othercflags <cflags>]
	[-- other arguments]
For more detailed information, enter \"$buildit -help\".
END_OF_USAGE
    exit 1;
}

sub help
{
    print <<END_OF_HELP;
Usage: $buildit projectSourceDirectory [build options] [-- other arguments]

Mandatory argument:
    projectSourceDirectory      The root directory of project source tree
        (can be absolute or relative path to the project source tree)

Build options:
    -release                    Use RC_CFLAGS (compiler flags) from specified release
        (use release name like Cheetah, Puma, Jaguar, ...)
    -arch <archName>            Build with specified architectures(s)
        (-arch can be multiply specified: -arch ppc -arch ppc64 -arch i386)
        (default is native architecture if no -arch is specified)
    -noclean                    Disables deletion of {OBJ,SYM,DST}ROOT before start of build
    -noinstall                  Disables invocation of install (build) target
    -noinstallhdrs              Disables invocation of installhdrs target
    -noinstallsrc               Disables invocation of installsrc target
    -noverify                   Disables verification of project DSTROOT after build
    -nosum                      Disables before-and-after checksumming of project source
    -noprebind                  Disables prebinding
    -noperms                    Disables permissions setting on DSTROOT after build
    -notilde                    Disables inclusion of '~' character in DSTROOT, etc., paths
    -noarchwarn                 Disables warning about not building architecture for local machine
    -merge <mergeDir>           Merges resulting DSTROOT into \"mergeDir\"
    -target <buildTarget>       Specifies an alternate Make or Jam build target
    -buildstyle <buildStyle>    Specifies a ProjectBuilder buildstyle
    -project <projectName>      Project name to be used for verification exceptions
    -othercflags <cflags>       Specified cflags are appended to RC_CFLAGS for build
    
Other arguments:
    Other arguments can be passed to the build (that is, to Make or Jam) by putting them
    at the end of the buildit argument list following a double dash (\"--\").
    Arguments that contain whitespace should be enclosed in quotes.
    
Examples:

    # Build in the current directory using all defaults
    buildit .
    
    # Build in specified directory for ppc architecture
    buildit myProjectDirectory -arch ppc
    
    # Build using install_debug Make or Jam target
    buildit myProjectDirectory -target install_debug
    
    # Only install headers and merge result into system root
    buildit myProjectDirectory -noinstall -merge /
    
    # Use existing {OBJ,SYM,DST}ROOT's and pass additional compiler flags
    buildit myProjectDirectory -noclean -othercflags \"-DMY_SWITCH -O3 -no-nasty-warnings\"
    
    # Suppress installsrc and pass other arguments to Make or Jam
    buildit myProjectDirectory -noinstallsrc -- -d \"NEXT_ROOT = /Network/Servers/builder/BuildRoot\"
    
END_OF_HELP
    exit 0;
}

sub parseArgs
{
    # default values
    $release		= '';
    @archs		= ();
    $noclean		= '';
    $noinstall		= '';
    $noinstallhdrs	= '';
    $noinstallsrc	= '';
    $noverify		= '';
    $nosum		= '';
    $noprebind		= '';
    $noperms		= '';
    $notilde		= '';
    $noarchwarn		= '';
    $merge		= '';
    $target		= '';
    $buildstyle		= '';
    $project		= '';
    $othercflags	= '';
    $help		= '';
    
    # parse arguments for options
    &usage() unless @ARGV and GetOptions(
	'release=s'	=> \$release,
	'arch:s@'	=> \@archs,
	'noclean'	=> \$noclean,
	'noinstall'	=> \$noinstall,
	'noinstallhdrs'	=> \$noinstallhdrs,
	'noinstallsrc'	=> \$noinstallsrc,
	'noverify'	=> \$noverify,
	'nosum'		=> \$nosum,
	'noprebind'	=> \$noprebind,
	'noperms'	=> \$noperms,
	'notilde'	=> \$notilde,
	'noarchwarn'	=> \$noarchwarn,
	'merge=s'	=> \$merge,
	'target=s'	=> \$target,
	'buildstyle=s'	=> \$buildstyle,
	'project=s'	=> \$project,
	'othercflags=s'	=> \$othercflags,
	'help'		=> \$help,
    );
    
    # check for help option
    &help() if $help;
    
    # make sure that release is defined
    $release = &defaultRelease() unless $release;
    warn "**** WARNING **** Can't get the release name for the OS version on your machine\n" unless $release;

    # If no arch specified, use the host arch
    if (scalar(@archs) == 0) {
        chomp(my ($arch) = `arch`);
        push(@archs, $arch);
    }
    
    # check that all specified archs are supported
    my @bogusArchs = ();
    my %knownArch;
    grep($knownArch{$_}++, @archList);
    foreach my $arch ( @archs ) {
        push(@bogusArchs, $arch) unless $knownArch{$arch}; 
    }
    die "**** ERROR **** The following specified archs are not supported by buildit: @bogusArchs\n" if @bogusArchs;

    # warn if none of the specified archs match this machine's arch and we're merging
    # note: allow "ppc64" to match local arch of "ppc"
    if ($merge and !$noarchwarn) {
        my $myArch = `arch`;
        chomp($myArch);
        my @matches = grep(/^$myArch/, @archs);
        unless (@matches) {
            print "**** WARNING **** The root you are about to produce does not contain the architecture of the machine on which you're building. "; 
            while (1) {
                print "Continue? ([y]/n) ";
                chomp($reply = <STDIN>);
                if ($reply eq "y" or $reply eq "Y" or $reply eq "") {
                    last;
                }
                if ($reply eq "n" or $reply eq "N") {
                    exit(1);
                }
            }
        }
    }
    
    # get the view path
    $rc = (getpwnam('rc'))[7];
    $viewPath = "$rc/Software/$release" if $rc;
    
    # get project source directory (must be first remaining argument)
    &usage("No project directory specified") unless @ARGV;
    $projectSourceDirectory = shift(@ARGV);
    @otherArguments = @ARGV;
}

sub main
{
    my ($origNextRoot) = $ENV{'NEXT_ROOT'};
    
    &parseArgs();			# process arguments
    &sourceSetupForRelease($release);	# load our environment
#     &printenv(); 

    # figure out the LD_SEG_ADDR_TABLE shell variable.
    # 1) If LD_SEG_ADDR_TABLE is set, use that.
    # 2) If seg_addr_table exists, set LD_SEG_ADDR_TABLE
    # 3) Leave LD_SEG_ADDR_TABLE unset.
    if (!defined($ENV{'LD_SEG_ADDR_TABLE'})) {
	my ($seg_addr_table) = "/AppleInternal/Developer/seg_addr_table";
	if ( -f $seg_addr_table) {
	    print "setting LD_SEG_ADDR_TABLE to '$seg_addr_table'\n";
	    $ENV{'LD_SEG_ADDR_TABLE'} = $seg_addr_table;
	}
    }

    # MDT 6/29/2001 set LD_TWOLEVEL_NAMESPACE
    print "setting LD_TWOLEVEL_NAMESPACE\n";
    $ENV{'LD_TWOLEVEL_NAMESPACE'} = 'YES';

    # figure out which NEXT_ROOT to use:
    # 1) If NEXT_ROOT is set as a shell variable, use it.
    #### 2) Hack: If we're building natively on beaker, clear NEXT_ROOT.
    # 2) Hack: If we're build for beaker on a non-beaker system, set NEXT_ROOT
    #          for Matt Watson.
    # 3) Use the NEXT_ROOT specified by the build for this system
    # 3a) look at the build's .crossRoot file
    # 3b) assume we've set it in sourceSetupForRelease
    if ($origNextRoot) {
	$NEXT_ROOT = $origNextRoot;
	# $ENV{'NEXT_ROOT'} = $origNextRoot;
    } elsif ($release eq 'Beaker' && &defaultRelease() ne 'Beaker') {
	$NEXT_ROOT = '/Local/Public/MacOSX';
	# $ENV{'NEXT_ROOT'} = $NEXT_ROOT;
    } else {
	$setup = "$viewPath/Updates/Newest$release/.crossRoot";
	if (-f $setup) {
	    # We're on DBS. Use the value in the .crossRoot file
	    chomp($NEXT_ROOT = `cat $setup`);
	} else {
	    # We're on the old system. Trust sourceSetupForRelease
	    $NEXT_ROOT = defined($ENV{'NEXT_ROOT'}) ? $ENV{'NEXT_ROOT'} : '';
	}
	# $ENV{'NEXT_ROOT'} = $NEXT_ROOT;
    }

    # set environment for prebinding
    if ($noprebind) {
	delete($ENV{'LD_PREBIND'});
    } else {
	$ENV{'LD_PREBIND'}='';
    }
    
    # go to specified project source directory
    chdir($projectSourceDirectory)
	|| die "Can't change directory to $projectSourceDirectory: $!\n";
    $projectSourceDirectory = getcwd();
#    print "projectSourceDirectory = $projectSourceDirectory\n";

    # extract project and project version
    $projectVersion = &basename($projectSourceDirectory);
    ($project = $projectVersion) =~ s/^(.*)-.*$/$1/ unless $project;
    ($projectSourceVersion = $projectVersion) =~ s/^.*-(.*)$/$1/;
    $projectSourceVersion = '' unless $projectSourceVersion and $projectSourceVersion ne $projectVersion;
#    print "projectVersion = $projectVersion\n";
    
    # make the roots directory
    &makeRootsDirectory($projectVersion);
    
    # define the various source and binary directories
    if ($notilde) {
        $srcRoot        = "$rootsDirectory/$projectVersion";
        $objRoot        = "$rootsDirectory/$projectVersion.obj";
        $symRoot        = "$rootsDirectory/$projectVersion.sym";
        $dstRoot        = "$rootsDirectory/$projectVersion.dst";
        $hdrObjRoot     = "$rootsDirectory/$projectVersion.hdrObj";
        $hdrSymRoot     = "$rootsDirectory/$projectVersion.hdrSym";
        $hdrDstRoot     = "$rootsDirectory/$projectVersion.hdrDst";
        $buildLog       = "$rootsDirectory/$projectVersion.log";
        $hdrLog 	= "$rootsDirectory/$projectVersion.hdrlog";
        $preBom 	= "$rootsDirectory/$projectVersion.prebom";
        $postBom        = "$rootsDirectory/$projectVersion.postbom";
        $subRoot        = "/usr/local/objs";
    } else {
        $srcRoot	= "$rootsDirectory/$projectVersion";
        $objRoot	= "$rootsDirectory/$projectVersion~obj";
        $symRoot	= "$rootsDirectory/$projectVersion~sym";
        $dstRoot	= "$rootsDirectory/$projectVersion~dst";
        $hdrObjRoot	= "$rootsDirectory/$projectVersion~hdrObj";
        $hdrSymRoot	= "$rootsDirectory/$projectVersion~hdrSym";
        $hdrDstRoot	= "$rootsDirectory/$projectVersion~hdrDst";
        $buildLog	= "$rootsDirectory/$projectVersion~log";
        $hdrLog		= "$rootsDirectory/$projectVersion~hdrlog";
        $preBom		= "$rootsDirectory/$projectVersion~prebom";
        $postBom	= "$rootsDirectory/$projectVersion~postbom";
        $subRoot	= "/usr/local/objs";
    }

    # set location for compiler options file
     $ENV{'CC_PRINT_OPTIONS_FILE'} = "$symRoot/.CC_PRINT_OPTIONS"
        if $ENV{'CC_PRINT_OPTIONS'};

    
    # get the build tool to be used for project build
    &getBuildTool($projectSourceDirectory);
    
    # do the installsrc
    if ($noinstallsrc) {
        $srcRoot = $projectSourceDirectory;
    } else {
        &installsrc();
    }
    
    # verify the sources
    &verifySrc($srcRoot)
        unless $noverify;

    # checksum the source directory
    &checksum($srcRoot, $preBom)
        unless $nosum;
    
    # empty out the binary roots
    &cleanRoots($objRoot, $symRoot, $dstRoot, 
                $hdrObjRoot, $hdrSymRoot, $hdrDstRoot)
        unless $noclean;

    # set up the build flags
    &buildParameters();

    # do the installhdrs
    &build('installhdrs', $hdrLog)
        unless $noinstallhdrs;

    # set permissions on installhdrs root
    &setPerms($hdrDstRoot) unless $noinstallhdrs or $noperms;

   # Merge headers
    &doMerge($hdrDstRoot, $hdrLog);
	
    # do the build
    &build('install', $buildLog)
        unless $noinstall;
	
    # set permissions
    &setPerms($dstRoot) unless $noinstall or $noperms;

    # verify the results
    &verify($dstRoot, $buildLog)
        unless $noverify;

    # Now that the root has had permissions fixed and been verified
    # we can merge it
    &doMerge($dstRoot, $buildLog);
	
    # checksum the source directory
    &checksum($srcRoot, $postBom)
        unless $nosum;
	
    # compare the prebom and postbom
    &compareBoms($preBom, $postBom)
        unless $nosum;
    
    # all done
    print "\n*** $buildit ***: Done\n";
    exit(0);
}

sub makeRootsDirectory
{
    my($projectVersion) = @_;
    
    # figure out where to put the roots directory
    if (defined($ENV{'BUILDIT_DIR'} && $ENV{'BUILDIT_DIR'})) {
	$rootsDirectory = "$ENV{'BUILDIT_DIR'}/$projectVersion.roots";
    } else {
	$rootsDirectory = "/private/tmp/$projectVersion.roots";
    }
    
    # make the roots directory
    &mkdirs($rootsDirectory) unless -d $rootsDirectory;
}

sub mkdirs
{
    my($dir) = @_;
    
    # recursively make all directories on path
    if (! -d $dir) {
	&mkdirs(&dirname($dir));
	mkdir($dir, 0755) || die "Can't mkdir $dir: $!\n";
    }
}

sub getBuildTool
{
    my($projectSourceDirectory) = @_;

    # first figure out whether the projectInfo command is installed
    $projectInfo = (-x '/usr/bin/projectInfo') ? '/usr/bin/projectInfo' : '';
    if (!$projectInfo) {
	$buildTool = 'make';
	$buildDirectory = '';
	$buildParams{'RC_OS'}  = 'nextstep';
	return;
    }
    
    # if we have an explicit OS name, use that.
    if ( -f "$viewPath/Updates/Newest$release/.osName" ) {
	chomp($buildParams{'RC_OS'} = `cat $viewPath/Updates/Newest$release/.osName`);
    } elsif ( -f "$viewPath/.osName" ) {
	chomp($buildParams{'RC_OS'} = `cat $viewPath/.osName`);
    } elsif ( -f "$viewPath/.releaseOSName" ) {
	chomp($buildParams{'RC_OS'} = `cat $viewPath/.releaseOSName`);
    } else {
    
	# now get OS name from pb_makefiles
	if ( -d "/System/Developer/Makefiles/pb_makefiles" ) {
	    $buildParams{'RC_OS'}  = 'teflon';
	} elsif ( -f "$NEXT_ROOT/NextDeveloper/Makefiles/pb_makefiles/platform.make" ) {
	    $platformFile = "$NEXT_ROOT/NextDeveloper/Makefiles/pb_makefiles/platform.make";
	    open(PLATFORMFILE, "< $platformFile")
		|| die "Can't open $platformFile: $!\n";
	    while (<PLATFORMFILE>) {
		m/^\s*(\S*)\s*=\s*(\S*)\s*$/;
		if ($1 eq 'PLATFORM_OS') {
		    $buildParams{'RC_OS'} = $2;
		}
	    }
	    close(PLATFORMFILE);
	    if (!$buildParams{'RC_OS'}) {
		die "Can't get PLATFORM_OS value from $platformFile\n";
	    }
	} else {	# default to NEXTSTEP
	    $buildParams{'RC_OS'}  = 'nextstep';
	}
    }
    
    # first try for OS-specific build tool property
    chomp($buildTool = `$projectInfo NEXTSTEP_BUILDTOOL -inDirectory "$projectSourceDirectory"`);
    die "$projectInfo reported errors accessing key NEXTSTEP_BUILDTOOL in $projectSourceDirectory"
	if $?;
	
    # if need be, try for generic property
    if (!$buildTool) {
	chomp($buildTool = `$projectInfo BUILDTOOL -inDirectory "$projectSourceDirectory"`);
	die "$projectInfo reported errors accessing key BUILDTOOL in $projectSourceDirectory"
	    if $?;
    }
    
    # couldn't get it, use default
    if (!$buildTool) {
	$buildTool = 'make';
	$buildDirectory = '';
# 	This looks like a bad idea to me - Mike Trent
# 	$buildParams{'RC_OS'} = (-d "/System/Developer/Makefiles/pb_makefiles" ) ? 'teflon' : 'nextstep';
	return;
    }
    
    # we can use the presence of the BUILDDIR property to distinguish old project makefiles
    $buildDirectory = `$projectInfo NEXTSTEP_BUILDDIR -inDirectory "$projectSourceDirectory"`;
    die "$projectInfo reported errors accessing key NEXTSTEP_BUILDDIR in $projectSourceDirectory"
	if $?;
    if (!$buildDirectory) {
	$buildDirectory = `$projectInfo BUILDDIR -inDirectory "$projectSourceDirectory"`;
	die "$projectInfo reported errors accessing key BUILDDIR in $projectSourceDirectory"
	    if $?;
    }
    chomp($buildDirectory);
    
#    print "buildTool is $buildTool\n";
#    print "buildDirectory is $buildDirectory\n";
}

sub installsrc
{
    # initialize the new source directory
    if (-d $srcRoot) {
	print "\n*** $buildit ***: Removing existing SRCROOT $srcRoot ...\n";
        &removeDirectory($srcRoot);
    }
    &mkdirs($srcRoot);
    
    # do the installsrc
    print "\n*** $buildit ***: Invoking installsrc target ...\n";
    print "$buildTool installsrc \"SRCROOT=$srcRoot\"\n";
    system($buildTool, 'installsrc', "SRCROOT=$srcRoot");
    if ($?) {
	print "Removing $srcRoot ... ";
	&removeDirectory($srcRoot);
	exit 1;
    }
    
    # clean the installed sources
    print "\n*** $buildit ***: Cleaning the installed sources ...\n";
    chdir($srcRoot)
	|| die "Can't change directory to $srcRoot: $!\n";
    if (!$buildDirectory) {
	print "$buildTool clean \"SRCROOT=$srcRoot\"\n";
	system($buildTool, 'clean', "SRCROOT=$srcRoot");
	if ($?) {
	    chdir("/")
		|| die "Can't change directory to /: $!\n";
	    print "Removing $srcRoot ... ";
	    &removeDirectory($srcRoot);
	    exit 1;
	}
    } else {
	print "$buildTool clean " .
	    "\"SRCROOT=$srcRoot\" " .
	    "\"OBJROOT=$srcRoot\" " .
	    "\"SYMROOT=$srcRoot\"\n";
	system($buildTool, 'clean',
	    "SRCROOT=$srcRoot", 
	    "OBJROOT=$srcRoot",
	    "SYMROOT=$srcRoot");
	if ($?) {
	    chdir("/")
		|| die "Can't change directory to /: $!\n";
	    print "Removing $srcRoot ... ";
	    &removeDirectory($srcRoot);
	    exit 1;
	}
    }
	
    # chmod the source to be unwritable
    print "\n*** $buildit ***: Chmod'ing $srcRoot ...\n";
    system('chmod', '-R', 'a-w', $srcRoot);
    if ($?) {
	chdir("/")
	    || die "Can't change directory to /: $!\n";
	print "Removing $srcRoot ... ";
	&removeDirectory($srcRoot);
	exit 1;
    }
}

sub scanForDylibsWithBadInstallNames
{
    my($dstroot) = @_;
    
    # tools and table must be available
    return unless -x "/usr/local/bin/check_dylib";
    return unless -x "/usr/bin/redo_prebinding";
    return unless -f "/Local/Developer/seg_addr_table";
    
    # chdir down to the dstroot
    my($cwd) = getcwd();
    chdir($dstroot) or die "**** ERROR **** Can't chdir to $dstroot\n";
    
    # run find command on the dstroot
    open(FIND, "find . -type f -exec /usr/bin/redo_prebinding -i -d {} \\; -print |")
        or die "Can't run find command on $dstroot\n";
	
    # process the found dylibs
    $dylibError = 0;
    while ($foundDylib = <FIND>) {
    
        # munge the dylib path
        chomp($foundDylib);
        $foundDylib =~ s|^\.||;
        $foundDylibPath = "$dstroot$foundDylib";
	
        # check the dylib
        $cmd = "/usr/local/bin/check_dylib $foundDylibPath -install_name $foundDylib -seg_addr_table /Local/Developer/seg_addr_table";
        # print "$cmd\n";
        $checkStat = system($cmd);
        $checkStat >>= 8;
        if ($checkStat == 2) {
            $dylibError = 1;
            print "\n**** ERROR **** Dylib $foundDylibPath has an incorrect install_name. The incorrect install_name can be examined by running \"otool -L $foundDylibPath\". The correct install_name is $foundDylib\n";
        }
	   
    }
    close(FIND);
    print "\n";
    if ($dylibError) {
        exit(1);
    }
    
    # chdir back to original directory
    chdir($cwd) or die "**** ERROR **** Can't chdir to $cwd\n";
}

sub setPerms
{
    my($dstRoot) = @_;
    my($rcdev) = (getpwnam('rcdev'))[7];
    
    print "\n*** $buildit ***: Setting permissions in $dstRoot ...\n";
    if ( ! -f "$rcdev/plib/Permissions.pm" ) {
        warn "Can't find Permissions.pm, permissions will not be set\n";
	return;
    }

    # reset the directory permissions
    require "Permissions.pm";
    &Permissions::applyPermissionsToDirectory($viewPath, $dstRoot);
}

sub verify
{
    my($dstroot, $log) = @_;
    local($verifyStatus, $verifyLog, $plib, $fatal);
    local($rcdev) = (getpwnam('rcdev'))[7];

    print "\n*** $buildit ***: Verifying $project $dstroot ...\n";

    # specialized dylib checking
    &scanForDylibsWithBadInstallNames($dstroot);

   # check for verification scripts
#     if ( ! -f "$rcdev/.Verification/Verify.config" ) {
# 	warn "Can't find ~rcdev/.Verification/Verify.config, skipping verification\n";
# 	return;
#     } elsif ( ! -f "$rcdev/plib/Verify.pm" ) {
# 	warn "Can't find ~rcdev/plib/Verify.pm, skipping verification\n";
# 	return;
#     }
	if ( ! -f "$rcdev/plib/Verify.pm" ) {
		warn "Can't find ~rcdev/plib/Verify.pm, skipping verification\n";
		return;
    }

    # include Verify.pm
    require "Verify.pm";

    # perform verification
    &Verify::readVerifyConfig("Newest$release");
    $verifyStatus = &Verify::verifyProject($project, $dstroot);
    $verifyLog = &Verify::errorLog();

    # print errors & warnings.
    if ($verifyLog) {
	$fatal = ($verifyStatus) ? ", and the overall project build has failed" : "";
	
	# print to console
        print "\nYour project has verification failures$fatal.  The list of verification failures follows below.\n\n";
	print "Consult the following web page for descriptions of these errors:\n\n";
	print "    http://xbs.apple.com/Docs/Verification.html\n\n";
	print "The verification failures are:\n\n";
	print "$verifyLog";
	
	# print to log
	open(LOG, ">>$log")
	    || die "Can't open $log: $!\n";
	select(LOG); $| = 1;
	select(STDOUT);
    print LOG "\nYour project has verification failures$fatal.  The list of verification failures follows below.\n\n";
	print LOG "Consult the following web page for descriptions of these errors:\n\n";
	print LOG "    http://xbs.apple.com/Docs/Verification.html\n\n";
	print LOG "The verification failures are:\n\n";
	print LOG "$verifyLog";
	close(LOG);
	
	die ("Verification failed with exit status $verifyStatus\n") if ($verifyStatus);
    }
}

sub verifySrc
{
    my($srcroot) = @_;
    local($verifyStatus, $verifyLog, $plib, $fatal);
    local($rcdev) = (getpwnam('rcdev'))[7];

    print "\n*** $buildit ***: Verifying $project $srcroot ...\n";

    if ( ! -f "$rcdev/plib/Verify.pm" ) {
	    warn "Can't find ~rcdev/plib/Verify.pm, skipping verification\n";
	    return;
    }

    # include Verify.pm
    require "Verify.pm";

    # locate the source verification file
    my $verifyConfig = -e "$viewPath/.Verification/SourceVerify.config" ?
			   "$viewPath/.Verification/SourceVerify.config" :
			   "$rcdev/xbs-Current/config/SourceVerify.config";    
    
    $verifyConfig = "SourceVerify.config" unless -e verifyConfig;
    
    # perform verification
    if (&Verify::readConfigFile($verifyConfig)) {
	print "skipping ... \n";
	return;
    }
    $verifyStatus = &Verify::verifyProject($project, $srcroot);
    $verifyLog = &Verify::errorLog();

    # print errors & warnings.
    if ($verifyLog) {
	$fatal = ($verifyStatus) ? ", and the overall project build has failed" : "";
        print "\nYour project sources have verification failures$fatal.  The list of verification failures follows below.\n\n";
	print "Consult the following web page for descriptions of these errors:\n\n";
	print "    http://xbs.apple.com/Docs/Verification.html\n\n";
	print "The verification failures are:\n\n";
	print "$verifyLog";
	die ("Verification failed with exit status $verifyStatus\n") if ($verifyStatus);
    }
}

sub checksum
{
    my($directory, $bomFile) = @_;
    
    # remove any existing bom file
    if (-f $bomFile) {
	unlink($bomFile)
	    || die "Can't unlink $bomFile: $!\n";
    }
    
    # checksum the source
    print "\n*** $buildit ***: Checksumming $srcRoot ...\n";
    executeCommand('mkbom', $directory, $bomFile);
}

sub compareBoms
{
    my($bom1, $bom2) = @_;
    my($tmp1, $tmp2) = ("$bom1.lsbom", "$bom2.lsbom");
    my($diffFile) = &dirname($bom1) . "/bomDiff.$$";
    
    # dump the boms to text files
    print "\n*** $buildit ***: Comparing source checksums ...\n";
    die "$bom1: $?\n"
	if system("lsbom $bom1 | sort > $tmp1");
    die "$bom2: $?\n"
	if system("lsbom $bom2 | sort > $tmp2");

    # compare the listings
    system("diff $tmp1 $tmp2 > $diffFile");
	
    # complain if diffs are found
    if (-s $diffFile) {
	print "**** WARNING **** The following project source files were modified:\n";
	system("cat $diffFile");
    }
    
    # clean up temp files
    unlink($tmp1, $tmp2, $diffFile);
}

sub cleanRoots
{
    my(@roots) = @_;
    
    # loop over the roots list
    print "\n*** $buildit ***: Cleaning up the binary roots ...\n";
    foreach $root (@roots) {
    
	# remove the root
	&removeDirectory($root)
	    if -d $root;
	
	# and recreate it
	&mkdirs($root);
    }
}

sub defaultRelease {
    local($file, $result, @list);
    
    # try sw_vers first
    if ( -x '/usr/bin/sw_vers' ) {
        open(SW_VERS, "/usr/bin/sw_vers |")
	    or die "**** ERROR **** Can't run /usr/bin/sw_vers: $!\n";
	while ($line = <SW_VERS>) {
	    next unless $line =~ m|BuildVersion:|;
	    chomp($line);
	    $systemBuildVersion = (split('\s+', $line))[1];
	    last if $systemBuildVersion;
	}
	close(SW_VERS);
	if ($systemBuildVersion) {
	    ($majorNumber = $systemBuildVersion) =~ s|^([0-9]+).*$|$1|;
	    if ($majorNumber) {
	        $mapFile = `echo ~rc/Data/release_to_majorNumber.map`;
		chomp($mapFile);
		if ( -f $mapFile) {
		    open(MAPFILE, "< $mapFile")
		        or die "**** ERROR **** Can't open $mapFile: $!\n";
		    while ($entry = <MAPFILE>) {
		        next unless $entry =~ m|^\s*[0-9]|;
			chomp($entry);
			$entry =~ s|^\s+||;
			($number, $releaseName) = split('\s+', $entry);
			if ($number == $majorNumber) {
			    return $releaseName;
			}
		    }
		}
	    }
	}
    }

    @list = (
	'/System/Library/CoreServices/release_name', 
 	'/System/Library/CoreServices/software_version', 
	'/NextLibrary/CoreServices/release_name',
 	'/NextLibrary/CoreServices/software_version',
 	'/usr/lib/NextStep/software_version',
    ); 
    foreach $file (@list) {
	if (-f $file) {
 	    chomp ($result = `cat $file | tail -1`);
 	    $result =~ s/\d+[a-zA-Z]\d*[a-z]?$//;
#	    chomp ($result = `cat $file`);
	    last;
	}
    }

    return $result;
}

# MDT deprecated by sourceSetupForRelease
sub cflagsForRelease {
    local($release) = @_;
    local($result,$setup, @lines, $line);

    $setup = "$viewPath/Updates/Newest$release/.setupBuildEnvironment";
    if (-f $setup) {
	open (SETUP, $setup) or die ("can't open $file: $_");
	chomp(@lines = <SETUP>);
	close(SETUP);

	foreach $line (@lines) {
	    if ($line =~ /^setenv MORE_RC_CFLAGS/) {
		($result = $line) =~ s/^setenv MORE_RC_CFLAGS//;
		$result =~ s/^\s*\"(.*)\"\s*$/$1/;
		last;
	    }
	}
    }

    return $result;
}

sub sourceSetupForRelease {
    local($release) = @_;
    local($key, $value ,$setup, @lines, $line);

    $setup = "$viewPath/Updates/Newest$release/.setupBuildEnvironment";
    if (-f $setup) {
	open (SETUP, $setup) or die ("can't open $file: $_");
	chomp($top = <SETUP>);
	close(SETUP);

	if ($top =~ /perl/) {
	    # file is a perl script
	    eval (`cat $setup`) || die ("can't eval setup file: $file");
	} else {
	    # file is a csh script
	    system ("/bin/echo unsetenv \\* > /tmp/setup.$$");
	    system ("/bin/cat $setup >> /tmp/setup.$$");
	    system ("/bin/echo printenv >> /tmp/setup.$$");
	    chomp ($line = `/bin/csh /tmp/setup.$$`);
	    @lines = split ('\n', $line);
	    foreach (@lines) {
		if (/^(\S+)=(.*)$/) {
		    $ENV{$1}=$2;
		}
	    }
	    unlink "/tmp/setup.$$";
	}
    } 
}

sub buildParameters
{
    # initialize optional parameters
    foreach $arch (@archList) {
	$buildParams{"RC_$arch"} = '';
    }

    # loop through the archs
	$buildParams{'RC_ARCHS'} = join(' ', @archs);
	foreach $arch (@archs) {
	    if ($buildParams{'RC_CFLAGS'}) {
		$buildParams{'RC_CFLAGS'} .= " -arch $arch";
	    } else {
		$buildParams{'RC_CFLAGS'} = "-arch $arch";
	    }
	    $buildParams{"RC_$arch"} = 'YES';
	}
    
    # append other cflags
    $buildParams{'RC_CFLAGS'} .= " -pipe";
    $buildParams{'RC_NONARCH_CFLAGS'} = "-pipe";
    $buildParams{'RC_CFLAGS'} .= " $othercflags"
	if $othercflags;
    $buildParams{'RC_NONARCH_CFLAGS'} .= " $othercflags"
	if $othercflags;
    $buildParams{'RC_CFLAGS'} .= " $ENV{'MORE_RC_CFLAGS'}"
	if (defined($ENV{'MORE_RC_CFLAGS'}));
    $buildParams{'RC_NONARCH_CFLAGS'} .= " $ENV{'MORE_RC_CFLAGS'}"
	if (defined($ENV{'MORE_RC_CFLAGS'}));
   
    # we would like to obsolete this
#     if (-f "$NEXT_ROOT/System/Library/CoreServices/release_name") {
# 	$buildParams{'RC_RELEASE'} = `cat $NEXT_ROOT/System/Library/CoreServices/release_name`;
# 	chomp($buildParams{'RC_RELEASE'});
#     } elsif (-f "$NEXT_ROOT/usr/lib/NextStep/release_name") {
# 	$buildParams{'RC_RELEASE'} = `cat $NEXT_ROOT/usr/lib/NextStep/release_name`;
# 	chomp($buildParams{'RC_RELEASE'});
#     } else {
# 	$buildParams{'RC_RELEASE'} = 'undefined';
#     }
    if ($release) {
	$buildParams{'RC_RELEASE'} = $release;
    } else {
	$buildParams{'RC_RELEASE'} = 'undefined';
    }
    
    # this is a production build
    $ENV{'RC_XBS'} = 'YES';
    $buildParams{'RC_XBS'} = "$ENV{'RC_XBS'}";
    
    # this is probably obsolete now
    $ENV{'RC_JASPER'} = 'YES';
    $buildParams{'RC_JASPER'} = "$ENV{'RC_JASPER'}";
    
    # defer setting of NEXT_ROOT env var until here
    $ENV{'NEXT_ROOT'} = $NEXT_ROOT if $NEXT_ROOT;
    
#     # set LD_SEG_ADDR_TABLE
#     $ENV{'LD_SEG_ADDR_TABLE'} = '/AppleInternal/Developer/seg_addr_table'
#         if -f '/AppleInternal/Developer/seg_addr_table';
}

sub removeDirectory
{
    my($directory) = @_;
    
    # first chmod all subdirectories to be writable
    &executeCommand('find', $directory, '-type', 'd', '-exec', 'chmod', '+w', '{}', ';');
    
    # now remove the directory
    &executeCommand('rm', '-rf', $directory);
}

sub writeLogHeader
{
    my($phase) = @_;
    
    $plusLine = "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";

    # get cc version
    open(CC, "cc -v 2>&1 |") or die "**** ERROR **** cc -v failed: $!";
    while (<CC>) {
        next unless /version/;
	s/^.*Inc\. //;
	$ccVersion = $_;
	chomp($ccVersion);
	last;
    }
    close(CC);
    
    # get cctools version
    my($cctoolsADotOut) = "/tmp/__XX__buildit__cctools.a.out__XX__";
    open(CCTOOLS, "as -v < /dev/null -o $cctoolsADotOut 2>&1 |") or die "**** ERROR **** as -v: $!";
    while (<CCTOOLS>) {
        next unless /version/;
	s/^.*Inc\. //;
	$ccToolsVersion = $_;
	chomp($ccToolsVersion);
	last;
    }
    close(CCTOOLS);
    unlink($cctoolsADotOut);
    
    # write banner
    print LOG $plusLine;
    printf LOG "STARTING $buildit ($phase) of $projectVersion on %s", `date`;
    print LOG $plusLine;
    
    print  LOG "Build configuration:\n";
    print  LOG "    Build host:           $hostname\n";
    print  LOG "    Build Tool:           $buildTool\n";
    printf LOG "    Build Target:         %s\n", ($target ? $target : $phase);
    print  LOG "    Target Update:        \n";
    print  LOG "    Build Layer:          \n";
    printf LOG "    Search path:          %s\n", $ENV{'PATH'};
    print  LOG "    cc version:           $ccVersion\n";
    print  LOG "    cctools version:      $ccToolsVersion\n";
    print LOG $plusLine;

    # write the various fields
    print LOG "Build parameters are:\n";
    print LOG  "    SRCROOT:                 $srcRoot\n";
    if ($phase eq 'install') {
	print LOG  "    OBJROOT:                 $objRoot\n";
	print LOG  "    SYMROOT:                 $symRoot\n";
	print LOG  "    DSTROOT:                 $dstRoot\n";
    } else {
	print LOG  "    OBJROOT:                 $hdrObjRoot\n";
	print LOG  "    SYMROOT:                 $hdrSymRoot\n";
	print LOG  "    DSTROOT:                 $hdrDstRoot\n";
    }
    print  LOG "    RC_ProjectName:          $project\n";
    print  LOG "    RC_ProjectSourceVersion: $projectSourceVersion\n";
    print  LOG "    RC_ProjectBuildVersion:  1\n";
    print  LOG "    RC_ReleaseStatus:        Development\n";
    printf LOG "    RC_CFLAGS:               %s\n", $buildParams{'RC_CFLAGS'};
    printf LOG "    RC_NONARCH_CFLAGS:       %s\n", $buildParams{'RC_NONARCH_CFLAGS'};
    printf LOG "    RC_ARCHS:                %s\n", $buildParams{'RC_ARCHS'};
    printf LOG "    RC_hppa:                 %s\n", $buildParams{'RC_hppa'};
    printf LOG "    RC_i386:                 %s\n", $buildParams{'RC_i386'};
    printf LOG "    RC_m68k:                 %s\n", $buildParams{'RC_m68k'};
    printf LOG "    RC_ppc:                  %s\n", $buildParams{'RC_ppc'};
    printf LOG "    RC_ppc64:                %s\n", $buildParams{'RC_ppc64'};
    printf LOG "    RC_sparc:                %s\n", $buildParams{'RC_sparc'};
    printf LOG "    RC_RELEASE:              %s\n", $buildParams{'RC_RELEASE'};
    printf LOG "    RC_OS:                   %s\n", $buildParams{'RC_OS'};
    print  LOG "    RC_DEVROOT:              \n";
    printf LOG "    RC_XBS:                  %s\n", $buildParams{'RC_XBS'};
    printf LOG "    RC_JASPER:               %s\n", $buildParams{'RC_JASPER'};
    printf LOG "    NEXT_ROOT:               %s\n", $NEXT_ROOT;
    printf LOG "    Prebinding:              %s\n", (exists($ENV{'LD_PREBIND'}) ? "YES" : "NO");
    printf LOG "    Other Args:              %s\n", join(' ', @otherArguments);
    print LOG $plusLine;
    print LOG "Environment variables:\n";
    my($maxLength) = 0;
    for $envVar ( keys(%ENV) ) {
	$keyLength = length($envVar);
	$maxLength = $keyLength if $keyLength > $maxLength;
    }
    for $envVar ( sort(keys(%ENV)) ) {
	printf LOG "%-${maxLength}s \"%s\"\n", $envVar, $ENV{$envVar};
    }
    print LOG $plusLine;
}

sub writeLogFooter
{
    my($phase) = @_;
    
    # write banner
    print LOG "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
    printf LOG "FINISHED $buildit ($phase) of $projectVersion on %s", `date`;
    print LOG "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
}

sub doMake
{
    my($log, $phase) = @_;

    MAKEFORK: 
    {
	if ($pid = fork()) {		# parent (tee)
	    close(WRITE);
	    while(<READ>) {
		print LOG $_;
		print $_;
	    }
	    waitpid($pid, 0);
	    my($childStatus) = $? / 256;
	    exit($childStatus) if $childStatus;
	    exit(0);
	} elsif (defined($pid)) {	# child (make)
	    close(READ);
	    close(STDOUT);
	    close(STDERR);
	    open(STDOUT, ">&WRITE")
		|| die "Can't dupe STDOUT to WRITE\n";
	    open(STDERR, ">&WRITE")
		|| die "Can't dupe STDERR to WRITE\n";
	    &printMakeCommand();
	    exec(@makeCommand);
	} elsif ($! =~ /No more process/) {
	    sleep 5;
	    redo MAKEFORK;
	} else {
	    die "$buildit: Can't fork make process: $!\n";
	}
    }
}

sub doMerge
{
    my($root, $log) = @_;
    my($mergeSrc);

    # skip unless merge option is specified
    return unless $merge;
    
    # print to log
	open(LOG, ">>$log")
	    || die "Can't open $log: $!\n";
	select(LOG); $| = 1;
	select(STDOUT);

    # write to log
    print LOG "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
    printf LOG "Merging $root to $merge\n";
    print LOG "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
    
    # write to console
    print "\n*** $buildit ***: Merging $root to $merge\n";
    
    # make sure that merge directory exists
    &mkdirs($merge);
    
    # do the merge 
    # save STDOUT/STDERR, hook them up to the log file, run the command,
    # then restore STDOUT/STDERR
    open(SAVEOUT, ">&STDOUT");
    open(SAVEERR, ">&STDERR");
    open(STDOUT, ">&LOG") || die "Can't open $log: $!\n";
    open(STDERR, ">&LOG") || die "Can't open $log: $!\n";
    &executeCommand("ditto -V $root $merge");
    close(STDOUT);
    close(STDERR);
    open(STDOUT, ">&SAVEOUT") || die "Can't open $log: $!\n";
    open(STDERR, ">&SAVEERR") || die "Can't open $log: $!\n";
    close(SAVEOUT);
    close(SAVEERR);
    close(LOG);
}

sub doTee
{
    my($log, $phase) = @_;

    TEEFORK: 
    {
	if ($pid = fork()) {		# parent (buildit)
	    waitpid($pid, 0);
	    exit($? / 256) if $?;
	} elsif (defined($pid)) {	# child (tee)
	    pipe(READ, WRITE);
	    &doMake($log, $phase);
	} elsif ($! =~ /No more process/) {
	    sleep 5;
	    redo TEEFORK;
	} else {
	    die "$buildit: Can't fork tee process: $!\n";
	}
    }
}

sub consMakeCommand
{
    my($phase) = @_;

    # push the command line arguments onto the make command list
    @makeCommand = ();
    push(@makeCommand, $buildTool);

    # if PB or XCode build tool, emit action, target, and build style
    if ($buildTool =~ /(pb)|(pbx)|(xcode)build$/) {
        push(@makeCommand, $phase);
        push(@makeCommand, '-target', $target) if $target;
        push(@makeCommand, '-buildstyle', $buildstyle) if $buildstyle;
    } else {		# probably a Make-based build
        if ($phase eq 'install') {
            push(@makeCommand, ($target ? $target : $phase));
        } else {
            push(@makeCommand, $phase);
        }
    }

    push(@makeCommand, "SRCROOT=$srcRoot");
    if ($phase eq 'install') {
	push(@makeCommand, "OBJROOT=$objRoot");
	push(@makeCommand, "SYMROOT=$symRoot");
	push(@makeCommand, "DSTROOT=$dstRoot");
    } else {
	push(@makeCommand, "OBJROOT=$hdrObjRoot");
	push(@makeCommand, "SYMROOT=$hdrSymRoot");
	push(@makeCommand, "DSTROOT=$hdrDstRoot");
    }
    push(@makeCommand, "SUBLIBROOTS=$subRoot");
    push(@makeCommand, "RC_XBS=$buildParams{'RC_XBS'}");
    push(@makeCommand, "RC_JASPER=$buildParams{'RC_JASPER'}");
    push(@makeCommand, "RC_CFLAGS=$buildParams{'RC_CFLAGS'}");
    push(@makeCommand, "RC_NONARCH_CFLAGS=$buildParams{'RC_NONARCH_CFLAGS'}");
    push(@makeCommand, "RC_ARCHS=$buildParams{'RC_ARCHS'}");
    push(@makeCommand, "RC_hppa=$buildParams{'RC_hppa'}");
    push(@makeCommand, "RC_i386=$buildParams{'RC_i386'}");
    push(@makeCommand, "RC_m68k=$buildParams{'RC_m68k'}");
    push(@makeCommand, "RC_ppc=$buildParams{'RC_ppc'}");
    push(@makeCommand, "RC_ppc64=$buildParams{'RC_ppc64'}");
    push(@makeCommand, "RC_sparc=$buildParams{'RC_sparc'}");
    push(@makeCommand, "RC_OS=$buildParams{'RC_OS'}");
    push(@makeCommand, "RC_RELEASE=$buildParams{'RC_RELEASE'}");
    push(@makeCommand, "RC_ProjectName=$project");
    push(@makeCommand, "RC_ProjectSourceVersion=$projectSourceVersion");
    push(@makeCommand, "RC_ProjectBuildVersion=1");
    push(@makeCommand, "RC_ReleaseStatus=Development");
    push(@makeCommand, "NEXT_ROOT=$NEXT_ROOT");
    push(@makeCommand, @otherArguments);
}

sub printMakeCommand
{
    # cough out the make command, quoting whitespaced arguments
    for ($i = 0; $i <= $#makeCommand; $i++) {
	if ($makeCommand[$i] =~ /\s/) {
	    print "\"$makeCommand[$i]\" ";
	} else {
	    print "$makeCommand[$i] ";
	}
    }
    print "\n";
}

sub build
{
    my($phase, $log) = @_;
    
    print "\n*** $buildit ***: $buildTool $phase ...\n";
    
    # open the log
    open(LOG, ">$log")
	|| die "Can't open $log: $!\n";
    select(LOG); $| = 1;
    select(STDOUT);
	
    # write the log header
    &writeLogHeader($phase);
    
    # construct the make command
    &consMakeCommand($phase);
    
    #fork the tee process
    &doTee($log, $phase);
    
    # write the log footer
    &writeLogFooter($phase);
    
    # close the log
    close(LOG);
}

sub executeCommand
{
    system(@_);
    if ( $? ) {
        $lowByte = ($? & 0377);
        if ($lowByte == 0177) {
            die "Command \"@_\" has been stopped\n";
        } elsif ($lowByte) {
            $sigNum = ($lowByte & 0177);
            print "Command \"@_\" was terminated by signal $sigNum.\n";
            if ($lowByte & 0200) {
                print "Core dumped.\n";
            }
            exit 1;
        }
        $childExitStatus = (($? >> 8) & 0377);
        die "Command \"@_\" failed with exit status $childExitStatus\n" ;
    }
}

sub printenv {
    foreach (sort keys %ENV) {
        print "$_: $ENV{$_}\n";
    }
}
