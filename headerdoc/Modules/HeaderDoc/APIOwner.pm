#! /usr/bin/perl -w
#
# Class name: APIOwner
# Synopsis: Abstract superclass for Header and OO structures
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2001/11/30 22:43:17 $
# 
# Method additions by SKoT McDonald <skot@tomandandy.com> Aug 2001 
#
# Copyright (c) 1999-2001 Apple Computer, Inc.  All Rights Reserved.
# The contents of this file constitute Original Code as defined in and are
# subject to the Apple Public Source License Version 1.1 (the "License").
# You may not use this file except in compliance with the License.  Please
# obtain a copy of the License at http://www.apple.com/publicsource and
# read it before using this file.
#
# This Original Code and all software distributed under the License are
# distributed on an TAS ISU basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the License for
# the specific language governing rights and limitations under the
# License.
#
######################################################################
package HeaderDoc::APIOwner;

BEGIN {
	foreach (qw(Mac::Files)) {
	    $MOD_AVAIL{$_} = eval "use $_; 1";
    }
}
use HeaderDoc::HeaderElement;
use HeaderDoc::DBLookup;
use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash);

use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';

# Inheritance
@ISA = qw(HeaderDoc::HeaderElement);
################ Portability ###################################
my $isMacOS;
my $pathSeparator;
if ($^O =~ /MacOS/i) {
	$pathSeparator = ":";
	$isMacOS = 1;
} else {
	$pathSeparator = "/";
	$isMacOS = 0;
}
################ General Constants ###################################
my $debugging = 0;
my $theTime = time();
my ($sec, $min, $hour, $dom, $moy, $year, @rest);
($sec, $min, $hour, $dom, $moy, $year, @rest) = localtime($theTime);
$moy++;
$year += 1900;
my $dateStamp = "$moy/$dom/$year";
######################################################################


# class variables and accessors
{
    my $_copyrightOwner;
    my $_defaultFrameName;
    my $_compositePageName;
    my $_apiUIDPrefix;
    
    sub copyrightOwner {    
        my $class = shift;
        if (@_) {
            $_copyrightOwner = shift;
        }
        return $_copyrightOwner;
    }

    sub defaultFrameName {    
        my $class = shift;
        if (@_) {
            $_defaultFrameName = shift;
        }
        return $_defaultFrameName;
    }

    sub compositePageName {    
        my $class = shift;
        if (@_) {
            $_compositePageName = shift;
        }
        return $_compositePageName;
    }

    sub apiUIDPrefix {    
        my $class = shift;
        if (@_) {
            $_apiUIDPrefix = shift;
        }
        return $_apiUIDPrefix;
    }
}


sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
    bless($self, $class);
    $self->_initialize();
    return($self);
}

sub _initialize {
    my($self) = shift;

    $self->SUPER::_initialize();
    
    $self->{OUTPUTDIR} = undef;
    $self->{CONSTANTS} = ();
    $self->{FUNCTIONS} = ();
    $self->{METHODS} = ();
    $self->{TYPEDEFS} = ();
    $self->{STRUCTS} = ();
    $self->{VARS} = ();
    $self->{PDEFINES} = ();
    $self->{ENUMS} = ();
    $self->{CONSTANTSDIR} = undef;
    $self->{DATATYPESDIR} = undef;
    $self->{STRUCTSDIR} = undef;
    $self->{VARSDIR} = undef;
    $self->{FUNCTIONSDIR} = undef;
    $self->{METHODSDIR} = undef;
    $self->{PDEFINESDIR} = undef;
    $self->{ENUMSDIR} = undef;
    $self->{EXPORTSDIR} = undef;
    $self->{EXPORTINGFORDB} = 0;
    $self->{TOCTITLEPREFIX} = 'GENERIC_OWNER:';
    $self->{HEADEROBJECT} = undef;
} 

sub outputDir {
    my $self = shift;

    if (@_) {
        my $rootOutputDir = shift;
		if (-e $rootOutputDir) {
			if (! -d $rootOutputDir) {
			    die "Error: $rootOutputDir is not a directory. Exiting.\n\t$!\n";
			} elsif (! -w $rootOutputDir) {
			    die "Error: Output directory $rootOutputDir is not writable. Exiting.\n$!\n";
			}
		} else {
			unless (mkdir ("$rootOutputDir", 0777)) {
			    die ("Error: Can't create output folder $rootOutputDir.\n$!\n");
			}
	    }
        $self->{OUTPUTDIR} = $rootOutputDir;
	    $self->constantsDir("$rootOutputDir$pathSeparator"."Constants");
	    $self->datatypesDir("$rootOutputDir$pathSeparator"."DataTypes");
	    $self->structsDir("$rootOutputDir$pathSeparator"."Structs");
	    $self->functionsDir("$rootOutputDir$pathSeparator"."Functions");
	    $self->methodsDir("$rootOutputDir$pathSeparator"."Methods");
	    $self->varsDir("$rootOutputDir$pathSeparator"."Vars");
	    $self->pDefinesDir("$rootOutputDir$pathSeparator"."PDefines");
	    $self->enumsDir("$rootOutputDir$pathSeparator"."Enums");
	    $self->exportsDir("$rootOutputDir$pathSeparator"."Exports");
    }
    return $self->{OUTPUTDIR};
}

sub tocTitlePrefix {
    my $self = shift;

    if (@_) {
        $self->{TOCTITLEPREFIX} = shift;
    }
    return $self->{TOCTITLEPREFIX};
}

sub headerObject {
    my $self = shift;

    if (@_) {
        $self->{HEADEROBJECT} = shift;
    }
    return $self->{HEADEROBJECT};
}

sub exportingForDB {
    my $self = shift;

    if (@_) {
        $self->{EXPORTINGFORDB} = shift;
    }
    return $self->{EXPORTINGFORDB};
}

sub exportsDir {
    my $self = shift;

    if (@_) {
        $self->{EXPORTSDIR} = shift;
    }
    return $self->{EXPORTSDIR};
}

sub constantsDir {
    my $self = shift;

    if (@_) {
        $self->{CONSTANTSDIR} = shift;
    }
    return $self->{CONSTANTSDIR};
}


sub datatypesDir {
    my $self = shift;

    if (@_) {
        $self->{DATATYPESDIR} = shift;
    }
    return $self->{DATATYPESDIR};
}

sub structsDir {
    my $self = shift;

    if (@_) {
        $self->{STRUCTSDIR} = shift;
    }
    return $self->{STRUCTSDIR};
}

sub varsDir {
    my $self = shift;

    if (@_) {
        $self->{VARSDIR} = shift;
    }
    return $self->{VARSDIR};
}

sub pDefinesDir {
    my $self = shift;

    if (@_) {
        $self->{PDEFINESDIR} = shift;
    }
    return $self->{PDEFINESDIR};
}

sub enumsDir {
    my $self = shift;

    if (@_) {
        $self->{ENUMSDIR} = shift;
    }
    return $self->{ENUMSDIR};
}


sub functionsDir {
    my $self = shift;

    if (@_) {
        $self->{FUNCTIONSDIR} = shift;
    }
    return $self->{FUNCTIONSDIR};
}

sub methodsDir {
    my $self = shift;

    if (@_) {
        $self->{METHODSDIR} = shift;
    }
    return $self->{METHODSDIR};
}

sub tocString {
    my $self = shift;
    my $contentFrameName = $self->name();
    $contentFrameName =~ s/(.*)\.h/$1/; 
    $contentFrameName = &safeName(filename => $contentFrameName);  
    $contentFrameName = $contentFrameName . ".html";
    
    my @funcs = $self->functions();
    my @methods = $self->methods();
    my @constants = $self->constants();
    my @typedefs = $self->typedefs();
    my @structs = $self->structs();
    my @enums = $self->enums();
    my @pDefines = $self->pDefines();
    my $tocString = "<h3><a href=\"$contentFrameName\" target =\"doc\">Introduction</a></h3>\n";

    # output list of functions as TOC
    if (@funcs) {
	    $tocString .= "<h4>Functions</h4>\n";
	    foreach my $obj (sort objName @funcs) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href = \"Functions/Functions.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	    }
    }
    if (@methods) {
	    $tocString .= "<h4>Methods</h4>\n";
	    foreach my $obj (sort objName @methods) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href = \"Methods/Methods.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	    }
    }
    if (@typedefs) {
	    $tocString .= "<h4>Defined Types</h4>\n";
	    foreach my $obj (sort objName @typedefs) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href = \"DataTypes/DataTypes.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	    }
    }
    if (@structs) {
	    $tocString .= "<h4>Structs</h4>\n";
	    foreach my $obj (sort objName @structs) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href = \"Structs/Structs.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	    }
    }
    if (@constants) {
	    $tocString .= "<h4>Constants</h4>\n";
	    foreach my $obj (sort objName @constants) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href = \"Constants/Constants.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	    }
	}
    if (@enums) {
	    $tocString .= "<h4>Enumerations</h4>\n";
	    foreach my $obj (sort objName @enums) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href = \"Enums/Enums.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	    }
	}
    if (@pDefines) {
	    $tocString .= "<h4>#defines</h4>\n";
	    foreach my $obj (sort objName @pDefines) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href = \"PDefines/PDefines.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	    }
	}
    return $tocString;
}

sub enums {
    my $self = shift;

    if (@_) {
        @{ $self->{ENUMS} } = @_;
    }
    ($self->{ENUMS}) ? return @{ $self->{ENUMS} } : return ();
}

sub addToEnums {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{ENUMS} }, $item);
        }
    }
    return @{ $self->{ENUMS} };
}

sub pDefines {
    my $self = shift;

    if (@_) {
        @{ $self->{PDEFINES} } = @_;
    }
    ($self->{PDEFINES}) ? return @{ $self->{PDEFINES} } : return ();
}

sub addToPDefines {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{PDEFINES} }, $item);
        }
    }
    return @{ $self->{PDEFINES} };
}

sub constants {
    my $self = shift;

    if (@_) {
        @{ $self->{CONSTANTS} } = @_;
    }
    ($self->{CONSTANTS}) ? return @{ $self->{CONSTANTS} } : return ();
}

sub addToConstants {
    my $self = shift;
    
    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{CONSTANTS} }, $item);
        }
    }
    return @{ $self->{CONSTANTS} };
}


sub functions {
    my $self = shift;

    if (@_) {
        @{ $self->{FUNCTIONS} } = @_;
    }
    ($self->{FUNCTIONS}) ? return @{ $self->{FUNCTIONS} } : return ();
}

sub addToFunctions {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{FUNCTIONS} }, $item);
        }
    }
    return @{ $self->{FUNCTIONS} };
}

sub methods {
    my $self = shift;

    if (@_) {
        @{ $self->{METHODS} } = @_;
    }
    ($self->{METHODS}) ? return @{ $self->{METHODS} } : return ();
}

sub addToMethods {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{METHODS} }, $item);
        }
    }
    return @{ $self->{METHODS} };
}

sub typedefs {
    my $self = shift;

    if (@_) {
        @{ $self->{TYPEDEFS} } = @_;
    }
    ($self->{TYPEDEFS}) ? return @{ $self->{TYPEDEFS} } : return ();
}

sub addToTypedefs {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{TYPEDEFS} }, $item);
        }
    }
    return @{ $self->{TYPEDEFS} };
}

sub structs {
    my $self = shift;

    if (@_) {
        @{ $self->{STRUCTS} } = @_;
    }
    ($self->{STRUCTS}) ? return @{ $self->{STRUCTS} } : return ();
}

sub addToStructs {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{STRUCTS} }, $item);
        }
    }
    return @{ $self->{STRUCTS} };
}

sub vars {
    my $self = shift;

    if (@_) {
        @{ $self->{VARS} } = @_;
    }
    ($self->{VARS}) ? return @{ $self->{VARS} } : return ();
}

sub addToVars {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{VARS} }, $item);
        }
    }
    return @{ $self->{VARS} };
}

##################################################################

sub createFramesetFile {
    my $self = shift;
    my $docNavigatorComment = $self->docNavigatorComment();
    my $class = ref($self);
    my $defaultFrameName = $class->defaultFrameName();

    my $filename = $self->name();
    my $outDir = $self->outputDir();
    
    my $outputFile = "$outDir$pathSeparator$defaultFrameName";    
    my $rootFileName = $self->name();
    $rootFileName =~ s/(.*)\.h/$1/; 
    $rootFileName = &safeName(filename => $rootFileName);
    open(OUTFILE, ">$outputFile") || die "Can't write $outputFile. \n$!\n";
    if ($isMacOS) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};
    print OUTFILE "<html><head><title>Documentation for $filename</title></head>\n";
    print OUTFILE "<frameset cols=\"190,100%\">\n";
    print OUTFILE "<frame src=\"toc.html\" name=\"toc\">\n";
    print OUTFILE "<frame src=\"$rootFileName.html\" name=\"doc\">\n";
    print OUTFILE "</frameset></html>\n";
    print OUTFILE "$docNavigatorComment\n";
    close OUTFILE;
}

# Overridden by subclasses to return HTML comment that identifies the 
# index file (Header vs. Class, name, etc.)
sub docNavigatorComment {
    return "";
}

sub createTOCFile {
    my $self = shift;
    my $rootDir = $self->outputDir();
    my $tocTitlePrefix = $self->tocTitlePrefix();
    my $outputFileName = "toc.html";    
    my $outputFile = "$rootDir$pathSeparator$outputFileName";    
    my $fileString = $self->tocString();    
    my $filename = $self->name();    

	open(OUTFILE, ">$outputFile") || die "Can't write $outputFile.\n$!\n";
    if ($isMacOS) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};
	print OUTFILE "<html><head><title>Documentation for $filename</title></head>\n";
	print OUTFILE "<body bgcolor=\"#cccccc\">\n";
	print OUTFILE "<table border=\"0\" cellpadding=\"0\" cellspacing=\"2\" width=\"148\">\n";
	print OUTFILE "<tr><td colspan=\"2\"><font size=\"5\" color=\"#330066\"><b>$tocTitlePrefix</b></font></td></tr>\n";
	print OUTFILE "<tr><td width=\"15\"></td><td><b><font size=\"+1\">$filename</font></b></td></tr>\n";
	print OUTFILE "</table><hr>\n";
	print OUTFILE $fileString;
	print OUTFILE "</body></html>\n";
	close OUTFILE;
}

sub createContentFile {
    my $self = shift;
    my $class = ref($self);
    my $copyrightOwner = $class->copyrightOwner();
    my $headerName = $self->name();    
    my $rootFileName = $headerName;    
    $rootFileName =~ s/(.*)\.h/$1/; 
    # for now, always shorten long names since some files may be moved to a Mac for browsing
    if (1 || $isMacOS) {$rootFileName = &safeName(filename => $rootFileName);};
    my $outputFileName = "$rootFileName.html";    
    my $rootDir = $self->outputDir();
    my $outputFile = "$rootDir$pathSeparator$outputFileName";    
   	open (OUTFILE, ">$outputFile") || die "Can't write header-wide content page $outputFile. \n$!\n";
    if ($isMacOS) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};


    my $headerDiscussion = $self->discussion();    
    my $headerAbstract = $self->abstract();  
    if ((length($headerDiscussion)) || (length($headerAbstract))) {
		print OUTFILE "<HTML><HEAD><TITLE>API Documentation</TITLE></HEAD>\n<BODY bgcolor=\"#ffffff\">\n";
		print OUTFILE "<H1>$headerName</H1><hr>\n";
		if (length($headerAbstract)) {
		    print OUTFILE "<b>Abstract: </b>$headerAbstract<hr><br>\n";    
		}
		print OUTFILE "$headerDiscussion<br><br>\n";
    } else {
        # warn "No header-wide comment found. Creating dummy file for default content page.\n";
		print OUTFILE "<HTML><HEAD><TITLE>API Documentation</TITLE></HEAD>\n<BODY bgcolor=\"#ffffff\">\n";
		print OUTFILE "<H1>Documentation for $headerName</H1>\n";
		print OUTFILE "<hr>Use the links in the table of contents to the left to access documentation.<br>\n";    
    }
	print OUTFILE "<hr><br><center>";
	print OUTFILE "&#169; $copyrightOwner &#151; " if (length($copyrightOwner));
	print OUTFILE "(Last Updated $dateStamp)\n";
	print OUTFILE "<br>";
	print OUTFILE "<font size =\"-1\">HTML documentation generated by <a href=\"http://www.opensource.apple.com/projects\" target=\"_blank\">HeaderDoc</a></font>\n";    
	print OUTFILE "</center>\n";
	print OUTFILE "</BODY>\n</HTML>\n";
	close OUTFILE;
}

sub writeHeaderElements {
    my $self = shift;
    my $rootOutputDir = $self->outputDir();
    my $functionsDir = $self->functionsDir();
    my $methodsDir = $self->methodsDir();
    my $dataTypesDir = $self->datatypesDir();
    my $structsDir = $self->structsDir();
    my $constantsDir = $self->constantsDir();
    my $varsDir = $self->varsDir();
    my $enumsDir = $self->enumsDir();
    my $pDefinesDir = $self->pDefinesDir();

	if (! -e $rootOutputDir) {
		unless (mkdir ("$rootOutputDir", 0777)) {die ("Can't create output folder $rootOutputDir. \n$!");};
    }
    
    if ($self->functions()) {
		if (! -e $functionsDir) {
			unless (mkdir ("$functionsDir", 0777)) {die ("Can't create output folder $functionsDir. \n$!");};
	    }
	    $self->writeFunctions();
    }
    if ($self->methods()) {
		if (! -e $methodsDir) {
			unless (mkdir ("$methodsDir", 0777)) {die ("Can't create output folder $methodsDir. \n$!");};
	    }
	    $self->writeMethods();
    }
    
    if ($self->constants()) {
		if (! -e $constantsDir) {
			unless (mkdir ("$constantsDir", 0777)) {die ("Can't create output folder $constantsDir. \n$!");};
	    }
	    $self->writeConstants();
    }
    
    if ($self->typedefs()) {
		if (! -e $dataTypesDir) {
			unless (mkdir ("$dataTypesDir", 0777)) {die ("Can't create output folder $dataTypesDir. \n$!");};
	    }
	    $self->writeTypedefs();
    }
    
    if ($self->structs()) {
		if (! -e $structsDir) {
			unless (mkdir ("$structsDir", 0777)) {die ("Can't create output folder $structsDir. \n$!");};
	    }
	    $self->writeStructs();
    }
    if ($self->vars()) {
		if (! -e $varsDir) {
			unless (mkdir ("$varsDir", 0777)) {die ("Can't create output folder $varsDir. \n$!");};
	    }
	    $self->writeVars();
    }
    if ($self->enums()) {
		if (! -e $enumsDir) {
			unless (mkdir ("$enumsDir", 0777)) {die ("Can't create output folder $enumsDir. \n$!");};
	    }
	    $self->writeEnums();
    }

    if ($self->pDefines()) {
		if (! -e $pDefinesDir) {
			unless (mkdir ("$pDefinesDir", 0777)) {die ("Can't create output folder $pDefinesDir. \n$!");};
	    }
	    $self->writePDefines();
    }
}


sub writeHeaderElementsToCompositePage { # All API in a single HTML page -- for printing
    my $self = shift;
    my $class = ref($self);
    my $compositePageName = $class->compositePageName();
    my $rootOutputDir = $self->outputDir();
    my $name = $self->name();
    my $compositePageString = $self->_getCompositePageString();
    my $outputFile = $rootOutputDir.$pathSeparator.$compositePageName;
    
	if (! -e $rootOutputDir) {
		unless (mkdir ("$rootOutputDir", 0777)) {die ("Can't create output folder $rootOutputDir. $!");};
    }
    $self->_createHTMLOutputFile($outputFile, $compositePageString, "$name");
}

sub _getCompositePageString { 
    my $self = shift;
    my $name = $self->name();
    my $compositePageString;
    my $contentString;
    
    my $abstract = $self->abstract();
    if (length($abstract)) {
	    $compositePageString .= "<h2>Abstract</h2>\n";
	    $compositePageString .= $abstract;
    }

    my $discussion = $self->discussion();
    if (length($discussion)) {
	    $compositePageString .= "<h2>Discussion</h2>\n";
	    $compositePageString .= $discussion;
    }
    
    if ((length($abstract)) || (length($discussion))) {
	    $compositePageString .= "<hr><br>";
    }

    $contentString= $self->_getFunctionDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Functions</h2>\n";
	    $compositePageString .= $contentString;
    }
    $contentString= $self->_getMethodDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Methods</h2>\n";
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getConstantDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Constants</h2>\n";
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getTypedefDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Typedefs</h2>\n";
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getStructDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Structs</h2>\n";
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getVarDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Globals</h2>\n";
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getPDefineDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>#defines</h2>\n";
	    $compositePageString .= $contentString;
    }
    return $compositePageString;
}


sub writeFunctions {
    my $self = shift;
    my $functionFile = $self->functionsDir().$pathSeparator."Functions.html";
    $self->_createHTMLOutputFile($functionFile, $self->_getFunctionDetailString(), "Functions");
}

sub _getFunctionDetailString {
    my $self = shift;
    my @funcObjs = $self->functions();
    my $contentString = "";

    foreach my $obj (sort objName @funcObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeMethods {
    my $self = shift;
    my $methodFile = $self->methodsDir().$pathSeparator."Methods.html";
    $self->_createHTMLOutputFile($methodFile, $self->_getMethodDetailString(), "Methods");
}

sub _getMethodDetailString {
    my $self = shift;
    my @methObjs = $self->methods();
    my $contentString = "";

    foreach my $obj (sort objName @methObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeConstants {
    my $self = shift;
    my $constantsFile = $self->constantsDir().$pathSeparator."Constants.html";
    $self->_createHTMLOutputFile($constantsFile, $self->_getConstantDetailString(), "Constants");
}

sub _getConstantDetailString {
    my $self = shift;
    my @constantObjs = $self->constants();
    my $contentString;

    foreach my $obj (sort objName @constantObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeTypedefs {
    my $self = shift;
    my $typedefsFile = $self->datatypesDir().$pathSeparator."DataTypes.html";
    $self->_createHTMLOutputFile($typedefsFile, $self->_getTypedefDetailString(), "Defined Types");
}

sub _getTypedefDetailString {
    my $self = shift;
    my @typedefObjs = $self->typedefs();
    my $contentString;

    foreach my $obj (sort objName @typedefObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeStructs {
    my $self = shift;
    my $structsFile = $self->structsDir().$pathSeparator."Structs.html";
    $self->_createHTMLOutputFile($structsFile, $self->_getStructDetailString(), "Structs");
}

sub _getStructDetailString {
    my $self = shift;
    my @structObjs = $self->structs();
    my $contentString;

    foreach my $obj (sort objName @structObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeVars {
    my $self = shift;
    my $varsFile = $self->varsDir().$pathSeparator."Vars.html";
    $self->_createHTMLOutputFile($varsFile, $self->_getVarDetailString(), "Data Members");
}

sub _getVarDetailString {
    my $self = shift;
    my @varObjs = $self->vars();
    my $contentString;

    foreach my $obj (sort objName @varObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeEnums {
    my $self = shift;
    my $enumsFile = $self->enumsDir().$pathSeparator."Enums.html";
    $self->_createHTMLOutputFile($enumsFile, $self->_getEnumDetailString(), "Enumerations");
}

sub _getEnumDetailString {
    my $self = shift;
    my @enumObjs = $self->enums();
    my $contentString;

    foreach my $obj (sort objName @enumObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writePDefines {
    my $self = shift;
    my $pDefinesFile = $self->pDefinesDir().$pathSeparator."PDefines.html";
    $self->_createHTMLOutputFile($pDefinesFile, $self->_getPDefineDetailString(), "#defines");
}

sub _getPDefineDetailString {
    my $self = shift;
    my @pDefineObjs = $self->pDefines();
    my $contentString;

    foreach my $obj (sort objName @pDefineObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}


sub writeExportsWithName {
    my $self = shift;
    my $name = shift;
    my $exportsDir = $self->exportsDir();
    my $functionsFile = $exportsDir.$pathSeparator.$name.".ftab";
    my $methodsFile = $exportsDir.$pathSeparator.$name.".ftab";
    my $parametersFile = $exportsDir.$pathSeparator.$name.".ptab";
    my $structsFile = $exportsDir.$pathSeparator.$name.".stab";
    my $fieldsFile = $exportsDir.$pathSeparator.$name.".mtab";
    my $enumeratorsFile = $exportsDir.$pathSeparator.$name.".ktab";
    my $funcString;
    my $methString;
    my $paramString;
    my $dataTypeString;
    my $typesFieldString;
    my $enumeratorsString;
    
	if (! -e $exportsDir) {
		unless (mkdir ("$exportsDir", 0777)) {die ("Can't create output folder $exportsDir. $!");};
    }
    ($funcString, $paramString) = $self->_getFunctionsAndParamsExportString();
    ($methString, $paramString) = $self->_getMethodsAndParamsExportString();
    ($dataTypeString, $typesFieldString) = $self->_getDataTypesAndFieldsExportString();
    $enumeratorsString = $self->_getEnumeratorsExportString();
    
    $self->_createExportFile($functionsFile, $funcString);
    $self->_createExportFile($methodsFile, $methString);
    $self->_createExportFile($parametersFile, $paramString);
    $self->_createExportFile($structsFile, $dataTypeString);
    $self->_createExportFile($fieldsFile, $typesFieldString);
    $self->_createExportFile($enumeratorsFile, $enumeratorsString);
}

sub _getFunctionsAndParamsExportString {
    my $self = shift;
    my @funcObjs = $self->functions();
    my $tmpString = "";
    my @funcLines;
    my @paramLines;
    my $funcString;
    my $paramString;
    my $sep = "<tab>";       
    
    foreach my $obj (sort objName @funcObjs) {
        my $funcName = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
		my $declaration = $obj->declaration();
        my @taggedParams = $obj->taggedParameters();
        my @parsedParams = $obj->parsedParameters();
        my $result = $obj->result();
        my $funcID = HeaderDoc::DBLookup->functionIDForName($funcName);
        # unused fields--declaring them for visibility in the string below
        my $managerID = "";
        my $funcEnglishName = "";
        my $specialConsiderations = "";
        my $versionNotes = "";
        my $groupName = "";
        my $order = "";
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract, $declaration, $result) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $managerID.$sep.$funcID.$sep.$funcName.$sep.$funcEnglishName.$sep.$abstract.$sep.$desc.$sep.$result.$sep.$specialConsiderations.$sep.$versionNotes.$sep.$groupName.$sep.$order;
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@funcLines, "$tmpString");
        
        if (@taggedParams) {
            my %parsedParmNameToObjHash;
        	# make lookup hash of parsed params
        	foreach my $parsedParam (@parsedParams) {
        		$parsedParmNameToObjHash{$parsedParam->name()} = $parsedParam;
        	}
        	foreach my $taggedParam (@taggedParams) {
        	    my $tName = $taggedParam->name();
        	    my $pObj;
		        my $pos = "UNKNOWN_POSITION";
		        my $type = "UNKNOWN_TYPE";

		        if (exists $parsedParmNameToObjHash{$tName}) {
		            $pObj = $parsedParmNameToObjHash{$tName};
		        	$pos = $pObj->position();
		        	$type = $pObj->type();
		        } else {
		        	print "---------------------------------------------------------------------------\n";
		        	warn "Tagged parameter '$tName' not found in declaration of function $funcName.\n";
		        	warn "Parsed declaration for $funcName is:\n$declaration\n";
		        	warn "Parsed params for $funcName are:\n";
		        	foreach my $pp (@parsedParams) {
		        	    my $n = $pp->name();
		        	    print "$n\n";
		        	}
		        	print "---------------------------------------------------------------------------\n";
		        }
	        	my $paramName = $taggedParam->name();
	        	my $disc = $taggedParam->discussion();
	     		$disc =~ s/\n<br><br>\n/\n\n/g;
	     		$disc =~ s/([^\n])\n([^\n])/$1 $2/g;
	        	my $tmpParamString = "";
	        	
	        	$tmpParamString = $funcID.$sep.$funcName.$sep.$pos.$sep.$disc.$sep.$sep.$sep.$paramName.$sep.$type;
        		$tmpParamString = &convertCharsForFileMaker($tmpParamString);
        		$tmpParamString =~ s/$sep/\t/g;
	        	push (@paramLines, "$tmpParamString");
        	}
        }
    }
    $funcString = join ("\n", @funcLines);
    $paramString = join ("\n", @paramLines);
    $funcString .= "\n";
    $paramString .= "\n";
    return ($funcString, $paramString);
}

sub _getMethodsAndParamsExportString {
    my $self = shift;
    my @methObjs = $self->methods();
    my $tmpString = "";
    my @methLines;
    my @paramLines;
    my $methString;
    my $paramString;
    my $sep = "<tab>";       
    
    foreach my $obj (sort objName @methObjs) {
        my $methName = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
		my $declaration = $obj->declaration();
        my @taggedParams = $obj->taggedParameters();
        my @parsedParams = $obj->parsedParameters();
        my $result = $obj->result();
        my $methID = HeaderDoc::DBLookup->methodIDForName($methName);
        # unused fields--declaring them for visibility in the string below
        my $managerID = "";
        my $methEnglishName = "";
        my $specialConsiderations = "";
        my $versionNotes = "";
        my $groupName = "";
        my $order = "";
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract, $declaration, $result) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $managerID.$sep.$methID.$sep.$methName.$sep.$methEnglishName.$sep.$abstract.$sep.$desc.$sep.$result.$sep.$specialConsiderations.$sep.$versionNotes.$sep.$groupName.$sep.$order;
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@methLines, "$tmpString");
        
        if (@taggedParams) {
            my %parsedParmNameToObjHash;
        	# make lookup hash of parsed params
        	foreach my $parsedParam (@parsedParams) {
        		$parsedParmNameToObjHash{$parsedParam->name()} = $parsedParam;
        	}
        	foreach my $taggedParam (@taggedParams) {
        	    my $tName = $taggedParam->name();
        	    my $pObj;
		        my $pos = "UNKNOWN_POSITION";
		        my $type = "UNKNOWN_TYPE";

		        if (exists $parsedParmNameToObjHash{$tName}) {
		            $pObj = $parsedParmNameToObjHash{$tName};
		        	$pos = $pObj->position();
		        	$type = $pObj->type();
		        } else {
		        	print "---------------------------------------------------------------------------\n";
		        	warn "Tagged parameter '$tName' not found in declaration of method $methName.\n";
		        	warn "Parsed declaration for $methName is:\n$declaration\n";
		        	warn "Parsed params for $methName are:\n";
		        	foreach my $pp (@parsedParams) {
		        	    my $n = $pp->name();
		        	    print "$n\n";
		        	}
		        	print "---------------------------------------------------------------------------\n";
		        }
	        	my $paramName = $taggedParam->name();
	        	my $disc = $taggedParam->discussion();
	     		$disc =~ s/\n<br><br>\n/\n\n/g;
	     		$disc =~ s/([^\n])\n([^\n])/$1 $2/g;
	        	my $tmpParamString = "";
	        	
	        	$tmpParamString = $methID.$sep.$methName.$sep.$pos.$sep.$disc.$sep.$sep.$sep.$paramName.$sep.$type;
        		$tmpParamString = &convertCharsForFileMaker($tmpParamString);
        		$tmpParamString =~ s/$sep/\t/g;
	        	push (@paramLines, "$tmpParamString");
        	}
        }
    }
    $methString = join ("\n", @methLines);
    $paramString = join ("\n", @paramLines);
    $methString .= "\n";
    $paramString .= "\n";
    return ($methString, $paramString);
}


sub _getDataTypesAndFieldsExportString {
    my $self = shift;
    my @structObjs = $self->structs();
    my @typedefs = $self->typedefs();
    my @constants = $self->constants();
    my @enums = $self->enums();
    my @dataTypeLines;
    my @fieldLines;
    my $dataTypeString;
    my $fieldString;

    my $sep = "<tab>";       
    my $tmpString = "";
    my $contentString = "";
    
    # unused fields -- here for clarity
    my $englishName = "";
    my $specialConsiderations = "";
    my $versionNotes = "";
    
    # get Enumerations
    foreach my $obj (sort objName @enums) {
        my $name = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
        my $declaration = $obj->declaration();
        my $enumID = HeaderDoc::DBLookup->typeIDForName($name);
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract, $declaration) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $enumID.$sep.$name.$sep.$englishName.$sep.$abstract.$sep.$desc.$sep.$specialConsiderations.$sep.$versionNotes."\n";
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@dataTypeLines, "$tmpString");
    }
    # get Constants
    foreach my $obj (sort objName @constants) {
        my $name = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
        my $constID = HeaderDoc::DBLookup->typeIDForName($name);
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $constID.$sep.$name.$sep.$englishName.$sep.$abstract.$sep.$desc.$sep.$specialConsiderations.$sep.$versionNotes."\n";
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@dataTypeLines, "$tmpString");
    }
    # get Structs
    foreach my $obj (sort objName @structObjs) {
        my $name = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
        my $declaration = $obj->declaration();
        my @fields = $obj->fields();
        my $structID = HeaderDoc::DBLookup->typeIDForName($name);
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract, $declaration) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $structID.$sep.$name.$sep.$englishName.$sep.$abstract.$sep.$desc.$sep.$specialConsiderations.$sep.$versionNotes."\n";
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@dataTypeLines, "$tmpString");
        
        if (@fields) {
        	foreach my $field (@fields) {
        	    my $fName = $field->name();
        	    my $discussion = $field->discussion();
	     		$discussion =~ s/\n<br><br>\n/\n\n/g;
	     		$discussion =~ s/([^\n])\n([^\n])/$1 $2/g;
        	    my $pos = 0;
        	    
        	    $pos = $self->_positionOfNameInBlock($fName, $declaration);
		        if (!$pos) {
		        	print "---------------------------------------------------------------------------\n";
		        	warn "Tagged parameter '$fName' not found in declaration of struct $name.\n";
		        	warn "Declaration for $name is:\n$declaration\n";
		        	print "---------------------------------------------------------------------------\n";
		        	$pos = "UNKNOWN_POSITION";
		        }
	        	my $tmpFieldString = "";
	        	$tmpFieldString = $structID.$sep.$name.$sep.$pos.$sep.$discussion.$sep.$sep.$sep.$fName;
        		$tmpFieldString = &convertCharsForFileMaker($tmpFieldString);
        		$tmpFieldString =~ s/$sep/\t/g;
	        	push (@fieldLines, "$tmpFieldString");
        	}
        }
    }
    # get Typedefs
    foreach my $obj (sort objName @typedefs) {
        my $name = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
        my $declaration = $obj->declaration();
        my @fields = $obj->fields();
        my $isFunctionPointer = $obj->isFunctionPointer();
        my $typedefID = HeaderDoc::DBLookup->typeIDForName($name);
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract, $declaration) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $typedefID.$sep.$name.$sep.$englishName.$sep.$abstract.$sep.$desc.$sep.$specialConsiderations.$sep.$versionNotes."\n";
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@dataTypeLines, "$tmpString");
        if (@fields) {
        	foreach my $field (@fields) {
        	    my $fName = $field->name();
        	    my $discussion = $field->discussion();
	     		$discussion =~ s/\n<br><br>\n/\n\n/g;
	     		$discussion =~ s/([^\n])\n([^\n])/$1 $2/g;
        	    my $pos = 0;
        	    
        	    if ($isFunctionPointer) {
        	        $pos = $self->_positionOfNameInFuncPtrDec($fName, $declaration);
        	    } else {
        	        $pos = $self->_positionOfNameInBlock($fName, $declaration);
        	    }
		        if (!$pos) {
		        	print "---------------------------------------------------------------------------\n";
		        	warn "Tagged parameter '$fName' not found in declaration of struct $name.\n";
		        	warn "Declaration for $name is:\n$declaration\n";
		        	print "---------------------------------------------------------------------------\n";
		        	$pos = "UNKNOWN_POSITION";
		        }
	        	my $tmpFieldString = "";
	        	$tmpFieldString = $typedefID.$sep.$name.$sep.$pos.$sep.$discussion.$sep.$sep.$sep.$fName;
        		$tmpFieldString = &convertCharsForFileMaker($tmpFieldString);
        		$tmpFieldString =~ s/$sep/\t/g;
	        	push (@fieldLines, "$tmpFieldString");
        	}
        }
    }
    $dataTypeString = join ("\n", @dataTypeLines);
    $fieldString = join ("\n", @fieldLines);
	$dataTypeString .= "\n";
	$fieldString .= "\n";
    return ($dataTypeString, $fieldString);
}


sub _getEnumeratorsExportString {
    my $self = shift;
    my @enums = $self->enums();
    my @fieldLines;
    my $fieldString;

    my $sep = "<tab>";       
    my $tmpString = "";
    my $contentString = "";
    
    # get Enumerations
    foreach my $obj (sort objName @enums) {
        my $name = $obj->name();
        my @constants = $obj->constants();
        my $declaration = $obj->declaration();
        my $enumID = HeaderDoc::DBLookup->typeIDForName($name);
        
        if (@constants) {
        	foreach my $enumerator (@constants) {
        	    my $fName = $enumerator->name();
        	    my $discussion = $enumerator->discussion();
        	    my $pos = 0;
        	    
	     		$discussion =~ s/\n<br><br>\n/\n\n/g;
	     		$discussion =~ s/([^\n])\n([^\n])/$1 $2/g;
        	    $pos = $self->_positionOfNameInEnum($fName, $declaration);
		        if (!$pos) {
		        	print "---------------------------------------------------------------------------\n";
		        	warn "Tagged parameter '$fName' not found in declaration of enum $name.\n";
		        	warn "Declaration for $name is:\n$declaration\n";
		        	print "---------------------------------------------------------------------------\n";
		        	$pos = "UNKNOWN_POSITION";
		        }
	        	my $tmpFieldString = "";
	        	$tmpFieldString = $enumID.$sep.$name.$sep.$pos.$sep.$discussion.$sep.$sep.$sep.$fName;
        		$tmpFieldString = &convertCharsForFileMaker($tmpFieldString);
        		$tmpFieldString =~ s/$sep/\t/g;
	        	push (@fieldLines, "$tmpFieldString");
        	}
        }
    }
    $fieldString = join ("\n", @fieldLines);
	$fieldString .= "\n";
    return $fieldString;
}

# this is simplistic is various ways--should be made more robust.
sub _positionOfNameInBlock {
    my $self = shift;
    my $name = shift;    
    my $block = shift;
    $block =~ s/\n/ /g;
    
    my $pos = 0;
    my $i = 0;
    my @chunks = split (/;/, $block);
    foreach my $string (@chunks) {
        $i++;
        $string = quotemeta($string);
        if ($string =~ /$name/) {
            $pos = $i;
            last;
        }
    }
    return $pos;
}

sub _positionOfNameInEnum {
    my $self = shift;
    my $name = shift;    
    my $block = shift;
    $block =~ s/\n/ /g;
    
    my $pos = 0;
    my $i = 0;
    my @chunks = split (/,/, $block);
    foreach my $string (@chunks) {
        $i++;
        $string = quotemeta($string);
        if ($string =~ /$name/) {
            $pos = $i;
            last;
        }
    }
    return $pos;
}

sub _positionOfNameInFuncPtrDec {
    my $self = shift;
    my $name = shift;    
    my $dec = shift;
    $dec =~ s/\n/ /g;
    
    my @decParts = split (/\(/, $dec);
    my $paramList = pop @decParts;
    
    
    my $pos = 0;
    my $i = 0;
    my @chunks = split (/,/, $paramList);
    foreach my $string (@chunks) {
        $i++;
        $string = quotemeta($string);
        if ($string =~ /$name/) {
            $pos = $i;
            last;
        }
    }
    return $pos;
}

sub _positionOfNameInMethPtrDec {
    my $self = shift;
    my $name = shift;    
    my $dec = shift;
    $dec =~ s/\n/ /g;
    
    my @decParts = split (/\(/, $dec);
    my $paramList = pop @decParts;
    
    my $pos = 0;
    my $i = 0;
    my @chunks = split (/,/, $paramList);
    foreach my $string (@chunks) {
        $i++;
        $string = quotemeta($string);
        if ($string =~ /$name/) {
            $pos = $i;
            last;
        }
    }
    return $pos;
}

sub _createExportFile {
    my $self = shift;
    my $outputFile = shift;    
    my $fileString = shift;    

	open(OUTFILE, ">$outputFile") || die "Can't write $outputFile.\n";
    if ($^O =~ /MacOS/i) {MacPerl::SetFileInfo('R*ch', 'TEXT', "$outputFile");};
	print OUTFILE $fileString;
	close OUTFILE;
}

sub _createHTMLOutputFile {
    my $self = shift;
    my $class = ref($self);
    my $copyrightOwner = $class->copyrightOwner();
    my $outputFile = shift;    
    my $fileString = shift;    
    my $heading = shift;    

	open(OUTFILE, ">$outputFile") || die "Can't write $outputFile.\n";
    if ($^O =~ /MacOS/i) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};
	print OUTFILE "<html><head><title>$heading</title></head><body bgcolor=\"#ffffff\"><font face=\"Geneva,Arial,Helvtica\"><h1>$heading</h1></font><hr><br>\n";
	print OUTFILE $fileString;
    print OUTFILE "<p>";
    print OUTFILE "<p>&#169; $copyrightOwner &#151; " if (length($copyrightOwner));
    print OUTFILE "(Last Updated $dateStamp)\n";
    print OUTFILE "</p>";
	print OUTFILE "</body></html>\n";
	close OUTFILE;
}

sub objName { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   return ($obj1->name() cmp $obj2->name());
}

##################### Debugging ####################################

sub printObject {
    my $self = shift;
 
    print "------------------------------------\n";
    print "APIOwner\n";
    print "outputDir: $self->{OUTPUTDIR}\n";
    print "constantsDir: $self->{CONSTANTSDIR}\n";
    print "datatypesDir: $self->{DATATYPESDIR}\n";
    print "functionsDir: $self->{FUNCTIONSDIR}\n";
    print "methodsDir: $self->{METHODSDIR}\n";
    print "typedefsDir: $self->{TYPEDEFSDIR}\n";
    print "constants:\n";
    &printArray(@{$self->{CONSTANTS}});
    print "functions:\n";
    &printArray(@{$self->{FUNCTIONS}});
    print "methods:\n";
    &printArray(@{$self->{METHODS}});
    print "typedefs:\n";
    &printArray(@{$self->{TYPEDEFS}});
    print "structs:\n";
    &printArray(@{$self->{STRUCTS}});
    print "Inherits from:\n";
    $self->SUPER::printObject();
}

1;

